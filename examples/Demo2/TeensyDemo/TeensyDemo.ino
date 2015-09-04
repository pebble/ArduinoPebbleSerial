#include <ArduinoPebbleSerial.h>

static const uint16_t SERVICE_ID = 0x1001;
static const uint16_t LED_ATTRIBUTE_ID = 0x0001;
static const size_t LED_ATTRIBUTE_LENGTH = 1;
static const uint16_t UPTIME_ATTRIBUTE_ID = 0x0002;
static const size_t UPTIME_ATTRIBUTE_LENGTH = 4;

static const uint16_t SERVICES[] = {SERVICE_ID};
static const uint8_t NUM_SERVICES = 1;

static const uint8_t PEBBLE_DATA_PIN = 1;
static uint8_t buffer[GET_PAYLOAD_BUFFER_SIZE(4)];


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
#if defined(__MK20DX256__) || defined(__MK20DX128__)
  // Teensy 3.0/3.1 uses hardware serial mode (pins 0/1) with RX/TX shorted together
  ArduinoPebbleSerial::begin_hardware(buffer, sizeof(buffer), Baud57600, SERVICES, NUM_SERVICES);
#elif defined(__AVR_ATmega32U4__)
  // Teensy 2.0 uses the one-wire software serial mode
  ArduinoPebbleSerial::begin_software(PEBBLE_DATA_PIN, buffer, sizeof(buffer), Baud57600, SERVICES,
                                      NUM_SERVICES);
#else
#error "This example will only work for the Teensy 2.0, 3.0, or 3.1 boards"
#endif
}

void handle_uptime_request(RequestType type, size_t length) {
  if (type != RequestTypeRead) {
    // unexpected request type
    return;
  }
  // write back the current uptime
  const uint32_t uptime = millis() / 1000;
  ArduinoPebbleSerial::write(true, (uint8_t *)&uptime, sizeof(uptime));
}

void handle_led_request(RequestType type, size_t length) {
  if (type != RequestTypeWrite) {
    // unexpected request type
    return;
  } else if (length != LED_ATTRIBUTE_LENGTH) {
    // unexpected request length
    return;
  }
  // set the LED
  digitalWrite(LED_BUILTIN, (bool) buffer[0]);
  // ACK that the write request was received
  ArduinoPebbleSerial::write(true, NULL, 0);
}

void loop() {
  if (ArduinoPebbleSerial::is_connected()) {
    static uint32_t last_notify_time = 0;
    const uint32_t current_time = millis() / 1000;
    if (current_time > last_notify_time) {
      ArduinoPebbleSerial::notify(SERVICE_ID, UPTIME_ATTRIBUTE_ID);
      last_notify_time = current_time;
    }
  }

  uint16_t service_id;
  uint16_t attribute_id;
  size_t length;
  RequestType type;
  if (ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type)) {
    // process the request
    if (service_id == SERVICE_ID) {
      switch (attribute_id) {
        case UPTIME_ATTRIBUTE_ID:
          handle_uptime_request(type, length);
          break;
        case LED_ATTRIBUTE_ID:
          handle_led_request(type, length);
          break;
        default:
          break;
      }
    }
  }
}

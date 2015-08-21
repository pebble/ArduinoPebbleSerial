#include <Arduino.h>
#include <ArduinoPebbleSerial.h>
#include <Wire.h>

#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))
#define PEBBLE_PIN        1
STATIC_ASSERT_VALID_ONE_WIRE_SOFT_SERIAL_PIN(PEBBLE_PIN);
static const uint16_t supported_services[] = {0x0000, 0x1001};
#define RECV_BUFFER_SIZE  200
static uint8_t pebble_buffer[RECV_BUFFER_SIZE];
static uint32_t connected_time = 0;

void setup() {
  // General init
  Serial.begin(115200);
  pinMode(PIN_D6, OUTPUT);
  digitalWrite(PIN_D6, LOW);

  // Setup the Pebble smartstrap connection using one wire software serial
  ArduinoPebbleSerial::begin_software(PEBBLE_PIN, pebble_buffer, RECV_BUFFER_SIZE, Baud57600,
                                      supported_services, ARRAY_LENGTH(supported_services));
}

void loop() {
  static uint32_t last_print = 0;
  if (millis() > last_print + 2000) {
    Serial.print(connected_time, DEC);
    Serial.print(' ');
    Serial.println(millis(), DEC);
    last_print = millis();
  }

  // Let the ArduinoPebbleSerial code do its processing
  size_t length;
  uint16_t service_id;
  uint16_t attribute_id;
  RequestType type;
  if (ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type)) {
    if ((service_id == 0) && (attribute_id == 0)) {
      // we have a raw data frame to process
      static bool led_status = false;
      led_status = !led_status;
      digitalWrite(PIN_D6, led_status);
      Serial.println("READ");
      if (type == RequestTypeRead) {
        // send a response to the Pebble - reuse the same buffer for the response
        uint32_t current_time = millis();
        memcpy(pebble_buffer, &current_time, 4);
        ArduinoPebbleSerial::write(true, pebble_buffer, 4);
        Serial.println("WRITE");
      } else {
        Serial.print("GOT RAW WRITE: ");
        Serial.println((uint8_t)pebble_buffer[0], DEC);
      }
    } else if ((service_id == 0x1001) && (attribute_id == 0x1001)) {
      static uint32_t s_test_attr_data = 99999;
      if (length == 4) {
        // read the previous value and write the new one
        uint32_t old_value = s_test_attr_data;
        memcpy(&s_test_attr_data, pebble_buffer, sizeof(s_test_attr_data));
        ArduinoPebbleSerial::write(true, (const uint8_t *)&old_value,
                                     sizeof(old_value));
      } else {
        ArduinoPebbleSerial::write(true, NULL, 0);
      }
    } else {
      ArduinoPebbleSerial::write(false, NULL, 0);
    }
  }

  if (ArduinoPebbleSerial::is_connected()) {
    if (!connected_time) {
      connected_time = millis();
    }
    // notify the pebble every 250ms
    static uint32_t last_check = 0;
    if (last_check == 0) {
      last_check = millis();
    }
    if (millis() - last_check  > 2500) {
      Serial.println("NOTIFY");
      ArduinoPebbleSerial::notify(0x1001, 0x1001);
      last_check = millis();
    }
  } else {
    connected_time = 0;
  }
}

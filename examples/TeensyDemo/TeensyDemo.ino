#include <Arduino.h>
#include <ArduinoPebbleSerial.h>
#include <Wire.h>

#define RECV_BUFFER_SIZE 200
static uint8_t pebble_buffer[RECV_BUFFER_SIZE];
static uint32_t connected_time = 0;

void setup() {
  // General init
  Serial.begin(115200);
  pinMode(PIN_D6, OUTPUT);
  digitalWrite(PIN_D6, LOW);

  // Setup the Pebble smartstrap connection
  ArduinoPebbleSerial::begin(pebble_buffer, RECV_BUFFER_SIZE);
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
  bool is_read;
  if (ArduinoPebbleSerial::feed(&length, &is_read)) {
    // we have a frame to process
    static bool led_status = false;
    led_status = !led_status;
    digitalWrite(PIN_D6, led_status);
    Serial.println("READ");
    if (is_read) {
      // send a response to the Pebble - reuse the same buffer for the response
      memset(pebble_buffer, 0, RECV_BUFFER_SIZE);
      snprintf((char *)pebble_buffer, RECV_BUFFER_SIZE, "%lu", (millis()-connected_time)/1000);
      ArduinoPebbleSerial::write(pebble_buffer, strlen((char *)pebble_buffer));
      Serial.println("WRITE");
    }
  }

  if (connected_time) {
    // notify the pebble every 500ms
    static uint32_t last_check = 0;
    if (millis() - last_check  > 1000) {
      Serial.println("NOTIFY");
      ArduinoPebbleSerial::notify();
      last_check = millis();
    }
  } else if (ArduinoPebbleSerial::is_connected()) {
    connected_time = millis();
  }
}

#include <Arduino.h>
#include <ArduinoPebbleSerial.h>
#include <Wire.h>
#include <xadow.h>

#define RECV_BUFFER_SIZE 200
static uint8_t pebble_buffer[RECV_BUFFER_SIZE];
static uint32_t connected_time = 0;

void setup() {
  // General init
  Serial.begin(115200);

  // Xadow init
  Xadow.init();
  Xadow.greenLed(LEDOFF);
  Xadow.redLed(LEDOFF);

  // Setup the Pebble smartstrap connection
  ArduinoPebbleSerial::begin(pebble_buffer, RECV_BUFFER_SIZE);
}

void loop() {
  // Let the ArduinoPebbleSerial code do its processing
  size_t length;
  bool is_read;
  if (ArduinoPebbleSerial::feed(&length, &is_read)) {
    // we have a frame to process
    if (is_read) {
      // send a response to the Pebble - reuse the same buffer for the response
      memset(pebble_buffer, 0, RECV_BUFFER_SIZE);
      uint16_t voltage = (uint16_t) (Xadow.getBatVol() * 1000);
      snprintf((char *)pebble_buffer, RECV_BUFFER_SIZE, "%lus / %dmV",
               (millis()-connected_time)/1000, voltage);
      ArduinoPebbleSerial::write(pebble_buffer, strlen((char *)pebble_buffer));
    }
    Xadow.redLed(LEDCHG);
  }

  if (connected_time) {
    // notify the pebble every 500ms
    static uint32_t last_check = 0;
    if (millis() - last_check  > 500) {
      ArduinoPebbleSerial::notify();
      last_check = millis();
    }
  } else if (ArduinoPebbleSerial::is_connected()) {
    connected_time = millis();
    Xadow.greenLed(LEDON);
  }
}


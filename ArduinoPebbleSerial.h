/*
 * This is an Arduino library wrapper around the PebbleSerial library.
 */
#ifndef __ARDUINO_PEBBLE_SERIAL_H__
#define __ARDUINO_PEBBLE_SERIAL_H__

#include <Arduino.h>

class ArduinoPebbleSerial {
public:
  static void begin(uint8_t *buffer, size_t length);
  static bool feed(size_t *length, bool *is_read);
  static bool write(const uint8_t *payload, size_t length);
  static void notify(void);
  static bool is_connected(void);
};

#endif //__ARDUINO_PEBBLE_SERIAL_H__

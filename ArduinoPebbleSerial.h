/*
 * This is an Arduino library wrapper around the PebbleSerial library.
 */
#ifndef __ARDUINO_PEBBLE_SERIAL_H__
#define __ARDUINO_PEBBLE_SERIAL_H__

#define USE_HARDWARE_SERIAL 0

#include <Arduino.h>
#include "utility/OneWireSoftSerial.h"

class ArduinoPebbleSerial {
public:
  static void begin_software(uint8_t pin, uint8_t *buffer, size_t length);
  static void begin_hardware(uint8_t *buffer, size_t length);
  static bool feed(size_t *length, bool *is_read);
  static bool write(const uint8_t *payload, size_t length);
  static void notify(void);
  static bool is_connected(void);
};

#endif //__ARDUINO_PEBBLE_SERIAL_H__

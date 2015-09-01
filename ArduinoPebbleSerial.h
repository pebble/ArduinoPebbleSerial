/*
 * This is an Arduino library wrapper around the PebbleSerial library.
 */
#ifndef __ARDUINO_PEBBLE_SERIAL_H__
#define __ARDUINO_PEBBLE_SERIAL_H__

#define USE_HARDWARE_SERIAL 0

#include <Arduino.h>
#include "utility/OneWireSoftSerial.h"
extern "C" {
#include "utility/PebbleSerial.h"
};

typedef enum {
  Baud9600,
  Baud14400,
  Baud19200,
  Baud28800,
  Baud38400,
  Baud57600,
  Baud62500,
  Baud115200,
  Baud125000,
  Baud230400,
  Baud250000,
  Baud460800,
} Baud;

typedef enum {
  RequestTypeRead,
  RequestTypeWrite,
  RequestTypeWriteRead
} RequestType;

class ArduinoPebbleSerial {
public:
  static void begin_software(uint8_t pin, uint8_t *buffer, size_t length, Baud baud,
                             const uint16_t *services, uint8_t num_services);
  static void begin_hardware(uint8_t *buffer, size_t length, Baud baud, const uint16_t *services,
                             uint8_t num_services);
  static bool feed(uint16_t *service_id, uint16_t *attribute_id, size_t *length, RequestType *type);
  static bool write(bool success, const uint8_t *payload, size_t length);
  static void notify(uint16_t service_id, uint16_t attribute_id);
  static bool is_connected(void);
};

#endif //__ARDUINO_PEBBLE_SERIAL_H__

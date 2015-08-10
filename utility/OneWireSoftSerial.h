/*
 * This file provides a one-wire (aka half duplex) software serial implementation to faciliate easy
 * connection and communication with a Pebble watch via the smartstrap port. It was heavily inspired
 * by Arduino's software serial library, with the code for calculating the delays copied verbatim.
 * As is required by the license on Arduino's software serial library, this library is released
 * under the LGPL v2.1 license.
 */

#ifndef __SOFT_SERIAL_H__
#define __SOFT_SERIAL_H__

#include <inttypes.h>

#define STATIC_ASSERT_VALID_ONE_WIRE_SOFT_SERIAL_PIN(pin) \
    static_assert(digitalPinToPCICR(pin) != NULL, "This pin does not support PC interrupts!")
#define _SS_MAX_RX_BUFF 64 // RX buffer size

class OneWireSoftSerial
{
public:
  // public methods
  static void begin(uint8_t pin, long speed);
  static int available();
  static void write(uint8_t byte, bool is_break = false);
  static int read();
};

#endif

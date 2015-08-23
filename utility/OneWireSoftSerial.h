/*
 * This file provides a one-wire (aka half duplex) software serial implementation to faciliate easy
 * connection and communication with a Pebble watch via the smartstrap port. It was heavily inspired
 * by Arduino's software serial library, with the code for calculating the delays copied verbatim.
 * As is required by the license on Arduino's software serial library, this file is released
 * under the LGPL v2.1 license.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __SOFT_SERIAL_H__
#define __SOFT_SERIAL_H__

#include <inttypes.h>

#if ARDUINO > 1000
#define STATIC_ASSERT_VALID_ONE_WIRE_SOFT_SERIAL_PIN(pin) \
    static_assert(digitalPinToPCICR(pin) != NULL, "This pin does not support PC interrupts!")
#else
#define STATIC_ASSERT_VALID_ONE_WIRE_SOFT_SERIAL_PIN(pin)
#endif
#define _SS_MAX_RX_BUFF 64 // RX buffer size

class OneWireSoftSerial
{
public:
  // public methods
  static void begin(uint8_t pin, long speed);
  static int available();
  static void set_tx_enabled(bool enabled);
  static void write(uint8_t byte, bool is_break = false);
  static int read();
};

#endif

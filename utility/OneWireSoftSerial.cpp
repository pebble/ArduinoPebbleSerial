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
#include "OneWireSoftSerial.h"

#ifdef __arm__
// this library is not yet implemented for ARM microcontrollers
void OneWireSoftSerial::begin(uint8_t pin, long speed) { }
int OneWireSoftSerial::available(void) { return 0; }
void OneWireSoftSerial::set_tx_enabled(bool enabled) { }
void OneWireSoftSerial::write(uint8_t byte, bool is_break) { }
int OneWireSoftSerial::read(void) { return -1; };

#else
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <util/delay_basic.h>


// Statics
////////////////////////////////////////////////////////////////////////////////

static char s_receive_buffer[_SS_MAX_RX_BUFF];
static volatile uint8_t s_receive_buffer_tail = 0;
static volatile uint8_t s_receive_buffer_head = 0;
static uint16_t s_rx_delay_centering = 0;
static uint16_t s_rx_delay_intrabit = 0;
static uint16_t s_rx_delay_stopbit = 0;
static uint16_t s_tx_delay = 0;
static uint8_t s_pin = 0;
static uint8_t s_bit_mask = 0;
static volatile uint8_t *s_port_output_register = 0;
static volatile uint8_t *s_port_input_register = 0;
static volatile uint8_t *s_pcint_mask_reg = 0;
static uint8_t s_pcint_mask_value = 0;
static bool s_tx_enabled = false;


// Helper macros
////////////////////////////////////////////////////////////////////////////////

#define SUBTRACT_CAP(a, b) ((a > b) ? (a - b) : 1)
#define TUNED_DELAY(x) _delay_loop_2(x)


// Helper methods
////////////////////////////////////////////////////////////////////////////////

static inline void prv_set_rx_int_msk(bool enable) {
  if (enable) {
    *s_pcint_mask_reg |= s_pcint_mask_value;
  } else {
    *s_pcint_mask_reg &= ~s_pcint_mask_value;
  }
}

static inline void prv_set_tx_enabled(bool enabled) {
  if (enabled) {
    // enable pullup first to avoid the pin going low briefly
    digitalWrite(s_pin, HIGH);
    pinMode(s_pin, OUTPUT);
  } else {
    pinMode(s_pin, INPUT);
    digitalWrite(s_pin, HIGH); // enable pullup
  }
}

// The receive routine called by the interrupt handler
static inline void prv_recv(void) {
#if !defined(__arm__) && GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Preserve the registers that the compiler misses
// (courtesy of Arduino forum user *etracer*)
  asm volatile(
    "push r18 \n\t"
    "push r19 \n\t"
    "push r20 \n\t"
    "push r21 \n\t"
    "push r22 \n\t"
    "push r23 \n\t"
    "push r26 \n\t"
    "push r27 \n\t"
    ::);
#endif

  uint8_t d = 0;

  // If RX line is high, then we don't see any start bit
  // so interrupt is probably not for us
  if (!(*s_port_input_register & s_bit_mask)) {
    // Disable further interrupts during reception, this prevents
    // triggering another interrupt directly after we return, which can
    // cause problems at higher baudrates.
    prv_set_rx_int_msk(false);

    // Wait approximately 1/2 of a bit width to "center" the sample
    TUNED_DELAY(s_rx_delay_centering);

    // Read each of the 8 bits
    for (uint8_t i=8; i > 0; --i) {
      TUNED_DELAY(s_rx_delay_intrabit);
      d >>= 1;
      if (*s_port_input_register & s_bit_mask) {
        d |= 0x80;
      }
    }

    const uint8_t next = (s_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF;
    if (next != s_receive_buffer_head) {
      // save new data in buffer: tail points to where byte goes
      s_receive_buffer[s_receive_buffer_tail] = d; // save new byte
      s_receive_buffer_tail = next;
    }

    // skip the stop bit
    TUNED_DELAY(s_rx_delay_stopbit);

    // Re-enable interrupts when we're sure to be inside the stop bit
    prv_set_rx_int_msk(true);
  }

#if !defined(__arm__) && GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Restore the registers that the compiler misses
  asm volatile(
    "pop r27 \n\t"
    "pop r26 \n\t"
    "pop r23 \n\t"
    "pop r22 \n\t"
    "pop r21 \n\t"
    "pop r20 \n\t"
    "pop r19 \n\t"
    "pop r18 \n\t"
    ::);
#endif
}


// Interrupt handling
////////////////////////////////////////////////////////////////////////////////

#if defined(PCINT0_vect)
ISR(PCINT0_vect) {
  prv_recv();
}
#endif

#if defined(PCINT1_vect)
ISR(PCINT1_vect, ISR_ALIASOF(PCINT0_vect));
#endif

#if defined(PCINT2_vect)
ISR(PCINT2_vect, ISR_ALIASOF(PCINT0_vect));
#endif

#if defined(PCINT3_vect)
ISR(PCINT3_vect, ISR_ALIASOF(PCINT0_vect));
#endif


// Public methods
////////////////////////////////////////////////////////////////////////////////

void OneWireSoftSerial::begin(uint8_t pin, long speed) {
  s_pin = pin;
  s_bit_mask = digitalPinToBitMask(s_pin);
  s_port_output_register = portOutputRegister(digitalPinToPort(s_pin));
  s_port_input_register = portInputRegister(digitalPinToPort(s_pin));
  if (!digitalPinToPCICR(s_pin)) {
    return;
  }
  pinMode(2, OUTPUT);

  s_rx_delay_centering = s_rx_delay_intrabit = s_rx_delay_stopbit = s_tx_delay = 0;

  // Precalculate the various delays, in number of 4-cycle delays
  uint16_t bit_delay = (F_CPU / speed) / 4;

  // 12 (gcc 4.8.2) or 13 (gcc 4.3.2) cycles from start bit to first bit,
  // 15 (gcc 4.8.2) or 16 (gcc 4.3.2) cycles between bits,
  // 12 (gcc 4.8.2) or 14 (gcc 4.3.2) cycles from last bit to stop bit
  // These are all close enough to just use 15 cycles, since the inter-bit
  // timings are the most critical (deviations stack 8 times)
  s_tx_delay = SUBTRACT_CAP(bit_delay, 15 / 4);

  #if GCC_VERSION > 40800
  // Timings counted from gcc 4.8.2 output. This works up to 115200 on
  // 16Mhz and 57600 on 8Mhz.
  //
  // When the start bit occurs, there are 3 or 4 cycles before the
  // interrupt flag is set, 4 cycles before the PC is set to the right
  // interrupt vector address and the old PC is pushed on the stack,
  // and then 75 cycles of instructions (including the RJMP in the
  // ISR vector table) until the first delay. After the delay, there
  // are 17 more cycles until the pin value is read (excluding the
  // delay in the loop).
  // We want to have a total delay of 1.5 bit time. Inside the loop,
  // we already wait for 1 bit time - 23 cycles, so here we wait for
  // 0.5 bit time - (71 + 18 - 22) cycles.
  s_rx_delay_centering = SUBTRACT_CAP(bit_delay / 2, (4 + 4 + 75 + 17 - 23) / 4);

  // There are 23 cycles in each loop iteration (excluding the delay)
  s_rx_delay_intrabit = SUBTRACT_CAP(bit_delay, 23 / 4);

  // There are 37 cycles from the last bit read to the start of
  // stopbit delay and 11 cycles from the delay until the interrupt
  // mask is enabled again (which _must_ happen during the stopbit).
  // This delay aims at 3/4 of a bit time, meaning the end of the
  // delay will be at 1/4th of the stopbit. This allows some extra
  // time for ISR cleanup, which makes 115200 baud at 16Mhz work more
  // reliably
  s_rx_delay_stopbit = SUBTRACT_CAP(bit_delay * 3 / 4, (37 + 11) / 4);
  #else
  // Timings counted from gcc 4.3.2 output
  // Note that this code is a _lot_ slower, mostly due to bad register
  // allocation choices of gcc. This works up to 57600 on 16Mhz and
  // 38400 on 8Mhz.
  s_rx_delay_centering = SUBTRACT_CAP(bit_delay / 2, (4 + 4 + 97 + 29 - 11) / 4);
  s_rx_delay_intrabit = SUBTRACT_CAP(bit_delay, 11 / 4);
  s_rx_delay_stopbit = SUBTRACT_CAP(bit_delay * 3 / 4, (44 + 17) / 4);
  #endif


  // Enable the PCINT for the entire port here, but never disable it
  // (others might also need it, so we disable the interrupt by using
  // the per-pin PCMSK register).
  *digitalPinToPCICR(s_pin) |= _BV(digitalPinToPCICRbit(s_pin));
  // Precalculate the pcint mask register and value, so setRxIntMask
  // can be used inside the ISR without costing too much time.
  s_pcint_mask_reg = digitalPinToPCMSK(s_pin);
  s_pcint_mask_value = _BV(digitalPinToPCMSKbit(s_pin));

  TUNED_DELAY(s_tx_delay); // if we were low this establishes the end

  s_receive_buffer_head = s_receive_buffer_tail = 0;

  prv_set_tx_enabled(false);
  prv_set_rx_int_msk(true);
}

// Read data from buffer
int OneWireSoftSerial::read() {
  if (s_receive_buffer_head == s_receive_buffer_tail) {
    return -1;
  }

  // Read from "head"
  uint8_t d = s_receive_buffer[s_receive_buffer_head]; // grab next byte
  s_receive_buffer_head = (s_receive_buffer_head + 1) % _SS_MAX_RX_BUFF;
  return d;
}

int OneWireSoftSerial::available() {
  return (s_receive_buffer_tail + _SS_MAX_RX_BUFF - s_receive_buffer_head) % _SS_MAX_RX_BUFF;
}

void OneWireSoftSerial::set_tx_enabled(bool enabled) {
  if (s_tx_enabled == enabled) {
    return;
  }
  static uint8_t s_old_sreg = 0;
  if (enabled) {
    s_old_sreg = SREG;
    cli();
    prv_set_rx_int_msk(false);
    prv_set_tx_enabled(true);
  } else {
    prv_set_tx_enabled(false);
    prv_set_rx_int_msk(true);
    SREG = s_old_sreg;
  }
  s_tx_enabled = enabled;
}

void OneWireSoftSerial::write(uint8_t b, bool is_break /* = false */) {
  if (!s_tx_enabled) {
    return;
  }

  // By declaring these as local variables, the compiler will put them
  // in registers _before_ disabling interrupts and entering the
  // critical timing sections below, which makes it a lot easier to
  // verify the cycle timings
  volatile uint8_t *reg = s_port_output_register;
  uint8_t reg_mask = s_bit_mask;
  uint8_t inv_mask = ~s_bit_mask;
  uint16_t delay = s_tx_delay;
  uint8_t num_bits = is_break ? 9 : 8;

  // Write the start bit
  *reg &= inv_mask;
  TUNED_DELAY(delay);

  // Write each of the 8 bits
  for (uint8_t i = num_bits; i > 0; --i) {
    if (b & 1) { // choose bit
      *reg |= reg_mask; // send 1
    } else {
      *reg &= inv_mask; // send 0
    }

    TUNED_DELAY(delay);
    b >>= 1;
  }

  // restore pin to natural state
  *reg |= reg_mask;
  TUNED_DELAY(s_tx_delay);
}
#endif // __arm__

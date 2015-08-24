#ifndef __BOARD_H__
#define __BOARD_H__

/*
 * This file contains definitions of board-specific variables and functions to support the
 * ArduinoPebbleSerial library.
 */

#include <Arduino.h>
#include <stdint.h>

// Helper macros for setting and clearing a bit within a register
#define cbi(sfr, bit) (sfr &= ~_BV(bit))
#define sbi(sfr, bit) (sfr |= _BV(bit))

// The board-specific variables are defined below
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega2560__)
/* Arduino Mega, Teensy 2.0, etc */
#define BOARD_SERIAL Serial1
static inline void board_begin(void) {
}
static inline void board_set_tx_enabled(bool enabled) {
  if (enabled) {
    bitSet(UCSR1B, TXEN1);
    bitClear(UCSR1B, RXEN1);
  } else {
    bitClear(UCSR1B, TXEN1);
    bitClear(DDRD, 3);
    bitSet(PORTD, 3);
    bitSet(UCSR1B, RXEN1);
  }
}
static inline void board_set_even_parity(bool enabled) {
  if (enabled) {
    bitSet(UCSR1C, UPM11);
  } else {
    bitClear(UCSR1C, UPM11);
  }
}

#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
/* Arduino Uno, etc */
#define BOARD_SERIAL Serial
static inline void board_begin(void) {
}
static inline void board_set_tx_enabled(bool enabled) {
  if (enabled) {
    bitSet(UCSR0B, TXEN0);
    bitClear(UCSR0B, RXEN0);
  } else {
    bitClear(UCSR0B, TXEN0);
    bitClear(DDRD, 1);
    bitSet(PORTD, 1);
    bitSet(UCSR0B, RXEN0);
  }
}
static inline void board_set_even_parity(bool enabled) {
  if (enabled) {
    bitSet(UCSR0C, UPM01);
  } else {
    bitClear(UCSR0C, UPM01);
  }
}
#elif defined(__MK20DX256__) || defined(__MK20DX128__)
/* Teensy 3.0, Teensy 3.1, etc */
#define BOARD_SERIAL Serial1
static inline void board_begin(void) {
  // configure TX as open-drain
  CORE_PIN1_CONFIG |= PORT_PCR_ODE;
}
static inline void board_set_tx_enabled(bool enabled) {
  // the TX and RX are tied together and we'll just drop any loopback frames
}
static inline void board_set_even_parity(bool enabled) {
  if (enabled) {
    serial_format(SERIAL_8E1);
  } else {
    serial_format(SERIAL_8N1);
  }
}

#else
#error "Board not supported!"
#endif

#endif // __BOARD_H__

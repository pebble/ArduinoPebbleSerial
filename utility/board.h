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
#define BOARD_SERIAL Serial1
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
#define BOARD_SERIAL Serial
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

#else
#error "Board not supported!"
#endif

#endif // __BOARD_H__

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
#if defined(__AVR_ATmega32U4__)
#define BOARD_SERIAL Serial1
#define BOARD_TX_PIN 1
#define BOARD_ENABLE_TX() sbi(UCSR1B, TXEN1)
#define BOARD_DISABLE_TX() cbi(UCSR1B, TXEN1)
#define BOARD_ENABLE_RX() sbi(UCSR1B, RXEN1)
#define BOARD_DISABLE_RX() cbi(UCSR1B, RXEN1)
#define BOARD_PARITY_EVEN() sbi(UCSR1C, UPM11)
#define BOARD_PARITY_NONE() cbi(UCSR1C, UPM11)
#define BOARD_TX_COMPLETE() (UCSR1A & (1 << 6))

#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
#define BOARD_SERIAL Serial
#define BOARD_TX_PIN 1
#define BOARD_ENABLE_TX() sbi(UCSR0B, TXEN1)
#define BOARD_DISABLE_TX() cbi(UCSR0B, TXEN1)
#define BOARD_ENABLE_RX() sbi(UCSR0B, RXEN1)
#define BOARD_DISABLE_RX() cbi(UCSR0B, RXEN1)
#define BOARD_PARITY_EVEN() sbi(UCSR0C, UPM11)
#define BOARD_PARITY_NONE() cbi(UCSR0C, UPM11)
#define BOARD_TX_COMPLETE() (UCSR0A & (1 << 6))

#elif defined(__AVR_ATmega2560__)
#define BOARD_SERIAL Serial1
#define BOARD_TX_PIN 18
#define BOARD_ENABLE_TX() sbi(UCSR1B, TXEN1)
#define BOARD_DISABLE_TX() cbi(UCSR1B, TXEN1)
#define BOARD_ENABLE_RX() sbi(UCSR1B, RXEN1)
#define BOARD_DISABLE_RX() cbi(UCSR1B, RXEN1)
#define BOARD_PARITY_EVEN() sbi(UCSR1C, UPM11)
#define BOARD_PARITY_NONE() cbi(UCSR1C, UPM11)
#define BOARD_TX_COMPLETE() (UCSR1A & (1 << 6))

#else
#error "Board not supported!
#endif

#endif // __BOARD_H__

/*
 * This is an Arduino library wrapper around the PebbleSerial library.
 */

#include "ArduinoPebbleSerial.h"
extern "C" {
#include <utility/PebbleSerial.h>
};


// Macros for setting and clearing a bit within a register
#define cbi(sfr, bit) (sfr &= ~_BV(bit))
#define sbi(sfr, bit) (sfr |= _BV(bit))


// The board-specific variables are defined below
#if defined(__AVR_ATmega32U4__)
static HardwareSerial *s_serial = &Serial1;
static const uint8_t s_tx_pin = 1;
#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
static HardwareSerial *s_serial = &Serial;
static const uint8_t s_tx_pin = 1;
#elif defined(__AVR_ATmega2560__)
static HardwareSerial *s_serial = &Serial1;
static const uint8_t s_tx_pin = 18;
#else
#error "Board not supported!
#endif


static bool s_is_hardware;
static uint8_t *s_buffer;
static size_t s_buffer_length;

static void prv_control_cb(PebbleControl cmd) {
  switch (cmd) {
  case PebbleControlEnableTX:
    // enable transmitter
    sbi(UCSR1B, TXEN1);
    // disable receiver
    cbi(UCSR1B, RXEN1);
    break;
  case PebbleControlDisableTX:
    // disable transmitter
    cbi(UCSR1B, TXEN1);
    // set TX pin as input with pullup
    pinMode(s_tx_pin, INPUT_PULLUP);
    // enable receiver
    sbi(UCSR1B, RXEN1);
    break;
  case PebbleControlFlushTX:
    // flush any buffered tx data
    s_serial->flush();
    // wait for the data to be transmitted
    while (!(UCSR1A & 0x60));
    // small delay for the lines to settle down
    delay(1);
    break;
  case PebbleControlSetParityEven:
    sbi(UCSR1C, UPM11);
    break;
  case PebbleControlSetParityNone:
    cbi(UCSR1C, UPM11);
    break;
  default:
    break;
  }
}

static void prv_write_byte_cb(uint8_t data) {
  s_serial->write(data);
}

void ArduinoPebbleSerial::begin(uint8_t *buffer, size_t length) {
  s_buffer = buffer;
  s_buffer_length = length;
  s_is_hardware = true;

  s_serial->begin(PEBBLE_DEFAULT_BAUDRATE);

  PebbleCallbacks callbacks;
  callbacks.write_byte = prv_write_byte_cb;
  callbacks.control = prv_control_cb;
  pebble_init(callbacks);
  pebble_prepare_for_read(s_buffer, s_buffer_length);
}

bool ArduinoPebbleSerial::feed(size_t *length, bool *is_read) {
  while (s_serial->available()) {
    uint8_t data = (uint8_t)s_serial->read();
    if (pebble_handle_byte(data, length, is_read)) {
      // we have a full frame
      pebble_prepare_for_read(s_buffer, s_buffer_length);
      return true;
    }
  }
  return false;
}

bool ArduinoPebbleSerial::write(const uint8_t *payload, size_t length) {
  return pebble_write(payload, length);
}

void ArduinoPebbleSerial::notify(void) {
  pebble_notify();
}

bool ArduinoPebbleSerial::is_connected(void) {
  return pebble_is_connected();
}

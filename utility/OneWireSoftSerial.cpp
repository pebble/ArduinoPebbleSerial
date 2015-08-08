#include "OneWireSoftSerial.h"

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <util/delay_basic.h>

// Statics
char OneWireSoftSerial::_receive_buffer[_SS_MAX_RX_BUFF];
volatile uint8_t OneWireSoftSerial::_receive_buffer_tail = 0;
volatile uint8_t OneWireSoftSerial::_receive_buffer_head = 0;
uint16_t OneWireSoftSerial::_rx_delay_centering = 0;
uint16_t OneWireSoftSerial::_rx_delay_intrabit = 0;
uint16_t OneWireSoftSerial::_rx_delay_stopbit = 0;
uint16_t OneWireSoftSerial::_tx_delay = 0;
uint8_t OneWireSoftSerial::_pin = 0;
uint8_t OneWireSoftSerial::_bit_mask = 0;
volatile uint8_t *OneWireSoftSerial::_port_output_register = 0;
volatile uint8_t *OneWireSoftSerial::_port_input_register = 0;
volatile uint8_t *OneWireSoftSerial::_pcint_maskreg = 0;
uint8_t OneWireSoftSerial::_pcint_maskvalue = 0;
bool OneWireSoftSerial::_is_break = 0;

#define SUBTRACT_CAP(a, b) ((a > b) ? (a - b) : 1)
#define TUNED_DELAY(x) _delay_loop_2(x)

void OneWireSoftSerial::set_tx_enabled(bool enabled) {
  if (enabled) {
    // enable pullup first to avoid the pin going low briefly
    digitalWrite(_pin, HIGH);
    pinMode(_pin, OUTPUT);
  } else {
    pinMode(_pin, INPUT);
    digitalWrite(_pin, HIGH); // enable pullup
  }
}

// Private methods

// The receive routine called by the interrupt handler
inline void OneWireSoftSerial::recv() {
#if GCC_VERSION < 40302
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
  if (!(*_port_input_register & _bit_mask)) {
    // Disable further interrupts during reception, this prevents
    // triggering another interrupt directly after we return, which can
    // cause problems at higher baudrates.
    set_rx_int_msk(false);

    // Wait approximately 1/2 of a bit width to "center" the sample
    TUNED_DELAY(_rx_delay_centering);

    // Read each of the 8 bits
    for (uint8_t i=8; i > 0; --i) {
      TUNED_DELAY(_rx_delay_intrabit);
      d >>= 1;
      if (*_port_input_register & _bit_mask) {
        d |= 0x80;
      }
    }

    const uint8_t next = (_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF;
    if (next != _receive_buffer_head) {
      // save new data in buffer: tail points to where byte goes
      _receive_buffer[_receive_buffer_tail] = d; // save new byte
      _receive_buffer_tail = next;
    }

    // skip the stop bit
    TUNED_DELAY(_rx_delay_stopbit);

    // Re-enable interrupts when we're sure to be inside the stop bit
    set_rx_int_msk(true);
  }

#if GCC_VERSION < 40302
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


#if defined(PCINT0_vect)
ISR(PCINT0_vect) {
  OneWireSoftSerial::recv();
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

void OneWireSoftSerial::begin(uint8_t pin, long speed) {
  _pin = pin;
  _bit_mask = digitalPinToBitMask(_pin);
  _port_output_register = portOutputRegister(digitalPinToPort(_pin));
  _port_input_register = portInputRegister(digitalPinToPort(_pin));
  if (!digitalPinToPCICR(_pin)) {
    return;
  }

  _rx_delay_centering = _rx_delay_intrabit = _rx_delay_stopbit = _tx_delay = 0;

  // Precalculate the various delays, in number of 4-cycle delays
  uint16_t bit_delay = (F_CPU / speed) / 4;

  // 12 (gcc 4.8.2) or 13 (gcc 4.3.2) cycles from start bit to first bit,
  // 15 (gcc 4.8.2) or 16 (gcc 4.3.2) cycles between bits,
  // 12 (gcc 4.8.2) or 14 (gcc 4.3.2) cycles from last bit to stop bit
  // These are all close enough to just use 15 cycles, since the inter-bit
  // timings are the most critical (deviations stack 8 times)
  _tx_delay = SUBTRACT_CAP(bit_delay, 15 / 4);

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
  _rx_delay_centering = SUBTRACT_CAP(bit_delay / 2, (4 + 4 + 75 + 17 - 23) / 4);

  // There are 23 cycles in each loop iteration (excluding the delay)
  _rx_delay_intrabit = SUBTRACT_CAP(bit_delay, 23 / 4);

  // There are 37 cycles from the last bit read to the start of
  // stopbit delay and 11 cycles from the delay until the interrupt
  // mask is enabled again (which _must_ happen during the stopbit).
  // This delay aims at 3/4 of a bit time, meaning the end of the
  // delay will be at 1/4th of the stopbit. This allows some extra
  // time for ISR cleanup, which makes 115200 baud at 16Mhz work more
  // reliably
  _rx_delay_stopbit = SUBTRACT_CAP(bit_delay * 3 / 4, (37 + 11) / 4);
  #else
  // Timings counted from gcc 4.3.2 output
  // Note that this code is a _lot_ slower, mostly due to bad register
  // allocation choices of gcc. This works up to 57600 on 16Mhz and
  // 38400 on 8Mhz.
  _rx_delay_centering = SUBTRACT_CAP(bit_delay / 2, (4 + 4 + 97 + 29 - 11) / 4);
  _rx_delay_intrabit = SUBTRACT_CAP(bit_delay, 11 / 4);
  _rx_delay_stopbit = SUBTRACT_CAP(bit_delay * 3 / 4, (44 + 17) / 4);
  #endif


  // Enable the PCINT for the entire port here, but never disable it
  // (others might also need it, so we disable the interrupt by using
  // the per-pin PCMSK register).
  *digitalPinToPCICR(_pin) |= _BV(digitalPinToPCICRbit(_pin));
  // Precalculate the pcint mask register and value, so setRxIntMask
  // can be used inside the ISR without costing too much time.
  _pcint_maskreg = digitalPinToPCMSK(_pin);
  _pcint_maskvalue = _BV(digitalPinToPCMSKbit(_pin));

  TUNED_DELAY(_tx_delay); // if we were low this establishes the end

  _receive_buffer_head = _receive_buffer_tail = 0;

  set_tx_enabled(false);
  set_rx_int_msk(true);
}

inline void OneWireSoftSerial::set_rx_int_msk(bool enable) {
  if (enable) {
    *_pcint_maskreg |= _pcint_maskvalue;
  } else {
    *_pcint_maskreg &= ~_pcint_maskvalue;
  }
}

// Read data from buffer
int OneWireSoftSerial::read() {
  if (_receive_buffer_head == _receive_buffer_tail) {
    return -1;
  }

  // Read from "head"
  uint8_t d = _receive_buffer[_receive_buffer_head]; // grab next byte
  _receive_buffer_head = (_receive_buffer_head + 1) % _SS_MAX_RX_BUFF;
  return d;
}

int OneWireSoftSerial::available() {
  return (_receive_buffer_tail + _SS_MAX_RX_BUFF - _receive_buffer_head) % _SS_MAX_RX_BUFF;
}

void OneWireSoftSerial::enable_break(bool enabled) {
  _is_break = enabled;
}

void OneWireSoftSerial::write(uint8_t b) {
  // By declaring these as local variables, the compiler will put them
  // in registers _before_ disabling interrupts and entering the
  // critical timing sections below, which makes it a lot easier to
  // verify the cycle timings
  volatile uint8_t *reg = _port_output_register;
  uint8_t reg_mask = _bit_mask;
  uint8_t inv_mask = ~_bit_mask;
  uint16_t delay = _tx_delay;
  uint8_t oldSREG = SREG;
  uint8_t num_bits = _is_break ? 9 : 8;

  // turn off interrupts and set the pin to TX
  cli();
  set_rx_int_msk(false);
  set_tx_enabled(true);

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
  TUNED_DELAY(_tx_delay);

  // set the pin to RX and turn interrupts back on
  set_tx_enabled(false);
  set_rx_int_msk(true);
  SREG = oldSREG;
}

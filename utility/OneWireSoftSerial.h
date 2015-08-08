#ifndef __SOFT_SERIAL_H__
#define __SOFT_SERIAL_H__

#include <inttypes.h>

#define STATIC_ASSERT_VALID_ONE_WIRE_SOFT_SERIAL_PIN(pin) \
    static_assert(digitalPinToPCICR(pin) != NULL, "This pin does not support PC interrupts!")
#define _SS_MAX_RX_BUFF 64 // RX buffer size

class OneWireSoftSerial
{
private:
  // per object data
  static uint8_t _pin;
  static uint8_t _bit_mask;
  static volatile uint8_t *_port_output_register;
  static volatile uint8_t *_port_input_register;
  static volatile uint8_t *_pcint_maskreg;
  static uint8_t _pcint_maskvalue;
  static bool _is_break;

  // Expressed as 4-cycle delays (must never be 0!)
  static uint16_t _rx_delay_centering;
  static uint16_t _rx_delay_intrabit;
  static uint16_t _rx_delay_stopbit;
  static uint16_t _tx_delay;

  // static data
  static char _receive_buffer[_SS_MAX_RX_BUFF];
  static volatile uint8_t _receive_buffer_tail;
  static volatile uint8_t _receive_buffer_head;

  // private methods
  static void set_rx_int_msk(bool enable) __attribute__((__always_inline__));

public:
  // public methods
  static void begin(uint8_t pin, long speed);
  static int available();
  static void write(uint8_t byte);
  static int read();

  static void enable_break(bool enabled);
  static void set_tx_enabled(bool enable);

  // public only for ISR
  static void recv() __attribute__((__always_inline__));
};

#endif

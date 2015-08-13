/*
 * This is an Arduino library wrapper around the PebbleSerial library.
 */

#include "ArduinoPebbleSerial.h"
#include "utility/board.h"
extern "C" {
#include "utility/PebbleSerial.h"
};

static bool s_is_hardware;
static uint8_t *s_buffer;
static size_t s_buffer_length;
static uint8_t s_pin;

static void prv_control_cb(PebbleControl cmd, uint32_t arg) {
  switch (cmd) {
  case PebbleControlSetBaudRate:
    if (s_is_hardware) {
      if (arg == 57600) {
        // The Arduino library intentionally uses bad prescalers for a baud rate of exactly 57600 so
        // we just increase it by 1 to prevent it from doing that.
        arg++;
      }
      BOARD_SERIAL.begin(arg);
    } else {
      OneWireSoftSerial::begin(s_pin, arg);
    }
    Serial.print("Setting baud rate to ");
    Serial.println(arg, DEC);
    break;
  case PebbleControlSetTxEnabled:
    if (s_is_hardware) {
      if (!arg) {
        BOARD_SERIAL.flush();
      }
      board_set_tx_enabled(arg);
    }
    break;
  case PebbleControlWriteByte:
    if (s_is_hardware) {
      BOARD_SERIAL.write((uint8_t)arg);
    } else {
      OneWireSoftSerial::write((uint8_t)arg);
    }
    break;
  case PebbleControlWriteBreak:
    if (s_is_hardware) {
      board_set_even_parity(true);
      BOARD_SERIAL.write((uint8_t)0);
      // need to flush before changing parity
      BOARD_SERIAL.flush();
      board_set_even_parity(false);
    } else {
      OneWireSoftSerial::write(0, true /* is_break */);
    }
    break;
  default:
    break;
  }
}

static bool prv_handle_attribute(uint16_t service_id, uint16_t attribute_id, uint8_t *buffer,
                                 uint16_t *length) {
  if ((service_id == 0x0001) && (attribute_id == 0x0001)) {
    // service discovery
    uint16_t service_id = 0x1001;
    memcpy(buffer, &service_id, sizeof(service_id));
    *length = sizeof(service_id);
    return true;
  } else if ((service_id == 0x1001) && (attribute_id == 0x1001)) {
    static uint32_t s_test_attr_data = 9999;
    if (*length == 0) {
      // this was a read
      memcpy(buffer, &s_test_attr_data, sizeof(s_test_attr_data));
      *length = sizeof(s_test_attr_data);
      s_test_attr_data--;
    } else {
      // this was a write
      memcpy(&s_test_attr_data, buffer, sizeof(s_test_attr_data));
      *length = 0;
    }
    return true;
  }
  return false;
}

static void prv_begin(uint8_t *buffer, size_t length) {
  s_buffer = buffer;
  s_buffer_length = length;

  PebbleCallbacks callbacks = {
    .control = prv_control_cb,
    .attribute = prv_handle_attribute
  };

  pebble_init(callbacks, PebbleBaud57600);
  pebble_prepare_for_read(s_buffer, s_buffer_length);
}

void ArduinoPebbleSerial::begin_software(uint8_t pin, uint8_t *buffer, size_t length) {
  s_is_hardware = false;
  s_pin = pin;
  prv_begin(buffer, length);
}

void ArduinoPebbleSerial::begin_hardware(uint8_t *buffer, size_t length) {
  s_is_hardware = true;
  prv_begin(buffer, length);
}

static int prv_available_bytes(void) {
  if (s_is_hardware) {
    return BOARD_SERIAL.available();
  } else {
    return OneWireSoftSerial::available();
  }
}

static uint8_t prv_read_byte(void) {
  if (s_is_hardware) {
    return (uint8_t)BOARD_SERIAL.read();
  } else {
    return (uint8_t)OneWireSoftSerial::read();
  }
}

bool ArduinoPebbleSerial::feed(size_t *length, bool *is_read) {
  while (prv_available_bytes()) {
    if (pebble_handle_byte(prv_read_byte(), length, is_read, millis())) {
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

void ArduinoPebbleSerial::notify(uint16_t service_id, uint16_t attribute_id) {
  pebble_notify_attribute(service_id, attribute_id);
}

bool ArduinoPebbleSerial::is_connected(void) {
  return pebble_is_connected();
}

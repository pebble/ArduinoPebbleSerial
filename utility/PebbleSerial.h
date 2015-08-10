#ifndef __PEBBLE_SERIAL_H__
#define __PEBBLE_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PEBBLE_MAX_PAYLOAD      80

typedef enum {
  PebbleControlSetBaudRate,
  PebbleControlSetTxEnabled,
  PebbleControlWriteByte,
  PebbleControlWriteBreak
} PebbleControl;

typedef enum {
  PebbleBaud9600,
  PebbleBaud14400,
  PebbleBaud19200,
  PebbleBaud28800,
  PebbleBaud38400,
  PebbleBaud57600,
  PebbleBaud62500,
  PebbleBaud115200,
  PebbleBaud125000,
  PebbleBaud230400,
  PebbleBaud250000,
  PebbleBaud460800,

  PebbleBaudInvalid
} PebbleBaud;


typedef void (*PebbleControlCallback)(PebbleControl cmd, uint32_t arg);
typedef bool (*PebbleHandleAttributeCallback)(uint16_t service_id, uint16_t attribute_id,
                                              uint8_t *buffer, uint16_t *length);

typedef struct {
  PebbleControlCallback control;
  PebbleHandleAttributeCallback attribute;
} PebbleCallbacks;

void pebble_init(PebbleCallbacks callbacks, PebbleBaud baud);
void pebble_prepare_for_read(uint8_t *buffer, size_t length);
bool pebble_handle_byte(uint8_t data, size_t *length, bool *is_read, uint32_t time_ms);
bool pebble_write(const uint8_t *payload, size_t length);
void pebble_notify(void);
void pebble_notify_attribute(uint16_t service_id, uint16_t attribute_id);
bool pebble_is_connected(void);

#endif // __PEBBLE_SERIAL_H__

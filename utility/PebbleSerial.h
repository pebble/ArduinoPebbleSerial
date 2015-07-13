#ifndef __PEBBLE_SERIAL_H__
#define __PEBBLE_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PEBBLE_DEFAULT_BAUDRATE 9600
#define PEBBLE_PROTOCOL_VERSION 1
#define PEBBLE_MAX_PAYLOAD      80

typedef enum {
  PebbleControlEnableTX,
  PebbleControlDisableTX,
  PebbleControlFlushTX,
  PebbleControlSetParityEven,
  PebbleControlSetParityNone
} PebbleControl;

typedef void (*PebbleWriteByteCallback)(uint8_t data);
typedef void (*PebbleControlCallback)(PebbleControl cmd);

typedef struct {
  PebbleWriteByteCallback write_byte;
  PebbleControlCallback control;
} PebbleCallbacks;

void pebble_init(PebbleCallbacks callbacks);
void pebble_prepare_for_read(uint8_t *buffer, size_t length);
bool pebble_handle_byte(uint8_t data, size_t *length, bool *is_read);
bool pebble_write(const uint8_t *payload, size_t length);
void pebble_notify(void);
bool pebble_is_connected(void);

#endif // __PEBBLE_SERIAL_H__

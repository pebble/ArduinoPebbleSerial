#ifndef __PEBBLE_SERIAL_H__
#define __PEBBLE_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PEBBLE_MIN_PAYLOAD      (20 + PEBBLE_PAYLOAD_OVERHEAD)
#define PEBBLE_PAYLOAD_OVERHEAD 9

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define GET_PAYLOAD_BUFFER_SIZE(max_data_length) \
  MAX(max_data_length + PEBBLE_PAYLOAD_OVERHEAD, PEBBLE_MIN_PAYLOAD)

typedef enum {
  SmartstrapCmdSetBaudRate,
  SmartstrapCmdSetTxEnabled,
  SmartstrapCmdWriteByte,
  SmartstrapCmdWriteBreak
} SmartstrapCmd;

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

typedef enum {
  SmartstrapResultOk = 0,
  SmartstrapResultNotSupported
} SmartstrapResult;

typedef enum {
  SmartstrapRequestTypeRead = 0,
  SmartstrapRequestTypeWrite = 1,
  SmartstrapRequestTypeWriteRead = 2
} SmartstrapRequestType;


typedef void (*SmartstrapCallback)(SmartstrapCmd cmd, uint32_t arg);

void pebble_init(SmartstrapCallback callback, PebbleBaud baud, const uint16_t *services,
                 uint8_t num_services);
void pebble_prepare_for_read(uint8_t *buffer, size_t length);
bool pebble_handle_byte(uint8_t data, uint16_t *service_id, uint16_t *attribute_id, size_t *length,
                        SmartstrapRequestType *type, uint32_t time_ms);
bool pebble_write(bool success, const uint8_t *buffer, uint16_t length);
void pebble_notify(uint16_t service_id, uint16_t attribute_id);
bool pebble_is_connected(uint32_t time);

#endif // __PEBBLE_SERIAL_H__

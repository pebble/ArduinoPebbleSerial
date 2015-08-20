#pragma once

#include <stdint.h>
#include <stdbool.h>

static const uint8_t ENCODING_FLAG = 0x7E;
static const uint8_t ENCODING_ESCAPE = 0x7D;
static const uint8_t ENCODING_ESCAPE_MASK = 0x20;

typedef struct {
  bool escape;
} EncodingStreamingContext;

void encoding_streaming_decode_reset(EncodingStreamingContext *ctx);
bool encoding_streaming_decode(EncodingStreamingContext *ctx, uint8_t *data, bool *complete,
                           bool *is_invalid);
bool encoding_encode(uint8_t *data);

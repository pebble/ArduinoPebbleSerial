#pragma once

#include <stdint.h>
#include <stdbool.h>

static const uint8_t HDLC_FLAG = 0x7E;
static const uint8_t HDLC_ESCAPE = 0x7D;
static const uint8_t HDLC_ESCAPE_MASK = 0x20;

typedef struct {
  bool escape;
  bool is_valid;
} HdlcStreamingContext;

void hdlc_streaming_decode_start(HdlcStreamingContext *);
bool hdlc_streaming_decode(HdlcStreamingContext *, uint8_t *data);
bool hdlc_streaming_decode_finish(HdlcStreamingContext *);
bool hdlc_encode(uint8_t *data);

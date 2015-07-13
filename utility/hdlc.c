#include "hdlc.h"


void hdlc_streaming_decode_start(HdlcStreamingContext *ctx) {
  ctx->escape = false;
  ctx->is_valid = true;
}

bool hdlc_streaming_decode(HdlcStreamingContext *ctx, uint8_t *data) {
  if (!ctx->is_valid) {
    // if this stream is invalid, no need to process any more
    return false;
  }
  if (*data == HDLC_FLAG) {
    // there shouldn't be any flags in the data
    ctx->is_valid = false;
    return false;
  } else if (*data == HDLC_ESCAPE) {
    if (ctx->escape) {
      // invalid sequence
      ctx->is_valid = false;
      return false;
    } else {
      // next character must be escaped and this one should be ignored
      ctx->escape = true;
      return false;
    }
  } else {
    // this is a valid character
    if (ctx->escape) {
      // unescape the current character before processing it
      *data ^= HDLC_ESCAPE_MASK;
      ctx->escape = false;
    }
    return true;
  }
}

bool hdlc_streaming_decode_finish(HdlcStreamingContext *ctx) {
  return ctx->is_valid && !ctx->escape;
}

bool hdlc_encode(uint8_t *data) {
  if (*data == HDLC_FLAG || *data == HDLC_ESCAPE) {
    *data ^= HDLC_ESCAPE_MASK;
    return true;
  }
  return false;
}

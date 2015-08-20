#include "encoding.h"

void encoding_streaming_decode_reset(EncodingStreamingContext *ctx) {
  ctx->escape = false;
}

bool encoding_streaming_decode(EncodingStreamingContext *ctx, uint8_t *data, bool *should_store,
                           bool *encoding_error) {
  bool is_complete = false;
  *encoding_error = false;
  *should_store = false;
  if (*data == ENCODING_FLAG) {
    if (ctx->escape) {
      // extra escape character before flag
      ctx->escape = false;
      *encoding_error = true;
    }
    // we've reached the end of the frame
    is_complete = true;
  } else if (*data == ENCODING_ESCAPE) {
    if (ctx->escape) {
      // invalid sequence
      ctx->escape = false;
      *encoding_error = true;
    } else {
      // ignore this character and escape the next one
      ctx->escape = true;
    }
  } else {
    if (ctx->escape) {
      *data ^= ENCODING_ESCAPE_MASK;
      ctx->escape = false;
    }
    *should_store = true;
  }

  return is_complete;
}

bool encoding_encode(uint8_t *data) {
  if (*data == ENCODING_FLAG || *data == ENCODING_ESCAPE) {
    *data ^= ENCODING_ESCAPE_MASK;
    return true;
  }
  return false;
}

/* This library is for communicating with a Pebble Time via the accessory port for Smart Straps. */

#include "PebbleSerial.h"

#include "crc.h"
#include "hdlc.h"
#include "board.h"

#include <util/delay_basic.h>

#define UNLIKELY(x) (__builtin_expect(!!(x), 0))

#define PROTOCOL_VERSION              1
#define DEFAULT_BAUDRATE              9600

#define FRAME_MIN_LENGTH              8
#define FRAME_VERSION_OFFSET          0
#define FRAME_FLAGS_OFFSET            1
#define FRAME_PROFILE_OFFSET          5
#define FRAME_PAYLOAD_OFFSET          7

#define FLAGS_IS_READ_OFFSET          0
#define FLAGS_IS_MASTER_OFFSET        1
#define FLAGS_IS_NOTIFICATION_OFFSET  2
#define FLAGS_RESERVED_OFFSET         3
#define FLAGS_IS_READ_MASK            (0x1 << FLAGS_IS_READ_OFFSET)
#define FLAGS_IS_MASTER_MASK          (0x1 << FLAGS_IS_MASTER_OFFSET)
#define FLAGS_IS_NOTIFICATION_MASK    (0x1 << FLAGS_IS_NOTIFICATION_OFFSET)
#define FLAGS_RESERVED_MASK           (~(FLAGS_IS_READ_MASK | \
                                         FLAGS_IS_MASTER_MASK | \
                                         FLAGS_IS_NOTIFICATION_MASK))
#define FLAGS_GET(flags, mask, offset) (((flags) & mask) >> offset)
#define FLAGS_SET(flags, mask, offset, value) \
  (flags) = ((flags) & ~mask) | (((value) << offset) & mask)

typedef enum {
  SmartstrapProfileInvalid = 0x00,
  SmartstrapProfileLinkControl = 0x01,
  SmartstrapProfileRawData = 0x02,
  NumSmartstrapProfiles
} SmartstrapProfile;

typedef enum {
  LinkControlTypeInvalid = 0,
  LinkControlTypeStatus = 1,
  LinkControlTypeProfiles = 2,
  LinkControlTypeBaud = 3,
  NumLinkControlTypes
} LinkControlType;

typedef enum {
  LinkControlStatusOk = 0,
  LinkControlStatusBaudRate = 1,
  LinkControlStatusDisconnect = 2
} LinkControlStatus;

typedef struct {
  uint8_t version;
  uint32_t flags;
  uint16_t profile;
} FrameHeader;

typedef struct {
  FrameHeader header;
  uint8_t *payload;
  uint8_t checksum;
  size_t length;
  size_t max_payload_length;
  uint8_t footer_byte;
  bool should_drop;
  bool read_ready;
  bool is_read;
  HdlcStreamingContext hdlc_ctx;
} PebbleFrameInfo;

static PebbleFrameInfo s_frame;
static PebbleCallbacks s_callbacks;
static bool s_connected;
static bool s_can_respond;

void pebble_init(PebbleCallbacks callbacks) {
  s_callbacks = callbacks;
  s_callbacks.control(PebbleControlSetBaudRate, DEFAULT_BAUDRATE);
  s_callbacks.control(PebbleControlSetParityNone, 0);
  s_callbacks.control(PebbleControlEnableTX, 0);
  s_callbacks.control(PebbleControlDisableTX, 0);
}

void pebble_prepare_for_read(uint8_t *buffer, size_t length) {
  s_frame = (PebbleFrameInfo) {
    .payload = buffer,
    .max_payload_length = length,
    .read_ready = true
  };
}

static void prv_send_flag(void) {
  s_callbacks.write_byte(HDLC_FLAG);
}

static void prv_send_byte(uint8_t data, uint8_t *parity) {
  crc8_calculate_bytes_streaming(&data, sizeof(data), parity);
  if (hdlc_encode(&data)) {
    s_callbacks.write_byte(HDLC_ESCAPE);
  }
  s_callbacks.write_byte(data);
}

static void prv_write_internal(SmartstrapProfile protocol, const uint8_t *data, size_t length) {
  uint8_t parity = 0;

  // enable tx
  s_callbacks.control(PebbleControlEnableTX, 0);

  // send flag
  prv_send_flag();

  // send version
  prv_send_byte(PROTOCOL_VERSION, &parity);

  // send header flags
  prv_send_byte(0, &parity);
  prv_send_byte(0, &parity);
  prv_send_byte(0, &parity);
  prv_send_byte(0, &parity);

  // send profile
  prv_send_byte(protocol, &parity);
  prv_send_byte(0, &parity);

  // send data
  size_t i;
  for (i = 0; i < length; i++) {
    prv_send_byte(data[i], &parity);
  }

  // send parity
  prv_send_byte(parity, &parity);

  // send flag
  prv_send_flag();

  // flush and disable tx
  s_callbacks.control(PebbleControlFlushTX, 0);
  s_callbacks.control(PebbleControlDisableTX, 0);
}

static void prv_handle_link_control(uint8_t *buffer) {
  // we will re-use the buffer for the response
  static bool did_change_baud = false;
  LinkControlType type = buffer[0];
  if (type == LinkControlTypeStatus) {
    if (did_change_baud) {
      buffer[1] = LinkControlStatusOk;
    } else {
      buffer[1] = LinkControlStatusBaudRate;
    }
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 2);
    s_connected = true;
  } else if (type == LinkControlTypeProfiles) {
    buffer[1] = SmartstrapProfileRawData;
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 2);
  } else if (type == LinkControlTypeBaud) {
    buffer[1] = 0x00;
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 2);
    s_callbacks.control(PebbleControlSetBaudRate, 9600);
    s_callbacks.control(PebbleControlSetParityNone, 0);
    s_callbacks.control(PebbleControlEnableTX, 0);
    s_callbacks.control(PebbleControlDisableTX, 0);
    did_change_baud = true;
  }
}

static void prv_store_byte(const uint8_t data) {
  // Find which field this byte belongs to based on the number of bytes we've received so far
  if (s_frame.length >= FRAME_PAYLOAD_OFFSET) {
    // This byte is part of either the payload or the checksum
    const uint32_t payload_length = s_frame.length - FRAME_PAYLOAD_OFFSET;
    if (payload_length > s_frame.max_payload_length) {
      // The payload is longer than the payload buffer so drop this byte
      s_frame.should_drop = true;
    } else {
      // The checksum byte comes after the payload in the frame. This byte we are receiving could
      // be the checksum byte, or it could be part of the payload; we don't know at this point. So,
      // we will store it temporarily in footer_byte and if we've previously stored a byte there,
      // will copy that previous byte into the payload. This avoids us overrunning the payload
      // buffer if it's conservatively sized.
      if (payload_length > 0) {
        // put the previous byte into the payload buffer
        s_frame.payload[payload_length - 1] = s_frame.footer_byte;
      }
      s_frame.footer_byte = data;
    }
  } else if (s_frame.length >= FRAME_PROFILE_OFFSET) {
    // This byte is part of the profile field
    const uint32_t byte_offset = s_frame.length - FRAME_PROFILE_OFFSET;
    s_frame.header.profile |= (data << (byte_offset * 8));
  } else if (s_frame.length >= FRAME_FLAGS_OFFSET) {
    // This byte is part of the flags field
    const uint32_t byte_offset = s_frame.length - FRAME_FLAGS_OFFSET;
    s_frame.header.flags |= (data << (byte_offset * 8));
  } else {
    // The version field should always be first (and a single byte)
    s_frame.header.version = data;
  }

  // increment the length run the CRC calculation
  s_frame.length++;
  crc8_calculate_bytes_streaming(&data, sizeof(data), &s_frame.checksum);
}

static void prv_frame_validate(void) {
  if ((s_frame.should_drop == false) &&
      (s_frame.header.version > 0) &&
      (s_frame.header.version <= PROTOCOL_VERSION) &&
      (FLAGS_GET(s_frame.header.flags, FLAGS_IS_MASTER_MASK, FLAGS_IS_MASTER_OFFSET) == 1) &&
      (FLAGS_GET(s_frame.header.flags, FLAGS_RESERVED_MASK, FLAGS_RESERVED_OFFSET) == 0) &&
      (s_frame.header.profile > SmartstrapProfileInvalid) &&
      (s_frame.header.profile < NumSmartstrapProfiles) &&
      (s_frame.length >= FRAME_MIN_LENGTH) &&
      (s_frame.checksum == 0)) {
    // this is a valid frame
  } else {
    // drop the frame
    s_frame.should_drop = true;
  }
}

bool pebble_handle_byte(uint8_t data, size_t *length, bool *is_read) {
  if (!s_frame.read_ready || s_frame.should_drop) {
    // we shouldn't be reading new data
    return false;
  }

  bool hdlc_err, should_store = false;
  bool is_complete = hdlc_streaming_decode(&s_frame.hdlc_ctx, &data, &should_store, &hdlc_err);
  if (hdlc_err) {
    s_frame.should_drop = true;
  } else if (is_complete) {
    prv_frame_validate();
  } else if (should_store) {
    prv_store_byte(data);
  }

  if (s_frame.should_drop || is_complete) {
    // prepare the HDLC context for the next frame
    hdlc_streaming_decode_reset(&s_frame.hdlc_ctx);
  }

  if (is_complete) {
    if (s_frame.should_drop) {
      // reset the frame
      pebble_prepare_for_read(s_frame.payload, s_frame.max_payload_length);
    } else {
      if (s_frame.header.profile == SmartstrapProfileLinkControl) {
        // handle this link control frame
        prv_handle_link_control(s_frame.payload);
        // prepare for the next frame
        pebble_prepare_for_read(s_frame.payload, s_frame.max_payload_length);
      } else {
        s_frame.read_ready = false;
        *length = s_frame.length - FRAME_MIN_LENGTH;
        *is_read = FLAGS_GET(s_frame.header.flags, FLAGS_IS_READ_MASK, FLAGS_IS_READ_OFFSET);
        return true;
      }
    }
  }

  return false;
}

bool pebble_write(const uint8_t *data, size_t length) {
  if (!s_can_respond) {
    return false;
  }
  prv_write_internal(SmartstrapProfileRawData, data, length);
  s_can_respond = false;
  return true;
}

void pebble_notify(void) {
  s_callbacks.control(PebbleControlEnableTX, 0);
  s_callbacks.control(PebbleControlSetParityEven, 0);
  s_callbacks.write_byte(0x00);
  s_callbacks.write_byte(0x00);
  s_callbacks.write_byte(0x00);
  // we must flush before changing the parity back
  s_callbacks.control(PebbleControlFlushTX, 0);
  s_callbacks.control(PebbleControlDisableTX, 0);
  s_callbacks.control(PebbleControlSetParityNone, 0);
}

bool pebble_is_connected(void) {
  return s_connected;
}

/* This library is for communicating with a Pebble Time via the accessory port for Smart Straps. */

#include "PebbleSerial.h"

#include "crc.h"
#include "encoding.h"
#include "board.h"

#define PROTOCOL_VERSION              1
#define GENERIC_SERVICE_VERSION       1

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
  SmartstrapProfileGenericService = 0x03,
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
  EncodingStreamingContext encoding_ctx;
} PebbleFrameInfo;

typedef struct __attribute__((packed)) {
  uint8_t version;
  uint16_t service_id;
  uint16_t attribute_id;
  uint8_t type;
  uint8_t error;
  uint16_t length;
  uint8_t data[];
} GenericServicePayload;

static SmartstrapRequestType s_last_generic_service_type;
static uint32_t s_last_message_time = 0;
static PebbleFrameInfo s_frame;
static SmartstrapCallback s_callback;
static bool s_connected;
static PebbleBaud s_current_baud = PebbleBaudInvalid;
static PebbleBaud s_target_baud = PebbleBaudInvalid;
static const uint32_t BAUDS[] = { 9600, 14400, 19200, 28800, 38400, 57600, 67500, 115200, 125000,
                                  230400, 250000, 460800 };
static uint16_t s_notify_service;
static uint16_t s_notify_attribute;
static const uint16_t *s_supported_services;
static uint8_t s_num_supported_services;
static struct {
  bool can_respond;
  uint16_t service_id;
  uint16_t attribute_id;
} s_pending_response;


void prv_set_baud(PebbleBaud baud) {
  if (baud == s_current_baud) {
    return;
  }
  s_current_baud = baud;
  s_callback(SmartstrapCmdSetBaudRate, BAUDS[baud]);
  s_callback(SmartstrapCmdSetTxEnabled, true);
  s_callback(SmartstrapCmdSetTxEnabled, false);
}

void pebble_init(SmartstrapCallback callback, PebbleBaud baud, const uint16_t *services,
                 uint8_t num_services) {
  s_callback = callback;
  s_target_baud = baud;
  s_supported_services = services;
  s_num_supported_services = num_services;
  prv_set_baud(PebbleBaud9600);
}

void pebble_prepare_for_read(uint8_t *buffer, size_t length) {
  s_frame = (PebbleFrameInfo) {
    .payload = buffer,
    .max_payload_length = length,
    .read_ready = true
  };
}

static void prv_send_flag(void) {
  s_callback(SmartstrapCmdWriteByte, ENCODING_FLAG);
}

static void prv_send_byte(uint8_t data, uint8_t *parity) {
  crc8_calculate_byte_streaming(data, parity);
  if (encoding_encode(&data)) {
    s_callback(SmartstrapCmdWriteByte, ENCODING_ESCAPE);
  }
  s_callback(SmartstrapCmdWriteByte, data);
}

static void prv_write_internal(SmartstrapProfile profile, const uint8_t *data1, size_t length1,
                               const uint8_t *data2, size_t length2, bool is_notify) {
  uint8_t parity = 0;

  // enable tx
  s_callback(SmartstrapCmdSetTxEnabled, true);

  // send flag
  prv_send_flag();

  // send version
  prv_send_byte(PROTOCOL_VERSION, &parity);

  // send header flags (currently just hard-coded)
  if (is_notify) {
    prv_send_byte(0x04, &parity);
  } else {
    prv_send_byte(0, &parity);
  }
  prv_send_byte(0, &parity);
  prv_send_byte(0, &parity);
  prv_send_byte(0, &parity);

  // send profile (currently well within a single byte)
  prv_send_byte(profile, &parity);
  prv_send_byte(0, &parity);

  // send data
  size_t i;
  for (i = 0; i < length1; ++i) {
    prv_send_byte(data1[i], &parity);
  }
  for (i = 0; i < length2; ++i) {
    prv_send_byte(data2[i], &parity);
  }

  // send parity
  prv_send_byte(parity, &parity);

  // send flag
  prv_send_flag();

  // flush and disable tx
  s_callback(SmartstrapCmdSetTxEnabled, false);
}

static bool prv_supports_raw_data_profile(void) {
  uint8_t i;
  for (i = 0; i < s_num_supported_services; i++) {
    if (s_supported_services[i] == 0x0000) {
      return true;
    }
  }
  return false;
}

static bool prv_supports_generic_profile(void) {
  uint8_t i;
  for (i = 0; i < s_num_supported_services; i++) {
    if (s_supported_services[i] > 0x0000) {
      return true;
    }
  }
  return false;
}

static void prv_handle_link_control(uint8_t *buffer) {
  // we will re-use the buffer for the response
  LinkControlType type = buffer[1];
  if (type == LinkControlTypeStatus) {
    if (s_current_baud != s_target_baud) {
      buffer[2] = LinkControlStatusBaudRate;
    } else {
      buffer[2] = LinkControlStatusOk;
      s_connected = true;
    }
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 3, NULL, 0, false);
  } else if (type == LinkControlTypeProfiles) {
    uint16_t profiles[2];
    uint8_t num_profiles = 0;
    if (prv_supports_raw_data_profile()) {
      profiles[num_profiles++] = SmartstrapProfileRawData;
    }
    if (prv_supports_generic_profile()) {
      profiles[num_profiles++] = SmartstrapProfileGenericService;
    }
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 2, (uint8_t *)profiles,
                       num_profiles * sizeof(uint16_t), false);
  } else if (type == LinkControlTypeBaud) {
    buffer[2] = 0x05;
    prv_write_internal(SmartstrapProfileLinkControl, buffer, 3, NULL, 0, false);
    prv_set_baud(s_target_baud);
  }
}

static bool prv_handle_generic_service(GenericServicePayload *data) {
  if (data->error != 0) {
    return true;
  }

  uint16_t service_id = data->service_id;
  uint16_t attribute_id = data->attribute_id;
  s_last_generic_service_type = data->type;
  uint16_t length = data->length;
  if ((service_id == 0x0101) && (attribute_id == 0x0002)) {
    // notification info attribute
    if (s_notify_service) {
      uint16_t info[2] = {s_notify_service, s_notify_attribute};
      length = sizeof(info);
      s_pending_response.can_respond = true;
      s_pending_response.service_id = service_id;
      s_pending_response.attribute_id = attribute_id;
      pebble_write(true, (uint8_t *)&info, length);
    }
    return true;
  } else if ((service_id == 0x0101) && (attribute_id == 0x0001)) {
    // this is a service discovery frame
    s_pending_response.can_respond = true;
    s_pending_response.service_id = service_id;
    s_pending_response.attribute_id = attribute_id;
    pebble_write(true, (uint8_t *)s_supported_services,
                 s_num_supported_services * sizeof(uint16_t));
    return true;
  }
  return false;
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
  crc8_calculate_byte_streaming(data, &s_frame.checksum);
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

bool pebble_handle_byte(uint8_t data, uint16_t *service_id, uint16_t *attribute_id, size_t *length,
                        SmartstrapRequestType *type, uint32_t time) {
  if (!s_frame.read_ready) {
    // we shouldn't be reading new data
    return false;
  }

  bool encoding_err, should_store = false;
  bool is_complete = encoding_streaming_decode(&s_frame.encoding_ctx, &data, &should_store,
                                               &encoding_err);
  if (encoding_err) {
    s_frame.should_drop = true;
  } else if (is_complete) {
    prv_frame_validate();
  } else if (should_store) {
    prv_store_byte(data);
  }

  if (s_frame.should_drop || is_complete) {
    // prepare the encoding context for the next frame
    encoding_streaming_decode_reset(&s_frame.encoding_ctx);
  }

  if (is_complete) {
    bool give_to_user = false;
    if (s_frame.should_drop) {
      // reset the frame
      pebble_prepare_for_read(s_frame.payload, s_frame.max_payload_length);
    } else if (s_frame.header.profile == SmartstrapProfileLinkControl) {
      s_last_message_time = time;
      // handle this link control frame
      prv_handle_link_control(s_frame.payload);
      // prepare for the next frame
      pebble_prepare_for_read(s_frame.payload, s_frame.max_payload_length);
    } else if (s_frame.header.profile == SmartstrapProfileGenericService) {
      GenericServicePayload header = *(GenericServicePayload *)s_frame.payload;
      memmove(s_frame.payload, &s_frame.payload[sizeof(header)], header.length);
      // handle this generic service frame
      if (prv_handle_generic_service(&header)) {
        s_last_message_time = time;
        // we handled it, so prepare for the next frame
        pebble_prepare_for_read(s_frame.payload, s_frame.max_payload_length);
      } else {
        // pass up to user to handle
        give_to_user = true;
        *service_id = header.service_id;
        *attribute_id = header.attribute_id;
        *length = header.length;
        *type = header.type;
      }
    } else {
      give_to_user = true;
      *service_id = 0;
      *attribute_id = 0;
      *length = s_frame.length - FRAME_MIN_LENGTH;
      if (FLAGS_GET(s_frame.header.flags, FLAGS_IS_READ_MASK, FLAGS_IS_READ_OFFSET)) {
        if (*length) {
          *type = SmartstrapRequestTypeWriteRead;
        } else {
          *type = SmartstrapRequestTypeRead;
        }
      } else {
        *type = SmartstrapRequestTypeWrite;
      }
    }
    if (give_to_user) {
      s_last_message_time = time;
      s_frame.read_ready = false;
      s_pending_response.service_id = *service_id;
      s_pending_response.attribute_id = *attribute_id;
      s_pending_response.can_respond = true;
      return true;
    }
  }

  if (time < s_last_message_time) {
    // wrapped around
    s_last_message_time = time;
  } else if (time - s_last_message_time > 10000) {
    // haven't received a valid frame in over 10 seconds so reset the baudrate
    prv_set_baud(PebbleBaud9600);
    s_connected = false;
  }

  return false;
}

bool pebble_write(bool success, const uint8_t *buffer, uint16_t length) {
  if (!s_pending_response.can_respond) {
    return false;
  }
  if (s_pending_response.service_id == 0) {
    if (s_pending_response.attribute_id != 0) {
      return false;
    }
    prv_write_internal(SmartstrapProfileRawData, buffer, length, NULL, 0, false);
  } else if (s_pending_response.service_id < 0x00FF) {
    return false;
  } else {
    GenericServicePayload frame = (GenericServicePayload ) {
      .version = GENERIC_SERVICE_VERSION,
      .service_id = s_pending_response.service_id,
      .attribute_id = s_pending_response.attribute_id,
      .type = s_last_generic_service_type,
      .error = success ? 0 : 1,
      .length = length
    };
    prv_write_internal(SmartstrapProfileGenericService, (uint8_t *)&frame, sizeof(frame), buffer,
                       length, false);
  }
  s_pending_response.can_respond = false;
  return true;
}

void pebble_notify(uint16_t service_id, uint16_t attribute_id) {
  s_notify_service = service_id;
  s_notify_attribute = attribute_id;
  SmartstrapProfile profile;
  if (service_id == 0) {
    profile = SmartstrapProfileRawData;
  } else {
    profile = SmartstrapProfileGenericService;
  }
  s_callback(SmartstrapCmdSetTxEnabled, true);
  s_callback(SmartstrapCmdWriteBreak, 0);
  s_callback(SmartstrapCmdWriteBreak, 0);
  s_callback(SmartstrapCmdWriteBreak, 0);
  s_callback(SmartstrapCmdSetTxEnabled, false);
  prv_write_internal(profile, NULL, 0, NULL, 0, true);
}

bool pebble_is_connected(uint32_t time) {
  if (time - s_last_message_time > 10000) {
    prv_set_baud(PebbleBaud9600);
    s_connected = false;
  }
  return s_connected;
}

#include <pebble.h>

#define TIMEOUT_MS 1000
#define MAX_READ_SIZE 100

static Window *s_main_window;
static TextLayer *s_status_layer;
static TextLayer *s_attr_text_layer;
static TextLayer *s_raw_text_layer;
static char s_text_buffer1[20];
static char s_text_buffer2[20];
static SmartstrapAttribute *s_raw_attribute;
static SmartstrapAttribute *s_attr_attribute;

static void prv_update_text(void) {
  if (smartstrap_service_is_available(SMARTSTRAP_RAW_DATA_SERVICE_ID)) {
    text_layer_set_text(s_status_layer, "Connected!");
  } else {
    text_layer_set_text(s_status_layer, "Connecting...");
  }
}

static void prv_did_read(SmartstrapAttribute *attr, SmartstrapResult result,
                              const uint8_t *data, size_t length) {
  if (attr == s_attr_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "did_read(s_attr_attribute, %d, %d)", result, length);
    if (result == SmartstrapResultOk && length == 4) {
      uint32_t num;
      memcpy(&num, data, 4);
      snprintf(s_text_buffer1, 20, "%u", (unsigned int)num);
      text_layer_set_text(s_attr_text_layer, s_text_buffer1);
    }
  } else if (attr == s_raw_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "did_read(s_raw_attribute, %d, %d)", result, length);
    if (result == SmartstrapResultOk && length == 4) {
      uint32_t time;
      memcpy(&time, data, 4);
      snprintf(s_text_buffer2, 20, "%u", (unsigned int)time);
      text_layer_set_text(s_raw_text_layer, s_text_buffer2);
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "did_read(<%p>, %d)", attr, result);
  }
}

static void prv_did_write(SmartstrapAttribute *attr, SmartstrapResult result) {
  if (attr == s_attr_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "did_write(s_attr_attribute, %d)", result);
  } else if (attr == s_raw_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "did_write(s_raw_attribute, %d)", result);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "did_write(<%p>, %d)", attr, result);
  }
}

static void prv_write_read_test_attr(void) {
  SmartstrapResult result;
  if (!smartstrap_service_is_available(smartstrap_attribute_get_service_id(s_attr_attribute))) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "s_attr_attribute is not available");
    return;
  }

  // get the write buffer
  uint8_t *buffer = NULL;
  size_t length = 0;
  result = smartstrap_attribute_begin_write(s_attr_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Write of s_attr_attribute failed with result %d", result);
    return;
  }

  // write the data into the buffer
  uint32_t num = rand() % 10000;
  memcpy(buffer, &num, 4);

  // send it off
  result = smartstrap_attribute_end_write(s_attr_attribute, sizeof(num), true);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Write of s_attr_attribute failed with result %d", result);
  }
}

static void prv_read_raw(void) {
  if (!smartstrap_service_is_available(smartstrap_attribute_get_service_id(s_raw_attribute))) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "s_raw_attribute is not available");
    return;
  }
  SmartstrapResult result = smartstrap_attribute_read(s_raw_attribute);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read of s_raw_attribute failed with result: %d", result);
  }
}

static void prv_send_request(void *context) {
  prv_write_read_test_attr();
  prv_read_raw();
  app_timer_register(900, prv_send_request, NULL);
}

static void prv_availablility_status_changed(SmartstrapServiceId service_id, bool is_available) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Availability for 0x%x is %d", service_id, is_available);
  prv_update_text();
}

static void prv_notified(SmartstrapAttribute *attr) {
  if (attr == s_attr_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "notified(s_attr_attribute)");
  } else if (attr == s_raw_attribute) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "notified(s_raw_attribute)");
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "notified(<%p>)", attr);
  }
}

static void prv_main_window_load(Window *window) {
  s_status_layer = text_layer_create(GRect(0, 15, 144, 40));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  prv_update_text();
  text_layer_set_text_color(s_status_layer, GColorBlack);
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_status_layer));

  s_attr_text_layer = text_layer_create(GRect(0, 60, 144, 40));
  text_layer_set_font(s_attr_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text(s_attr_text_layer, "-");
  text_layer_set_text_color(s_attr_text_layer, GColorBlack);
  text_layer_set_background_color(s_attr_text_layer, GColorClear);
  text_layer_set_text_alignment(s_attr_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_attr_text_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_attr_text_layer));

  s_raw_text_layer = text_layer_create(GRect(0, 100, 144, 40));
  text_layer_set_font(s_raw_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text(s_raw_text_layer, "-");
  text_layer_set_text_color(s_raw_text_layer, GColorBlack);
  text_layer_set_background_color(s_raw_text_layer, GColorClear);
  text_layer_set_text_alignment(s_raw_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_raw_text_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_raw_text_layer));
}

static void prv_main_window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
}

static void prv_init(void) {
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = prv_main_window_load,
    .unload = prv_main_window_unload
  });
  window_stack_push(s_main_window, true);
  SmartstrapHandlers handlers = (SmartstrapHandlers) {
    .availability_did_change = prv_availablility_status_changed,
    .did_write = prv_did_write,
    .did_read = prv_did_read,
    .notified = prv_notified
  };
  smartstrap_subscribe(handlers);
  smartstrap_set_timeout(50);
  s_raw_attribute = smartstrap_attribute_create(0, 0, 2000);
  s_attr_attribute = smartstrap_attribute_create(0x1001, 0x1001, 20);
  app_timer_register(1000, prv_send_request, NULL);
}

static void prv_deinit(void) {
  window_destroy(s_main_window);
  smartstrap_unsubscribe();
}

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "STARTING APP");
  if (s_attr_attribute && s_raw_attribute) {
    app_event_loop();
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "ENDING APP");
  prv_deinit();
}


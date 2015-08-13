#include <pebble.h>

#define TIMEOUT_MS 1000
#define MAX_READ_SIZE 100

static Window *s_main_window;
static TextLayer *s_status_layer;
static TextLayer *s_info_layer;
#if 0
static uint8_t s_buffer[MAX_READ_SIZE + 1];
static bool s_did_set;
#endif
static char s_text_buffer[20];
static SmartstrapAttribute *s_attr;

static void prv_update_text(void) {
  if (smartstrap_service_is_available(SMARTSTRAP_RAW_DATA_SERVICE_ID)) {
    text_layer_set_text(s_status_layer, "Connected!");
  } else {
    text_layer_set_text(s_status_layer, "Connecting...");
  }
}

#if 0
static void prv_read_done(uint16_t service_id, uint16_t attribute_id, bool success,
                          uint32_t length) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "HERE: success=%d, length=%d", success, (int)length);
  if (success) {
    if (s_did_set) {
      if (length != 0) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "UNEXPECTED LENGTH");
        return;
      }
      // this is a response to our write, so do a read now
      smartstrap_get_attribute(service_id, attribute_id, s_buffer, MAX_READ_SIZE);
      s_did_set = false;
    } else {
      if (length != 4) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "UNEXPECTED LENGTH");
        return;
      }
      // this is the response to our read
      uint32_t time;
      memcpy(&time, s_buffer, 4);
      snprintf(s_text_buffer, 20, "%u", (unsigned int)time);
      text_layer_set_text(s_info_layer, s_text_buffer);
    }
  }
}

static void prv_notify_callback(uint16_t service_id, uint16_t attribute_id) {
  if (service_id != 0x1001 || attribute_id != 0x1001) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "GOT NOTIFY FOR UNEXPECTED SERVICE/ATTRIBUTE (0x%x,0x%x)",
            service_id, attribute_id);
    return;
  }
  uint32_t data = rand() % 1000;
  memcpy(s_buffer, &data, 4);
  smartstrap_set_attribute(0x1001, 0x1001, s_buffer, 4);
  s_did_set = true;
}
#endif

static void prv_read_complete(SmartstrapAttribute *attr, SmartstrapResult result,
                              const uint8_t *data, size_t length) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "GOT READ: result=%d, length=%d", result, length);
  if (result == SmartstrapResultOk) {
    if (length == 4) {
      uint32_t time;
      memcpy(&time, data, 4);
      snprintf(s_text_buffer, 20, "%u", (unsigned int)time);
      text_layer_set_text(s_info_layer, s_text_buffer);
    }
  }
}

static void prv_send_request(void *context) {
  SmartstrapResult result = smartstrap_attribute_read(s_attr);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "DID READ: result=%d", result);
  app_timer_register(900, prv_send_request, NULL);
}

static void prv_availablility_status_changed(SmartstrapServiceId service_id, bool is_available) {
  prv_update_text();
}

static void prv_main_window_load(Window *window) {
  s_status_layer = text_layer_create(GRect(0, 25, 144, 40));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  prv_update_text();
  text_layer_set_text_color(s_status_layer, GColorBlack);
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_status_layer));

  s_info_layer = text_layer_create(GRect(0, 70, 144, 60));
  text_layer_set_font(s_info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text(s_info_layer, "-");
  text_layer_set_text_color(s_info_layer, GColorBlack);
  text_layer_set_background_color(s_info_layer, GColorClear);
  text_layer_set_text_alignment(s_info_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_info_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_info_layer));
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
    .did_read = prv_read_complete
  };
  smartstrap_subscribe(handlers);
  smartstrap_set_timeout(50);
  s_attr = smartstrap_attribute_create(0, 0, 20);
  (void)*(uint8_t *)s_attr; // ASSERT that it's valid
  app_timer_register(1000, prv_send_request, NULL);
}

static void prv_deinit(void) {
  window_destroy(s_main_window);
  smartstrap_unsubscribe();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}


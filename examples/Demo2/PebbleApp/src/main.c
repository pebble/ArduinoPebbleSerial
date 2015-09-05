#include <pebble.h>

static const SmartstrapServiceId SERVICE_ID = 0x1001;
static const SmartstrapAttributeId LED_ATTRIBUTE_ID = 0x0001;
static const size_t LED_ATTRIBUTE_LENGTH = 1;
static const SmartstrapAttributeId UPTIME_ATTRIBUTE_ID = 0x0002;
static const size_t UPTIME_ATTRIBUTE_LENGTH = 4;

static SmartstrapAttribute *led_attribute;
static SmartstrapAttribute *uptime_attribute;

static Window *window;
static TextLayer *status_text_layer;
static TextLayer *uptime_text_layer;


static void prv_availability_changed(SmartstrapServiceId service_id, bool available) {
  if (service_id != SERVICE_ID) {
    return;
  }

  if (available) {
    text_layer_set_background_color(status_text_layer, GColorGreen);
    text_layer_set_text(status_text_layer, "Connected!");
  } else {
    text_layer_set_background_color(status_text_layer, GColorRed);
    text_layer_set_text(status_text_layer, "Disconnected!");
  }
}

static void prv_did_read(SmartstrapAttribute *attr, SmartstrapResult result,
                         const uint8_t *data, size_t length) {
  if (attr != uptime_attribute) {
    return;
  }
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read failed with result %d", result);
    return;
  }
  if (length != UPTIME_ATTRIBUTE_LENGTH) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Got response of unexpected length (%d)", length);
    return;
  }

  static char uptime_buffer[20];
  snprintf(uptime_buffer, 20, "%u", (unsigned int)*(uint32_t *)data);
  text_layer_set_text(uptime_text_layer, uptime_buffer);
}

static void prv_notified(SmartstrapAttribute *attribute) {
  if (attribute != uptime_attribute) {
    return;
  }
  smartstrap_attribute_read(uptime_attribute);
}


static void prv_set_led_attribute(bool on) {
  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(led_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = on;

  result = smartstrap_attribute_end_write(led_attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_set_led_attribute(true);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_set_led_attribute(false);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);

  // text layer for connection status
  status_text_layer = text_layer_create(GRect(0, 0, 144, 40));
  text_layer_set_font(status_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(status_text_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(status_text_layer));
  prv_availability_changed(SERVICE_ID, smartstrap_service_is_available(SERVICE_ID));

  // text layer for showing the attribute
  uptime_text_layer = text_layer_create(GRect(0, 60, 144, 40));
  text_layer_set_font(uptime_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(uptime_text_layer, GTextAlignmentCenter);
  text_layer_set_text(uptime_text_layer, "-");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(uptime_text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(status_text_layer);
  text_layer_destroy(uptime_text_layer);
}

static void init(void) {
  // setup window
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  // setup smartstrap
  SmartstrapHandlers handlers = (SmartstrapHandlers) {
    .availability_did_change = prv_availability_changed,
    .did_read = prv_did_read,
    .notified = prv_notified
  };
  smartstrap_subscribe(handlers);
  led_attribute = smartstrap_attribute_create(SERVICE_ID, LED_ATTRIBUTE_ID, LED_ATTRIBUTE_LENGTH);
  uptime_attribute = smartstrap_attribute_create(SERVICE_ID, UPTIME_ATTRIBUTE_ID,
                                                 UPTIME_ATTRIBUTE_LENGTH);
}

static void deinit(void) {
  window_destroy(window);
  smartstrap_attribute_destroy(led_attribute);
  smartstrap_attribute_destroy(uptime_attribute);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

#include <Arduino.h>
#include <ArduinoPebbleSerial.h>
#include <Wire.h>
#include <xadow.h>

#define NES_STROBE_PIN  15
#define NES_CLOCK_PIN   16
#define NES_DATA_PIN    14
#define NES_DELAY_US    100

static const uint8_t PEBBLE_RECV_BUFFER_SIZE = 250;
static uint8_t s_pebble_buffer[PEBBLE_RECV_BUFFER_SIZE+5];

void prv_nes_init(void) {
  pinMode(NES_STROBE_PIN, OUTPUT);
  pinMode(NES_CLOCK_PIN, OUTPUT);
  pinMode(NES_DATA_PIN, INPUT);
}

void prv_nes_strobe(void) {
  digitalWrite(NES_STROBE_PIN, HIGH);
  delayMicroseconds(NES_DELAY_US);
  digitalWrite(NES_STROBE_PIN, LOW);
}

uint8_t prv_nes_shift_in(void) {
  uint8_t ret = digitalRead(NES_DATA_PIN);
  delayMicroseconds(NES_DELAY_US);
  digitalWrite(NES_CLOCK_PIN, HIGH);
  delayMicroseconds(NES_DELAY_US);
  digitalWrite(NES_CLOCK_PIN, LOW);
  return ret;
}

uint8_t prv_nes_get_buttons(void) {
  uint8_t ret = 0;
  uint8_t i;
  prv_nes_strobe();
  for (i = 0; i < 8; i++) {
    ret |= prv_nes_shift_in() << i;
  }
  return ~ret;
}

void setup() {
  // General init
  Serial.begin(115200);
  prv_nes_init();

  // Xadow init
  Xadow.init();
  Xadow.greenLed(LEDON);
  Xadow.redLed(LEDON);

  // Setup the Pebble smartstrap connection
  ArduinoPebbleSerial::begin(s_pebble_buffer, PEBBLE_RECV_BUFFER_SIZE);
}

void loop() {
  static uint8_t prev_buttons = 0;
  uint8_t buttons = prv_nes_get_buttons();
  if (buttons != prev_buttons) {
    Xadow.greenLed(LEDCHG);
    prev_buttons = buttons;
  }
  uint8_t length;
  bool is_read;
  if (ArduinoPebbleSerial::feed(&length, &is_read)) {
    if (is_read) {
      // re-use buffer for response
      s_pebble_buffer[0] = buttons;
      pebble_write(s_pebble_buffer, 1);
    }
    Xadow.redLed(LEDCHG);
  }
}


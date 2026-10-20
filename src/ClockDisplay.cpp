#include "ClockDisplay.h"

#include <Wire.h>
#include <string.h>

#include "Config.h"

static bool command(uint8_t c) {
  Wire.beginTransmission(CLOCK_ADDR);
  Wire.write(c);
  return Wire.endTransmission() == 0;
}

static uint16_t reverse16(uint16_t v) {
  uint16_t r = 0;
  for (int i = 0; i < 16; i++) r = (uint16_t)(r << 1 | ((v >> i) & 1));
  return r;
}

bool ClockDisplay::begin() {
  ok_ = command(0x21);  // system oscillator on
  return ok_;
}

void ClockDisplay::show(const uint16_t rows[8], uint8_t level) {
  if (!ok_) return;
  if (level != level_) {
    command(level ? 0x81 : 0x80);                     // display on / off, no blinking
    if (level) command((uint8_t)(0xE0 | (level - 1)));  // 16 dimming steps
    level_ = level;
  }
  if (synced_ && !memcmp(rows, shown_, sizeof shown_)) return;  // only touch I2C when a digit changes
  memcpy(shown_, rows, sizeof shown_);
  synced_ = true;
  Wire.beginTransmission(CLOCK_ADDR);
  Wire.write((uint8_t)0x00);  // display RAM: two bytes per row, columns 0-7 then 8-15
  for (int r = 0; r < 8; r++) {
    uint16_t v = rows[CLOCK_FLIP_Y ? 7 - r : r];
    if (CLOCK_FLIP_X) v = reverse16(v);
    Wire.write((uint8_t)(v & 0xFF));
    Wire.write((uint8_t)(v >> 8));
  }
  Wire.endTransmission();
}

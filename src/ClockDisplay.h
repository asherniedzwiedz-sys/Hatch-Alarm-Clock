#pragma once
// Optional HT16K33 16x8 dot-matrix clock behind the fabric, on the same I2C bus as the RTC.
#include <stdint.h>

#include "Config.h"

class ClockDisplay {
 public:
  bool begin();  // tries CLOCK_ADDR..+7; false if nothing answers: the clock is optional
  void show(const uint16_t rows[8], uint8_t level);  // level 0 = off, 1-16 brightness
  bool present() const { return ok_; }
  uint8_t address() const { return addr_; }

 private:
  bool command(uint8_t c);
  bool ok_ = false, synced_ = false;
  uint8_t addr_ = CLOCK_ADDR;
  uint16_t shown_[8] = {};
  int level_ = -1;
};

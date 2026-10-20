#pragma once
// Optional HT16K33 16x8 dot-matrix clock behind the fabric, on the same I2C bus as the RTC.
#include <stdint.h>

class ClockDisplay {
 public:
  bool begin();  // false if nothing answers at CLOCK_ADDR: the clock is optional
  void show(const uint16_t rows[8], uint8_t level);  // level 0 = off, 1-16 brightness
  bool present() const { return ok_; }

 private:
  bool ok_ = false, synced_ = false;
  uint16_t shown_[8] = {};
  int level_ = -1;
};

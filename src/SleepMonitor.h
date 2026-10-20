#pragma once
// Contactless sleep tracking: HLK-LD2410C radar on UART2 -> SleepTracker -> last SLEEP_NIGHTS nights in NVS.
// Everything stays on the clock; nothing is sent anywhere.
#include <Arduino.h>
#include <Preferences.h>

#include "SleepLogic.h"

class SleepMonitor {
 public:
  void begin();
  void loop(bool timeValid, bool enabled, uint16_t rangeCm);  // call every loop
  bool sensorOk() const { return lastFrameMs_ && millis() - lastFrameMs_ < 3000; }
  void liveJson(JsonObject out) const;
  void nightsJson(JsonArray out) const;  // newest first
  void clear();

 private:
  Ld2410Parser parser_;
  RadarReport last_ = {};
  uint32_t lastFrameMs_ = 0;
  time_t lastSec_ = 0;
  bool present_ = false;
  SleepTracker tracker_;
  Preferences prefs_;
  Night nights_[SLEEP_NIGHTS];
  int count_ = 0;
};

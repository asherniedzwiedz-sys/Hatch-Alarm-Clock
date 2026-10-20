#include "SleepMonitor.h"

#include <time.h>

void SleepMonitor::begin() {
  Serial2.begin(256000, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);  // LD2410C default baud
  prefs_.begin("sleep", false);
  const size_t len = prefs_.isKey("nights") ? prefs_.getBytesLength("nights") : 0;
  if (len && len % sizeof(Night) == 0 && len <= sizeof nights_) {
    prefs_.getBytes("nights", nights_, len);
    count_ = (int)(len / sizeof(Night));
    if (nights_[0].version != NIGHT_VERSION) count_ = 0;  // layout changed by a firmware update
  }
}

void SleepMonitor::loop(bool timeValid, bool enabled, uint16_t rangeCm) {
  RadarReport r;
  while (Serial2.available())
    if (parser_.feed((uint8_t)Serial2.read(), r)) {
      last_ = r;
      lastFrameMs_ = millis();
    }

  const time_t now = time(nullptr);
  if (now == lastSec_) return;
  lastSec_ = now;
  present_ = sensorOk() && inBed(last_, rangeCm);
  if (!enabled) {
    tracker_ = SleepTracker{};  // switched off: forget any night in progress
    return;
  }
  if (!timeValid) return;
  struct tm lt;
  localtime_r(&now, &lt);
  const uint8_t motion = present_ && (last_.state & 1) ? last_.moveEnergy : 0;
  if (!tracker_.second(now, lt.tm_hour, present_, motion)) return;

  memmove(nights_ + 1, nights_, sizeof(Night) * (SLEEP_NIGHTS - 1));  // newest first
  nights_[0] = tracker_.night;
  if (count_ < SLEEP_NIGHTS) count_++;
  prefs_.putBytes("nights", nights_, sizeof(Night) * count_);
}

void SleepMonitor::liveJson(JsonObject out) const {
  out["sensor"] = sensorOk();
  out["present"] = present_;
  out["motion"] = sensorOk() && (last_.state & 1) ? last_.moveEnergy : 0;  // calibrate SLEEP_MOVE_THRESHOLD against this
  out["distance"] = (last_.state & 2) ? last_.stillCm : last_.moveCm;
  out["tracking"] = tracker_.inSession;
  if (tracker_.inSession) {
    out["since"] = tracker_.night.start;
    out["asleep"] = tracker_.asleep;
    out["asleepMin"] = tracker_.night.asleepMin;
  }
}

void SleepMonitor::nightsJson(JsonArray out) const {
  for (int i = 0; i < count_; i++) nightToJson(nights_[i], out.add<JsonObject>());
}

void SleepMonitor::clear() {
  count_ = 0;
  tracker_ = SleepTracker{};
  prefs_.remove("nights");
}

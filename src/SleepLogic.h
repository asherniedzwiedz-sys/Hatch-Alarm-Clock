#pragma once
// Pure sleep-tracking logic, compiled into the firmware and the native tests: an LD2410/LD2410C
// report parser and a per-minute tracker. Radar gives presence plus a movement energy, so this is
// movement-based (actigraphy-style) tracking: time in bed, asleep vs awake, wake-ups. No sleep stages.
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>

#include "Config.h"

struct RadarReport {
  uint8_t state;  // bit0 = moving target, bit1 = still target
  uint16_t moveCm, stillCm;
  uint8_t moveEnergy, stillEnergy;  // 0-100
};

// Byte-at-a-time parser for the radar's report frames (basic or engineering mode):
// F4 F3 F2 F1, u16 length, [type, AA, state, move cm (2), move energy, still cm (2), still energy, ...,
// 55, 00], F8 F7 F6 F5. The protocol has no checksum, so the length, head, tail and footer are all checked.
struct Ld2410Parser {
  uint8_t buf[64];
  uint8_t n = 0;

  bool feed(uint8_t b, RadarReport& out) {
    static const uint8_t kHead[4] = {0xF4, 0xF3, 0xF2, 0xF1};
    if (n < 4) {
      if (b == kHead[n]) buf[n++] = b;
      else n = b == kHead[0] ? 1 : 0;  // resync
      return false;
    }
    buf[n++] = b;
    if (n < 6) return false;
    const unsigned len = buf[4] | buf[5] << 8;
    if (len < 13 || 6 + len + 4 > sizeof buf) {
      n = 0;
      return false;
    }
    if (n < 6 + len + 4) return false;
    n = 0;
    const uint8_t* d = buf + 6;
    const uint8_t* f = d + len;
    if (f[0] != 0xF8 || f[1] != 0xF7 || f[2] != 0xF6 || f[3] != 0xF5) return false;
    if ((d[0] != 0x01 && d[0] != 0x02) || d[1] != 0xAA || d[len - 2] != 0x55 || d[len - 1] != 0x00) return false;
    out.state = d[2] & 0x03;
    out.moveCm = (uint16_t)(d[3] | d[4] << 8);
    out.moveEnergy = d[5];
    out.stillCm = (uint16_t)(d[6] | d[7] << 8);
    out.stillEnergy = d[8];
    return true;
  }
};

// Someone in bed: a moving or still target within the bed range.
inline bool inBed(const RadarReport& r, uint16_t rangeCm) {
  return ((r.state & 1) && r.moveCm <= rangeCm) || ((r.state & 2) && r.stillCm <= rangeCm);
}

enum : uint8_t { SLEEP_AWAY = 0, SLEEP_AWAKE = 1, SLEEP_ASLEEP = 2 };
constexpr int SLEEP_BUCKET_MIN = 5;
constexpr int SLEEP_MAX_BUCKETS = 168;  // 14 h of 5-minute buckets

struct Night {
  uint8_t version;
  uint8_t wakeups;  // times asleep again after waking in the night
  uint8_t buckets;
  int64_t start, end;  // epoch: into bed, out of bed
  uint16_t inBedMin, asleepMin, awakeMin, latencyMin;  // latency: minutes from into bed to asleep
  uint8_t stage[SLEEP_MAX_BUCKETS];                    // SLEEP_AWAY / AWAKE / ASLEEP per 5 minutes
};
constexpr uint8_t NIGHT_VERSION = 1;

// Feed one sample per second. A night opens after SLEEP_OPEN_MIN minutes in bed (inside the
// bedtime window) and closes after SLEEP_CLOSE_AWAY_MIN minutes out of bed. Asleep = 5 still
// minutes in a row; awake again after 2 restless minutes or leaving the bed, so turning over
// is not a wake-up.
struct SleepTracker {
  bool inSession = false, asleep = false, lastPresent = false;
  uint8_t lastMotion = 0;
  Night night = {};

  // Returns true when a night long enough to keep has just ended; it is then in `night`.
  bool second(int64_t now, int localHour, bool present, uint8_t motion) {
    const int64_t m = now / 60;
    bool done = false;
    if (m != minute_) {
      if (samples_) done = endMinute(m * 60, localHour);
      minute_ = m;
      samples_ = presentSecs_ = 0;
      moveSum_ = 0;
    }
    samples_++;
    if (present) {
      presentSecs_++;
      moveSum_ += motion;
    }
    return done;
  }

 private:
  int64_t minute_ = -1;
  uint16_t samples_ = 0, presentSecs_ = 0, presentRun_ = 0, stillRun_ = 0, restRun_ = 0, awayRun_ = 0;
  uint32_t moveSum_ = 0;
  uint8_t votes_[3] = {}, bucketMin_ = 0;

  bool endMinute(int64_t t, int hour) {  // t = end of the finished minute
    const bool present = presentSecs_ * 2 >= samples_;
    const uint8_t motion = present ? (uint8_t)(moveSum_ / presentSecs_) : 0;
    const bool still = present && motion < SLEEP_MOVE_THRESHOLD;
    lastPresent = present;
    lastMotion = motion;
    presentRun_ = present ? presentRun_ + 1 : 0;
    stillRun_ = still ? stillRun_ + 1 : 0;
    restRun_ = still ? 0 : restRun_ + 1;
    awayRun_ = present ? 0 : awayRun_ + 1;
    if (!inSession) {
      if (presentRun_ >= SLEEP_OPEN_MIN && (hour >= SLEEP_OPEN_FROM_H || hour < SLEEP_OPEN_UNTIL_H)) open(t);
      return false;
    }
    const bool was = asleep;
    if (!present || restRun_ >= 2) asleep = false;
    else if (stillRun_ >= 5) asleep = true;
    if (asleep && !was) {
      if (night.latencyMin == 0xFFFF) night.latencyMin = (uint16_t)((t - 60 - night.start) / 60);
      else night.wakeups++;  // counted when sleep resumes, so getting up in the morning isn't one
    }
    if (asleep) night.asleepMin++;
    record(!present ? SLEEP_AWAY : asleep ? SLEEP_ASLEEP : SLEEP_AWAKE);
    if (awayRun_ >= SLEEP_CLOSE_AWAY_MIN) return close(t - awayRun_ * 60);
    if (t - night.start >= (int64_t)SLEEP_MAX_BUCKETS * SLEEP_BUCKET_MIN * 60) return close(t);
    return false;
  }

  void open(int64_t t) {
    night = Night{};
    night.version = NIGHT_VERSION;
    night.start = t - presentRun_ * 60;  // back to when they got into bed
    night.latencyMin = 0xFFFF;
    inSession = true;
    asleep = false;
    for (uint16_t i = 0; i < presentRun_; i++) record(SLEEP_AWAKE);
  }

  void record(uint8_t state) {
    votes_[state]++;
    if (++bucketMin_ == SLEEP_BUCKET_MIN) flush();
  }

  void flush() {  // majority of the bucket's minutes; ties go to asleep, then awake
    if (night.buckets < SLEEP_MAX_BUCKETS)
      night.stage[night.buckets++] = votes_[2] >= votes_[1] && votes_[2] >= votes_[0] ? SLEEP_ASLEEP
                                     : votes_[1] >= votes_[0]                         ? SLEEP_AWAKE
                                                                                      : SLEEP_AWAY;
    memset(votes_, 0, sizeof votes_);
    bucketMin_ = 0;
  }

  bool close(int64_t end) {
    if (bucketMin_) flush();
    inSession = asleep = false;
    presentRun_ = 0;
    night.end = end;
    const int64_t mins = (end - night.start) / 60;
    night.inBedMin = (uint16_t)mins;
    const int64_t keep = (mins + SLEEP_BUCKET_MIN - 1) / SLEEP_BUCKET_MIN;  // drop the trailing away buckets
    if (night.buckets > keep) night.buckets = (uint8_t)keep;
    if (night.asleepMin > night.inBedMin) night.asleepMin = night.inBedMin;
    night.awakeMin = (uint16_t)(night.inBedMin - night.asleepMin);
    if (night.latencyMin == 0xFFFF) night.latencyMin = night.inBedMin;  // never fell asleep
    return mins >= SLEEP_MIN_NIGHT;
  }
};

inline void nightToJson(const Night& n, JsonObject o) {
  o["start"] = n.start;
  o["end"] = n.end;
  o["inBed"] = n.inBedMin;
  o["asleep"] = n.asleepMin;
  o["awake"] = n.awakeMin;
  o["latency"] = n.latencyMin;
  o["wakeups"] = n.wakeups;
  char stages[SLEEP_MAX_BUCKETS + 1];  // one digit per 5 minutes: 0 away, 1 awake, 2 asleep
  uint8_t k = 0;
  for (; k < n.buckets && k < SLEEP_MAX_BUCKETS; k++) stages[k] = (char)('0' + n.stage[k]);
  stages[k] = 0;
  o["stages"] = stages;
}

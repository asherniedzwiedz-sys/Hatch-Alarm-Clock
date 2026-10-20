#pragma once
// Pure, hardware-free logic. Compiled into the firmware and into the native unit tests (test/).
#include <ArduinoJson.h>
#include <math.h>
#include <stdint.h>

#include "Config.h"

struct RGB {
  uint8_t r, g, b;
};

// Black-body colour approximation (Tanner Helland's curve fit), good for 1000-40000 K.
inline RGB kelvinToRGB(float kelvin) {
  const float t = kelvin / 100.0f;
  float r, g, b;
  if (t <= 66.0f) {
    r = 255.0f;
    g = 99.4708025861f * logf(t) - 161.1195681661f;
  } else {
    r = 329.698727446f * powf(t - 60.0f, -0.1332047592f);
    g = 288.1221695283f * powf(t - 60.0f, -0.0755148492f);
  }
  if (t >= 66.0f) b = 255.0f;
  else if (t <= 19.0f) b = 0.0f;
  else b = 138.5177312231f * logf(t - 10.0f) - 305.0447927307f;
  auto clamp8 = [](float v) -> uint8_t { return v <= 0.0f ? 0 : v >= 255.0f ? 255 : (uint8_t)(v + 0.5f); };
  return {clamp8(r), clamp8(g), clamp8(b)};
}

// Sunrise light for ramp progress p = t / 30 min: brightness = max * p^2.2.
inline float sunriseLevel(float p) { return p <= 0.0f ? 0.0f : p >= 1.0f ? 1.0f : powf(p, 2.2f); }

inline float sunriseKelvin(float p) {
  p = p < 0.0f ? 0.0f : p > 1.0f ? 1.0f : p;
  return SUNRISE_K_START + (SUNRISE_K_END - SUNRISE_K_START) * p;
}

struct Alarm {
  uint8_t hour, minute;
  uint8_t days;  // bit0 = Sunday ... bit6 = Saturday; 0 = one-shot (next match, then disables)
  bool enabled;
};

constexpr int32_t DAY_SEC = 86400, WEEK_SEC = 7 * DAY_SEC;

// Smallest offset `off` (seconds from now to an occurrence of `a`) with lo < off <= hi.
// wday: 0 = Sunday; sec: seconds since local midnight. The window must be shorter than a week.
inline bool occurrenceIn(const Alarm& a, int wday, int32_t sec, int32_t lo, int32_t hi, int32_t& off) {
  const int32_t now = wday * DAY_SEC + sec;
  bool found = false;
  for (int d = 0; d < 7; d++) {
    if (a.days && !(a.days & (1 << d))) continue;
    const int32_t ahead = ((d * DAY_SEC + a.hour * 3600 + a.minute * 60 - now) % WEEK_SEC + WEEK_SEC) % WEEK_SEC;
    const int32_t candidates[2] = {ahead - WEEK_SEC, ahead};
    for (int32_t c : candidates) {
      if (c > lo && c <= hi && (!found || c < off)) {
        off = c;
        found = true;
      }
    }
  }
  return found;
}

// Index of the alarm with the earliest occurrence in (lo, hi], or -1.
inline int findAlarm(const Alarm* al, int n, int wday, int32_t sec, int32_t lo, int32_t hi, bool enabledOnly,
                     int32_t& off) {
  int best = -1;
  for (int i = 0; i < n; i++) {
    int32_t o = 0;
    if ((enabledOnly && !al[i].enabled) || !occurrenceIn(al[i], wday, sec, lo, hi, o)) continue;
    if (best < 0 || o < off) {
      best = i;
      off = o;
    }
  }
  return best;
}

enum class Phase : uint8_t { Idle, Sunrise, Ringing, Snoozed };

// Alarm state machine. Callers pass epoch seconds plus the matching local weekday and second-of-day.
// An occurrence is live from SUNRISE_SEC before its time until RING_SEC after, so a reboot or power
// cut inside that window resumes the sunrise/ringing instead of losing the alarm.
struct AlarmMachine {
  Phase phase = Phase::Idle;
  int index = -1;           // alarm being served
  int64_t occ = 0;          // epoch of the served occurrence
  int64_t until = 0;        // Ringing: auto fade-out time; Snoozed: snooze end
  int64_t handledUpTo = 0;  // occurrences at or before this never fire again (persisted)
  float progress = 0;       // sunrise progress 0..1
  bool timedOut = false;    // last Ringing -> Idle was the auto fade-out, not a dismiss
  bool dirty = false;       // handledUpTo or alarms changed: persist

  void tick(int64_t now, int wday, int32_t sec, Alarm* al, int n) {
    if (handledUpTo > now + DAY_SEC) handledUpTo = 0;  // clock was set back
    if (phase == Phase::Ringing && now >= until) {
      finish(al, n);
      timedOut = true;
    }
    if (phase == Phase::Snoozed && now >= until) ring(now);
    if (phase != Phase::Idle && phase != Phase::Sunrise) return;

    const int64_t h = handledUpTo - now;
    const int32_t lo = h >= (int64_t)SUNRISE_SEC ? (int32_t)SUNRISE_SEC : h > -(int64_t)RING_SEC ? (int32_t)h : -(int32_t)RING_SEC;
    int32_t off = 0;
    const int i = findAlarm(al, n, wday, sec, lo, (int32_t)SUNRISE_SEC, true, off);
    if (i < 0) {
      phase = Phase::Idle;
      index = -1;
      return;
    }
    index = i;
    occ = now + off;
    if (off > 0) {
      phase = Phase::Sunrise;
      progress = 1.0f - (float)off / (float)SUNRISE_SEC;
    } else {
      phase = Phase::Ringing;
      progress = 1.0f;
      until = occ + RING_SEC;
    }
  }

  void ring(int64_t now) {
    phase = Phase::Ringing;
    progress = 1.0f;
    until = now + RING_SEC;
  }

  void snooze(int64_t now) {
    if (phase != Phase::Ringing && phase != Phase::Snoozed) return;
    phase = Phase::Snoozed;
    until = now + SNOOZE_SEC;
  }

  void dismiss(Alarm* al, int n) {
    if (phase == Phase::Idle) return;
    finish(al, n);
    timedOut = false;
  }

  // Alarms were edited: forget the served index and never fire occurrences already in the past.
  void edited(int64_t now) {
    index = -1;
    if (now > handledUpTo) handledUpTo = now;
    dirty = true;
  }

 private:
  void finish(Alarm* al, int n) {
    if (occ > handledUpTo) handledUpTo = occ;
    if (index >= 0 && index < n && al[index].days == 0) al[index].enabled = false;  // one-shot done
    phase = Phase::Idle;
    index = -1;
    dirty = true;
  }
};

inline void alarmsToJson(const Alarm* al, int n, JsonArray out) {
  for (int i = 0; i < n; i++) {
    JsonObject o = out.add<JsonObject>();
    o["hour"] = al[i].hour;
    o["minute"] = al[i].minute;
    o["days"] = al[i].days;
    o["enabled"] = al[i].enabled;
  }
}

// Parses untrusted JSON. Returns the alarm count, or -1 if anything is invalid (`out` is then scratch).
inline int alarmsFromJson(JsonArrayConst in, Alarm* out, int max) {
  if (in.isNull() || (int)in.size() > max) return -1;
  int n = 0;
  for (JsonVariantConst v : in) {
    JsonObjectConst o = v.as<JsonObjectConst>();
    if (o.isNull() || !o["hour"].is<int>() || !o["minute"].is<int>()) return -1;
    if ((!o["days"].isNull() && !o["days"].is<int>()) || (!o["enabled"].isNull() && !o["enabled"].is<bool>())) return -1;
    const int h = o["hour"], m = o["minute"], d = o["days"] | 0;
    if (h < 0 || h > 23 || m < 0 || m > 59 || d < 0 || d > 127) return -1;
    out[n++] = {(uint8_t)h, (uint8_t)m, (uint8_t)d, o["enabled"] | true};
  }
  return n;
}

// 3x5 digits for the 16x8 dot clock, one 3-bit number per row (4 = left dot); index 10 = dash.
// Same glyphs as the enclosure render in enclosure/sunrise_clock.scad.
constexpr uint8_t CLOCK_FONT[11][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
                                       {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
                                       {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}, {0, 0, 7, 0, 0}};

// Draws H:MM (12 h, no leading zero) or HH:MM (24 h) into rows[8]: bit c = column c, 0 = left.
// Digits sit in columns 0-2, 4-6, 8-10, 12-14 with the colon in column 7. hour < 0 shows "--:--".
inline void renderClock(int hour, int minute, bool h24, uint16_t rows[8]) {
  int d[4] = {10, 10, 10, 10};
  if (hour >= 0) {
    const int h = h24 ? hour : (hour % 12 == 0 ? 12 : hour % 12);
    d[0] = (h24 || h >= 10) ? h / 10 : -1;
    d[1] = h % 10;
    d[2] = minute / 10;
    d[3] = minute % 10;
  }
  for (int r = 0; r < 8; r++) rows[r] = 0;
  for (int k = 0; k < 4; k++) {
    if (d[k] < 0) continue;
    for (int r = 0; r < 5; r++)
      for (int b = 0; b < 3; b++)
        if (CLOCK_FONT[d[k]][r] & (4 >> b)) rows[r + 1] |= (uint16_t)(1u << (k * 4 + b));
  }
  rows[2] |= 1u << 7;
  rows[4] |= 1u << 7;
}

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
inline int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

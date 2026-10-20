#pragma once
// Time and persistence: the DS3231 is the time authority (UTC; SNTP writes into it when online),
// alarms + settings live as JSON in NVS, and AlarmMachine (Logic.h) decides when to wake.
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "Logic.h"

struct Settings {
  char tz[64] = "UTC0";  // POSIX TZ, e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
  char ssid[33] = "", pass[65] = "", adminPass[33] = "";
  uint8_t sunriseMax = 100;  // % of the LED current budget
  uint8_t nightPct = 30;
  uint16_t nightKelvin = 2700;
  bool autoDim = false;
  uint8_t alarmVolume = 60, soundVolume = 40;
  uint8_t clockLevel = 4;  // dot clock brightness 1-16, 0 = off
  bool clock24 = false;
  bool sleepTracking = true;
  uint16_t sleepRangeCm = 200;  // radar only counts someone this close: the bed, not the room
};

class AlarmScheduler {
 public:
  Settings settings;
  Alarm alarms[MAX_ALARMS];
  int count = 0;

  bool begin();  // NVS + RTC -> system clock; false if no DS3231 answers
  void tick();   // call every loop
  Phase phase() const { return m_.phase; }
  float progress() const { return m_.progress; }
  bool timedOut() const { return m_.timedOut; }
  void snooze() { m_.snooze(time(nullptr)); }
  void dismiss() { m_.dismiss(alarms, count); }
  int toggleNext();                      // alarm button: 1 = enabled, 0 = disabled, -1 = no alarms
  int next(int32_t& secondsAway) const;  // next enabled alarm or -1

  bool setAlarms(JsonArrayConst in);                         // validated replace + save
  bool applySettings(JsonObjectConst in, bool* wifiChanged);  // validated partial update + save
  void settingsJson(JsonObject out, bool secrets) const;
  void saveSettingsSoon() { settingsDirtyAt_ = millis() | 1; }  // coalesce encoder turns

  void setTime(time_t utc);  // web UI / SNTP: system clock + RTC
  bool timeValid() const { return time(nullptr) > 1704067200; }  // after 2024-01-01
  bool rtcOk() const { return rtcOk_; }

 private:
  bool syncFromRtc();
  bool writeRtc(time_t utc);
  void persist();
  void saveSettings();
  static bool parseSettings(JsonObjectConst in, Settings& s);
  static int32_t localSec(time_t t, int& wday);

  AlarmMachine m_;
  Preferences prefs_;
  bool rtcOk_ = false;
  int lastToggled_ = -1;
  time_t lastSec_ = 0;
  uint32_t lastRtcSync_ = 0, settingsDirtyAt_ = 0;
};

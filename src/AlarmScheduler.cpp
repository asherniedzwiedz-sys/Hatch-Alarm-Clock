#include "AlarmScheduler.h"

#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include <algorithm>
#include <atomic>

static constexpr int RTC_ADDR = 0x68;  // DS3231
static std::atomic<bool> s_ntpSynced{false};

static uint8_t bcd(int v) { return (uint8_t)((v / 10) << 4 | v % 10); }
static int unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

bool AlarmScheduler::begin() {
  prefs_.begin("sunrise", false);
  JsonDocument doc;
  if (prefs_.isKey("cfg") && !deserializeJson(doc, prefs_.getString("cfg"))) parseSettings(doc.as<JsonObjectConst>(), settings);
  if (prefs_.isKey("alarms") && !deserializeJson(doc, prefs_.getString("alarms")))
    count = std::max(0, alarmsFromJson(doc.as<JsonArrayConst>(), alarms, MAX_ALARMS));
  m_.handledUpTo = (int64_t)prefs_.getULong64("handled", 0);

  setenv("TZ", settings.tz, 1);
  tzset();
  sntp_set_time_sync_notification_cb([](struct timeval*) { s_ntpSynced = true; });  // runs in the lwIP task
  return syncFromRtc();
}

void AlarmScheduler::tick() {
  const uint32_t ms = millis();
  if (s_ntpSynced.exchange(false)) writeRtc(time(nullptr));
  if (ms - lastRtcSync_ >= 10 * 60 * 1000) syncFromRtc();  // keep the ESP32 clock disciplined to the RTC
  if (settingsDirtyAt_ && ms - settingsDirtyAt_ >= 3000) saveSettings();

  const time_t now = time(nullptr);
  if (now == lastSec_ || !timeValid()) return;  // alarms run off a valid clock only, once per second
  lastSec_ = now;
  int wday;
  const int32_t sec = localSec(now, wday);
  m_.tick(now, wday, sec, alarms, count);
  if (m_.dirty) persist();
}

int32_t AlarmScheduler::localSec(time_t t, int& wday) {
  struct tm lt;
  localtime_r(&t, &lt);
  wday = lt.tm_wday;
  return lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
}

int AlarmScheduler::next(int32_t& secondsAway) const {
  int wday;
  const int32_t sec = localSec(time(nullptr), wday);
  secondsAway = 0;
  return findAlarm(alarms, count, wday, sec, 0, WEEK_SEC, true, secondsAway);
}

int AlarmScheduler::toggleNext() {
  const time_t now = time(nullptr);
  int wday;
  const int32_t sec = localSec(now, wday);
  int32_t off = 0;
  // A second press re-enables the alarm the first press switched off, instead of moving on.
  int i = lastToggled_ >= 0 && lastToggled_ < count && !alarms[lastToggled_].enabled
              ? lastToggled_
              : findAlarm(alarms, count, wday, sec, 0, WEEK_SEC, true, off);
  if (i < 0) i = findAlarm(alarms, count, wday, sec, 0, WEEK_SEC, false, off);
  if (i < 0) return -1;
  alarms[i].enabled = !alarms[i].enabled;
  lastToggled_ = alarms[i].enabled ? -1 : i;
  m_.edited(now);
  persist();
  return alarms[i].enabled ? 1 : 0;
}

bool AlarmScheduler::setAlarms(JsonArrayConst in) {
  Alarm tmp[MAX_ALARMS];
  const int n = alarmsFromJson(in, tmp, MAX_ALARMS);
  if (n < 0) return false;
  memcpy(alarms, tmp, sizeof(Alarm) * n);
  count = n;
  lastToggled_ = -1;
  m_.edited(time(nullptr));  // a new 07:00 alarm saved at 07:05 must not ring right away
  persist();
  return true;
}

void AlarmScheduler::persist() {
  JsonDocument doc;
  alarmsToJson(alarms, count, doc.to<JsonArray>());
  String s;
  serializeJson(doc, s);
  prefs_.putString("alarms", s);
  prefs_.putULong64("handled", (uint64_t)m_.handledUpTo);
  m_.dirty = false;
}

// ---- Settings ----

template <typename T>
static bool takeInt(JsonObjectConst in, const char* key, int lo, int hi, T& out) {
  JsonVariantConst v = in[key];
  if (v.isNull()) return true;
  if (!v.is<int>() || v.as<int>() < lo || v.as<int>() > hi) return false;
  out = (T)v.as<int>();
  return true;
}

static bool takeStr(JsonObjectConst in, const char* key, char* out, size_t cap, size_t minLen, bool (*ok)(char)) {
  JsonVariantConst v = in[key];
  if (v.isNull()) return true;
  if (!v.is<const char*>()) return false;
  const char* s = v.as<const char*>();
  const size_t n = strlen(s);
  if (n >= cap || (n && n < minLen)) return false;
  for (size_t i = 0; i < n; i++)
    if (!ok(s[i])) return false;
  memcpy(out, s, n + 1);
  return true;
}

static bool takeBool(JsonObjectConst in, const char* key, bool& out) {
  JsonVariantConst v = in[key];
  if (v.isNull()) return true;
  if (!v.is<bool>()) return false;
  out = v.as<bool>();
  return true;
}

static bool printable(char c) { return c >= 0x20 && c < 0x7F; }
static bool tzChar(char c) { return isalnum((unsigned char)c) || strchr("+-,.:/<>", c); }

bool AlarmScheduler::parseSettings(JsonObjectConst in, Settings& s) {
  if (in.isNull()) return false;
  return takeBool(in, "autoDim", s.autoDim) && takeBool(in, "clock24", s.clock24) &&
         takeBool(in, "sleepTracking", s.sleepTracking) && takeInt(in, "sleepRangeCm", 30, 600, s.sleepRangeCm) &&
         takeInt(in, "clockLevel", 0, 16, s.clockLevel) && takeStr(in, "tz", s.tz, sizeof s.tz, 1, tzChar) &&
         takeStr(in, "ssid", s.ssid, sizeof s.ssid, 1, printable) &&
         takeStr(in, "pass", s.pass, sizeof s.pass, 8, printable) &&  // WPA2: empty (open) or 8-64
         takeStr(in, "adminPass", s.adminPass, sizeof s.adminPass, 1, printable) &&
         takeInt(in, "sunriseMax", 1, 100, s.sunriseMax) && takeInt(in, "nightPct", 1, 100, s.nightPct) &&
         takeInt(in, "nightKelvin", 1700, 6500, s.nightKelvin) && takeInt(in, "alarmVolume", 0, 100, s.alarmVolume) &&
         takeInt(in, "soundVolume", 0, 100, s.soundVolume);
}

bool AlarmScheduler::applySettings(JsonObjectConst in, bool* wifiChanged) {
  Settings s = settings;
  if (!parseSettings(in, s)) return false;
  if (wifiChanged) *wifiChanged = strcmp(s.ssid, settings.ssid) || strcmp(s.pass, settings.pass);
  if (strcmp(s.tz, settings.tz)) {
    setenv("TZ", s.tz, 1);
    tzset();
  }
  settings = s;
  saveSettings();
  return true;
}

void AlarmScheduler::settingsJson(JsonObject out, bool secrets) const {
  const Settings& s = settings;
  out["tz"] = s.tz;
  out["ssid"] = s.ssid;
  out["sunriseMax"] = s.sunriseMax;
  out["nightPct"] = s.nightPct;
  out["nightKelvin"] = s.nightKelvin;
  out["autoDim"] = s.autoDim;
  out["alarmVolume"] = s.alarmVolume;
  out["soundVolume"] = s.soundVolume;
  out["clockLevel"] = s.clockLevel;
  out["clock24"] = s.clock24;
  out["sleepTracking"] = s.sleepTracking;
  out["sleepRangeCm"] = s.sleepRangeCm;
  if (secrets) {
    out["pass"] = s.pass;
    out["adminPass"] = s.adminPass;
  }
}

void AlarmScheduler::saveSettings() {
  JsonDocument doc;
  settingsJson(doc.to<JsonObject>(), true);
  String s;
  serializeJson(doc, s);
  prefs_.putString("cfg", s);
  settingsDirtyAt_ = 0;
}

// ---- DS3231 (UTC, 24 h) ----

void AlarmScheduler::setTime(time_t utc) {
  timeval tv;
  tv.tv_sec = utc;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  writeRtc(utc);
}

bool AlarmScheduler::syncFromRtc() {
  lastRtcSync_ = millis();
  uint8_t r[16];  // registers 0x00-0x0F
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);
  rtcOk_ = Wire.endTransmission(false) == 0 && Wire.requestFrom(RTC_ADDR, 16) == 16;
  if (!rtcOk_) return false;
  for (uint8_t& b : r) b = (uint8_t)Wire.read();
  if (r[15] & 0x80) return true;  // OSF: oscillator stopped (dead coin cell) - time is garbage, keep ours
  const int64_t t = (int64_t)daysFromCivil(2000 + unbcd(r[6]), unbcd(r[5] & 0x1F), unbcd(r[4] & 0x3F)) * DAY_SEC +
                    unbcd(r[2] & 0x3F) * 3600 + unbcd(r[1] & 0x7F) * 60 + unbcd(r[0] & 0x7F);
  timeval tv;
  tv.tv_sec = (time_t)t;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  return true;
}

bool AlarmScheduler::writeRtc(time_t utc) {
  struct tm g;
  gmtime_r(&utc, &g);
  const uint8_t regs[8] = {0x00,  // start register, then sec min hour weekday date month year
                           bcd(g.tm_sec),  bcd(g.tm_min),     bcd(g.tm_hour),       bcd(g.tm_wday + 1),
                           bcd(g.tm_mday), bcd(g.tm_mon + 1), bcd(g.tm_year - 100)};
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(regs, sizeof regs);
  bool ok = Wire.endTransmission() == 0;
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x0F);
  Wire.write((uint8_t)0x00);  // clear OSF: the time is valid again
  ok = Wire.endTransmission() == 0 && ok;
  rtcOk_ = ok;
  return ok;
}

// DIY sunrise alarm clock. loop() owns the policy: alarm phase -> light + sound, buttons -> actions.
#include <Arduino.h>
#include <Wire.h>

#include <mutex>

#include "AlarmScheduler.h"
#include "AudioEngine.h"
#include "ClockDisplay.h"
#include "Config.h"
#include "InputHandler.h"
#include "LightEngine.h"
#include "SleepMonitor.h"
#include "WebUI.h"

static std::mutex gLock;  // loop() and the async web server task share everything below
static AlarmScheduler sched;
static LightEngine light;
static AudioEngine audio;
static InputHandler input;
static ClockDisplay clockDisplay;
static SleepMonitor sleepMon;
static WebUI web;

static Phase lastPhase = Phase::Idle;
static bool volumeMode = false;  // encoder push toggles brightness <-> volume
static float lux = -1;           // smoothed BH1750 reading, -1 = no sensor
static float dimFactor = 1;      // auto-dim, shared by night light and clock
static uint32_t lastLux = 0;

static constexpr int BH1750_ADDR = 0x23;
static constexpr RGB kGreen{0, 60, 0}, kRed{60, 0, 0}, kBlue{0, 0, 70}, kAmber{70, 35, 0};

// ~10 % steps above 20 so 1..100 takes ~35 clicks and low levels stay fine-grained.
static int stepPct(int v, int detents, int lo) {
  for (int i = 0; i < abs(detents); i++) v = constrain(v + (detents > 0 ? 1 : -1) * (v >= 20 ? v / 10 : 1), lo, 100);
  return v;
}

static float sunriseMax() { return sched.settings.sunriseMax / 100.0f; }

static void onPhase(Phase from, Phase to) {
  const Settings& s = sched.settings;
  switch (to) {
    case Phase::Sunrise:
      light.setNight(false, s.nightPct, s.nightKelvin);  // the sunrise layer takes over
      break;
    case Phase::Ringing:  // T+0 (also after a snooze, or on boot inside the ring window)
      light.setNight(false, s.nightPct, s.nightKelvin);
      light.setAlarm(sunriseMax(), SUNRISE_K_END, 1000);
      audio.setVolume(s.alarmVolume);
      audio.chime(CHIME_FADE_MS);  // replaces any sleep sound and cancels its timer
      break;
    case Phase::Snoozed:
      light.setAlarm(sunriseMax() * SNOOZE_LEVEL, SUNRISE_K_END, 2000);
      audio.stop(1000);
      break;
    case Phase::Idle: {
      const uint32_t fade = from == Phase::Ringing && sched.timedOut() ? AUTO_OFF_FADE_MS : DISMISS_FADE_MS;
      light.clearAlarm(fade);
      if (from == Phase::Ringing) audio.stop(fade);
      break;
    }
  }
}

static void handleInput(const InputEvents& e) {
  Settings& s = sched.settings;
  const Phase ph = sched.phase();
  if (ph == Phase::Ringing || ph == Phase::Snoozed) {  // any button ends or pauses the alarm
    if (e.lightPress) sched.snooze();
    if (e.alarmPress || e.encPress) sched.dismiss();
  } else if (ph == Phase::Sunrise) {
    if (e.alarmPress) sched.dismiss();  // awake early; other buttons are ignored so a bump can't cancel it
  } else {
    if (e.lightPress) light.setNight(!light.nightOn(), s.nightPct, s.nightKelvin);
    if (e.alarmPress) {
      const int r = sched.toggleNext();
      light.flash(r == 1 ? kGreen : kRed, r < 0 ? 3 : r == 1 ? 2 : 1);  // 2 green = on, 1 red = off, 3 red = none
    }
    if (e.encPress) {
      volumeMode = !volumeMode;
      light.flash(volumeMode ? kBlue : kAmber, 1);
    }
  }
  if (!e.turn) return;
  if (volumeMode) {
    const int v = stepPct(audio.volume(), e.turn, 0);
    audio.setVolume((uint8_t)v);
    (audio.source() == AudioEngine::Source::Chime ? s.alarmVolume : s.soundVolume) = (uint8_t)v;
    sched.saveSettingsSoon();
  } else if (ph == Phase::Idle) {
    s.nightPct = (uint8_t)stepPct(s.nightPct, e.turn, 1);
    light.setNight(true, s.nightPct, s.nightKelvin, 150);
    sched.saveSettingsSoon();
  }
}

static void updateLux() {
  if (lux < 0 || millis() - lastLux < 1000) return;
  lastLux = millis();
  if (Wire.requestFrom(BH1750_ADDR, 2) != 2) return;
  const int hi = Wire.read(), lo = Wire.read();  // two statements: operand order matters
  lux += ((hi << 8 | lo) / 1.2f - lux) * 0.2f;  // ~5 s smoothing
  // Dimmer in a dark room, full setting once the room is lit (>= 75 lx).
  dimFactor = sched.settings.autoDim ? constrain(0.25f + lux / 100.0f, 0.25f, 1.0f) : 1.0f;
  light.setDimFactor(dimFactor);
}

static void updateClock() {
  static uint32_t last = 0;
  if (!clockDisplay.present() || millis() - last < 250) return;
  last = millis();
  const time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  uint16_t rows[8];
  renderClock(sched.timeValid() ? lt.tm_hour : -1, lt.tm_min, sched.settings.clock24, rows);
  const int level = sched.settings.clockLevel;
  clockDisplay.show(rows, (uint8_t)(level ? max(1, (int)(level * dimFactor + 0.5f)) : 0));  // never dims to off
}

void setup() {
  Serial.begin(115200);
  const bool brownout = esp_reset_reason() == ESP_RST_BROWNOUT;  // the supply sagged under LED + amp + Wi-Fi load
  if (brownout) Serial.println("Last reset was a brownout: lower Sunrise max or volume, or use a 5 V 3 A supply");
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  light.begin();
  input.begin();
  if (!sched.begin()) Serial.println("DS3231 not responding: alarms need the RTC or Wi-Fi time");
#ifdef WOKWI
  if (!sched.count && sched.timeValid()) {  // simulator: a one-time alarm 3 min out, so sunrise starts now and rings soon
    const time_t t = time(nullptr) + 180;
    tm lt;
    localtime_r(&t, &lt);
    JsonDocument doc;
    JsonObject a = doc.to<JsonArray>().add<JsonObject>();
    a["hour"] = lt.tm_hour;
    a["minute"] = lt.tm_min;
    a["enabled"] = true;
    if (sched.setAlarms(doc.as<JsonArrayConst>())) Serial.printf("Test alarm set for %02d:%02d\n", lt.tm_hour, lt.tm_min);
  }
#endif
  if (!audio.begin()) Serial.println("No SD card: built-in sleep sounds only");
  audio.setVolume(sched.settings.soundVolume);

  Wire.beginTransmission(BH1750_ADDR);
  Wire.write((uint8_t)0x10);  // continuous high-resolution mode
  if (Wire.endTransmission() == 0) lux = 50;
  if (clockDisplay.begin()) Serial.printf("Dot clock found at 0x%02X\n", clockDisplay.address());
  else Serial.println("No dot clock answered at 0x70-0x77");
  sleepMon.begin();

  light.setNight(false, sched.settings.nightPct, sched.settings.nightKelvin);
  web.begin(sched, light, audio, sleepMon, gLock, lux, clockDisplay.present());
  if (!sched.timeValid()) light.flash(kRed, 5);  // set the time from the web UI
  else if (brownout) light.flash(kAmber, 3);     // visible without a serial monitor
  Serial.printf("Sunrise clock %s ready\n", FW_VERSION);
}

void loop() {
  {
    std::lock_guard<std::mutex> guard(gLock);
    sched.tick();
    const Phase ph = sched.phase();
    if (ph != lastPhase) {
      onPhase(lastPhase, ph);
      lastPhase = ph;
    }
    if (ph == Phase::Sunrise) {  // progress moves each second; the 1 s fade smooths it at 10 Hz
      const float p = sched.progress();
      light.setAlarm(sunriseLevel(p) * sunriseMax(), sunriseKelvin(p), 1000);
    }
    handleInput(input.poll());
    updateLux();
    updateClock();
    sleepMon.loop(sched.timeValid(), sched.settings.sleepTracking, sched.settings.sleepRangeCm);
    audio.update();
    light.update();
    web.loop();
  }
  vTaskDelay(1);  // 1 ms yield so the web task can take the lock and the idle task runs
}

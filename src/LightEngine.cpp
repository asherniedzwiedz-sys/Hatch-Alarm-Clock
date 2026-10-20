#include "LightEngine.h"

void LightEngine::begin() {
  strip_.Begin();
  strip_.ClearTo(RgbColor(0));
  strip_.Show();
}

void LightEngine::setNight(bool on, uint8_t pct, uint16_t kelvin, uint32_t fadeMs) {
  nightOn_ = on;
  nightPct_ = pct;
  nightK_ = kelvin;
  retarget(fadeMs);
}

void LightEngine::setDimFactor(float f) {
  if (fabsf(f - dim_) < 0.02f) return;
  dim_ = f;
  retarget(2000);
}

void LightEngine::setAlarm(float level, float kelvin, uint32_t fadeMs) {
  alarm_ = true;
  alarmLevel_ = level;
  alarmK_ = kelvin;
  retarget(fadeMs);
}

void LightEngine::clearAlarm(uint32_t fadeMs) {
  alarm_ = false;
  retarget(fadeMs);
}

void LightEngine::flash(RGB color, uint8_t count) {
  flash_ = color;
  flashLeft_ = (uint8_t)(count * 2);
  flashNext_ = millis();
}

void LightEngine::retarget(uint32_t fadeMs) {
  const float level = alarm_ ? alarmLevel_ : nightOn_ ? nightPct_ / 100.0f * dim_ : 0.0f;
  const float kelvin = alarm_ ? alarmK_ : nightOn_ ? nightK_ : cur_.kelvin;  // fade out without a colour shift
  if (level == to_.level && kelvin == to_.kelvin) return;  // keep a running fade
  from_ = cur_;
  to_ = {level < 0 ? 0 : level > 1 ? 1 : level, kelvin};
  fadeStart_ = millis();
  fadeMs_ = fadeMs;
}

void LightEngine::update() {
  const uint32_t now = millis();
  if (now - lastFrame_ < 100) return;  // 10 Hz
  lastFrame_ = now;

  const uint32_t t = now - fadeStart_;
  const float k = fadeMs_ && t < fadeMs_ ? (float)t / (float)fadeMs_ : 1.0f;
  cur_.level = from_.level + (to_.level - from_.level) * k;
  cur_.kelvin = from_.kelvin + (to_.kelvin - from_.kelvin) * k;

  if (flashLeft_ && (int32_t)(now - flashNext_) >= 0) {
    flashLeft_--;
    flashNext_ = now + 250;
  }
  const RGB base = kelvinToRGB(cur_.kelvin);
  float r = base.r * cur_.level, g = base.g * cur_.level, b = base.b * cur_.level;
  if (flashLeft_ & 1) {
    r = flash_.r;
    g = flash_.g;
    b = flash_.b;
  }
  const float ma = (r + g + b) / 255.0f * LED_MA_PER_CHANNEL * LED_COUNT;
  if (ma > LED_MAX_MA) {
    const float s = LED_MAX_MA / ma;
    r *= s;
    g *= s;
    b *= s;
  }
  const RgbColor c((uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f));
  if (c == shown_) return;
  strip_.ClearTo(c);
  strip_.Show();
  shown_ = c;
}

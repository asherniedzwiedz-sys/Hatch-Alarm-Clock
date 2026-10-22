#pragma once
// WS2812B panel: one colour for all LEDs, two layers (alarm overrides night light), linear fades,
// a supply current cap and short feedback flashes. Renders at 10 Hz.
#include <Arduino.h>
#ifndef WOKWI
#include <NeoPixelBus.h>
#endif

#include "Config.h"
#include "Logic.h"

class LightEngine {
 public:
  void begin();
  void update();  // call every loop

  void setNight(bool on, uint8_t pct, uint16_t kelvin, uint32_t fadeMs = 400);
  void setDimFactor(float f);  // BH1750 auto-dim, 0..1
  void setAlarm(float level, float kelvin, uint32_t fadeMs);
  void clearAlarm(uint32_t fadeMs);
  void flash(RGB color, uint8_t count);  // UI feedback blinks over the current output
  bool nightOn() const { return nightOn_; }

 private:
  struct State {
    float level, kelvin;  // level = PWM duty 0..1
  };
  void retarget(uint32_t fadeMs);
  void show(uint8_t r, uint8_t g, uint8_t b);

#ifndef WOKWI
  NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip_{LED_COUNT, PIN_LED_DATA};
#endif
  State from_{0, 2700}, to_{0, 2700}, cur_{0, 2700};
  uint32_t fadeStart_ = 0, fadeMs_ = 0, lastFrame_ = 0;
  bool nightOn_ = false, alarm_ = false;
  uint8_t nightPct_ = 30;
  float nightK_ = 2700, dim_ = 1, alarmLevel_ = 0, alarmK_ = SUNRISE_K_END;
  RGB flash_{0, 0, 0};
  uint8_t flashLeft_ = 0;  // odd = flash colour showing
  uint32_t flashNext_ = 0;
  uint32_t shown_ = 0;  // last frame as 0xRRGGBB
};

#pragma once
// Interrupt-driven quadrature encoder + debounced active-low buttons. poll() never blocks.
#include <Arduino.h>

#include "Config.h"

struct InputEvents {
  int8_t turn = 0;  // encoder detents since last poll, + = clockwise
  bool encPress = false, alarmPress = false, lightPress = false;
};

class InputHandler {
 public:
  void begin();
  InputEvents poll();  // call every loop

 private:
  struct Button {
    explicit Button(uint8_t p) : pin(p) {}
    bool pressed(uint32_t now);
    uint8_t pin;
    bool raw = true, stable = true;  // HIGH = released (pull-up)
    uint32_t since = 0;
  };
  Button enc_{PIN_ENC_SW}, alarm_{PIN_BTN_ALARM}, light_{PIN_BTN_LIGHT};
  int32_t consumed_ = 0;
};

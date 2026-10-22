#include "InputHandler.h"

#include <initializer_list>
#include <soc/gpio_reg.h>

static_assert(PIN_ENC_A >= 32 && PIN_ENC_B >= 32, "encoder ISR reads GPIO_IN1_REG (GPIO32-39)");

static volatile int32_t s_quarters = 0;  // quadrature edges, signed
static volatile uint8_t s_state = 0;     // previous AB << 2 | current AB

static void IRAM_ATTR encoderIsr() {
  static const DRAM_ATTR int8_t kStep[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  const uint32_t in = REG_READ(GPIO_IN1_REG);
  const uint8_t ab = (uint8_t)(((in >> (PIN_ENC_A - 32)) & 1) << 1 | ((in >> (PIN_ENC_B - 32)) & 1));
  s_state = (uint8_t)(((s_state << 2) | ab) & 0x0F);
  s_quarters = s_quarters + kStep[s_state];  // invalid (bounced) transitions add 0
}

void InputHandler::begin() {
  pinMode(PIN_ENC_A, INPUT);
  pinMode(PIN_ENC_B, INPUT);
  for (Button* b : {&enc_, &alarm_, &light_}) {
    pinMode(b->pin, INPUT_PULLUP);
    b->raw = b->stable = digitalRead(b->pin);  // held at power-up is not a press: a stuck button would dismiss every resumed alarm
  }
  s_state = (uint8_t)(digitalRead(PIN_ENC_A) << 1 | digitalRead(PIN_ENC_B));
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderIsr, CHANGE);
}

bool InputHandler::Button::pressed(uint32_t now) {
  const bool level = digitalRead(pin);
  if (level != raw) {
    raw = level;
    since = now;
  } else if (level != stable && now - since >= 25) {  // 25 ms debounce
    stable = level;
    return !stable;  // falling edge = press
  }
  return false;
}

InputEvents InputHandler::poll() {
  InputEvents e;
  const int32_t detents = (s_quarters - consumed_) / ENC_STEPS_PER_DETENT;
  consumed_ += detents * ENC_STEPS_PER_DETENT;
  e.turn = (int8_t)(detents * ENC_DIRECTION);
  const uint32_t now = millis();
  e.encPress = enc_.pressed(now);
  e.alarmPress = alarm_.pressed(now);
  e.lightPress = light_.pressed(now);
  return e;
}

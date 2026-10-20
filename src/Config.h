#pragma once
// Wiring and tunables. Pin numbers match the README wiring table.
#include <stdint.h>

// ---- Pins ----
constexpr int PIN_LED_DATA = 16;  // -> 74AHCT125 -> 470R -> WS2812B DIN
constexpr int PIN_I2S_BCLK = 26, PIN_I2S_WS = 25, PIN_I2S_DOUT = 32;  // MAX98357A
constexpr int PIN_SDA = 21, PIN_SCL = 22;                             // DS3231 + optional BH1750
constexpr int PIN_SD_CS = 5, PIN_SD_SCK = 18, PIN_SD_MISO = 19, PIN_SD_MOSI = 23;  // VSPI
constexpr int PIN_ENC_A = 34, PIN_ENC_B = 35;  // input-only pins: need external 10k pull-ups
constexpr int PIN_ENC_SW = 33, PIN_BTN_ALARM = 27, PIN_BTN_LIGHT = 14;  // to GND, internal pull-ups
constexpr int PIN_RADAR_RX = 4, PIN_RADAR_TX = 13;  // HLK-LD2410C on UART2 (its TX -> GPIO4)

// ---- Hardware calibration ----
constexpr int LED_COUNT = 40;
constexpr float LED_MA_PER_CHANNEL = 20.0f;  // WS2812B current per colour channel at 255
constexpr float LED_MAX_MA = 1800.0f;        // LED share of the 5 V 3 A supply; ESP32 + amp need the rest
constexpr int ENC_STEPS_PER_DETENT = 4;      // quadrature edges per click (2 on some encoders)
constexpr int ENC_DIRECTION = 1;             // -1 if clockwise turns the wrong way

constexpr int CLOCK_ADDR = 0x70;            // optional HT16K33 16x8 dot clock on the I2C bus
constexpr bool CLOCK_FLIP_X = false, CLOCK_FLIP_Y = false;  // if the digits come out mirrored/upside down

// ---- Behaviour ----
constexpr uint32_t SUNRISE_SEC = 30 * 60;          // light ramp before the alarm time
constexpr uint32_t RING_SEC = 20 * 60;             // auto fade-out this long after ringing starts
constexpr uint32_t SNOOZE_SEC = 9 * 60;
constexpr float SNOOZE_LEVEL = 0.5f;               // light during snooze, fraction of sunrise max
constexpr uint32_t CHIME_FADE_MS = 2 * 60 * 1000;  // chime fade-in
constexpr uint32_t AUTO_OFF_FADE_MS = 60 * 1000;   // fade-out when nobody dismisses
constexpr uint32_t DISMISS_FADE_MS = 3000;
constexpr uint32_t SLEEP_FADE_MS = 10 * 60 * 1000;  // sleep timer fades out over its last 10 min
constexpr uint16_t SUNRISE_K_START = 1700, SUNRISE_K_END = 3200;
constexpr int MAX_ALARMS = 10;

// ---- Sleep tracking (HLK-LD2410C radar) ----
constexpr uint8_t SLEEP_MOVE_THRESHOLD = 15;  // minute mean move energy (0-100) above this = restless; calibrate in the UI
constexpr int SLEEP_OPEN_MIN = 10;            // minutes in bed before a night starts
constexpr int SLEEP_CLOSE_AWAY_MIN = 15;      // minutes out of bed that end it
constexpr int SLEEP_MIN_NIGHT = 60;           // shorter sessions are dropped
constexpr int SLEEP_OPEN_FROM_H = 18, SLEEP_OPEN_UNTIL_H = 4;  // a night can only start in this local-time window
constexpr int SLEEP_NIGHTS = 7;               // history kept in NVS

// ---- Network ----
constexpr const char* AP_SSID = "SunriseClock-Setup";
constexpr const char* AP_PASS = "sunrise-setup";  // public default: change before flashing (WPA2, >= 8 chars)
constexpr const char* HOSTNAME = "sunrise";       // http://sunrise.local
constexpr const char* FW_VERSION = "1.0.0";

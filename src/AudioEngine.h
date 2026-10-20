#pragma once
// I2S output on its own FreeRTOS task: looped 16-bit/44.1 kHz WAVs from SD, or a synthesized chime
// (so the alarm still sounds without a card). Public methods only queue commands; they never block.
#include <Arduino.h>
#include <FS.h>

#include <atomic>
#include <vector>

class AudioEngine {
 public:
  enum class Source : uint8_t { Off, Sound, Chime };

  bool begin();                   // I2S + SD + task; returns whether the SD card mounted
  bool play(const String& name);  // loop /sounds/<name>; false unless it is one of sounds()
  void chime(uint32_t fadeInMs);
  void stop(uint32_t fadeMs);
  void setVolume(uint8_t v) { volume_ = v > 100 ? 100 : v; }
  uint8_t volume() const { return volume_; }
  void setSleepTimer(uint16_t minutes);  // 0 = off; fades out over the timer's last SLEEP_FADE_MS
  uint32_t sleepTimerLeft() const;       // seconds
  void update();                         // call every loop
  Source source() const { return (Source)src_.load(); }
  const String& current() const { return current_; }
  const std::vector<String>& sounds() const { return sounds_; }
  bool sdOk() const { return sdOk_; }

 private:
  struct Cmd {
    Source src;
    uint32_t fadeMs;
    char name[40];
  };
  static void taskEntry(void* self);
  void task();
  void send(Source s, uint32_t fadeMs, const char* name = "");
  void handle(const Cmd& c);
  void close();
  bool openWav(const char* path);
  bool readWav(int16_t* buf, size_t frames);
  void renderChime(int16_t* buf, size_t frames);
  void applyGain(int16_t* buf, size_t frames);

  QueueHandle_t queue_ = nullptr;
  std::atomic<uint8_t> volume_{40}, src_{0};
  std::vector<String> sounds_;
  String current_;
  bool sdOk_ = false, timerFading_ = false;
  uint32_t timerEnd_ = 0;  // millis, 0 = no sleep timer

  // Audio-task state.
  File file_;
  uint32_t dataStart_ = 0, dataLen_ = 0, pos_ = 0;
  uint8_t channels_ = 1;
  Source playing_ = Source::Off;
  float fade_ = 0, fadeStep_ = 0, fadeTarget_ = 0, volGain_ = 0;
  bool stopping_ = false;
  struct Voice {
    uint32_t phase, inc;
    float env;
  } voices_[3] = {};
  uint32_t chimeT_ = 0;
};

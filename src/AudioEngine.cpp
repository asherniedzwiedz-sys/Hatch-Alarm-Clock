#include "AudioEngine.h"

#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

#include <algorithm>

#include "Config.h"

static constexpr uint32_t SR = 44100;
static constexpr float SAMPLES_PER_MS = SR / 1000.0f;
static constexpr size_t FRAMES = 256;  // per i2s_write, ~5.8 ms
static int16_t s_buf[FRAMES * 2];      // interleaved stereo
static int16_t s_sine[1024];

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

bool AudioEngine::begin() {
  for (int i = 0; i < 1024; i++) s_sine[i] = (int16_t)(32767.0f * sinf(i * 6.2831853f / 1024.0f));

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SR;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 8;  // 8 x 256 frames = 46 ms of slack for SD reads
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;  // silence, not a repeated buffer, on underrun or when idle
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK || i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK)
    log_e("I2S init failed");

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk_ = SD.begin(PIN_SD_CS, SPI, 20000000);
  if (sdOk_) {
    File dir = SD.open("/sounds");
    for (File f = dir ? dir.openNextFile() : File(); f; f = dir.openNextFile()) {
      String n = f.name();
      String lower = n;
      lower.toLowerCase();
      if (!f.isDirectory() && lower.endsWith(".wav") && n.length() < sizeof(Cmd::name)) sounds_.push_back(n);
    }
    std::sort(sounds_.begin(), sounds_.end());
  }

  queue_ = xQueueCreate(4, sizeof(Cmd));
  xTaskCreatePinnedToCore(taskEntry, "audio", 6144, this, 3, nullptr, 1);
  return sdOk_;
}

bool AudioEngine::play(const String& name) {
  if (std::find(sounds_.begin(), sounds_.end(), name) == sounds_.end()) return false;  // no path tricks
  current_ = name;
  timerFading_ = false;
  send(Source::Sound, 1500, name.c_str());
  return true;
}

void AudioEngine::chime(uint32_t fadeInMs) {
  timerEnd_ = 0;
  current_ = "";
  send(Source::Chime, fadeInMs);
}

void AudioEngine::stop(uint32_t fadeMs) { send(Source::Off, fadeMs); }

void AudioEngine::setSleepTimer(uint16_t minutes) {
  timerEnd_ = minutes ? millis() + minutes * 60000UL : 0;
  timerFading_ = false;
}

uint32_t AudioEngine::sleepTimerLeft() const {
  const int32_t left = timerEnd_ ? (int32_t)(timerEnd_ - millis()) : 0;
  return left > 0 ? (uint32_t)left / 1000 : 0;
}

void AudioEngine::update() {
  if (!timerEnd_) return;
  const int32_t left = (int32_t)(timerEnd_ - millis());
  if (!timerFading_ && left <= (int32_t)SLEEP_FADE_MS) {
    send(Source::Off, left > 0 ? (uint32_t)left : 0);
    timerFading_ = true;
  }
  if (left <= 0) timerEnd_ = 0;
}

void AudioEngine::send(Source s, uint32_t fadeMs, const char* name) {
  Cmd c{s, fadeMs, {}};
  snprintf(c.name, sizeof c.name, "%s", name);
  xQueueSend(queue_, &c, 0);
}

void AudioEngine::taskEntry(void* self) { static_cast<AudioEngine*>(self)->task(); }

void AudioEngine::task() {
  for (;;) {
    Cmd c;
    while (xQueueReceive(queue_, &c, playing_ == Source::Off ? portMAX_DELAY : 0) == pdTRUE) handle(c);
    if (playing_ == Source::Off) continue;
    if (playing_ == Source::Chime) {
      renderChime(s_buf, FRAMES);
    } else if (!readWav(s_buf, FRAMES)) {
      log_e("SD read failed");
      close();
      continue;
    }
    applyGain(s_buf, FRAMES);
    size_t written = 0;
    i2s_write(I2S_NUM_0, s_buf, sizeof s_buf, &written, portMAX_DELAY);  // blocks this task only
    if (stopping_ && fade_ <= 0.0f) close();
  }
}

void AudioEngine::handle(const Cmd& c) {
  if (c.src == Source::Off) {
    if (playing_ == Source::Off) return;
    stopping_ = true;
    fadeTarget_ = 0;
    fadeStep_ = c.fadeMs ? -fade_ / (c.fadeMs * SAMPLES_PER_MS) : -1.0f;
    return;
  }
  if (file_) file_.close();
  if (c.src == Source::Sound) {
    char path[64];
    snprintf(path, sizeof path, "/sounds/%s", c.name);
    if (!openWav(path)) {
      log_e("unsupported or missing WAV %s (need PCM 16-bit 44.1 kHz)", path);
      close();
      return;
    }
  } else {
    chimeT_ = 0;
    for (Voice& v : voices_) v.env = 0;
  }
  playing_ = c.src;
  src_ = (uint8_t)c.src;
  stopping_ = false;
  fade_ = 0;
  fadeTarget_ = 1;
  fadeStep_ = c.fadeMs ? 1.0f / (c.fadeMs * SAMPLES_PER_MS) : 1.0f;
}

void AudioEngine::close() {
  if (file_) file_.close();
  playing_ = Source::Off;
  src_ = (uint8_t)Source::Off;
  stopping_ = false;
  fade_ = 0;
  i2s_zero_dma_buffer(I2S_NUM_0);
}

bool AudioEngine::openWav(const char* path) {
  file_ = SD.open(path);
  uint8_t h[16];
  if (!file_ || file_.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
  bool fmtOk = false;
  for (;;) {  // walk chunks: need "fmt " then "data", skip anything else (LIST, fact, ...)
    if (file_.read(h, 8) != 8) return false;
    const uint32_t len = le32(h + 4), next = file_.position() + len + (len & 1);
    if (!memcmp(h, "fmt ", 4)) {
      if (len < 16 || file_.read(h, 16) != 16) return false;
      channels_ = (uint8_t)le16(h + 2);
      if (le16(h) != 1 || (channels_ != 1 && channels_ != 2) || le32(h + 4) != SR || le16(h + 14) != 16) return false;
      fmtOk = true;
    } else if (!memcmp(h, "data", 4)) {
      dataStart_ = file_.position();
      dataLen_ = len - len % (2u * channels_);
      pos_ = 0;
      return fmtOk && dataLen_ > 0;
    }
    if (!file_.seek(next)) return false;
  }
}

bool AudioEngine::readWav(int16_t* buf, size_t frames) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  const size_t want = frames * 2 * channels_;
  for (size_t got = 0; got < want;) {
    if (pos_ >= dataLen_) {  // seamless loop: jump straight back to the first sample
      file_.seek(dataStart_);
      pos_ = 0;
    }
    const int r = file_.read(dst + got, std::min<size_t>(want - got, dataLen_ - pos_));
    if (r <= 0) return false;
    got += r;
    pos_ += r;
  }
  if (channels_ == 1)  // mono -> L/R, back to front so nothing is overwritten before it is read
    for (size_t i = frames; i-- > 0;) buf[2 * i] = buf[2 * i + 1] = buf[i];
  return true;
}

void AudioEngine::renderChime(int16_t* buf, size_t frames) {
  // Soft C5-E5-G5 arpeggio every 4 s; each strike is a sine plus a quiet octave with a 0.8 s decay.
  static const float kNotes[3] = {523.25f, 659.25f, 783.99f};
  static const float kDecay = expf(-1.0f / (0.8f * SR));
  for (size_t i = 0; i < frames; i++) {
    const uint32_t t = chimeT_++ % (4 * SR);
    for (int v = 0; v < 3; v++) {
      if (t == v * SR * 6 / 10) voices_[v] = {0, (uint32_t)(kNotes[v] / SR * 4294967296.0f), 1.0f};
    }
    float s = 0;
    for (Voice& v : voices_) {
      s += v.env * (s_sine[v.phase >> 22] + 0.3f * s_sine[(v.phase * 2) >> 22]);
      v.phase += v.inc;
      v.env *= kDecay;
    }
    buf[2 * i] = buf[2 * i + 1] = (int16_t)(s * 0.35f);  // 3 overlapping voices stay below full scale
  }
}

void AudioEngine::applyGain(int16_t* buf, size_t frames) {
  const float v = volume_ / 100.0f, volStep = (v * v - volGain_) / frames;  // squared = perceptual-ish
  for (size_t i = 0; i < frames; i++) {
    fade_ += fadeStep_;
    if (fadeStep_ > 0 ? fade_ > fadeTarget_ : fade_ < fadeTarget_) fade_ = fadeTarget_;
    volGain_ += volStep;
    const float g = fade_ * fade_ * volGain_;  // squared fade sounds even, not front-loaded
    buf[2 * i] = (int16_t)(buf[2 * i] * g);
    buf[2 * i + 1] = (int16_t)(buf[2 * i + 1] * g);
  }
}

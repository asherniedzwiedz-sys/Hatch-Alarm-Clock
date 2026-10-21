#pragma once
// Built-in sleep sounds, synthesized live from filtered noise (44.1 kHz mono), so no SD card is
// needed. Endless, so there is no loop seam. Pure logic: tested natively in test/test_logic.
#include <math.h>
#include <stdint.h>
#include <string.h>

enum class SynthSound : uint8_t { None, White, Pink, Rain, Ocean, Fan };
constexpr const char* SYNTH_NAMES[] = {"white", "pink", "rain", "ocean", "fan"};

inline SynthSound synthByName(const char* name) {
  for (int i = 0; i < 5; i++)
    if (!strcmp(name, SYNTH_NAMES[i])) return (SynthSound)(i + 1);
  return SynthSound::None;
}

struct NoiseSynth {
  SynthSound kind = SynthSound::None;

  void start(SynthSound s) {
    *this = NoiseSynth();
    kind = s;
  }

  int16_t next() {  // one sample; levels are matched so switching sounds keeps the loudness
    const float w = white();
    if ((t_++ & 255) == 0) slow();  // envelopes move slowly: update every 5.8 ms
    float y = 0;
    switch (kind) {
      case SynthSound::None: break;
      case SynthSound::White: y = 0.17f * w; break;
      case SynthSound::Pink: y = 0.058f * pink(w); break;
      case SynthSound::Rain: {
        const float hiss = w - prev_;  // first difference: bright, like rain on a roof
        if (rand01() < 12.0f / 44100.0f) drop_ = 0.3f + 0.7f * rand01();  // ~12 drops a second
        drop_ *= 0.9955f;                                                   // 5 ms decay
        y = 0.069f * hiss + 0.031f * pink(w) + 0.44f * drop_ * hiss;
        break;
      }
      case SynthSound::Ocean: {
        brown_ = 0.997f * brown_ + 0.03f * w;
        lp_ += 0.08f * (brown_ - lp_);
        y = 0.9f * lp_ * env_ + 0.04f * (w - prev_) * env_ * env_ * env_;  // surf + spray on the crest
        break;
      }
      case SynthSound::Fan: {
        brown_ = 0.997f * brown_ + 0.03f * w;
        lp_ += 0.4f * (w - lp_);  // ~4 kHz air
        y = (0.44f * brown_ + 0.035f * lp_) * env_;
        break;
      }
    }
    prev_ = w;
    y = y > 0.98f ? 0.98f : y < -0.98f ? -0.98f : y;
    return (int16_t)(y * 32767.0f);
  }

 private:
  uint32_t rng_ = 0x9E3779B9u, t_ = 0;
  float prev_ = 0, b0_ = 0, b1_ = 0, b2_ = 0, brown_ = 0, lp_ = 0, drop_ = 0, env_ = 1;

  float white() {  // xorshift32, uniform -1..1
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(int32_t)rng_ * (1.0f / 2147483648.0f);
  }
  float rand01() { return (white() + 1.0f) * 0.5f; }
  float pink(float w) {  // Paul Kellet's economy filter, -3 dB/octave
    b0_ = 0.99765f * b0_ + w * 0.0990460f;
    b1_ = 0.96300f * b1_ + w * 0.2965164f;
    b2_ = 0.57000f * b2_ + w * 1.0526913f;
    return b0_ + b1_ + b2_ + w * 0.1848f;
  }
  void slow() {
    const float s = t_ / 44100.0f;
    if (kind == SynthSound::Ocean) {  // uneven ~9 s waves
      const float sw = 0.55f * sinf(6.2831853f * s / 9.0f) + 0.3f * sinf(6.2831853f * s / 13.7f + 1.0f) +
                       0.15f * sinf(6.2831853f * s / 5.3f + 2.0f);
      env_ = 0.12f + 0.88f * powf((sw + 1.0f) * 0.5f, 1.6f);
    } else if (kind == SynthSound::Fan) {
      env_ = 1.0f + 0.04f * sinf(6.2831853f * 7.0f * s);  // slight blade flutter
    }
  }
};

#!/usr/bin/env python3
"""Synthesize seamless sleep-sound loops for the SD card: 16-bit / 44.1 kHz mono WAV.

    python tools/gen_sounds.py [out_dir]      (default: sd_card/sounds) -> copy to /sounds on the card

Every sound is circular by construction: noise is shaped in the frequency domain and inverse-FFT'd
(so it is exactly periodic), and every envelope or tone has a whole number of cycles per loop.
The last sample therefore flows into the first with no click. No recorded audio is used.
"""
import sys
import wave
from pathlib import Path

import numpy as np

SR = 44100
SECONDS = 60
N = SR * SECONDS
rng = np.random.default_rng(20261018)
f_axis = np.maximum(np.fft.rfftfreq(N, 1 / SR), 1.0)  # avoid 1/0 at DC


def noise(shape):
    """Periodic noise with amplitude spectrum shape(f), unit RMS."""
    spec = (rng.normal(size=f_axis.size) + 1j * rng.normal(size=f_axis.size)) * shape(f_axis)
    spec[0] = 0
    x = np.fft.irfft(spec, n=N)
    return x / np.sqrt(np.mean(x**2))


def lowpass(fc, order=2):
    return lambda f: 1 / np.sqrt(1 + (f / fc) ** (2 * order))


def highpass(fc, order=2):
    return lambda f: 1 / np.sqrt(1 + (fc / f) ** (2 * order))


def lfo(cycles, phase=0.0):
    """Sine with a whole number of cycles per loop."""
    return np.sin(2 * np.pi * (cycles * np.arange(N) / N + phase))


def white():
    return noise(lambda f: np.ones_like(f))


def pink():
    return noise(lambda f: 1 / np.sqrt(f))  # -3 dB/octave


def rain():
    hiss = noise(lambda f: highpass(800)(f) * lowpass(9000)(f))
    patter = noise(lambda f: lowpass(1200)(f) / np.sqrt(f)) * 0.6
    drops = np.zeros(N)
    burst = noise(highpass(2500))  # reuse one noise bed for the droplets
    for _ in range(SECONDS * 12):  # ~12 audible drops per second
        start, length = rng.integers(N), rng.integers(SR // 400, SR // 60)
        idx = (start + np.arange(length)) % N  # wraps around the loop seam
        drops[idx] += burst[idx] * np.exp(-np.arange(length) / (length / 5)) * rng.uniform(0.3, 1.5)
    return hiss + patter + drops


def ocean():
    surf = noise(lambda f: lowpass(2500)(f) / f**0.75)
    spray = noise(highpass(1500))
    swell = 0.55 * lfo(6) + 0.3 * lfo(9, 0.31) + 0.15 * lfo(4, 0.77)  # ~8 s waves, uneven
    env = (swell - swell.min()) / np.ptp(swell)
    env = 0.12 + 0.88 * env**1.6
    return surf * env + 0.35 * spray * env**3


def fan():
    rumble = noise(lambda f: lowpass(600)(f) / f**0.5)
    air = noise(lambda f: lowpass(4000)(f)) * 0.35
    blade = round(28 * SECONDS) / SECONDS  # blade-pass hum, whole cycles per loop
    t = np.arange(N) / SR
    hum = sum(a * np.sin(2 * np.pi * k * blade * t) for k, a in ((1, 0.25), (2, 0.12), (3, 0.05)))
    return (rumble + air) * (1 + 0.04 * lfo(round(7 * SECONDS))) + hum


SOUNDS = {"white": white, "pink": pink, "rain": rain, "ocean": ocean, "fan": fan}


def to_pcm(x, rms_dbfs=-20.0):
    """Same loudness for every sound, so switching sounds doesn't jump in volume."""
    x = x * (10 ** (rms_dbfs / 20) / np.sqrt(np.mean(x**2)))
    peak = np.max(np.abs(x))
    if peak > 0.98:
        x *= 0.98 / peak
    return np.round(x * 32767).astype("<i2")


def write(path, pcm):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def check(path):
    """Self-check: format the firmware accepts, and no step at the loop seam."""
    with wave.open(str(path), "rb") as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()) == (1, 2, SR, N), path
        x = np.frombuffer(w.readframes(N), "<i2").astype(np.int32)
    seam = abs(x[0] - x[-1])
    typical = np.percentile(np.abs(np.diff(x)), 99.9)
    assert seam <= typical, f"{path.name}: seam step {seam} > p99.9 step {typical}"
    return seam, typical


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "sd_card/sounds")
    out.mkdir(parents=True, exist_ok=True)
    for name, make in SOUNDS.items():
        path = out / f"{name}.wav"
        write(path, to_pcm(make()))
        seam, typical = check(path)
        print(f"{path}  {path.stat().st_size / 1e6:.1f} MB  seam step {seam} (p99.9 step {typical:.0f})")


if __name__ == "__main__":
    main()

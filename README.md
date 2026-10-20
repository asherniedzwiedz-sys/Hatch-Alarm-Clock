# DIY Sunrise Alarm Clock (ESP32)

A clean-room, Hatch-style wake-up light: a 30-minute sunrise ramp, a gentle chime, sleep sounds, a night light, a dimmable dot-matrix clock, contactless sleep tracking with a radar, and a web UI. The enclosure copies the Hatch look: an arch-shaped body with a thin rim, a linen front that glows above a soft horizon, the clock dots showing through the fabric, and a dome knob on top. It was written from a feature list only. No vendor firmware, app or audio was used, and every sound is synthesized by `tools/gen_sounds.py`.

```
src/          firmware (PlatformIO, Arduino-ESP32 2.0.17)
  main.cpp            policy: alarm phase -> light + sound, buttons -> actions
  Logic.h             pure logic: sunrise curve, kelvinToRGB, alarm state machine, JSON validation
  LightEngine.*       WS2812B via NeoPixelBus (RMT), fades, current cap, 10 Hz
  AudioEngine.*       I2S task: looped SD WAVs + synthesized chime
  AlarmScheduler.*    DS3231 RTC, NTP -> RTC, alarms/settings as JSON in NVS
  WebUI.*             Wi-Fi STA + setup AP, JSON API, OTA
  InputHandler.*      encoder ISR + debounced buttons
  ClockDisplay.*      optional HT16K33 16x8 dot clock (I2C)
  SleepLogic.h        pure logic: LD2410C frame parser, per-minute sleep tracker
  SleepMonitor.*      radar on UART2, last 7 nights in NVS
data/index.html       the whole web UI (LittleFS)
test/test_logic/      native unit tests for Logic.h and SleepLogic.h
tools/gen_sounds.py   synthesizes the 5 seamless sleep loops
tools/mock_server.py  fake clock API for working on the web UI without hardware
enclosure/            OpenSCAD model, STLs, build/check script, assembly guide
```

## Wiring

| Module | Module pin | Connects to |
|---|---|---|
| 5 V 3 A USB-C supply | 5V / GND | LED rail, MAX98357A VIN, ESP32 **5V/VIN** pin; all grounds common |
| WS2812B strip (40) | 5V / GND | straight from the supply (not through the devkit); **1000 µF/10 V** across 5V-GND at the strip input |
| WS2812B strip | DIN | 74AHCT125 **1Y → 470 Ω → DIN** |
| 74AHCT125 | VCC / GND / 1OE / 1A | 5 V / GND / GND / **GPIO16**; unused A inputs to GND, unused OE to 5 V |
| MAX98357A | VIN / GND | 5 V / GND |
| MAX98357A | BCLK / LRC / DIN | **GPIO26 / GPIO25 / GPIO32**; GAIN and SD left open (9 dB, L+R mix) |
| Speaker 4 Ω 3 W | + / − | MAX98357A speaker terminals |
| DS3231 + CR2032 | VCC / GND / SDA / SCL | 3V3 / GND / **GPIO21 / GPIO22** |
| BH1750 (optional) | VCC / GND / SDA / SCL / ADDR | 3V3 / GND / GPIO21 / GPIO22 / GND (address 0x23) |
| HT16K33 16×8 dot clock (optional) | VCC / GND / SDA / SCL | 5 V / GND / GPIO21 / GPIO22 **through a BSS138 I2C level shifter** (address 0x70) |
| HLK-LD2410C radar (sleep tracking) | VCC / GND / TX / RX / OUT | 5 V / GND / **GPIO4** / **GPIO13** / not connected (3.3 V UART, 256000 baud) |
| microSD (VSPI) | CS / SCK / MISO / MOSI | **GPIO5 / 18 / 19 / 23**; VCC 5 V if the module has a regulator, else 3V3 |
| Rotary encoder | A(CLK) / B(DT) / SW / + / GND | **GPIO34 / GPIO35 / GPIO33** / 3V3 / GND |
| Alarm button | one side / other | **GPIO27** / GND |
| Light / snooze button | one side / other | **GPIO14** / GND |

Hardware notes that matter:
- The LED strip is **96 LED/m**: that is what fits 40 LEDs in the Hatch-size dome. A 60 LED/m strip fits only ~29; to use one, set `LED_COUNT` in `src/Config.h` and `led_count` / `led_per_m` in the enclosure model to match.
- The dot clock's white LEDs need 5 V. Its backpack pulls SDA/SCL up to its own VCC, so go through a BSS138 I2C level shifter; the ESP32 is not 5 V tolerant. A red or amber matrix also runs directly from 3V3, without the shifter. If the digits come out mirrored or upside down, flip `CLOCK_FLIP_X` / `CLOCK_FLIP_Y`.
- The radar sits behind the lower-left of the fabric, antenna side forward, with nothing metal in front of it. Put the clock at mattress height, facing the bed. Set **Bed distance** in the web UI to just past the far side of the bed, so someone walking around the room doesn't count. Then watch the live **motion** value while you lie still and while you turn over. If lying still reads above ~15, raise `SLEEP_MOVE_THRESHOLD` in `src/Config.h`.
- **GPIO34/35 have no internal pull-ups.** Use 10 kΩ pull-ups to 3V3 on encoder A and B; KY-040 boards already have them. If one click moves two steps, set `ENC_STEPS_PER_DETENT = 2` in `src/Config.h`. If the direction is reversed, set `ENC_DIRECTION = -1`.
- **DS3231 "ZS-042" boards charge the coin cell** through a diode and 200 Ω resistor meant for an LIR2032. With a CR2032, remove that resistor or diode, or the cell can swell.
- LED current is capped in firmware at `LED_MAX_MA = 1800`, which leaves headroom on the 3 A supply for the ESP32 and the amp. Full 3200 K sunrise draws about 1.76 A. Use ≥ 22 AWG for the LED power wires, and inject 5 V at both ends of the strip, using the second hole in the LED plate.
- Mount the BH1750 under the top-wall window, facing up and shielded from the diffuser. Otherwise it measures its own light and auto-dim drifts up.
- Do not plug in the USB cable and the 5 V supply at the same time unless your devkit has a diode on USB VBUS.

## Build and flash

```bash
pio run -e esp32 -t upload
```
```bash
pio run -e esp32 -t uploadfs
```
The first command flashes the firmware; the second uploads the web UI to LittleFS.

On Windows, keep the project in a short path, or set `PLATFORMIO_WORKSPACE_DIR` to a short folder such as `C:\pio-ws`. Library paths under `.pio` can exceed the 260-character limit, and the compiler then reports missing files.

Sleep sounds: generate them, then copy `sd_card/sounds/*.wav` to `/sounds/` on a FAT32 card. Each file is 60 s, 16-bit, 44.1 kHz mono, about 5.3 MB. Any other 16-bit / 44.1 kHz PCM WAV in `/sounds` also shows up in the UI.
```bash
python tools/gen_sounds.py
```

Unit tests run on the host and need a GCC/MinGW in PATH:
```bash
pio test -e native
```

## First setup

1. Power on. With no Wi-Fi saved, the clock opens the AP **SunriseClock-Setup** (password `sunrise-setup`). If the saved network is unreachable for 30 s, it opens the same AP again, and keeps retrying the network in the background.
2. Browse to `http://192.168.4.1`. Then:
   - Pick your time zone.
   - Press **Set clock from this device**.
   - Enter your Wi-Fi network and an **admin password**.
   - Save. The clock reboots onto your network as `http://sunrise.local`.
3. Once online, NTP keeps the DS3231 accurate. The RTC is the time authority: after a power cut, or with no Wi-Fi at all, the time comes from the RTC and alarms still fire.

Security: change `AP_PASS` in `src/Config.h` before flashing, because the default is public. Set an admin password during first setup. Firmware upload stays disabled until one exists, and once set it also protects every change plus the sleep and occupancy data. The API only answers requests addressed to the clock itself (its IP, `sunrise` or `sunrise.local`), and rejects other websites' requests. That blocks drive-by attacks from a page open in your browser.

Five red blinks at boot mean the clock has no valid time (DS3231 missing, or its coin cell was dead). Set the time from the web UI.

## Behaviour

| | |
|---|---|
| Sunrise | Starts 30 min before the alarm. `brightness = max * (t/30 min)^2.2` (t = 15 min → 21.8 %). Colour 1700 K → 3200 K. Updated at 10 Hz with 1 s smoothing. |
| T+0 | Full 3200 K at "Sunrise max", plus a synthesized C-E-G chime that fades in over 2 min. It needs no SD card. |
| Snooze | 9 min at 50 % light, chime off, then rings again. |
| Auto-off | 20 min after ringing starts, light and sound fade out over 60 s. |
| Power cut | Rebooting inside a sunrise or ring window resumes it. A dismissed alarm stays dismissed (stored in NVS). |
| Alarms | Up to 10, per-weekday repeat. No days = one-time; it disables itself after it fires. |
| Sleep timer | 15-120 min; the sound fades out over the timer's last 10 min. |
| Night light | Default 2700 K, 1-100 %. With auto-dim on, it drops to 25 % of the set level in a dark room. |
| Sleep tracking | Starts once someone has been in bed for 10 min between 18:00 and 04:00, backdated to when they got in. Ends after 15 min out of bed; nights under 1 h are dropped. Asleep = 5 still minutes in a row. Awake = 2 restless minutes, or leaving the bed (turning over isn't a wake-up). The web UI shows each night's time in bed and asleep, time to fall asleep, wake-ups and a 5-minute timeline, plus a 7-night average. Movement-based only: no sleep stages, one sleeper, not a medical device. Nights stay in NVS on the clock (newest 7). |
| Clock | H:MM (12 h) or HH:MM (24 h) behind the fabric. Brightness 1-16 or off, set in the web UI. Auto-dim lowers it in a dark room, never below 1. Shows `--:--` until the time is set. |

Controls:

| Control | Idle | Sunrise ramp | Ringing / snoozed |
|---|---|---|---|
| Light button (14) | night light on/off | ignored, so a bump can't cancel the alarm | **snooze** |
| Alarm button (27) | toggle the next alarm: 2 green blinks = on, 1 red = off, 3 red = no alarms. A second press undoes the first. | **dismiss** | **dismiss** |
| Encoder push (33) | switch encoder mode: amber blink = brightness, blue = volume | ignored | **dismiss** |
| Encoder turn | brightness (turns the night light on) or volume | volume | volume |

The web UI covers alarms, light, sleep history, sounds and sleep timer, time zone and clock, Wi-Fi, the admin password, and OTA. Upload `.pio/build/esp32/firmware.bin` as Firmware, or `.pio/build/esp32/littlefs.bin` (from `pio run -t buildfs`) as Web UI. A failed or interrupted OTA leaves the old firmware running.

Factory reset (clears alarms, settings and Wi-Fi):
```bash
pio run -e esp32 -t erase
```

## Enclosure

Hatch-style arch, 184 × 140 × 72 mm. See [enclosure/ASSEMBLY.md](enclosure/ASSEMBLY.md) for the parts, fabric wrapping and assembly, and [front.png](enclosure/front.png) for the look. Parts are printed from `enclosure/stl/`; rebuild and re-check them with `python enclosure/build.py`.

## Acceptance status

| Criterion | How it was checked | Result |
|---|---|---|
| PlatformIO builds with zero warnings | Clean `pio run -e esp32`, whole log including libraries. Project sources also build with `-Wall -Wextra -Werror`. | 0 warnings; RAM 17 %, flash 53 % of one OTA slot |
| Sunrise spot-check t = 15 min → ~22 % | `test_sunrise_curve`, `test_sunrise_ring_snooze_dismiss` (14 native tests in total, including the clock font, radar parser and simulated nights) | 0.2176 |
| Web UI saves an alarm that fires from the RTC with Wi-Fi off | Page driven in a browser against `tools/mock_server.py`: posted `[{"hour":5,"minute":30,"days":124,...}]`. The same JSON shape is parsed by the firmware validator in `test_alarm_json`. Firing is time-only (RTC → system clock, no network): `test_*` cover sunrise, ring, snooze, auto-off, weekend skip, power-loss resume and one-shot. | passes; **not yet run on hardware** |
| STLs manifold, parts fit with clearances | `enclosure/build.py`: edge-manifold check per STL, ≤ 220 mm bed check, pairwise interference volumes of the 12 assembled pairs that meet | 7/7 manifold, largest footprint 184 × 140 mm; 0 mm³ overlap on every pair except the intended 0.33 mm³ diffuser crush-rib press fit |

On-device check for the RTC criterion:
1. Save an alarm 2 minutes ahead.
2. Switch the router off, or set a wrong SSID.
3. Power-cycle the clock.

The light should come straight up to ~86 % (the ramp resumes 93 % of the way through) and the chime should start at the alarm minute.

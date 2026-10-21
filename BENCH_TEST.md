# Bench test

Test the clock in two stages before you print the case or solder anything:
- **Part 1, the Wokwi simulator:** do this now, before you buy the parts. It shows whether the firmware starts and whether the LEDs, knob, buttons and alarm behave.
- **Part 2, the real parts on your desk:** do this when they arrive. Everything is connected with jumper wires, one module at a time, so a dead part or a wiring mistake shows up while it's still easy to swap.

If a step doesn't match what's described, copy the serial monitor text and say what you saw.

---

## Part 1: Wokwi simulator

**What it can test:** the ESP32, the LED strip, the knob, both buttons and the real-time clock chip. Wokwi only has the DS1307, which stands in for the DS3231 because the code reads the same registers from both.

**What it can't test:** sound, the dot clock, the radar, the light sensor, power, part fit, or the look through fabric. That's Part 2.

### Set up (once)

1. Install [VS Code](https://code.visualstudio.com/).
2. In VS Code, open **Extensions** (the four-squares icon on the left) and install these two:
   - **PlatformIO IDE**
   - **Wokwi for VS Code**
3. Get the project:
   1. On the [GitHub page](https://github.com/asherniedzwiedz-sys/Hatch-Alarm-Clock), click **Code → Download ZIP**.
   2. Unzip it to a **short path**, such as `C:\sunrise-clock`. Long Windows paths, like a OneDrive folder, can break the build.
4. In VS Code, use **File → Open Folder** and pick that folder. PlatformIO sets itself up the first time, which takes a few minutes.
5. Activate Wokwi:
   1. Press **F1** and choose **Wokwi: Request a new License**.
   2. Sign in on the page that opens, then click **GET YOUR LICENSE** and confirm.

### Build (again after any code change)

Open a terminal (**Terminal → New Terminal**) and run these two commands, one after the other:

```bash
pio run -e wokwi -t buildfs
```
```bash
pio run -e wokwi
```

The first builds the web page; the second builds the firmware and merges everything into one file. The second command should end with `wokwi_merge: wrote ...firmware.merged.bin` and `SUCCESS`.

If `pio` isn't found, use the PlatformIO sidebar instead: the ant-head icon, then **Miscellaneous → New Terminal**.

### Run

Press **F1** and choose **Wokwi: Start Simulator**. A diagram opens with these parts:
- the ESP32
- a strip of 40 LEDs
- the clock chip
- the knob
- a **red button**: Alarm, GPIO27
- a **yellow button**: Light / snooze, GPIO14

The serial monitor appears at the bottom. The simulation can run slower than real time, so the timings below can stretch.

### Checks

| # | Do | You should see |
|---|---|---|
| 1 | Start it and wait about 10 s | Serial shows `Test alarm set for HH:MM` and `Sunrise clock 1.0.0 ready`. `No SD card` and `No dot clock answered at 0x70-0x77` are normal here, because Wokwi has neither. It must **not** keep rebooting, or print `Guru Meditation` or `Backtrace`. |
| 2 | Watch the strip | It fades up to warm light, around 80 % within a few seconds. The test alarm is 3 minutes away, so the 30-minute sunrise picks up near its end. |
| 3 | Wait for the alarm minute | Full warm white. The chime is silent in Wokwi; that's expected. |
| 4 | Click the **yellow** button | Snooze: the light drops to about half. |
| 5 | Click the **red** button | Dismiss: the light fades out over about 3 s. |
| 6 | Click **yellow** again | The night light turns on, warm and dim. Click again: off. |
| 7 | With the night light on, turn the knob | The brightness changes. |
| 8 | Click the knob | 1 blue blink: the knob now sets the volume. Click again: 1 amber blink, back to brightness. |
| 9 | Click **red** while nothing is ringing | 2 green blinks: it switched the test alarm back on, because a one-time alarm switches itself off after it fires. Click again: 1 red blink = off. |
| 10 | Optional: open http://localhost:8180 in your browser while it runs | The clock's web page. Try adding an alarm a minute or two ahead and watch it fire. If the page doesn't load, Wokwi's network forwarding may not be on your plan. Skip this check; Part 2 tests the page for real. |

To stop, close the simulator tab, or press **F1** and choose **Wokwi: Stop Simulator**. Each start begins from a blank clock, with a new test alarm.

Checks 1–9 passing means the firmware boots and the light, controls, clock chip and alarm logic work on a simulated ESP32. Order the parts.

---

## Part 2: Real parts on the desk

### Before you start
- **Measure the boards against the model.** Tell me anything that differs, and I'll change the model before you print.

  | Part | Size the model expects |
  |---|---|
  | Radar | 22 × 16 mm |
  | Clock board, LED area | 40.6 × 20.3 mm |
  | Clock board, whole board | about 41 × 23 mm |
  | Speaker | 51 mm across |
  | LED strip | 10 mm wide |
  | ESP32 mounting holes | 22 × 60 mm apart, if it has holes |

- **Two power sources, one ground.** During these tests:
  - The ESP32 runs from your computer's USB cable, so you can see the serial monitor.
  - The LEDs, level shifter and amp run from the 5 V USB-C supply.
  - Connect **GND to GND** between them.
  - **Don't** connect the supply's 5 V to the ESP32 while its USB cable is plugged in (see the README).
- Unplug both before you change any wiring.

### 1. Flash the firmware (ESP32 alone, on USB)

```bash
pio run -e esp32 -t upload
```
```bash
pio run -e esp32 -t uploadfs
```
```bash
pio device monitor -b 115200
```

The first flashes the firmware, the second uploads the web page, and the third opens the serial monitor. You should see `Sunrise clock 1.0.0 ready`. Lines saying the DS3231 and the dot clock are missing are fine at this point.

### 2. Dot clock (first, because it's the biggest unknown)

Wire VCC → 3V3, GND → GND, SDA → GPIO21, SCL → GPIO22. Then press the ESP32's **EN** button to restart it.
- `Dot clock found at 0x7_` plus `--:--` on the display means it works.
- **Mirrored or upside down:** set `CLOCK_FLIP_X` and/or `CLOCK_FLIP_Y` to `true` in `src/Config.h`, then upload again.
- **`No dot clock answered`:** check the 4 wires. If they're right, read me the chip's name off the back of the board.

### 3. Real-time clock

1. On the DS3231 board, remove the charging resistor first (see the README), then put in the CR2032.
2. Connect it to the same 4 wires as the clock.
3. Press EN. The `DS3231 not responding` line should be gone.

### 4. Wi-Fi and the web page

1. On your phone, join **SunriseClock-Setup** (password `sunrise-setup`).
2. Open http://192.168.4.1.
3. Set your time zone, press **Set clock from this device**, enter your Wi-Fi and an admin password, and save.
4. The clock restarts onto your Wi-Fi at http://sunrise.local.

The dot clock should now show the time.

### 5. LED strip and level shifter

| From | To |
|---|---|
| Supply 5 V | Strip 5V, 74AHCT125 pin 14 (VCC) |
| Supply GND | Strip GND, 74AHCT125 pin 7 (GND), ESP32 GND |
| ESP32 GPIO16 | 74AHCT125 pin 2 (1A) |
| 74AHCT125 pin 3 (1Y) | 470 Ω resistor → strip DIN |
| 74AHCT125 pin 1 (1OE) | GND |
| 74AHCT125 pins 5, 9, 12 (unused inputs) | GND |
| 74AHCT125 pins 4, 10, 13 (unused OE) | 5 V |
| 1000 µF capacitor | across strip 5V and GND, at the strip; **striped leg to GND** |

The whole uncut strip is fine for this test; only the first 40 LEDs light. Pin 1 of the chip is next to the notch or dot.

Press the light button, or use the web page's Light section: you should get a steady warm light. **Flickering or random colors** points at the 74AHCT125 first; it may be a relabeled 74HC125.

### 6. Knob and buttons

Wire them up:
- Knob: CLK → GPIO34, DT → GPIO35, SW → GPIO33, + → 3V3, GND → GND.
- Alarm button: GPIO27 to GND.
- Light button: GPIO14 to GND.

Run checks 6–9 from Part 1.
- **Knob turns the wrong way:** set `ENC_DIRECTION = -1` in `src/Config.h`.
- **One click moves two steps:** set `ENC_STEPS_PER_DETENT = 2`.

### 7. Amp and speaker

Wire the amp: VIN → supply 5 V, GND → GND, BCLK → GPIO26, LRC → GPIO25, DIN → GPIO32. The speaker goes on the amp's + and − screw terminals.

In the web page, play each sleep sound (white, pink, rain, ocean, fan) and move the volume. They should sound smooth, with no crackling or buzz. A bare speaker sounds thin; it fills out in the case.

### 8. Alarm at full power (the important one)

1. In the web page:
   - set **Sunrise max** to 100 % and the alarm volume high;
   - add an alarm 2 minutes ahead.
2. Watch it ramp up, ring, snooze (light button) and dismiss (alarm button). Serial must **not** show a restart.
3. **The real power test:**
   1. Unplug the ESP32's USB cable.
   2. Connect the supply's 5 V to the ESP32's **5V/VIN** pin, so everything runs from the one supply, as it will in the case.
   3. Set another alarm 2 minutes ahead and let it ring at full power.
   - **3 amber blinks** right after it starts back up mean it restarted from a power dip. Lower Sunrise max or the volume, or use a stronger 5 V 3 A supply.
   - No blinks: the supply is holding up.
4. **The no-Wi-Fi test:**
   1. Set an alarm 2 minutes ahead.
   2. Switch your router off, or change the clock's Wi-Fi to a wrong network.
   3. Unplug the clock and plug it back in.
   - The alarm should still go off on time.

### 9. Radar

Wire it: VCC → supply 5 V, GND → GND, TX → GPIO4, RX → GPIO13. In the web page's **Sleep** section:
- The live line should show you present, with motion going up when you move.
- Set **Bed distance** to just past the far side of your bed.
- Lie still, then turn over, and watch the motion number change.

Real sleep numbers need a few nights to judge.

### 10. Light sensor (optional)

Wire it: VCC → 3V3, GND → GND, SDA → GPIO21, SCL → GPIO22, ADDR → GND.
1. Turn on auto-dim in the web page.
2. Turn the night light on and cover the sensor. The light should drop to about a quarter.

### 11. Look and fit, before printing everything

1. **Fabric:** shine a phone flashlight through it; it should glow evenly. Then hold it over the lit dot clock; the digits should stay readable.
2. **Light parts first:** print just the frame, diffuser and light box. Lay the strip rows in the light box channels behind the diffuser and check that the glow looks even.
3. Then print the rest, solder everything onto the protoboard to match the README wiring table, and assemble following [enclosure/ASSEMBLY.md](enclosure/ASSEMBLY.md).

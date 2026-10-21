# Parts list

> **Cheaper build:** the sleep sounds are now synthesized on the ESP32, so the **microSD module and card are optional** (only for your own WAVs). See [Lean build](#lean-build) at the end.

Picked on Amazon on 2026-10-20, with each listing's specs checked against this build. Prices change. Most items are multipacks, so you'll have spares. Not included: the ESP32 devkit and the white and black PLA, which you already have.

## Electronics

| Part | Listing | Price | Why this one |
|---|---|---|---|
| LED strip, WS2812B 100 LED/m, 10 mm, IP30, 1 m | [BTF-LIGHTING WS2812B 100LED](https://www.amazon.com/dp/B088B8G8LD) | $11.99 | 10 mm wide fits the channels; 100/m fits 40 LEDs in the dome |
| 74AHCT125 level shifter (DIP-14) | [5 pcs + IC sockets](https://www.amazon.com/dp/B0FXJY53QG) | $8.45 | Genuine AHCT part (5 V output from a 3.3 V input) |
| 470 Ω resistor | [California JOS 100 pcs](https://www.amazon.com/dp/B0B4FSVCPT) | $3.99 | LED data line |
| 1000 µF 16 V capacitor | [10-pack, low ESR](https://www.amazon.com/dp/B0D78PBK4K) | $5.99 | Across the LED 5 V supply |
| MAX98357A I2S amp | [AITRIP 3 pcs](https://www.amazon.com/dp/B0GCM7QRP6) | $7.59 | |
| Speaker, 2" 4 Ω 3 W | [Gikfun EK1725, 2 pcs](https://www.amazon.com/dp/B01CHYIU26) | $11.49 | No 57 mm 4 Ω 3 W driver is sold on Amazon; the model's seat is now set for this one (51 mm) |
| DS3231 RTC (ZS-042) | [2 pcs with jumper wires](https://www.amazon.com/dp/B09LLMYBM1) | $7.99 | Remove the charging resistor before fitting a CR2032 |
| CR2032 coin cells | [NICEBATT 24 pack](https://www.amazon.com/dp/B0F28618FV) | $7.99 | |
| microSD module (SPI), optional | [HiLetgo 5 pcs](https://www.amazon.com/dp/B07BJ2P6X6) | $6.99 | Has its own regulator: power it from 5 V |
| microSD card, 32 GB, optional | [SanDisk Ultra 32GB](https://www.amazon.com/dp/B00M55C0NS) | $19.85 | Any 32 GB or smaller card works (FAT32) |
| Rotary encoder | [KY-040, 5 pcs](https://www.amazon.com/dp/B06XQTHDRR) | $9.39 | Built-in 10 kΩ pull-ups (GPIO34/35 need them); 6 mm D shaft fits the knob |
| 2 × 12 mm momentary buttons | [Gebildet 5 pcs, prewired](https://www.amazon.com/dp/B08L49F7DV) | $7.99 | Fits a 12 mm hole. Pick the **Flat Head** option for a cleaner top. |
| USB-C power input | [eleUniverse 2 pcs, dual 5.1 kΩ](https://www.amazon.com/dp/B0GR8SNSCQ) | $8.90 | Has the CC resistors USB-C chargers need; 3 A. New listing with no reviews yet, but the specs match. |
| 5 V 3 A USB-C supply | [5 V 3 A, 5 ft cable](https://www.amazon.com/dp/B09JW4QQJ2) | $9.99 | Raspberry Pi-style, fixed cable |
| Sleep radar HLK-LD2410C | [AEDIKO 2 pcs](https://www.amazon.com/dp/B0GQYYTJPS) | $13.99 | Only 9 reviews; [3-pack alternative](https://www.amazon.com/dp/B0CH8CRDS6). The frame pocket is 22 × 16 mm: measure yours and set `radar_size` if it differs. |
| BH1750 light sensor (optional) | [HiLetgo 3 pcs](https://www.amazon.com/dp/B00M0F29OS) | $7.49 | Auto-dim |

## Not on Amazon

| Part | Where | Why |
|---|---|---|
| Dot-matrix clock: **Adafruit 0.8" 8x16 LED Matrix FeatherWing**, warm white | adafruit.com (search "8x16 LED Matrix FeatherWing") | Amazon only had no-review or 1★ generic boards, a 64 mm red one that doesn't fit the pocket, and a white one with no size or wiring info. The Adafruit board runs from 3V3, so it needs no level shifter. |

## Wiring and build supplies

| Part | Listing | Price |
|---|---|---|
| Protoboards + male/female headers | [58 pcs kit, 6 sizes](https://www.amazon.com/dp/B0CKZ7GFSK) | $13.99 |
| 22 AWG silicone wire, 10 colors | [BNTECHGO](https://www.amazon.com/dp/B01M0O1NXM) | $13.48 |
| Female-female jumper wires | [40 pcs, 20 cm](https://www.amazon.com/dp/B0BRTJQGS6) | $3.99 |
| M3 heat-set inserts, M3 × 5.7 × 4.6 mm | [100 pcs](https://www.amazon.com/dp/B0DQL4ZSCP) | $5.99 |
| M3 countersunk screws (6/8/10 mm and up) | [630 pcs kit](https://www.amazon.com/dp/B0F8HPFXX5) | $8.99 |
| Rubber feet, 10 × 3 mm | [QKP 300 pcs](https://www.amazon.com/dp/B0GW3MNBP4) | $4.99 |
| Linen fabric, natural | [Simple&Opulence 100 % linen, 1 yd](https://www.amazon.com/dp/B0CGV3XTTC) | $17.99 |
| Spray adhesive | [3M Super 77](https://www.amazon.com/dp/B0000AZ735) | $15.89 |

Amazon total: about **$235**, plus the Adafruit clock.

Notes:
- **Screws:** countersunk heads work everywhere in this build, including the spots the assembly guide lists as pan head.
- **Protoboard holes:** the devkit standoffs are 22 × 60 mm apart. Drill the protoboard to match, or set `pcb_holes` in the model to your board's holes.
- **Fabric:** before gluing, hold the linen up to a lamp. It should glow; if it's too dense, use a thinner weave.
- **Level shifter:** a BSS138 I2C level shifter is only needed if you use a 5 V clock board instead of the Adafruit one.

## Lean build

The total above is mostly packaging: multipacks, plus supplies you may already have. For one clock with every function:

| | Cost |
|---|---|
| **Skip:** microSD module + card (sounds are built in now) | −$26.84 |
| **Use what you have:** a USB-C phone charger (5 V 3 A) instead of the power supply, an old linen shirt or pillowcase for the fabric, hot glue instead of spray adhesive, a coin cell from the drawer, the strip's own wires plus jumpers instead of the wire kit, no rubber feet | −$70 or so |
| **Cheaper fabric** if you have none: [muslin, 1.3 yd](https://www.amazon.com/dp/B0G8HGNHWH) | $6.99 |
| **Amazon, lean:** strip, [74AHCT125 10 pcs](https://www.amazon.com/dp/B08R6BCSYC), resistor, capacitor, [amp 2 pcs](https://www.amazon.com/dp/B0DPJRLMDJ), speaker, RTC, encoder, buttons, USB-C board, radar, jumpers, protoboards, inserts, screws | **$129.55** (+$7.49 for the auto-dim light sensor) |
| **Plus** the Adafruit clock from adafruit.com | about $15 + shipping |

Ordering single units from AliExpress instead brings the parts to roughly **$35–55** in total, at 1–3 weeks' shipping. That's a typical price range, not checked today. Each module there is about $0.50–3 rather than a $7–14 multipack.

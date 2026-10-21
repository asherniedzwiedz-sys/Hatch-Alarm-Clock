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

## Amazon vs AliExpress

The same lean build, one clock, every function. Prices checked 2026-10-20. AliExpress prices are the regular ones, not the one-time "new shopper" deals. AliExpress shipping showed free, with delivery around 6–11 days.

| Part | Amazon | AliExpress | Quality notes |
|---|---|---|---|
| LED strip, 1 m | [BTF-Lighting 100/m](https://www.amazon.com/dp/B088B8G8LD) **$11.99**, 4.6★ | [96/m IP30, ANTVLED store](https://www.aliexpress.us/item/3256807796309465.html) **$4.59**, 4.8★, 5,000+ sold | BTF is a known brand with consistent color; the AliExpress store has 94.8 % positive feedback. Either density fits the channels. Pick "1m 96 IP30". |
| 74AHCT125 | [10 pcs](https://www.amazon.com/dp/B08R6BCSYC) **$7.99**, 4.7★ | [5 pcs](https://www.aliexpress.us/item/3256807469514168.html) **$2.41**, 4.9★, 2,000+ sold | Cheap chips can be relabeled 74HC parts, which won't shift 3.3 V to 5 V. If the LEDs flicker, that's the first suspect. |
| 470 Ω resistor | [100 pcs](https://www.amazon.com/dp/B0B4FSVCPT) **$3.99** | [100 pcs, pick 470R](https://www.aliexpress.us/item/3256809879566136.html) **$1.84** | Same part |
| 1000 µF 16 V capacitor | [10 pcs](https://www.amazon.com/dp/B0D78PBK4K) **$5.99** | [pick 16V 1000uF](https://www.aliexpress.us/item/3256811625053645.html) **$2.75**, 4.8★ | Same part |
| MAX98357A amp | [2 pcs](https://www.amazon.com/dp/B0DPJRLMDJ) **$6.88**, 4.5★ | [1 pc](https://www.aliexpress.us/item/3256807442706139.html) **$2.34**, 4.8★, 4,000+ sold | Same board design |
| Speaker 2" 4 Ω 3 W | [Gikfun, 2 pcs](https://www.amazon.com/dp/B01CHYIU26) **$11.49**, 776 reviews | [SOTAMIA 50 mm, 2 pcs](https://www.aliexpress.us/item/3256808046771474.html) **$7.34**, 528 sold | Gikfun is the better-proven one; both fit the seat (glue it in) |
| DS3231 RTC | [2 pcs](https://www.amazon.com/dp/B09LLMYBM1) **$7.99**, 4.4★ | [1 pc](https://www.aliexpress.us/item/3256810013782326.html) **$2.83**, 4.9★, 900+ sold | Pick the "DS3231SN" option. "M" chips are less accurate but still work. |
| KY-040 encoder | [5 pcs](https://www.amazon.com/dp/B06XQTHDRR) **$9.39**, 4.5★ | [1 pc + knob](https://www.aliexpress.us/item/2255800145982269.html) **$1.48**, 4.7★, 1,000+ sold | Same module |
| 2 × 12 mm buttons | [Gebildet, 5 pcs](https://www.amazon.com/dp/B08L49F7DV) **$7.99**, 981 reviews | [1 pc each](https://www.aliexpress.us/item/2251832630370510.html) **$4.40** for 2, 90 sold | Amazon's are much better reviewed. The AliExpress listing is the weakest here. |
| USB-C power input | [2 pcs](https://www.amazon.com/dp/B0GR8SNSCQ) **$8.90**, no reviews | [dual 5.1 kΩ](https://www.aliexpress.us/item/3256807162617616.html) **$0.41**, 4.8★, 2,000+ sold | AliExpress is the safer pick here: proven listing |
| HLK-LD2410C radar | [2 pcs](https://www.amazon.com/dp/B0GQYYTJPS) **$13.99**, 9 reviews | [1 pc](https://www.aliexpress.us/item/3256811718206907.html) **$4.19**, 571 sold | Same Hi-Link module |
| Jumper wires, 40 F-F | [40 pcs](https://www.amazon.com/dp/B0BRTJQGS6) **$3.99**, 4.8★ | [40 pcs, pick F-F 20 cm](https://www.aliexpress.us/item/3256812379729214.html) **$3.32** | Same |
| Protoboards | [58 pcs kit + headers](https://www.amazon.com/dp/B0CKZ7GFSK) **$13.99** | [5 pcs, pick 3x7](https://www.aliexpress.us/item/3256808585524708.html) **$4.15**, 560 sold | The Amazon kit includes headers and terminals |
| M3 heat-set inserts | [100 pcs, M3×5.7×4.6](https://www.amazon.com/dp/B0DQL4ZSCP) **$5.99** | [pick M3, longest](https://www.aliexpress.us/item/3256804349544912.html) **$2.71**, 54,000+ sold | Amazon's size is confirmed; on AliExpress pick M3 × 5.7 × 4.6 if offered |
| M3 countersunk screws | [630 pcs kit](https://www.amazon.com/dp/B0F8HPFXX5) **$8.99** | [kit, pick M3](https://www.aliexpress.us/item/3256805779852270.html) **$5.65**, 4.8★ | Same |
| **Total** | **$129.55** | **$50.41** | |
| Light sensor (optional) | [BH1750, 3 pcs](https://www.amazon.com/dp/B00M0F29OS) +$7.49 | [BH1750](https://www.aliexpress.us/item/3256812166809412.html) +$4.05, 516 sold | |
| Dot-matrix clock (both) | [Adafruit 0.8" 8x16 FeatherWing](https://www.adafruit.com/product/3149) white $19.95, or yellow $11.95, plus shipping | same | No good match on either site |
| Free if you have them | USB-C phone charger (5 V 3 A), fabric (old linen shirt or muslin), CR2032, hot glue | | |

**Quality difference:** most of these are the same Chinese-made boards on both sites; Amazon sellers often rebrand them.

What Amazon gets you:
- 1–2 day delivery and easy returns.
- Spares, because of the multipacks.
- The better-reviewed buttons and speaker.

What AliExpress gets you:
- Single parts at about $0.40–4.60 each.
- Usually longer review histories on the modules.
- Some risk of a dead or relabeled part. Test each module when it arrives.

The mixed build is a good one: buy the cheap modules on AliExpress, and the buttons (plus the speaker, if you like) on Amazon.

# Hardware

## Components
| Part | Model | Notes |
|---|---|---|
| Servo driver + motor | MKS SERVO42ES | Closed-loop NEMA17. Not the SERVO42E. Motor phases on a 4-pin screw terminal; everything else on a white 2×11 connector (see below). |
| MCU | ESP32-S3 dev board, **N16R8** | 16 MB flash, 8 MB octal PSRAM. Two USB-C ports, a CH343P USB-serial chip, BOOT/RST buttons, RGB LED. GPIO 4 and 5 sit next to 3V3 on the left header. |
| CAN transceiver | SN65HVD230 (VP230) module | Header pins: 3.3V, GND, RX, TX. Screw terminal: CANH, CANL. Has a **120 Ω termination resistor** ("121", marked 120R) with a yellow jumper next to it, probably the termination on/off. |
| Power | Lab bench PSU | 24 V (D-002). Current limit: TBD. |

## SERVO42ES signal connector (2×11)
From manual p.7, saved as `references/servo42es-connector-pinout.png`. The manual's board photo is labelled "42ES BUS", i.e. the CAN version.
**UNVERIFIED:** which row is which, and which end is pin 1. Check with a multimeter before applying power (e.g. continuity to the GND pins, or check the supplied cable).

| Row (top = end nearest the blue motor terminal) | Column A: row 10 wire is **red** | Wire | Column B: row 10 wire is **black** | Wire |
|---|---|---|---|---|
| 1 | VIN (20–48 V) | red | VIN (20–48 V) | red |
| 2 | GND | black | GND | black |
| 3 | ALM− | yellow | ALM+ | yellow |
| 4 | PEND− | green | PEND+ | green |
| 5 | IN− | blue | IN+ | blue |
| 6 | **CAN L** | purple | **CAN H** | purple |
| 7 | **CAN L** | brown | **CAN H** | brown |
| 8 | RS485 B | grey | RS485 A | grey |
| 9 | RS485 B | white | RS485 A | white |
| 10 | **5V out** (100 mA) | **red** | GND | black |
| 11 | SWCLK | orange | SWDIO/RESET | orange |

Wire colours come from the user's photos of the supplied cable (2026-09-11). Both rows use the same colour sequence, except row 10: red on one side, black on the other. That matches the manual's 5V/GND difference in row 10, which also confirms the orientation (row 1 at the motor-terminal end). The colours in the manual drawing are only label colours, not wire colours.
- Column A = the manual's left column (5V side). Column B = the right column (GND side).
- **Danger:** there are 3 red wires. Two are VIN (row 1). The third, red in row 10 of column A, is the **5V output**. Never connect 24 V to it.
- Each colour appears twice among the loose ends, so the ends must be traced back to their row before use. Unused ends (ALM, PEND, IN, RS485, SWD, 5V) should be insulated.

Notes:
- The two CAN pairs are connected in parallel, so either one works; the other can continue the bus to the next motor.
- The two VIN pins are paired, as are the two GND pins.
- The motor's own CAN termination isn't documented. Measured: it has none (see below).

## Measurements (VERIFIED 2026-09-11, power off, cable plugged into motor)
The user labelled the loose ends: 24V+, 24V−, CAN H, CAN L.
| Test | Result | Meaning |
|---|---|---|
| 24V+ ↔ other row-1 red, continuity | beep | the 24V+ label is on a VIN wire |
| 24V+ ↔ 24V−, Ω | climbs from 0 through 60k, 120k, then over range | capacitor charging, no short |
| CAN H ↔ brown on the same side, continuity | beep | CAN rows 6/7 are paralleled, so the H label is on the right side |
| CAN H ↔ CAN L, Ω | ~30 kΩ, no beep | **no termination resistor in the motor**; a proper pair, not shorted |
Meter note: this meter shows over-range as something that reads like "0". A real short beeps.

## MCU notes (ESP32-S3 N16R8)
These are general ESP32-S3 facts. Check them against the actual board's pinout.
- **Pins to avoid:**
  - GPIO 35, 36, 37 are used by the octal PSRAM.
  - GPIO 26–32 are used by flash/PSRAM.
  - GPIO 0, 3, 45, 46 are strapping pins (they affect boot).
  - GPIO 19/20 are the native USB pins.
  - GPIO 43/44 are the serial port (UART0).
- **CAN pins:** TWAI TX/RX can go on almost any free GPIO. The choice will be made at the wiring step.
- **Arduino IDE board settings:**
  - Board: "ESP32S3 Dev Module"
  - Flash Size: 16MB
  - PSRAM: "OPI PSRAM"
  - Partition Scheme: a 16M one
  - USB CDC On Boot depends on which USB port the serial monitor uses.

## Wiring
| From | To | Notes |
|---|---|---|
| ESP32 GPIO 4 (TWAI TX), proposed | Module TX | Pin choice not confirmed yet |
| ESP32 GPIO 5 (TWAI RX), proposed | Module RX | Pin choice not confirmed yet |
| ESP32 3V3 | Module 3.3V | |
| ESP32 GND | Module GND | Also a common ground with the motor's GND (manual) |
| Module CANH | Cable purple, column B (CAN H) | twisted with CANL |
| Module CANL | Cable purple, column A (CAN L) | twisted with CANH |
| Module GND / ESP32 GND | Cable black, row 2 (GND) | common ground |
| PSU +24 V | Both row-1 reds (VIN) | not the row-10 red, which is the 5V output |
| PSU GND | Row-2 black (GND) | |

## Bus settings
- Bitrate: 500K is the factory default (manual). Options are 125K/250K/500K/1M.
- Termination: the correct setup is 120 Ω at each end of the bus.
  - The CAN module has one (120R, with a jumper). The motor has none (measured ~30 kΩ between H and L).
  - For the short bench link (<1 m), the module's terminator alone is enough.
  - For the arm (longer bus, 6 motors), add a 120 Ω resistor at the far end. Easiest spot: across the spare brown pair on the last motor, since those wires are the same CAN H/L lines.
- Motor CAN ID: 01 is the factory default (manual)
- The driver's onboard CAN transceiver is a TJA1051T. It has two parallel CAN connectors, and either one works.
- The manual says the host and motor must share a common ground, and recommends shielded twisted pair.

## Power settings
- Driver supply range: **20–48 V** (manual). Below 20 V it won't run properly and raises an undervoltage alarm (status 5).
- SERVO42ES phase current: 0–3000 mA, default 1600 mA (set with command 83H)
- Chosen supply voltage: **24 V** (D-002)
- Current limit on the PSU: TBD
- Don't plug or unplug the power or signal cables while powered (manual §13.1).

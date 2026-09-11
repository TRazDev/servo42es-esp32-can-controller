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

| Row (manual drawing, top = the end nearest the motor terminal) | Left column | Right column |
|---|---|---|
| 1 | VIN (20–48 V) | VIN (20–48 V) |
| 2 | GND | GND |
| 3 | ALM− | ALM+ |
| 4 | PEND− | PEND+ |
| 5 | IN− | IN+ |
| 6 | CAN L | CAN H |
| 7 | CAN L | CAN H |
| 8 | RS485 B | RS485 A |
| 9 | RS485 B | RS485 A |
| 10 | 5V out (100 mA) | GND |
| 11 | SWCLK | SWDIO/RESET |

Notes:
- The two CAN pairs are connected in parallel, so either one works; the other can continue the bus to the next motor.
- The two VIN pins are paired, as are the two GND pins.
- The motor's own CAN termination isn't documented. Check it by measuring CAN H–CAN L with power off: about 120 Ω means it has termination; open means it doesn't.

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
| Module CANH | SERVO42ES CAN H (connector row 6 or 7) | twisted pair |
| Module CANL | SERVO42ES CAN L (connector row 6 or 7) | twisted pair |
| PSU +24 V / GND | SERVO42ES VIN / GND (rows 1–2) | |

## Bus settings
- Bitrate: 500K is the factory default (manual). Options are 125K/250K/500K/1M.
- Termination: 120 Ω at each end of the bus. Check whether the module and driver already have it.
- Motor CAN ID: 01 is the factory default (manual)
- The driver's onboard CAN transceiver is a TJA1051T. It has two parallel CAN connectors, and either one works.
- The manual says the host and motor must share a common ground, and recommends shielded twisted pair.

## Power settings
- Driver supply range: **20–48 V** (manual). Below 20 V it won't run properly and raises an undervoltage alarm (status 5).
- SERVO42ES phase current: 0–3000 mA, default 1600 mA (set with command 83H)
- Chosen supply voltage: **24 V** (D-002)
- Current limit on the PSU: TBD
- Don't plug or unplug the power or signal cables while powered (manual §13.1).

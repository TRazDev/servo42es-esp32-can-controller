# Hardware

## Components
| Part | Model | Notes |
|---|---|---|
| Servo driver + motor | MKS SERVO42ES | Closed-loop NEMA17. Not the SERVO42E. |
| MCU | ESP32-S3 dev board, **N16R8** | 16 MB flash, 8 MB octal PSRAM (see MCU notes) |
| CAN transceiver | SN65HVD230 (VP230) module | 3.3 V logic, so it connects directly to the ESP32 |
| Power | Lab bench PSU | Voltage/current limit: TBD (check manual) |

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
| ESP32 GPIO ? (TWAI TX) | SN65HVD230 CTX / TXD | TBD |
| ESP32 GPIO ? (TWAI RX) | SN65HVD230 CRX / RXD | TBD |
| ESP32 3V3 | SN65HVD230 3V3 | |
| ESP32 GND | SN65HVD230 GND | Common ground with the motor driver too |
| SN65HVD230 CANH | SERVO42ES CANH | |
| SN65HVD230 CANL | SERVO42ES CANL | |

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

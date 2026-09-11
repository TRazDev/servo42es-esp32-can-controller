# SERVO42ES ESP32 CAN Controller

![Web UI](docs/images/ui.png)

Firmware and a browser UI for controlling an MKS SERVO42ES closed-loop stepper over CAN from an ESP32-S3. It's being built as the joint controller for a 6-DOF robotic arm.

## Why

Makerbase ships the SERVO42ES with a PDF manual and no example code for the CAN interface. This project covers the missing part:

- a CAN command reference, checked against the manual's own examples, with the manual's errors listed ([docs/protocol.md](docs/protocol.md))
- ESP32 firmware that talks to the motor over CAN
- a web page served by the ESP32 over WiFi: no app to install and no internet needed

The UI works in joint degrees. You set the gearbox ratio and the firmware converts to motor units.

## Hardware

| Part | Notes |
|---|---|
| MKS SERVO42ES (CAN version) | Closed-loop NEMA17. **Not** the SERVO42E, which uses a different protocol. |
| ESP32-S3 dev board (N16R8) | Has a built-in CAN controller (TWAI) |
| SN65HVD230 CAN transceiver module | 3.3 V, connects directly to the ESP32 |
| 24 V power supply | The driver accepts 20–48 V |

## Wiring

![Wiring diagram](docs/images/wiring.svg)

| From | To |
|---|---|
| ESP32 3V3 (left header, 2nd pin) | CAN module 3.3V |
| ESP32 GND (left header, bottom pin) | CAN module GND |
| ESP32 GPIO 4 (left header) | CAN module TX |
| ESP32 GPIO 5 (left header) | CAN module RX |
| CAN module CANH | Motor cable CAN H (purple, side B, row 6) |
| CAN module CANL | Motor cable CAN L (purple, side A, row 6) |
| ESP32 GND (right header, bottom pin) | Motor cable GND (black, side B, row 10; any black works) |
| PSU + (24 V) | Motor cable VIN (red, row 1; either side) |
| PSU − | Motor cable GND (black, row 2) |

The motor's cable has 22 wires, and most colours appear twice, once per connector row. Side A is the row whose 10th wire is red; side B is the row whose 10th wire is black.

- Twist CAN H and CAN L together and keep them short.
- The red wire in row 10 (side A) is the motor's 5V output. Don't connect it.
- Insulate every unused wire end.
- The CAN module's 120 Ω termination is enough for a short bench cable. The motor has no termination of its own.

Full pinout and measurements: [docs/hardware.md](docs/hardware.md).

## Features (v1)

- Enable/disable, jog, step, and move to an absolute angle
- Live angle, speed and position error, with a 10 s trace
- Set zero; homing and limit-switch configuration
- Alarm and stall reporting, clear stall
- E-STOP that stops and holds position (doesn't release the shaft)
- Motor settings (current, direction, stall protection, link-loss timeout) saved to the motor

## Status

Early stage. The protocol notes and UI design are done; firmware is next. The screenshot shows the UI design, not the running firmware yet.

## Layout

```
docs/      protocol reference, hardware notes, design decisions
design/    UI design reference
firmware/  ESP32 sketch (Arduino IDE, coming next)
```

## License

MIT, see [LICENSE](LICENSE). The SERVO42ES manual in `docs/references/` belongs to Makerbase.

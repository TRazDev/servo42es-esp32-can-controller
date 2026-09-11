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

Wiring and pin assignments: [docs/hardware.md](docs/hardware.md) (in progress).

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

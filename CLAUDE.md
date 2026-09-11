# MKS SERVO42ES Controller

ESP32-S3 firmware + GUI for controlling an MKS SERVO42ES closed-loop stepper over CAN.
End goal: joints of a 6-DOF robotic arm (6 motors on one CAN bus). v1 controls one motor.

## Hardware (summary — details in docs/hardware.md)
- Motor/driver: MKS SERVO42ES (NOT the SERVO42E — protocols differ, don't mix up docs)
- MCU: ESP32-S3 dev board N16R8 (built-in TWAI/CAN controller). Firmware is an Arduino sketch (Arduino IDE, ESP32 core 3.3.11).
- CAN transceiver: SN65HVD230 (VP230) module, 3.3 V
- Power: lab bench supply. The driver needs 20–48 V.
- CAN factory defaults: 500 kbit/s, motor ID 01. The motor must be in mode 05 (bus closed-loop) for CAN control.

## Current status
See docs/progress.md (top entry = latest state and next steps).

## Knowledge base
- docs/hardware.md — components, wiring, pinout, power settings
- docs/protocol.md — SERVO42ES CAN protocol notes (commands, frame formats, gotchas)
- docs/gui.md — GUI spec: v1 scope, gear-ratio math, zeroing plan
- docs/design-brief.md — self-contained UI brief for Claude Design (the GUI is a web page served by the ESP32)
- design/ — exported UI design from Claude Design (once it exists)
- docs/decisions.md — decision log: what was chosen, alternatives, why
- docs/progress.md — session log, newest first
- docs/references/ — datasheets and manuals (local copies)

Upstream docs: https://github.com/makerbase-motor/MKS-SERVO42ES-57ES (manual only, no code)

## Rules for working on this project
- Read docs/progress.md at the start of a session.
- Record any non-obvious decision in docs/decisions.md as you make it.
- Put verified protocol facts in docs/protocol.md. Mark anything unverified "UNVERIFIED".
- At the end of a session, add an entry to docs/progress.md: what changed, what's next, open issues.
- Keep this file short. Put details in docs/.
- Git commits: plain messages. No Claude signatures, session links or co-author lines.

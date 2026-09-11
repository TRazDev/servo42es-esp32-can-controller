# Progress Log

Newest first. The top entry should always describe the current state.

---

## 2026-09-11 — Project kickoff
- **Done:**
  - Set up the knowledge base (CLAUDE.md + docs/).
  - Downloaded the CAN user manual V1.0.1 into docs/references/ (where it came from is in references/README.md).
  - Pulled all CAN commands into protocol.md, with a GUI rating for each and a list of errors found in the manual.
  - Added the supply range (20–48 V), bus defaults and board notes (ESP32-S3 N16R8) to hardware.md.
  - Wrote the v1 spec, gear-ratio math and zeroing plan in gui.md, plus a design brief (design-brief.md).
  - The user designed the UI in Claude Design. I pulled it into design/ and reviewed it (gui.md → Design review).
  - Started a git repo with a first commit. Added README.md with a UI screenshot (docs/images/ui.png). Created the GitHub repo TRazDev/servo42es-esp32-can-controller, added the MIT license, and made it public (D-010).
- **Decisions so far:**
  - D-002: 24 V supply
  - D-003: 6-DOF arm, mode 05
  - D-004: GUI v1 scope
  - D-005: web page served by the ESP32
  - D-006: port the design to a dependency-free page
  - D-007: E-STOP holds position
  - D-008: join home WiFi only
  - D-009: Arduino IDE + ESP32 core 3.3.11
  - D-010: public repo, MIT
  - D-011: http://servo-control.local, no login
- **Wiring (in progress):** the user sent photos of the ESP32 board, the CAN module and the motor. The CAN module has a 120 Ω terminator. Put the motor's 2×11 connector pinout (manual p.7) in hardware.md; which row is which is still unverified.
- **Next:**
  1. Wire it up: check whether the user has a cable for the motor's 2×11 connector, confirm the CAN pins (GPIO 4/5 proposed), check termination and connector orientation with a multimeter, set the PSU current limit
  2. First milestone: send one CAN command and read back a response
  3. Port the UI and wire it to the firmware
- **Open questions:** CAN bus termination, PSU current limit, and the hardware checks listed in gui.md and protocol.md

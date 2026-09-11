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
  1. Wiring done and checked from photos (hardware.md → As built). First 24 V power-up: the motor holds position. The user insulated the loose ends and confirmed the jumpers are on GPIO 4/5. Wrote `firmware/can_test` (read-only: 40H version, 31H position every second, bus status).
  - **Milestone reached: the ESP32 talks to the motor over CAN.** Uploaded with arduino-cli through the COM port (`/dev/cu.usbmodem5C831103731`) and read the serial output from the Mac. Version and position replies are correct, with 0 bus errors (protocol.md → VERIFIED).
  - **Milestone: first motion.** `firmware/move_test` (waits for 'g' over serial; any key afterwards = E-STOP) enabled the motor and moved +90° and back at 60 RPM, with correct replies. The motor was already in mode 05, so nothing was written or saved. Details: protocol.md → VERIFIED first motion.
  - Direction: +counts = counter-clockwise seen from the shaft end (user observed).
  - Open: whether the position fully settles after "done".
  - Started the real firmware, `firmware/servo_controller`. **Step 1**: WiFi (station) + mDNS `servo-control.local` + CAN polling (31/32/39/F1/3A/3E/37 fast; 40 and mode slow) + a status page at `/` and JSON at `/api/status`. Read-only. Compiles with no warnings (D-012: no extra libraries).
  - **Step 1 VERIFIED end to end:** the ESP32 joined the WiFi (192.168.1.139), `servo-control.local` resolves, and `/api/status` shows the motor online (~67 CAN replies/s, V1.0.1, mode 05, enabled). Had to enable IPv6 to remove a 5 s `.local` delay on macOS (network.md).
  - The user confirmed the step 1 page in the browser.
  - **Step 2a done:** the design/ UI ported to plain HTML/CSS/JS (`ui/index.html`, 22 KB → 6.7 KB gzipped via `tools/build_ui.py`), served at `/`. The `/ws` WebSocket pushes telemetry at 20 Hz (measured 20.3 Hz). Position is polled at 50 Hz and other values every ~140 ms (~100 CAN requests/s). Live: header status, readouts, 10 s trace, alarms, homed flag, device info, mode banner. **All controls are shown but disabled.** Gear ratio is fixed at 1 (config.h) until 2c.
  - Build: `arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=default" firmware/servo_controller`. Upload port: `/dev/cu.usbmodem5C831103731` (COM USB-C).
  - The user confirmed the 2a UI looks right (screenshot).
  - **Step 2b written and uploaded:** control.cpp (command queue, safety checks, jog deadman, E-STOP latch, zero tracking, heartbeat 89H) + WebSocket commands + a UI with working controls, toasts and confirmations (D-013). E-STOP/Release tested over the WebSocket on the motor at rest: latch works, the motor stays enabled.
  - **Step 2b VERIFIED by the user:** jog both ways, step, go-to, set zero, go-to-zero, smooth stop and E-STOP/release all work. F6 direction bit 0 = angle up. Not yet exercised: stall clear (needs a stall).
  - Next: 2c, the Settings tab. 2c-1: gear ratio + invert (ESP32 NVS), run current, stall protection, link-loss timeout (motor, saved with 60H), reload from motor. 2c-2 later: homing + limit switches (needs a switch to test).
  2. First milestone: send one CAN command and read back a response
  3. Port the UI and wire it to the firmware
- **Open questions:** CAN bus termination, PSU current limit, and the hardware checks listed in gui.md and protocol.md

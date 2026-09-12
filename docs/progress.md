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
  - **Step 2c-1 written and uploaded:** settings.cpp (NVS), units.h (all joint/motor conversions, incl. invert), Settings tab (joint: gear, invert; motor: current, stall protection/tolerance, link-loss timeout; dirty state, reload, save → writes 83/88/89, saves with 60H, reads back). Reading settings over the WebSocket VERIFIED (gear 1, current 1600, stall on/180°, timeout 1000 ms). D-014.
  - **Step 2c-1 VERIFIED by the user:** saving settings (writes 83/88/89 + 60H, NVS for gear/invert) works and persists across a page reload.
- **Stall test (user, 2026-09-11):** blocking the shaft made the motor **restart**, not trip stall protection. The position counter reset to ~0, and the stall flag was clear afterwards. **Cause VERIFIED:** the PSU hit its 1.5 A current limit (CC light on) and the voltage fell to ~8 V, so the driver browned out. Fix: raise the PSU limit to ~3 A (hardware.md). **Re-test at ~3 A: stall detection worked correctly** (user).
  - Added diagnostics, shown as toasts: motor stopped responding, motor restarted (position jump > 3000 counts between samples, i.e. faster than possible), stall detected, motor alarms (e.g. undervoltage). A restart clears "zeroed".
  - Added boot logging of the ESP32 settings. NVS works: keys are stored and loaded. A gear of 1.0 after a reboot means 1.0 was the last value saved (the user confirmed they had set it back to 1).
- **Upstream PR:** https://github.com/makerbase-motor/MKS-SERVO42ES-57ES/pull/2 (from the fork TRazDev/MKS-SERVO42ES-57ES, branch `add-esp32-can-example`). It adds a "Community projects" section to their README with a link to this repo and points to their issue #1 ("provide a working example code"). That repo had no PRs before and no activity since Dec 2025, so a reply may take a while.
- **Published:** blog post https://artsiom-seliuzhytski-dev.co.uk/controlling-an-mks-servo42es-over-can-from-an-esp32-with-a-web-ui/ and demo video https://youtu.be/Dbk16jcY2PQ . Both are linked from the README intro.
- **Blog post draft:** `~/Desktop/servo42es-blog-post/` (post.md, cover, a GIF under 2 MB for WordPress, wiring PNG, photos). Waiting on the user: the [ADD] spots, which cover, and whether to keep the optional photos.
- **Left to do:**
  1. README: **done** (status, features, build/flash steps, supply current note). The top image is now `docs/images/demo.gif`, made from the user's screen recording of the running UI (1400 px, 12 fps, 4.1 MB, via imageio-ffmpeg in the scratchpad venv). The old design screenshot `docs/images/ui.png` is no longer referenced. The rest below is parked at the user's request ("leave other stuff for later").
  2. Advanced tab actions: **done. Restart and calibration VERIFIED by the user; factory reset not tested** (the user was unsure about recovery; recovery path documented in protocol.md). Restart motor (41H), factory reset (3FH), encoder calibration (80H, calibration banner until the motor is power-cycled), fix mode (82 05 + 60H, also on the wrong-mode banner). A mode-write reply "82 00/01" is told apart from a mode read by a pending flag (control::expectingModeWriteReply). Expected restarts are reported as warnings, not errors.
  3. Homing + limit switches (2c-2): waiting until the user has a switch.
  4. Hardware checks: position settling after "done" (98H threshold); speed accuracy (commanded vs 32H readback). Stall detection: done.
  5. Optional: firmware updates over WiFi (OTA), useful once the ESP32 is built into the arm.
  6. Later, for the arm: 6 motors on the bus (IDs, J1–J6 selector), revisit the 600 RPM cap and the no-login decision (D-011).
  2. First milestone: send one CAN command and read back a response
  3. Port the UI and wire it to the firmware
- **Open questions:** CAN bus termination, PSU current limit, and the hardware checks listed in gui.md and protocol.md

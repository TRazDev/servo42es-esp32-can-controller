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
  - Started a git repo with a first commit. Added README.md with a UI screenshot (docs/images/ui.png). Created a private GitHub repo, TRazDev/servo42es-esp32-can-controller, and pushed.
- **Decisions so far:**
  - D-002: 24 V supply
  - D-003: 6-DOF arm, mode 05
  - D-004: GUI v1 scope
  - D-005: web page served by the ESP32
  - D-006: port the design to a dependency-free page
  - D-007: E-STOP holds position
  - D-008: join home WiFi only
  - D-009: Arduino IDE + ESP32 core 3.3.11
- **Next:**
  1. Discuss network details: credentials, how the browser finds the ESP32 (the user wants this as a separate topic)
  2. Wire it up and choose the CAN pins in hardware.md
  3. First milestone: send one CAN command and read back a response
  4. Port the UI and wire it to the firmware
- **Open questions:** CAN bus termination, PSU current limit, and the hardware checks listed in gui.md and protocol.md

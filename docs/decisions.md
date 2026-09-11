# Decision Log

Newest first. Each entry: what was decided, what else was considered, and why.
Don't delete old entries. If a decision changes, add a new entry that says it supersedes the old one.

---

## D-010 — Public repo under the MIT license (2026-09-11)
- **Decision:** the GitHub repo is public, under the MIT license (copyright "Traz"). The Makerbase manual and design/support.js stay in the repo.
- **Alternatives:** Apache-2.0, GPL-3.0; removing the third-party files first.
- **Why:** MIT is simple and lets anyone reuse the firmware and UI. The user is fine with including the manual and the design runtime.

## D-009 — Firmware toolchain: Arduino IDE + arduino-esp32 core (2026-09-11)
- **Decision:** write the firmware as an Arduino sketch. Build it with the Arduino IDE the user already has (ESP32 core 3.3.11).
- **Alternatives:** PlatformIO (what Claude suggested), ESP-IDF.
- **Why:** it's already installed. The core supports the ESP32-S3 and its TWAI (CAN) driver.
- **Consequences:**
  - The IDE includes `arduino-cli` (`/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`), so builds can also run from the terminal.
  - The sketch folder name must match the `.ino` name.
  - The web page is embedded in the firmware as a gzipped header file, so no filesystem-upload plugin is needed.

## D-008 — WiFi: join the home network only (2026-09-11)
- **Decision:** the ESP32 connects to the existing home WiFi. It doesn't create its own network.
- **Alternatives:** the ESP32's own network (AP mode), or home WiFi with an AP fallback.
- **Why:** the laptop keeps its internet connection, and there's no second network to manage.
- **Consequences:** network details (credentials, finding the device's address) are still **open**, to be discussed separately. The mockup footer's 192.168.4.1 (the usual address of an ESP32's own network) doesn't apply.

## D-007 — E-STOP stops and holds position (2026-09-11)
- **Decision:** E-STOP stops motion immediately and keeps the motor enabled (holding torque). It latches until released. Disable, which frees the shaft, stays a separate button.
- **Alternatives:** E-STOP also disables the motor (what the design mockup did).
- **Why:** on an arm joint under gravity, releasing the shaft would drop the arm.

## D-006 — Port the Claude Design mockup to a dependency-free page (2026-09-11)
- **Decision:** design/ holds the reference. The page the ESP32 serves will be a hand port of the same layout to plain HTML/CSS/JS, with no framework and no CDN, talking to the ESP32 over a WebSocket.
- **Alternatives:**
  - Serve the .dc.html as it is. It loads React from unpkg.com, so it fails with no internet (for example when the ESP32 runs its own WiFi network).
  - Bundle React locally. That's about 140 KB of runtime for a single page.
- **Why:** it meets the "no external resources, keep it light" constraint (D-005). The template is simple enough to port directly.
- **Consequences:** the visuals follow design/. The fixes from the design review in gui.md get applied during the port.

## D-005 — GUI is a web page served by the ESP32 over WiFi (2026-09-11)
- **Decision:** the ESP32-S3 hosts the GUI as a web page and pushes live data over a WebSocket.
- **Alternatives:** a desktop app on the Mac with the ESP32 as a USB↔CAN bridge. Claude recommended this for later 6-joint kinematics and link reliability.
- **Why:** the user's choice.
- **Consequences:**
  - Keep the page light, stored in flash, no CDN or web fonts.
  - WiFi can drop, so use the motor heartbeat (89H) as a safety net and keep the E-STOP handled on the ESP32.
  - Heavy arm maths (kinematics) may need to run somewhere else later.
- **UI design:** done in Claude Design using docs/design-brief.md. The result goes into design/.

## D-004 — GUI v1 scope (2026-09-11)
- **Decision:** v1 is single-motor control in mode 05. It has adjustable gear ratio, Set zero, limit-switch/homing settings, and the safety basics (enable, stop, E-STOP, status). Full scope is in gui.md.
- **Alternatives:** exposing every one of the 53 commands.
- **Why:** these are the features the user needs for a robot-arm joint. The rest is either pulse mode only or needs multiple motors.
- **Consequences:** the GUI shows joint degrees and the firmware converts to motor counts. The main move command is F5 (absolute encoder coordinates).

## D-003 — Application: 6-DOF robotic arm, bus closed-loop FOC (2026-09-11)
- **Decision:** the motor drives a geared arm joint in mode 05 (bus closed-loop FOC).
- **Alternatives:** pulse/direction modes (00–03), open-loop bus mode (04).
- **Why:** closed loop doesn't lose steps, and bus mode gives position readback and a single cable for all 6 joints.
- **Consequences:** firmware and GUI take a motor ID as a parameter from day one. Multi-motor commands (8B slave ID, 4A/4B sync) come later.

## D-002 — 24 V motor supply (2026-09-11)
- **Decision:** power the driver at 24 V from the lab PSU.
- **Alternatives:** anywhere in the 20–48 V range the driver accepts.
- **Why:** a common, safe voltage that's well inside the range.

## D-001 — Keep project knowledge in the repo as Markdown (2026-09-11)
- **Decision:** CLAUDE.md at the root (loaded automatically each session) plus a docs/ folder for details.
- **Alternatives:** keeping context only in chat sessions or in the AI's private memory.
- **Why:** it's versioned in git, readable by people and AI tools, and it stays with the code.

<!-- Template:
## D-00X — Title (YYYY-MM-DD)
- **Decision:**
- **Alternatives:**
- **Why:**
- **Consequences / follow-ups:**
-->

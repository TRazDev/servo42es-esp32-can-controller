# Design Brief: Servo Joint Controller (web UI)

This document is self-contained. It's written for a designer or design tool that has no other project context.

## What we're building
A browser-based control panel for one **joint of a 6-DOF robotic arm**. Each joint is driven by an MKS SERVO42ES closed-loop stepper motor, sitting behind a gearbox.
- An **ESP32-S3** microcontroller hosts the page over **WiFi**. You open it from a laptop or phone browser on the same network.
- The ESP32 talks to the motor over a CAN bus.
- v1 controls **one motor**. The layout must later scale to **6 joints** (e.g. a joint selector or an overview of all 6).

User: a single maker/engineer building the arm on a lab bench. It's a technical tool, so precise numbers and clear state matter more than decoration.

## Technical constraints
- The page is stored in the ESP32's flash and served from there. Keep it **light**: plain HTML/CSS/JS or a very small framework, no large images.
- **No external resources** (no CDN, no web fonts): the ESP32 may run its own WiFi network with no internet access. Use system fonts and inline SVG icons.
- Live data arrives over a WebSocket at about 10–20 updates per second.
- It must work on a **laptop (primary)** and a **phone (secondary)**. It should use the full width on desktop and stay usable on mobile.
- Dark theme preferred (bench/workshop tool). Light theme optional.

## Units
The UI always shows **joint** values, meaning after the gearbox. The firmware converts to motor units.
- Angle: degrees (°), 2 decimal places
- Speed: °/s
- Acceleration: °/s²
- Gear ratio R: a number like 50 (motor turns per joint turn)
- Current: mA

## Screens / areas

### 1. Always-visible header / status bar
- **E-STOP button**: always visible, large, red, reachable with one tap from anywhere
- Connection: WiFi link to the ESP32 (connected / reconnecting / lost), CAN link to the motor (OK / no response)
- Selected joint (v1 shows "Joint 1"; later a selector for 1–6)
- Motor state summary: **Enabled / Disabled**, motion state (Stopped, Accelerating, Cruising, Decelerating, Homing)
- Fault badge: shown when there's an alarm or stall (see States)

### 2. Main control screen
Live readouts, large and easy to scan:
- Joint angle (°)
- Joint speed (°/s)
- Position error: how far the motor lags behind the commanded position (°), a small value normally
- Live chart: angle and error over the last ~10 s

Controls:
- **Enable / Disable** toggle (Disable lets the shaft turn freely)
- **Jog**: hold-to-move − / + buttons, with a jog speed setting
- **Step**: buttons that move −/+ by a selectable step (0.1° / 1° / 10° / custom)
- **Go to angle**: number input + Go button, with speed and acceleration fields (defaults filled in)
- **Stop** (smooth stop) next to motion controls, separate from E-STOP
- **Set zero**: "the joint is at zero right now." Needs a light confirmation.
- **Home**: runs the automatic homing routine. Disabled or greyed out until homing is configured (no limit switch yet).
- **Go to zero**: move back to the zero position
- **Clear stall**: shown only when the motor is stalled
- Indicators: "Zero set" / "Homed" / "Not zeroed" (after power-up the zero is lost until you set it or run homing)
- Limit switch indicators: Home/Left switch, Right switch (active / inactive)

### 3. Settings screen
Settings are edited locally, then saved to the motor. Needs an **"Unsaved changes"** indicator, plus **Save to motor** and **Reload from motor** buttons.
- **Joint:**
  - Gear ratio R
  - Invert direction (on/off)
- **Motor:**
  - Run current: 0–3000 mA, default 1600
  - Stall protection: on/off, plus tolerance in degrees
  - Connection-loss timeout: 0 = off; otherwise the motor stops if it hears nothing for this many ms
- **Homing:**
  - Method: Limit switch / Hard stop (drives gently into a mechanical end stop) / Single-turn / Disabled
  - Direction: forward / reverse / nearest
  - Fast speed, slow speed
  - Homing current (for the hard-stop method)
  - Offset after homing
  - Timeout
  - "Home automatically at power-up" (on/off)
- **Limit switches:**
  - Enable
  - Active level: switch closed = low / high
  - Use a second (right) limit switch (on/off)

### 4. Advanced / maintenance screen
Every action here needs an explicit confirmation dialog explaining the consequence.
- **Control mode**: must be "Bus closed-loop FOC". Show a clear warning banner if the motor reports any other mode, with a one-click fix.
- **Encoder calibration**:
  - Warning: the motor must have NO load and will spin 20+ turns at 600 RPM. Only do this before mounting it in the arm.
  - The motor restarts; the result shows up about 10 s later.
- **Factory reset**: warning that all settings are lost and the motor restarts.
- **Restart motor**
- Device info: firmware version, hardware version
- (Later) CAN bus settings: motor ID, bitrate

## States to design for
- **Disconnected** (WiFi lost or motor not responding): readouts show clearly stale values; controls disabled.
- **Motor disabled**: motion controls disabled; Enable is highlighted.
- **Moving**: show the target vs. current angle, and progress if possible.
- **Move finished / stopped by limit switch**: brief notification.
- **Stalled**: prominent alert with a Clear stall action.
- **Alarms** (from the motor): Overcurrent, Phase loss, Overvoltage, Undervoltage, Position error, Encoder error.
- **Not zeroed**: warn before absolute moves ("Go to angle" means nothing until zero is set).
- **Wrong control mode**: blocking banner with a fix.
- **Unsaved settings**
- **E-STOP pressed**: clear "stopped" state; the user re-enables deliberately.

## Out of scope for v1
Coordinated 6-joint motion, trajectories and inverse kinematics, a 3D arm view. Leave room in the navigation for a future "Arm" screen.

## Deliverable
HTML/CSS/JS (or exported code) for the screens above. It will be put into the project's `design/` folder and wired up to the ESP32 firmware.

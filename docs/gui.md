# GUI Spec

Target: one joint of a 6-DOF robotic arm, driven by a SERVO42ES in **bus closed-loop FOC mode (05)**.
v1 controls one motor. Design everything so it can take a motor ID, since the arm will have 6 motors on one CAN bus.

## v1 scope
User requirements (2026-09-11):
1. **FOC closed-loop control** over CAN (mode 05)
2. **Adjustable reduction ratio**: the GUI works in joint degrees; the firmware converts to motor units
3. **Set zero**: tell the motor "this is the zero position"
4. **Limit switch / homing**: the switch isn't bought yet, so build the settings UI now and test it later

Basics the GUI needs in order to work at all:
- Enable/disable, Stop, **E-STOP**
- Live readouts: joint angle, speed, position error, alarm/stall status
- Clear stall
- Settings: run current, direction, stall protection, heartbeat; read back from the motor; save to flash
- Advanced, behind a confirmation: encoder calibration (80H), factory reset (3FH)

Not in v1: multi-motor sync, trajectories, kinematics.

## Reduction ratio math
`R` = gear ratio (motor turns per joint turn), set in the GUI and stored on the ESP32.
- Motor encoder: 16384 counts per motor turn (31H, F4, F5)
- `joint_deg = counts × 360 / (16384 × R)`
- `counts = joint_deg × 16384 × R / 360`
- `motor_RPM = joint_deg_per_s × R / 6` (maximum motor speed is 3000 RPM)
- A move can target at most ±8,388,607 counts, which is ±512 motor turns or **±512/R joint turns**. For example, at R=50 that's ±10 joint turns, so it's fine for an arm.
- Use **F5 (absolute coordinate move)** as the main move command. It uses encoder units, so it doesn't depend on microstepping. You can also change its target while it's moving, which will be useful for streaming arm motion later.

## Direction
With the motor defaults, a positive move turns the shaft counter-clockwise, seen from the shaft end (VERIFIED). The GUI's "Invert direction" setting flips the sign of joint angles. Whether a joint needs it depends on how the motor is mounted in the arm.

## Zeroing
Two separate things, and the GUI should keep them apart:
- **Set zero (92H):** "the joint is at zero right now." This is what the user asked for. It's a main-screen button.
- **Encoder calibration (80H):** a one-time correction for how the encoder magnet is mounted. The motor is factory-calibrated. It needs **no load** (it spins 20+ turns at 600 RPM), so do it only before mounting the motor in the arm. Advanced only.

Plan:
- **Now (no switch):** after power-up, jog the joint to its reference pose and press Set zero.
- **Later (with a switch):** switch homing (95H mode 00) gives a repeatable zero automatically, and can run at power-up (97H trig 01).
- **Option with no extra hardware:** hard-stop homing (95H mode 01). The motor drives into a mechanical stop at a low current (96H). Only works if the joint has a solid end stop.

## Design review (2026-09-11), fixes to make in the port
The design in design/ covers the whole brief: all 4 tabs, the E-STOP latch, confirmation dialogs, stale/offline states, a J1–J6 selector and an "unsaved changes" indicator. Things to change:
1. **E-STOP behaviour:** the mock disables the motor (shaft goes free). **Decided (D-007):** stop and hold position, latched until released. Don't disable.
2. **Jog relies on the button-release event.** If WiFi drops while you're holding jog, the release never arrives and the joint keeps moving. Firmware needs a deadman:
   - While jog is held, the page sends keepalives every ~100 ms. The ESP32 stops the jog if none arrive for ~300 ms.
   - The ESP32 also stops the jog when the WebSocket closes.
   - The motor heartbeat (89H) covers the case where the ESP32 itself hangs.
   - Also handle `touchcancel`, and don't let mouse and touch events both fire on phones.
3. **Factory reset text is wrong.** It says the gear ratio is erased. The gear ratio lives on the ESP32; 3FH resets the motor only. It should instead warn that the mode goes back to pulse (03) and the CAN ID/bitrate go back to defaults.
4. **Limits should come from the gear ratio.** Jog max is fixed at 120 °/s; it should be 3000 RPM × 6 / R. Go speed isn't validated. Homing slow speed has a motor limit of 100 RPM = 600/R °/s.
5. **Step presets** are 0.1/1/10/45°. The brief also asked for a custom value.
6. **Home button is always disabled.** Enable it once a homing method is configured.
7. **Firmware/hardware version** is hardcoded. Read it from 40H. Motor ID and bitrate come from ESP32 config.
8. **Theme is light.** The brief said dark was preferred; the design chose light. Keep it unless the user says otherwise.

## Open questions (test on hardware)
- Does the 92H zero survive a power cycle? The manual suggests that without homing, the power-on position becomes zero. Observed 2026-09-11: after a motor power cycle, 31H read 0 (it was 5 before), so the count restarts near zero at every power-up. Not yet tested with 92H.
- On the 42ES, can the single IN port be both the homing switch and the left limit?
- Does the §8.1 microstep speed scaling also apply to F4/F5 speeds?

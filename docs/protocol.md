# MKS SERVO42ES CAN Protocol Notes

Source: `docs/references/MKS-SERVO42-57ES_CAN_User_Manual_V1.0.1.pdf` (manual V1.0.1, firmware V1.0.1).
Everything below comes from the manual and is **UNVERIFIED** unless marked VERIFIED (tested on the real motor).

## Frame format
- CAN 2.0 **standard frames** (11-bit ID), up to 8 data bytes.
- Bitrate: 125K / 250K / **500K (default)** / 1M.
- CAN ID = the motor's slave ID: 1–0x7FF, **default 01**. ID 00 is broadcast; group IDs are set with 8DH.
  - Broadcast and group commands get **no response**.
  - IDs 1–10 can be set from the driver's menu. Higher IDs have to be set by command (8BH).
- Data layout, both directions: `[code] [payload...] [CRC]`. DLC counts all of these bytes.
- Responses come back on the **same CAN ID** as the motor's ID.
- Multi-byte values are **big-endian**.
- **CRC = (CAN_ID + code + all payload bytes) & 0xFF**. This applies to both directions.
  - Example: read position `ID=01: 31 32` → reply `ID=01: 31 00 00 00 00 00 03 35`.
  - Open question: the manual doesn't say how IDs above 0xFF enter the sum. Probably the low byte only.
- **Direction + speed encoding** (F6, FD, FF): byte A = `dir<<7 | (speed>>8 & 0x0F)`, byte B = `speed & 0xFF`. Speed is 0–3000.
- **Speed encoding** (F4, F5, FE): uint16 speed, no direction bit. Direction comes from the sign of the target.
- **acc** (0–255): speed changes by 1 RPM every `(256-acc) × 50 µs`. acc=0 means no ramp (instant).

## VERIFIED on hardware (2026-09-11, firmware/can_test)
Setup: ESP32-S3 TWAI on GPIO 4/5, 500 kbit/s, one 120 Ω terminator (on the module), motor ID 01, motor still in factory mode 03.
- Standard 11-bit frames at **500 kbit/s** and **motor ID 01** work as documented. The bus ran with 0 TX/RX errors.
- **CRC = (ID + bytes) & 0xFF** is correct in both directions (ID 01).
- Replies come back on the **same CAN ID** (0x001).
- **40H** reply `40 92 01 00 01 D5` = E series (b7=1), cal 1 (b5-4), **hardware 2 = S42ES_BUS**, **firmware V1.0.1**. It matches this manual's version.
- **31H** reply `31 00 00 00 00 00 03 35` = 3 counts, byte for byte the same as the manual's example. The value stayed at 3 while the motor was holding.
- **00H** read-parameter works: `00 82` → `82 05` = the motor was **already in mode 05** (bus closed-loop FOC) out of the box, before anything was written. The manual says the factory default is 03, but this CAN unit shipped in 05.

## VERIFIED first motion (2026-09-11, firmware/move_test)
- **F3 01** (enable) → `F3 01` (status 1).
- **F4** relative move `F4 00 3C 02 00 10 00` (60 RPM, acc 2, +4096 counts) → `F4 01` (started), then `F4 02` (done). With the default response mode 02, both replies arrive.
- Positive F4 counts → encoder count increases. Start 3, then **4082 after +4096** (0.37° short), then **5 after −4096**.
- **Direction:** positive F4 counts turn the shaft **counter-clockwise, seen from the shaft end** (observed by the user), and the encoder count (31H) increases. This matches 40H "cal 1" (encoder decreases when turning CW) and 32H (CCW speed > 0). Setting 86H (direction) was left at its default.
- Open: the "done" reply probably comes once within the 98H position-reached threshold (default 800/65535 × 360° ≈ 4.4°). The position was read right after "done" and may not have settled yet. Check by reading again ~0.5 s later.

## Units
| Quantity | Unit |
|---|---|
| Encoder position (30H/31H), coordinates (F4/F5) | 16384 counts / revolution |
| Position error (39H) | 51200 counts / revolution |
| Stall tolerance (88H) | 0x64 = 180°, 0xC8 = 360°, i.e. 1 count = 1.8° |
| Position-reached threshold (98H) | 65535 = 360° |
| Pulses (33H, FD, FE) | depends on microstepping (16 µsteps → 3200 / rev) |
| Speed (32H) | RPM, int16; CCW > 0, CW < 0 |

## Required setup for CAN control
The manual says the factory default mode is 03 (pulse + direction), but our CAN unit arrived already in 05 (VERIFIED). The motion commands below only work in bus mode.
1. `82 05`: set bus closed-loop FOC mode
2. `60 01`: save to flash
3. `F3 01`: enable the motor (in bus mode, enable is set by command and the En pin is ignored)

Write commands (82H–9FH, FF, 4A) are **not persistent** until `60 01` (save) is sent.

---

## Command reference

GUI relevance ratings:
- **Core**: needed on the main control/monitoring screen.
- **Settings**: goes on a configuration page.
- **Advanced**: hide it or put it behind a confirmation.
- **Later**: needed once the arm has several motors.
- **Skip**: not needed.

### Monitoring (read-only)
| Code | Name | Request payload | Response payload | GUI |
|---|---|---|---|---|
| 31H | Read cumulative encoder (position) | — | int48 position (16384/rev) | **Core**: main position readout; same units as F4/F5 |
| 32H | Read real-time speed | — | int16 RPM | **Core**: live speed readout/graph |
| 39H | Read position error | — | int32 (51200/rev) | **Core**: tracking error graph, early warning of stall |
| 37H | Read alarm status | — | uint8: 0 running, 1 stopped, 2 overcurrent, 3 phase loss, 4 overvoltage, 5 undervoltage, 6 position error, 7 encoder error | **Core**: status/fault indicator |
| F1H | Read run status (bus mode only) | — | uint8: 0 query failed, 1 stopped, 2 accelerating, 3 decelerating, 4 full speed, 5 homing | **Core**: motion state indicator |
| 3AH | Read enable status | — | 0 disabled / 1 enabled | **Core**: enable toggle state |
| 3EH | Read stall status | — | 0 no / 1 stalled | **Core**: stall indicator |
| 3BH | Read homing status | — | 0 not homed or failed / 1 homed | **Core**: "homed" badge; gates absolute moves |
| 34H | Read IO ports | — | bits: b0 IN_1, b1 IN_2 (on 42ES: En pin), b2 PEND (1 = in position), b3 ALM (1 = no alarm) | **Settings**: limit-switch/IO indicators |
| 40H | Read version | — | b7 series (1=E), b5-4 cal direction, b3-0 hw version (2 = S42ES_BUS), then firmVer[3] | **Core**: shown on connect; confirms it's the right device |
| 30H | Read encoder as carry + value | — | int32 carry, uint16 value (0–0x3FFF) | **Skip**: 31H gives the same info as one number |
| 33H | Read received pulse count | — | int32 | **Settings**: position in pulse units (the FE example reads it) |
| 00H | Read a config parameter | 1 byte: the parameter's code (e.g. 82) | code + value in the same format as the write; FFFF if not readable | **Core (under the hood)**: fills the settings page from the motor |
| 01H | Auto-report a read-only value periodically | code (1) + interval ms (uint16); 0 = off | ack `01 code status`, then periodic `code value` frames | **Core (under the hood)**: telemetry without polling |
| 42H | Read / write 32-bit user ID | write: uint32; read: — | write: status; read: uint32 | **Skip**: could label motors, but not needed |

### Motion (bus mode only)
| Code | Name | Request payload | Response | GUI |
|---|---|---|---|---|
| F3H | Enable / disable motor | 1 byte: 00 release shaft / 01 lock | status | **Core**: Enable toggle |
| F6H | Speed mode (run continuously) | dir+speed (2), acc (1); optional runTime uint24 in 10 ms units | 0 fail, 1 started, 2 done (timed run), 5 queued for sync | **Core**: jog / continuous-speed control |
| F6H | Speed-mode stop | `00 00 acc`; acc=0 stops instantly | 0 fail, 1 stopping, 2 stopped | **Core**: Stop button |
| FDH | Relative move, pulses | dir+speed (2), acc (1), pulses uint24 | 0 fail, 1 started, 2 done, 3 hit limit, 5 queued for sync | **Core**: step N pulses (units depend on microstepping) |
| FEH | Absolute move, pulses | speed uint16, acc (1), target int24 | same as FD | **Settings**: absolute move in pulse units |
| F4H | Relative move, encoder coordinates | speed uint16, acc (1), delta int24 | same as FD | **Core**: "move ±X degrees" (16384 = 360°) |
| F5H | Absolute move, encoder coordinates | speed uint16, acc (1), target int24 | same as FD | **Core**: "go to X degrees"; can update speed/target mid-move |
| FD/FE/F4/F5 | Stop | same frame with speed=0, target=0, acc = ramp (0 = instant) | 0 fail, 1 stopping, 2 stopped, 3 hit limit | **Core**: Stop button for position moves |
| F7H | Emergency stop | — | 0 fail / 1 ok | **Core**: E-STOP button. The manual advises against it above 1000 RPM. |
| 3DH | Release stall | — | 0 fail / 1 ok | **Core**: "Clear stall" button |
| 92H | Set current position as zero | `00` | status | **Core**: "Set zero" button |
| 91H | Run homing | `00` home using the configured method / `01` go to coordinate zero | 0 fail, 1 started, 2 done, 3 timeout | **Core**: "Home" and "Go to zero" buttons |
| 4BH | Sync-execute (send on broadcast ID 00) | — | none | **Later**: starts all joints moving at once |

### Configuration (send 60H afterwards to make it persistent)
| Code | Name | Payload | Values / defaults | GUI |
|---|---|---|---|---|
| 82H | Working mode | mode (1) | 00/01/02/03 pulse modes (03 = default); **04 bus open-loop, 05 bus closed-loop**; 10/11/14 = no-encoder variants | **Core**: must be 05 for CAN control; show a warning if it isn't |
| 83H | Run current | uint16 mA | 42ES: 0–3000, default 1600 | **Settings** |
| 84H | Microstepping | uint8 | 0 (=256), 2, 4, 8, 16 (default), 32, 64, 128, 5, 10, 20, 25, 40, 50, 100, 200 | **Settings**: pulse moves (FD/FE) and the speed scale depend on it |
| 86H | Direction | uint8 | 00 CW (default) / 01 CCW; also affects bus motion | **Settings** |
| 88H | Stall protection | enable (1), tolerance uint16 | enabled, 0x64 (180°) | **Settings** |
| 89H | Heartbeat timeout | uint32 ms | 0 = off (default); motor stops if no command arrives within this time | **Settings**: safety net if the connection drops |
| 8CH | Response mode | uint8 | 00 none, 01 immediate ack only, **02 ack + completion message (default)** | **Advanced**: GUI should keep 02 |
| 8FH | Lock shaft on bus-mode start | uint8 | 01 locked (default) | **Settings** |
| 98H | Position-reached threshold | enable (1), uint16 | enabled, 800 | **Settings** |
| 95H | Homing mode/direction/speeds | mode, dir, hiSpeed uint16, loSpeed uint16 | mode 00 switch (default), 01 hard stop, 02 single-turn, 03 disabled; dir 00 fwd, 01 rev, 02 nearest (single-turn only); hi 1–3000 (100), lo 1–100 (10) | **Settings**: homing page |
| 96H | Homing current + origin offset | uint16 mA, int32 offset | 42ES default 300 mA, offset 0x2000 | **Settings**: homing page |
| 97H | Homing trigger + timeout | trig (1), uint32 ms | trig 00 command (default), 01 at power-on (bus mode), 02 En pulse (pulse mode); timeout 120000 | **Settings**: homing page |
| 9EH | Limit switches | enable, level, remap (FF = leave unchanged) | disabled, active-low, no remap. On the 42ES the right limit needs remap=01 (uses the En pin). You must home once after changing. | **Settings** |
| FFH | Auto-run at power-on | dir+speed (2), acc (1) | speed 0 = off (default) | **Advanced**: motor spins on power-up, so it's risky |
| 8AH | CAN bitrate | uint8 | 00 125K, 01 250K, 02 500K (default), 03 1M | **Advanced**: ESP32 must switch too, or you lose contact |
| 8BH | Slave ID | uint16 | 1–0x7FF, default 01 | **Later**: each of the 6 joints needs its own ID. IDs 1–6 can also be set from the driver menu. The ESP32 must switch IDs too. |
| 8DH | Group ID | uint16 | — | **Later**: optional, e.g. to group joints |
| 4AH | Sync-flag mode | uint8 | 00 off (default) / 01 on (motion waits for 4BH) | **Later**: coordinated arm moves. If it's on by accident, moves never start. |
| 85H | En pin active level | uint8 | 00 low (default), 01 high, 02 always on; pulse mode only | **Skip**: not used in bus mode |
| 87H | Pulse delay | uint8 | 0 / 4 / 20 (default) / 40 ms; pulse mode only | **Skip** |
| 9FH | PEND pin as pulse-divider output | level (1), uint32 period | period < 100 = off | **Skip** |
| 36H | Write ALM/PEND outputs | bitfield (masks + values) | exact bit layout is unclear in the manual | **Skip** |
| 0FH | Skip CRC checking (test only, not saved) | `01` | — | **Skip** |

### System
| Code | Name | Payload | Response | GUI |
|---|---|---|---|---|
| 60H | Save all parameters to flash | `01` | status | **Core**: "Save to motor" button; show when settings aren't saved yet |
| 41H | Restart the motor (keeps settings) | — | status | **Settings** |
| 3FH | Factory reset (then restarts) | — | status | **Advanced**: needs confirmation. It resets the mode to 03 and the bitrate/ID to defaults. |
| 80H | Calibrate encoder | `01` | none; restarts, ~10 s, LED shows the result | **Advanced**: motor must have no load and spins 20+ turns at 600 RPM. Needs confirmation. |

---

## Errors and ambiguities in manual V1.0.1
- **Direction is inconsistent.**
  - 30H/31H say CW adds +0x4000 per turn, but the examples show CCW adding it.
  - The dir field in F6/FD is labelled "0–1 (CCW/CW)" and also "forward/reverse".
  - 86H says 00 = CW.
  - We'll work out the real meaning on hardware.
- **Speed scaling note (§8.1) doesn't add up.** It says speed is exact at 16/32/64 µsteps, ×2 at 8, and ÷8 at 128. Needs testing.
- **Sync example has a wrong CRC:** `00 60 01 62` should be `00 60 01 61`.
- **FE stop example (§10.2.3) has an extra byte.** The correct frame is `01 FE 00 00 02 00 00 00 01`.
- **8CH example frame (§4.3.9) has 10 bytes.** That's impossible in classic CAN; it's a typo.
- **§4.2.9 and §4.2.10 label the replies "Downlink".** They are actually uplink (motor → host).
- **Absolute and relative coordinate moves (F4/F5/FE) take an int24 target.** That's ±8,388,607 counts, or about ±512 turns from zero. The position readout (31H) is int48, so it can go past what a single move can target.

## Gotchas found in testing
- (none yet)

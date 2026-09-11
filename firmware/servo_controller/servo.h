#pragma once
#include <stdint.h>

// Latest known state of the motor, filled by polling over CAN.
struct ServoState {
  bool online = false;        // a reply arrived within MOTOR_TIMEOUT_MS
  uint32_t rxFrames = 0;
  bool versionKnown = false;
  uint8_t hardware = 0;       // 40H: 2 = S42ES_BUS
  uint8_t firmware[3] = {0, 0, 0};
  int mode = -1;              // 82H working mode, -1 = unknown (05 = bus closed-loop)
  bool enabled = false;       // 3AH
  int64_t positionCounts = 0; // 31H, 16384 per motor turn
  int16_t speedRpm = 0;       // 32H, CCW > 0
  int32_t errorCounts = 0;    // 39H, 51200 per motor turn
  uint8_t runStatus = 0;      // F1H: 1 stopped, 2 accel, 3 decel, 4 full speed, 5 homing
  uint8_t alarm = 0;          // 37H: 0 running, 1 stopped, 2+ = fault
  bool stalled = false;       // 3EH
};

namespace servo {

void begin();
// Call from loop(): sends the next poll request and parses replies.
void update();
// Thread-safe copy for the web server task.
ServoState snapshot();

}  // namespace servo

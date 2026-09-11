#include "servo.h"
#include <Arduino.h>
#include "can_bus.h"
#include "config.h"
#include "control.h"

namespace servo {
namespace {

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
ServoState state;
uint32_t lastReplyMs = 0;
bool havePosition = false;

// Fast telemetry, one request every POLL_STEP_MS in turn. Position (31H) is
// every other slot (50 Hz) for a smooth trace; the rest refresh every ~140 ms.
const uint8_t FAST_POLL[] = {0x31, 0x32, 0x31, 0x39, 0x31, 0xF1, 0x31, 0x3A,
                             0x31, 0x3E, 0x31, 0x37, 0x31, 0x3B};
constexpr uint32_t POLL_STEP_MS = 10;
constexpr uint32_t SLOW_POLL_MS = 2000;  // version and working mode
uint8_t pollIndex = 0;
uint32_t lastPollMs = 0;
uint32_t lastSlowPollMs = 0;

int64_t readInt(const uint8_t *d, uint8_t bytes) {
  int64_t v = 0;
  for (uint8_t i = 0; i < bytes; i++) v = (v << 8) | d[i];
  const int64_t sign = 1LL << (bytes * 8 - 1);
  if (v & sign) v -= sign << 1;
  return v;
}

void parse(const twai_message_t &msg) {
  if (msg.identifier != MOTOR_ID) return;
  const uint8_t *d = msg.data;
  const uint8_t len = msg.data_length_code;
  // "82 xx" answers both "read mode" and "set mode". While a mode write is pending,
  // 00/01 is its status, not a mode.
  const bool modeWriteReply = d[0] == 0x82 && len == 3 && d[1] <= 1 && control::expectingModeWriteReply();

  portENTER_CRITICAL(&mux);
  state.rxFrames++;
  switch (d[0]) {
    case 0x31:
      if (len == 8) {
        const int64_t p = readInt(d + 1, 6);
        // Faster than the motor can turn: the counter was reset (motor restart or set zero).
        if (havePosition && llabs(p - state.positionCounts) > POSITION_JUMP_COUNTS) state.positionJumps++;
        state.positionCounts = p;
        havePosition = true;
      }
      break;
    case 0x32: if (len == 4) state.speedRpm = (int16_t)readInt(d + 1, 2); break;
    case 0x39: if (len == 6) state.errorCounts = (int32_t)readInt(d + 1, 4); break;
    case 0xF1: if (len == 3) state.runStatus = d[1]; break;
    case 0x3A: if (len == 3) state.enabled = d[1] == 1; break;
    case 0x3E: if (len == 3) state.stalled = d[1] == 1; break;
    case 0x37: if (len == 3) state.alarm = d[1]; break;
    case 0x3B: if (len == 3) state.homed = d[1] == 1; break;
    case 0x82: if (len == 3 && !modeWriteReply) state.mode = d[1]; break;  // reply to "00 82" (read mode)
    case 0x40:
      if (len == 6) {
        state.hardware = d[1] & 0x0F;
        state.firmware[0] = d[2]; state.firmware[1] = d[3]; state.firmware[2] = d[4];
        state.versionKnown = true;
      }
      break;
  }
  portEXIT_CRITICAL(&mux);
  lastReplyMs = millis();

  switch (d[0]) {  // replies to control and settings commands
    case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF7: case 0x92: case 0x3D:
    case 0x83: case 0x88: case 0x89: case 0x60: case 0x41: case 0x3F:
      control::onReply(msg);
      break;
    case 0x82:
      if (modeWriteReply) control::onReply(msg);
      break;
  }
}

}  // namespace

void begin() {}

void update() {
  twai_message_t msg;
  while (can_bus::receive(msg)) parse(msg);

  const uint32_t now = millis();
  if (now - lastSlowPollMs >= SLOW_POLL_MS) {
    lastSlowPollMs = now;
    const uint8_t readMode = 0x82;
    can_bus::send(MOTOR_ID, 0x40);
    can_bus::send(MOTOR_ID, 0x00, &readMode, 1);
    can_bus::maintain();
  }
  if (now - lastPollMs >= POLL_STEP_MS) {
    lastPollMs = now;
    can_bus::send(MOTOR_ID, FAST_POLL[pollIndex]);
    pollIndex = (pollIndex + 1) % sizeof(FAST_POLL);
  }

  const bool online = lastReplyMs != 0 && now - lastReplyMs < MOTOR_TIMEOUT_MS;
  portENTER_CRITICAL(&mux);
  const bool wasOnline = state.online;
  state.online = online;
  portEXIT_CRITICAL(&mux);
  if (online && !wasOnline) control::onMotorOnline();
}

ServoState snapshot() {
  portENTER_CRITICAL(&mux);
  ServoState copy = state;
  portEXIT_CRITICAL(&mux);
  return copy;
}

}  // namespace servo

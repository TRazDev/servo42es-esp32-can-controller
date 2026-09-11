#include "control.h"
#include <Arduino.h>
#include <math.h>
#include "can_bus.h"
#include "config.h"
#include "servo.h"
#include "web.h"

namespace control {
namespace {

QueueHandle_t queue = nullptr;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Status st;

enum class Motion : uint8_t { None, Speed, Relative, Absolute };
Motion lastMotion = Motion::None;
bool stopping = false;
bool estopWhileMoving = false;  // F7 on a motor at rest replies "failed"; only report it when moving
int8_t jogDir = 0;
uint16_t jogRpm = 0;
uint32_t lastJogMs = 0;

// ---- unit conversion (joint units -> motor units) ----
int32_t toCounts(float deg) { return lroundf(deg * 16384.0f * GEAR_RATIO / 360.0f); }
float toDeg(int64_t counts) { return counts * 360.0f / 16384.0f / GEAR_RATIO; }

uint16_t toRpm(float degPerS) {
  long rpm = lroundf(fabsf(degPerS) * GEAR_RATIO / 6.0f);
  return (uint16_t)constrain(rpm, 1L, (long)MAX_MOTOR_RPM);
}

// The motor ramps by 1 RPM every (256 - acc) x 50 us, i.e. 20000 / (256 - acc) RPM/s.
// acc = 1 is the gentlest ramp (~78 RPM/s); 0 would mean no ramp, which we never send.
uint8_t toAcc(float degPerS2) {
  const float rpmPerS = fabsf(degPerS2) * GEAR_RATIO / 6.0f;
  if (rpmPerS <= 0) return 1;
  long acc = lroundf(256.0f - 20000.0f / rpmPerS);
  return (uint8_t)constrain(acc, 1L, 255L);
}

void setStatus(void (*fn)(Status &)) {
  portENTER_CRITICAL(&mux);
  fn(st);
  portEXIT_CRITICAL(&mux);
}

void event(const char *text, const char *level) { web::publishEvent(text, level); }

// Returns nullptr if motion is allowed, else the reason.
const char *blockedReason() {
  const ServoState s = servo::snapshot();
  if (st.estop) return "BLOCKED / RELEASE E-STOP FIRST";
  if (!s.online) return "BLOCKED / MOTOR NOT RESPONDING";
  if (s.mode != 5) return "BLOCKED / MOTOR NOT IN BUS CLOSED-LOOP MODE";
  if (s.stalled) return "BLOCKED / CLEAR THE STALL FIRST";
  if (!s.enabled) return "BLOCKED / ENABLE THE MOTOR FIRST";
  return nullptr;
}

// ---- motor commands ----
void sendSpeed(int8_t dir, uint16_t rpm, uint8_t acc) {
  // F6 byte 2: bit 7 = direction, bits 3-0 = speed high nibble.
  const uint8_t dirBit = dir > 0 ? F6_DIR_BIT_POSITIVE : !F6_DIR_BIT_POSITIVE;
  const uint8_t p[3] = {uint8_t((dirBit << 7) | ((rpm >> 8) & 0x0F)), uint8_t(rpm), acc};
  can_bus::send(MOTOR_ID, 0xF6, p, 3);
}

void sendPosition(uint8_t code, int32_t counts, uint16_t rpm, uint8_t acc) {  // F4 or F5
  const uint8_t p[6] = {uint8_t(rpm >> 8), uint8_t(rpm), acc,
                        uint8_t(counts >> 16), uint8_t(counts >> 8), uint8_t(counts)};
  can_bus::send(MOTOR_ID, code, p, 6);
}

void smoothStop() {
  switch (lastMotion) {
    case Motion::Relative: sendPosition(0xF4, 0, 0, STOP_ACC); break;
    case Motion::Absolute: sendPosition(0xF5, 0, 0, STOP_ACC); break;
    default: {
      const uint8_t p[3] = {0, 0, STOP_ACC};
      can_bus::send(MOTOR_ID, 0xF6, p, 3);
    }
  }
  stopping = true;
  jogDir = 0;
  setStatus([](Status &s) { s.jogging = false; s.hasTarget = false; });
}

void startMove(uint8_t code, float targetDeg, int32_t counts, float speed, float accel) {
  if (counts > 8388607 || counts < -8388607) {
    event("REFUSED / TARGET OUTSIDE THE MOTOR'S +/-512 TURN RANGE", "err");
    return;
  }
  sendPosition(code, counts, toRpm(speed), toAcc(accel));
  lastMotion = code == 0xF4 ? Motion::Relative : Motion::Absolute;
  stopping = false;
  portENTER_CRITICAL(&mux);
  st.hasTarget = true;
  st.targetDeg = targetDeg;
  portEXIT_CRITICAL(&mux);
}

void execute(const Command &c) {
  const char *why = nullptr;
  switch (c.type) {
    case Cmd::Enable: {
      const uint8_t on = 1;
      can_bus::send(MOTOR_ID, 0xF3, &on, 1);
      break;
    }
    case Cmd::Disable: {
      const uint8_t off = 0;
      jogDir = 0;
      setStatus([](Status &s) { s.jogging = false; s.hasTarget = false; });
      can_bus::send(MOTOR_ID, 0xF3, &off, 1);
      break;
    }
    case Cmd::Stop:
      smoothStop();
      break;
    case Cmd::EStop:
      estopWhileMoving = st.jogging || st.hasTarget || servo::snapshot().runStatus != 1;
      can_bus::send(MOTOR_ID, 0xF7);
      jogDir = 0;
      setStatus([](Status &s) { s.estop = true; s.jogging = false; s.hasTarget = false; });
      event("E-STOP ENGAGED / MOTOR HOLDS POSITION", "err");
      break;
    case Cmd::Release:
      setStatus([](Status &s) { s.estop = false; });
      event("E-STOP RELEASED", "warn");
      break;
    case Cmd::Jog:
      if (c.dir == 0) {
        if (st.jogging) smoothStop();
        break;
      }
      if ((why = blockedReason())) { event(why, "err"); break; }
      lastJogMs = millis();
      if (!st.jogging || c.dir != jogDir || toRpm(c.speed) != jogRpm) {
        jogDir = c.dir;
        jogRpm = toRpm(c.speed);
        sendSpeed(jogDir, jogRpm, JOG_ACC);
        lastMotion = Motion::Speed;
        stopping = false;
        setStatus([](Status &s) { s.jogging = true; s.hasTarget = false; });
      }
      break;
    case Cmd::Step: {
      if ((why = blockedReason())) { event(why, "err"); break; }
      const float from = toDeg(servo::snapshot().positionCounts);
      startMove(0xF4, from + c.deg, toCounts(c.deg), c.speed, c.accel);
      break;
    }
    case Cmd::Goto:
      if ((why = blockedReason())) { event(why, "err"); break; }
      startMove(0xF5, c.deg, toCounts(c.deg), c.speed, c.accel);
      break;
    case Cmd::Zero:
      if (st.jogging || st.hasTarget) { event("REFUSED / STOP THE MOTOR BEFORE SETTING ZERO", "err"); break; }
      {
        const uint8_t p = 0x00;
        can_bus::send(MOTOR_ID, 0x92, &p, 1);
      }
      break;
    case Cmd::ClearStall:
      can_bus::send(MOTOR_ID, 0x3D);
      break;
    case Cmd::ClientGone:
      if (st.jogging) {
        smoothStop();
        event("JOG STOPPED / BROWSER DISCONNECTED", "warn");
      }
      break;
  }
}

}  // namespace

void begin() { queue = xQueueCreate(16, sizeof(Command)); }

bool enqueue(const Command &cmd) { return queue && xQueueSend(queue, &cmd, 0) == pdTRUE; }

void update() {
  Command c;
  while (xQueueReceive(queue, &c, 0) == pdTRUE) execute(c);

  // Deadman: a held jog button repeats every ~100 ms; silence means stop.
  if (st.jogging && millis() - lastJogMs > JOG_TIMEOUT_MS) {
    smoothStop();
    event("JOG STOPPED / NO KEEPALIVE FROM THE BROWSER", "warn");
  }
  if (st.jogging && !servo::snapshot().online) {
    jogDir = 0;
    setStatus([](Status &s) { s.jogging = false; });
  }
}

Status status() {
  portENTER_CRITICAL(&mux);
  Status copy = st;
  portEXIT_CRITICAL(&mux);
  return copy;
}

void onReply(const twai_message_t &msg) {
  const uint8_t code = msg.data[0];
  const uint8_t s = msg.data_length_code >= 3 ? msg.data[1] : 0xFF;
  switch (code) {
    case 0xF3:
      if (s != 1) event("MOTOR REFUSED THE ENABLE/DISABLE COMMAND", "err");
      break;
    case 0xF4:
    case 0xF5:
      if (s == 0) {
        event(stopping ? "STOP FAILED" : "MOVE REFUSED BY THE MOTOR", "err");
        setStatus([](Status &x) { x.hasTarget = false; });
      } else if (s == 2) {
        event(stopping ? "STOPPED" : "MOVE COMPLETE", "ok");
        stopping = false;
        setStatus([](Status &x) { x.hasTarget = false; });
      } else if (s == 3) {
        event("STOPPED BY LIMIT SWITCH", "warn");
        stopping = false;
        setStatus([](Status &x) { x.hasTarget = false; });
      }
      break;
    case 0xF6:
      if (s == 0) {
        event("JOG REFUSED BY THE MOTOR", "err");
        jogDir = 0;
        setStatus([](Status &x) { x.jogging = false; });
      }
      break;
    case 0xF7:
      if (s != 1 && estopWhileMoving) event("MOTOR DID NOT CONFIRM THE E-STOP", "err");
      break;
    case 0x92:
      if (s == 1) {
        setStatus([](Status &x) { x.zeroed = true; });
        event("ZERO SET AT CURRENT POSITION", "ok");
      } else {
        event("MOTOR REFUSED SET ZERO", "err");
      }
      break;
    case 0x3D:
      event(s == 1 ? "STALL CLEARED" : "COULD NOT CLEAR THE STALL", s == 1 ? "ok" : "err");
      break;
  }
}

void onMotorOnline() {
  // A motor that was offline has probably been power-cycled: its position restarts near 0.
  setStatus([](Status &s) { s.zeroed = false; s.jogging = false; s.hasTarget = false; });
  jogDir = 0;
  // Motor-side heartbeat (89H, not saved): the motor stops by itself if the ESP32 goes silent.
  const uint8_t p[4] = {uint8_t(MOTOR_HEARTBEAT_MS >> 24), uint8_t(MOTOR_HEARTBEAT_MS >> 16),
                        uint8_t(MOTOR_HEARTBEAT_MS >> 8), uint8_t(MOTOR_HEARTBEAT_MS)};
  can_bus::send(MOTOR_ID, 0x89, p, 4);
}

}  // namespace control

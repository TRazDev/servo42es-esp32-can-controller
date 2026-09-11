#include "control.h"
#include <Arduino.h>
#include <math.h>
#include "can_bus.h"
#include "config.h"
#include "servo.h"
#include "settings.h"
#include "units.h"
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

// ---- motor parameters (Settings tab) ----
enum : uint8_t { P_CURRENT = 1, P_STALL = 2, P_HEARTBEAT = 4, P_ALL = 7 };
struct MotorParams {
  int32_t currentMa = -1;    // -1 = unknown / not readable
  int32_t stallOn = -1;
  int32_t stallCounts = -1;  // 88H units, 1 count = 1.8 motor degrees
  int64_t heartbeatMs = -1;
};
MotorParams params;
uint8_t readPending = 0;
uint32_t readDeadline = 0;
uint8_t writePending = 0, writeFailed = 0;
bool savePending = false;
uint32_t writeDeadline = 0;

// ---- unit conversion (joint units -> motor units) ----
uint16_t toRpm(float degPerS, const settings::Joint &j) {
  long rpm = lroundf(fabsf(degPerS) * j.gear / 6.0f);
  return (uint16_t)constrain(rpm, 1L, (long)MAX_MOTOR_RPM);
}

// The motor ramps by 1 RPM every (256 - acc) x 50 us, i.e. 20000 / (256 - acc) RPM/s.
// acc = 1 is the gentlest ramp (~78 RPM/s); 0 would mean no ramp, which we never send.
uint8_t toAcc(float degPerS2, const settings::Joint &j) {
  const float rpmPerS = fabsf(degPerS2) * j.gear / 6.0f;
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

bool moving() { return st.jogging || st.hasTarget; }

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
void sendSpeed(int8_t motorDir, uint16_t rpm, uint8_t acc) {
  // F6 byte 2: bit 7 = direction, bits 3-0 = speed high nibble.
  const uint8_t dirBit = motorDir > 0 ? F6_DIR_BIT_POSITIVE : !F6_DIR_BIT_POSITIVE;
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
  const settings::Joint j = settings::joint();
  sendPosition(code, counts, toRpm(speed, j), toAcc(accel, j));
  lastMotion = code == 0xF4 ? Motion::Relative : Motion::Absolute;
  stopping = false;
  portENTER_CRITICAL(&mux);
  st.hasTarget = true;
  st.targetDeg = targetDeg;
  portEXIT_CRITICAL(&mux);
}

// ---- settings: read from the motor, write + save ----
void startRead() {
  params = MotorParams();
  readPending = P_ALL;
  readDeadline = millis() + 600;
  for (uint8_t code : {0x83, 0x88, 0x89}) can_bus::send(MOTOR_ID, 0x00, &code, 1);
}

void publishSettings() {
  const settings::Joint j = settings::joint();
  char cur[8] = "null", son[8] = "null", stol[16] = "null", hb[12] = "null";
  if (params.currentMa >= 0) snprintf(cur, sizeof(cur), "%ld", (long)params.currentMa);
  if (params.stallOn >= 0) snprintf(son, sizeof(son), "%ld", (long)params.stallOn);
  if (params.stallCounts >= 0) snprintf(stol, sizeof(stol), "%.3f", units::stallCountsToDeg(params.stallCounts, j));
  if (params.heartbeatMs >= 0) snprintf(hb, sizeof(hb), "%lld", (long long)params.heartbeatMs);
  char json[200];
  snprintf(json, sizeof(json), "{\"set\":{\"gear\":%.4f,\"inv\":%d,\"cur\":%s,\"son\":%s,\"stol\":%s,\"hb\":%s}}",
           j.gear, j.invert, cur, son, stol, hb);
  web::publishJson(json);
}

void finishRead() {
  readPending = 0;
  publishSettings();
}

void saveSettings(const Command &c) {
  if (moving()) { event("REFUSED / STOP THE MOTOR BEFORE CHANGING SETTINGS", "err"); return; }
  settings::Joint j;
  j.gear = c.gear;
  j.invert = c.invert;
  j.heartbeatMs = c.heartbeatMs;
  if (c.currentMa < 100 || c.currentMa > MAX_CURRENT_MA || !(c.stallTolDeg >= 0) || !settings::save(j)) {
    event("REFUSED / A SETTING IS OUT OF RANGE", "err");
    return;
  }
  if (!servo::snapshot().online) {
    event("SAVED ON THE ESP32 / MOTOR OFFLINE, MOTOR SETTINGS NOT WRITTEN", "warn");
    publishSettings();
    return;
  }
  const long counts = constrain(lroundf(c.stallTolDeg * j.gear / 1.8f), 1L, 0x7FFFL);
  const uint8_t cur[2] = {uint8_t(c.currentMa >> 8), uint8_t(c.currentMa)};
  const uint8_t stall[3] = {uint8_t(c.stallOn ? 1 : 0), uint8_t(counts >> 8), uint8_t(counts)};
  const uint8_t hb[4] = {uint8_t(j.heartbeatMs >> 24), uint8_t(j.heartbeatMs >> 16),
                         uint8_t(j.heartbeatMs >> 8), uint8_t(j.heartbeatMs)};
  can_bus::send(MOTOR_ID, 0x83, cur, 2);
  can_bus::send(MOTOR_ID, 0x88, stall, 3);
  can_bus::send(MOTOR_ID, 0x89, hb, 4);
  writePending = P_ALL;
  writeFailed = 0;
  savePending = false;
  writeDeadline = millis() + 1000;
}

void onWriteStatus(uint8_t bit, uint8_t status) {
  if (!(writePending & bit)) return;
  writePending &= ~bit;
  if (status != 1) writeFailed |= bit;
  if (writePending) return;
  if (writeFailed) {
    char msg[96];
    snprintf(msg, sizeof(msg), "MOTOR REJECTED:%s%s%s", writeFailed & P_CURRENT ? " CURRENT" : "",
             writeFailed & P_STALL ? " STALL" : "", writeFailed & P_HEARTBEAT ? " TIMEOUT" : "");
    event(msg, "err");
  }
  const uint8_t save = 0x01;
  can_bus::send(MOTOR_ID, 0x60, &save, 1);  // persist everything the motor accepted
  savePending = true;
  writeDeadline = millis() + 1000;
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
      estopWhileMoving = moving() || servo::snapshot().runStatus != 1;
      can_bus::send(MOTOR_ID, 0xF7);
      jogDir = 0;
      setStatus([](Status &s) { s.estop = true; s.jogging = false; s.hasTarget = false; });
      event("E-STOP ENGAGED / MOTOR HOLDS POSITION", "err");
      break;
    case Cmd::Release:
      setStatus([](Status &s) { s.estop = false; });
      event("E-STOP RELEASED", "warn");
      break;
    case Cmd::Jog: {
      if (c.dir == 0) {
        if (st.jogging) smoothStop();
        break;
      }
      if ((why = blockedReason())) { event(why, "err"); break; }
      const settings::Joint j = settings::joint();
      const int8_t motorDir = j.invert ? -c.dir : c.dir;
      lastJogMs = millis();
      if (!st.jogging || c.dir != jogDir || toRpm(c.speed, j) != jogRpm) {
        jogDir = c.dir;
        jogRpm = toRpm(c.speed, j);
        sendSpeed(motorDir, jogRpm, JOG_ACC);
        lastMotion = Motion::Speed;
        stopping = false;
        setStatus([](Status &s) { s.jogging = true; s.hasTarget = false; });
      }
      break;
    }
    case Cmd::Step: {
      if ((why = blockedReason())) { event(why, "err"); break; }
      const settings::Joint j = settings::joint();
      const double from = units::countsToDeg(servo::snapshot().positionCounts, j);
      startMove(0xF4, from + c.deg, units::degToCounts(c.deg, j), c.speed, c.accel);
      break;
    }
    case Cmd::Goto:
      if ((why = blockedReason())) { event(why, "err"); break; }
      startMove(0xF5, c.deg, units::degToCounts(c.deg, settings::joint()), c.speed, c.accel);
      break;
    case Cmd::Zero:
      if (moving()) { event("REFUSED / STOP THE MOTOR BEFORE SETTING ZERO", "err"); break; }
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
    case Cmd::GetSettings:
      if (servo::snapshot().online) startRead();
      else { params = MotorParams(); publishSettings(); }
      break;
    case Cmd::SaveSettings:
      saveSettings(c);
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

  const uint32_t now = millis();
  if (readPending && (int32_t)(now - readDeadline) > 0) finishRead();  // unanswered = unknown
  if ((writePending || savePending) && (int32_t)(now - writeDeadline) > 0) {
    writePending = 0;
    savePending = false;
    event("NO REPLY FROM THE MOTOR WHILE SAVING", "err");
  }
}

Status status() {
  portENTER_CRITICAL(&mux);
  Status copy = st;
  portEXIT_CRITICAL(&mux);
  return copy;
}

void onReply(const twai_message_t &msg) {
  const uint8_t *d = msg.data;
  const uint8_t len = msg.data_length_code;
  const uint8_t s = len >= 3 ? d[1] : 0xFF;
  // Reading a parameter the motor can't report returns "code FF FF".
  const bool unreadable = len == 4 && d[1] == 0xFF && d[2] == 0xFF;

  switch (d[0]) {
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

    // Settings: a 3-byte reply is the status of a write; longer ones answer "00 <code>".
    case 0x83:
      if (len == 3) onWriteStatus(P_CURRENT, s);
      else if (len == 4 && (readPending & P_CURRENT)) {
        if (!unreadable) params.currentMa = (d[1] << 8) | d[2];
        readPending &= ~P_CURRENT;
      }
      break;
    case 0x88:
      if (len == 3) onWriteStatus(P_STALL, s);
      else if (readPending & P_STALL) {
        if (len == 5) { params.stallOn = d[1]; params.stallCounts = (d[2] << 8) | d[3]; }
        readPending &= ~P_STALL;
      }
      break;
    case 0x89:
      if (len == 3) onWriteStatus(P_HEARTBEAT, s);
      else if (readPending & P_HEARTBEAT) {
        if (len == 6) params.heartbeatMs = ((uint32_t)d[1] << 24) | ((uint32_t)d[2] << 16) | (d[3] << 8) | d[4];
        readPending &= ~P_HEARTBEAT;
      }
      break;
    case 0x60:
      if (!savePending) break;
      savePending = false;
      if (s == 1) event("SETTINGS SAVED (ESP32 + MOTOR)", "ok");
      else event("MOTOR COULD NOT SAVE ITS SETTINGS", "err");
      startRead();  // show what the motor now reports
      break;
  }
  if (readPending == 0 && (d[0] == 0x83 || d[0] == 0x88 || d[0] == 0x89) && len > 3) finishRead();
}

void onMotorOnline() {
  // A motor that was offline has probably been power-cycled: its position restarts near 0.
  setStatus([](Status &s) { s.zeroed = false; s.jogging = false; s.hasTarget = false; });
  jogDir = 0;
  // Motor-side heartbeat (89H, not saved here): the motor stops by itself if the ESP32 goes silent.
  const uint32_t hb = settings::joint().heartbeatMs;
  const uint8_t p[4] = {uint8_t(hb >> 24), uint8_t(hb >> 16), uint8_t(hb >> 8), uint8_t(hb)};
  can_bus::send(MOTOR_ID, 0x89, p, 4);
}

}  // namespace control

#pragma once
#include <stdint.h>
#include "driver/twai.h"

// Motion, safety and settings logic. Commands arrive from the web server task and
// are queued; update() executes them in the main loop, so all CAN writes happen there.
namespace control {

enum class Cmd : uint8_t {
  Enable, Disable, Stop, EStop, Release, Jog, Step, Goto, Zero, ClearStall, ClientGone,
  GetSettings, SaveSettings,
  RestartMotor, FactoryReset, Calibrate, FixMode
};

struct Command {
  Cmd type;
  int8_t dir = 0;     // Jog: -1, 0 (release), +1
  float deg = 0;      // Step: relative joint degrees; Goto: absolute joint degrees
  float speed = 0;    // joint deg/s
  float accel = 0;    // joint deg/s^2
  // SaveSettings
  float gear = 1;
  bool invert = false;
  uint16_t currentMa = 0;
  bool stallOn = true;
  float stallTolDeg = 0;  // joint degrees
  uint32_t heartbeatMs = 0;
};

struct Status {
  bool estop = false;      // latched until Release
  bool zeroed = false;     // 92H succeeded since the motor last powered up
  bool jogging = false;
  bool hasTarget = false;
  float targetDeg = 0;
  bool calibrating = false;  // 80H sent; cleared when the motor is power-cycled or after a timeout
};

void begin();
bool enqueue(const Command &cmd);  // safe from any task
void update();                     // call from loop()
Status status();                   // safe from any task

// Called by servo.cpp from the main loop.
void onReply(const twai_message_t &msg);
void onMotorOnline();
// True while a mode write (82 05) is waiting for its "82 <status>" reply, which has the
// same shape as the reply to reading the mode ("00 82" -> "82 <mode>").
bool expectingModeWriteReply();

}  // namespace control

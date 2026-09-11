#pragma once
#include <stdint.h>

// Joint settings stored on the ESP32 (NVS), kept across reboots and motor factory resets.
namespace settings {

constexpr float GEAR_MIN = 0.01f;
constexpr float GEAR_MAX = 1000.0f;
constexpr uint32_t HEARTBEAT_MAX_MS = 600000;

struct Joint {
  float gear = 1.0f;            // motor turns per joint turn
  bool invert = false;          // flips the sign of joint angles and moves
  uint32_t heartbeatMs = 1000;  // motor link-loss timeout (89H), 0 = off
};

void begin();              // loads from NVS
Joint joint();             // safe from any task
bool save(const Joint &j); // validates and stores; call from the main loop

}  // namespace settings

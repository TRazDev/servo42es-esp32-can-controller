#pragma once
#include <stdint.h>
#include <driver/gpio.h>

// Hardware (see docs/hardware.md)
constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_5;
constexpr uint16_t MOTOR_ID = 0x01;

// Network (see docs/network.md)
constexpr const char *HOSTNAME = "servo-control";

// A motor counts as offline if nothing was received for this long.
constexpr uint32_t MOTOR_TIMEOUT_MS = 500;

// Gear ratio, direction and the motor heartbeat are user settings: see settings.h.

// Telemetry pushed to the browser over the WebSocket.
constexpr uint32_t TELEMETRY_INTERVAL_MS = 50;  // 20 Hz

// Motion limits and safety
constexpr uint16_t MAX_MOTOR_RPM = 600;        // cap for every motion command (bench safety)
constexpr uint8_t JOG_ACC = 100;               // ~128 RPM/s ramp for jog start/stop
constexpr uint8_t STOP_ACC = 100;              // ramp for the smooth STOP button
constexpr uint32_t JOG_TIMEOUT_MS = 300;       // stop a jog if the browser's keepalive stops
constexpr uint16_t MAX_CURRENT_MA = 3000;      // SERVO42ES limit for 83H

// Position is sampled every ~20 ms. At the motor's 3000 RPM maximum that is at most
// ~820 counts per sample (up to ~1640 if a sample is late), so a bigger step means the
// counter was reset (motor restart or set zero).
constexpr int64_t POSITION_JUMP_COUNTS = 3000;
// F6 (speed mode) direction bit that makes the encoder count go up (= CCW seen from
// the shaft end). VERIFIED 2026-09-11: jog with bit 0 increased the angle.
constexpr uint8_t F6_DIR_BIT_POSITIVE = 0;

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

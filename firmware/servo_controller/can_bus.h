#pragma once
#include <stdint.h>
#include "driver/twai.h"

// TWAI (CAN) link to the SERVO42ES motors. Frame layout: [code] [payload...] [CRC],
// CRC = (ID + all preceding bytes) & 0xFF. See docs/protocol.md.
namespace can_bus {

bool begin();
bool send(uint16_t id, uint8_t code, const uint8_t *payload = nullptr, uint8_t len = 0);
// Non-blocking. Returns true for a frame with a valid checksum.
bool receive(twai_message_t &msg);
// Restarts the controller after bus-off. Call regularly.
void maintain();
uint8_t checksum(uint16_t id, const uint8_t *data, uint8_t len);

}  // namespace can_bus

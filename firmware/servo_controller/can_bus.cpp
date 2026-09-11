#include "can_bus.h"
#include <Arduino.h>
#include "config.h"

namespace can_bus {

uint8_t checksum(uint16_t id, const uint8_t *data, uint8_t len) {
  uint32_t sum = id & 0xFF;  // how IDs above 0xFF enter the sum is undocumented
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  return sum & 0xFF;
}

bool begin() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  g.rx_queue_len = 32;
  g.tx_queue_len = 16;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  return twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK;
}

bool send(uint16_t id, uint8_t code, const uint8_t *payload, uint8_t len) {
  if (len > 6) return false;
  twai_message_t msg = {};
  msg.identifier = id;
  msg.data_length_code = len + 2;
  msg.data[0] = code;
  for (uint8_t i = 0; i < len; i++) msg.data[1 + i] = payload[i];
  msg.data[len + 1] = checksum(id, msg.data, len + 1);
  return twai_transmit(&msg, 0) == ESP_OK;
}

bool receive(twai_message_t &msg) {
  while (twai_receive(&msg, 0) == ESP_OK) {
    uint8_t len = msg.data_length_code;
    if (len >= 2 && checksum(msg.identifier, msg.data, len - 1) == msg.data[len - 1]) return true;
  }
  return false;
}

void maintain() {
  twai_status_info_t s;
  if (twai_get_status_info(&s) != ESP_OK) return;
  if (s.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
  else if (s.state == TWAI_STATE_STOPPED) twai_start();
}

}  // namespace can_bus

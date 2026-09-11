// First CAN test for the MKS SERVO42ES.
// Read-only: asks for the firmware version (40H) once, then the encoder
// position (31H) every second, and prints every frame it sends and receives.
// It sends no motion or configuration commands, so the motor won't move.
//
// Board settings (Arduino IDE): ESP32S3 Dev Module, Flash Size 16MB,
// PSRAM "OPI PSRAM", USB CDC On Boot "Disabled". Upload and open the serial
// monitor (115200 baud) through the USB-C port labelled COM.

#include "driver/twai.h"

constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_5;
constexpr uint16_t MOTOR_ID = 0x01;          // factory default
constexpr uint32_t POLL_INTERVAL_MS = 1000;
constexpr uint32_t STATUS_INTERVAL_MS = 5000;

uint32_t lastPoll = 0;
uint32_t lastStatus = 0;
uint32_t framesReceived = 0;

// CRC = (ID + all bytes before the CRC) & 0xFF. How IDs above 0xFF enter the
// sum is undocumented; the low byte is assumed.
uint8_t checksum(uint16_t id, const uint8_t *data, uint8_t len) {
  uint32_t sum = id & 0xFF;
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  return sum & 0xFF;
}

void printFrame(const char *dir, uint32_t id, const uint8_t *data, uint8_t len) {
  Serial.printf("%s id=0x%03lX dlc=%u :", dir, (unsigned long)id, len);
  for (uint8_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
  Serial.println();
}

bool sendCommand(uint8_t code, const uint8_t *payload = nullptr, uint8_t payloadLen = 0) {
  twai_message_t msg = {};
  msg.identifier = MOTOR_ID;
  msg.data_length_code = payloadLen + 2;
  msg.data[0] = code;
  for (uint8_t i = 0; i < payloadLen; i++) msg.data[1 + i] = payload[i];
  msg.data[payloadLen + 1] = checksum(MOTOR_ID, msg.data, payloadLen + 1);

  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(50));
  if (err != ESP_OK) {
    Serial.printf("TX failed (%s)\n", esp_err_to_name(err));
    return false;
  }
  printFrame("TX", msg.identifier, msg.data, msg.data_length_code);
  return true;
}

void decode(const twai_message_t &msg) {
  const uint8_t *d = msg.data;
  const uint8_t len = msg.data_length_code;
  if (len < 2) return;

  if (checksum(msg.identifier, d, len - 1) != d[len - 1]) {
    Serial.println("   ^ bad checksum");
    return;
  }

  switch (d[0]) {
    case 0x40:  // version: series / calibration / hardware, then firmware x.y.z
      if (len == 6) {
        Serial.printf("   version: %s series, hardware %u, cal %u, firmware V%u.%u.%u\n",
                      (d[1] & 0x80) ? "E" : "D", d[1] & 0x0F, (d[1] >> 4) & 0x03,
                      d[2], d[3], d[4]);
      }
      break;
    case 0x31:  // cumulative encoder, int48, 16384 counts per turn
      if (len == 8) {
        int64_t value = 0;
        for (uint8_t i = 1; i <= 6; i++) value = (value << 8) | d[i];
        if (value & (1LL << 47)) value -= (1LL << 48);  // sign-extend 48 -> 64 bits
        Serial.printf("   position: %lld counts = %.2f deg\n", (long long)value,
                      value * 360.0 / 16384.0);
      }
      break;
  }
}

void printBusStatus() {
  twai_status_info_t s;
  if (twai_get_status_info(&s) != ESP_OK) return;
  const char *state = s.state == TWAI_STATE_RUNNING    ? "running"
                    : s.state == TWAI_STATE_BUS_OFF    ? "BUS-OFF"
                    : s.state == TWAI_STATE_RECOVERING ? "recovering"
                                                       : "stopped";
  Serial.printf("-- bus %s | rx frames %lu | tx errors %lu | rx errors %lu | tx failed %lu | queued %lu\n",
                state, (unsigned long)framesReceived, (unsigned long)s.tx_error_counter,
                (unsigned long)s.rx_error_counter, (unsigned long)s.tx_failed_count,
                (unsigned long)s.msgs_to_tx);
  if (framesReceived == 0) {
    Serial.println("   no replies yet: check motor power, CAN H/L, common GND and the 500 kbit/s bitrate");
  }
  if (s.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
  if (s.state == TWAI_STATE_STOPPED) twai_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSERVO42ES CAN test (read-only)");

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK || twai_start() != ESP_OK) {
    Serial.println("TWAI driver failed to start");
    while (true) delay(1000);
  }
  Serial.printf("TWAI started: TX=GPIO%d RX=GPIO%d, 500 kbit/s, motor ID 0x%02X\n",
                CAN_TX_PIN, CAN_RX_PIN, MOTOR_ID);

  sendCommand(0x40);  // read version once
}

void loop() {
  twai_message_t msg;
  while (twai_receive(&msg, 0) == ESP_OK) {
    framesReceived++;
    printFrame("RX", msg.identifier, msg.data, msg.data_length_code);
    decode(msg);
  }

  uint32_t now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    sendCommand(0x31);  // read position
  }
  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    printBusStatus();
  }
  delay(5);
}

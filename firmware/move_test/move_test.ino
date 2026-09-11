// First motion test for the MKS SERVO42ES.
// 1. Reads the working mode; if it isn't 05 (bus closed-loop FOC), sets 05 and saves it.
// 2. Enables the motor.
// 3. Turns +90 deg at 60 RPM, waits, turns -90 deg back, and reports the position.
// Nothing happens until 'g' is sent from the serial monitor. After that, any
// character sent triggers an emergency stop (F7).
//
// Board settings (Arduino IDE): ESP32S3 Dev Module, Flash Size 16MB,
// PSRAM "OPI PSRAM", USB CDC On Boot "Disabled". Serial monitor 115200 baud
// on the USB-C port labelled COM.

#include "driver/twai.h"

constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_5;
constexpr uint16_t MOTOR_ID = 0x01;
constexpr uint8_t MODE_BUS_CLOSED_LOOP = 0x05;
constexpr uint16_t TEST_RPM = 60;
constexpr uint8_t TEST_ACC = 2;
constexpr int32_t QUARTER_TURN = 4096;  // encoder counts (16384 per turn)

bool stopped = false;

uint8_t checksum(uint16_t id, const uint8_t *data, uint8_t len) {
  uint32_t sum = id & 0xFF;
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  return sum & 0xFF;
}

void printFrame(const char *dir, const twai_message_t &msg) {
  Serial.printf("%s id=0x%03lX :", dir, (unsigned long)msg.identifier);
  for (uint8_t i = 0; i < msg.data_length_code; i++) Serial.printf(" %02X", msg.data[i]);
  Serial.println();
}

void emergencyStopIfRequested() {
  if (!Serial.available()) return;
  while (Serial.available()) Serial.read();
  twai_message_t msg = {};
  msg.identifier = MOTOR_ID;
  msg.data_length_code = 2;
  msg.data[0] = 0xF7;
  msg.data[1] = checksum(MOTOR_ID, msg.data, 1);
  twai_transmit(&msg, pdMS_TO_TICKS(50));
  printFrame("TX", msg);
  Serial.println("*** EMERGENCY STOP sent ***");
  stopped = true;
}

bool sendCommand(uint8_t code, const uint8_t *payload = nullptr, uint8_t payloadLen = 0) {
  twai_message_t msg = {};
  msg.identifier = MOTOR_ID;
  msg.data_length_code = payloadLen + 2;
  msg.data[0] = code;
  for (uint8_t i = 0; i < payloadLen; i++) msg.data[1 + i] = payload[i];
  msg.data[payloadLen + 1] = checksum(MOTOR_ID, msg.data, payloadLen + 1);
  if (twai_transmit(&msg, pdMS_TO_TICKS(50)) != ESP_OK) {
    Serial.println("TX failed");
    return false;
  }
  printFrame("TX", msg);
  return true;
}

// Waits for a reply whose first byte is `code`. Other frames are printed and skipped.
bool waitReply(uint8_t code, twai_message_t &out, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    emergencyStopIfRequested();
    if (twai_receive(&out, pdMS_TO_TICKS(10)) != ESP_OK) continue;
    printFrame("RX", out);
    uint8_t len = out.data_length_code;
    if (len < 2 || checksum(out.identifier, out.data, len - 1) != out.data[len - 1]) {
      Serial.println("   ^ bad checksum, ignored");
      continue;
    }
    if (out.data[0] == code) return true;
  }
  Serial.printf("   no reply to %02X within %lu ms\n", code, (unsigned long)timeoutMs);
  return false;
}

// Sends a command and returns the 1-byte status from its reply (-1 on timeout).
int command(uint8_t code, const uint8_t *payload = nullptr, uint8_t payloadLen = 0,
            uint32_t timeoutMs = 500) {
  twai_message_t reply;
  if (!sendCommand(code, payload, payloadLen) || !waitReply(code, reply, timeoutMs)) return -1;
  return reply.data[1];
}

int readMode() {
  const uint8_t param = 0x82;
  twai_message_t reply;
  if (!sendCommand(0x00, &param, 1) || !waitReply(0x82, reply, 500)) return -1;
  return reply.data_length_code == 3 ? reply.data[1] : -1;
}

bool readPosition(int64_t &counts) {
  twai_message_t reply;
  if (!sendCommand(0x31) || !waitReply(0x31, reply, 500) || reply.data_length_code != 8) return false;
  int64_t v = 0;
  for (uint8_t i = 1; i <= 6; i++) v = (v << 8) | reply.data[i];
  if (v & (1LL << 47)) v -= (1LL << 48);
  counts = v;
  return true;
}

void printPosition(const char *label) {
  int64_t c;
  if (readPosition(c)) {
    Serial.printf(">> %s: %lld counts = %.2f deg\n", label, (long long)c, c * 360.0 / 16384.0);
  }
}

// F4: relative move in encoder counts. Waits for "started" (1) and "done" (2) or "limit" (3).
bool moveRelative(int32_t counts, uint16_t rpm, uint8_t acc) {
  const uint8_t p[6] = {uint8_t(rpm >> 8), uint8_t(rpm), acc, uint8_t(counts >> 16),
                        uint8_t(counts >> 8), uint8_t(counts)};
  twai_message_t reply;
  if (!sendCommand(0xF4, p, sizeof(p)) || !waitReply(0xF4, reply, 500)) return false;
  if (reply.data[1] != 1) {
    Serial.printf("   move refused (status %u)\n", reply.data[1]);
    return false;
  }
  Serial.println("   moving...");
  if (!waitReply(0xF4, reply, 10000)) return false;
  Serial.printf("   move finished (status %u: %s)\n", reply.data[1],
                reply.data[1] == 2 ? "done" : reply.data[1] == 3 ? "stopped by limit" : "other");
  return reply.data[1] == 2;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSERVO42ES first motion test. Send 'g' to start.");
  while (Serial.read() != 'g') delay(10);
  Serial.println("Started. Send any character to emergency-stop.");

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK || twai_start() != ESP_OK) {
    Serial.println("TWAI driver failed to start");
    return;
  }

  int mode = readMode();
  Serial.printf(">> working mode: %d\n", mode);
  if (mode != MODE_BUS_CLOSED_LOOP) {
    const uint8_t m = MODE_BUS_CLOSED_LOOP, save = 0x01;
    Serial.printf(">> set mode 05: status %d\n", command(0x82, &m, 1));
    Serial.printf(">> save parameters: status %d\n", command(0x60, &save, 1, 1000));
    mode = readMode();
    Serial.printf(">> working mode now: %d\n", mode);
  }
  if (mode != MODE_BUS_CLOSED_LOOP) {
    Serial.println("Motor is not in mode 05, stopping here.");
    return;
  }

  const uint8_t enable = 0x01;
  Serial.printf(">> enable: status %d\n", command(0xF3, &enable, 1));
  delay(500);
  printPosition("start");

  if (!stopped && moveRelative(QUARTER_TURN, TEST_RPM, TEST_ACC)) printPosition("after +90 deg");
  delay(1000);
  if (!stopped && moveRelative(-QUARTER_TURN, TEST_RPM, TEST_ACC)) printPosition("after -90 deg");

  Serial.println(stopped ? "Test aborted by emergency stop." : "Test finished. Motor stays enabled (holding).");
}

void loop() {
  emergencyStopIfRequested();
  twai_message_t msg;
  while (twai_receive(&msg, 0) == ESP_OK) printFrame("RX", msg);
  delay(10);
}

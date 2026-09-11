#include "settings.h"
#include <Arduino.h>
#include <Preferences.h>

namespace settings {
namespace {

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Joint current;
Preferences prefs;

bool valid(const Joint &j) {
  return j.gear >= GEAR_MIN && j.gear <= GEAR_MAX && j.heartbeatMs <= HEARTBEAT_MAX_MS;
}

}  // namespace

void begin() {
  if (!prefs.begin("servo", false)) Serial.println("Settings: NVS namespace failed to open");
  Joint j;
  j.gear = prefs.getFloat("gear", 1.0f);
  j.invert = prefs.getBool("invert", false);
  j.heartbeatMs = prefs.getUInt("hb", 1000);
  Serial.printf("Settings: loaded gear %.4f, invert %d, link-loss timeout %lu ms (stored keys: gear=%d invert=%d hb=%d)\n",
                j.gear, j.invert, (unsigned long)j.heartbeatMs,
                prefs.isKey("gear"), prefs.isKey("invert"), prefs.isKey("hb"));
  if (!valid(j)) {
    Serial.println("Settings: stored values invalid, using defaults");
    j = Joint();
  }
  portENTER_CRITICAL(&mux);
  current = j;
  portEXIT_CRITICAL(&mux);
}

Joint joint() {
  portENTER_CRITICAL(&mux);
  Joint j = current;
  portEXIT_CRITICAL(&mux);
  return j;
}

bool save(const Joint &j) {
  if (!valid(j)) return false;
  const size_t written = prefs.putFloat("gear", j.gear) + prefs.putBool("invert", j.invert) +
                         prefs.putUInt("hb", j.heartbeatMs);
  Serial.printf("Settings: saved gear %.4f, invert %d, link-loss timeout %lu ms (%u bytes written)\n",
                j.gear, j.invert, (unsigned long)j.heartbeatMs, (unsigned)written);
  if (written != sizeof(float) + sizeof(bool) + sizeof(uint32_t)) return false;
  portENTER_CRITICAL(&mux);
  current = j;
  portEXIT_CRITICAL(&mux);
  return true;
}

}  // namespace settings

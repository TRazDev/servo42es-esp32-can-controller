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
  prefs.begin("servo", false);
  Joint j;
  j.gear = prefs.getFloat("gear", 1.0f);
  j.invert = prefs.getBool("invert", false);
  j.heartbeatMs = prefs.getUInt("hb", 1000);
  if (!valid(j)) j = Joint();
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
  prefs.putFloat("gear", j.gear);
  prefs.putBool("invert", j.invert);
  prefs.putUInt("hb", j.heartbeatMs);
  portENTER_CRITICAL(&mux);
  current = j;
  portEXIT_CRITICAL(&mux);
  return true;
}

}  // namespace settings

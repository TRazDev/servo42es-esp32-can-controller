#pragma once
#include <math.h>
#include <stdint.h>
#include "settings.h"

// Motor units <-> joint units. Joint = motor / gear ratio, sign flipped when inverted.
// Encoder: 16384 counts per motor turn. Position error (39H): 51200 per turn.
namespace units {

inline double sign(const settings::Joint &j) { return j.invert ? -1.0 : 1.0; }

inline double countsToDeg(int64_t counts, const settings::Joint &j) {
  return sign(j) * counts * 360.0 / 16384.0 / j.gear;
}
inline int32_t degToCounts(double deg, const settings::Joint &j) {
  return (int32_t)llround(sign(j) * deg * 16384.0 * j.gear / 360.0);
}
inline double rpmToDegPerS(int rpm, const settings::Joint &j) { return sign(j) * rpm * 6.0 / j.gear; }
inline double errorToDeg(int32_t errCounts, const settings::Joint &j) {
  return sign(j) * errCounts * 360.0 / 51200.0 / j.gear;
}
// 88H stall tolerance: 1 count = 1.8 motor degrees.
inline double stallCountsToDeg(uint16_t counts, const settings::Joint &j) { return counts * 1.8 / j.gear; }

}  // namespace units

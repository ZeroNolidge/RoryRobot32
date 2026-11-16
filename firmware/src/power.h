// Battery power latch, battery level and switching Rory off.
#pragma once

namespace Power {
void holdOn();          // call FIRST in setup(): keeps Rory on when running from the battery
float batteryVolts();   // ~3.3 (empty) .. 4.2 (full); meaningless without a battery
bool charging();
bool hasBattery(float volts);   // false when running on USB with no battery fitted
int percent(float volts);       // rough 0..100 for a 1-cell LiPo
void off();             // cuts battery power (on USB she just stays on)
}  // namespace Power

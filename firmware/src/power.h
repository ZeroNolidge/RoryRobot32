// Battery power latch, battery level and switching Rory off.
#pragma once

namespace Power {
void holdOn();          // call FIRST in setup(): keeps Rory on when running from the battery
float batteryVolts();   // ~3.3 (empty) .. 4.2 (full); meaningless without a battery
bool charging();
void off();             // cuts battery power (on USB she just stays on)
}  // namespace Power

#include "power.h"
#include <Arduino.h>
#include "config.h"

namespace Power {

void holdOn() {
  pinMode(PIN_BAT_HOLD, OUTPUT);
  digitalWrite(PIN_BAT_HOLD, HIGH);
  pinMode(PIN_CHARGING, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}

float batteryVolts() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  return (mv / 8) * 3.0f / 1000.0f;  // board divides the battery voltage by 3
}

bool charging() { return digitalRead(PIN_CHARGING) == LOW; }

bool hasBattery(float v) { return v > 2.8f; }

int percent(float v) {
  // typical 1-cell LiPo resting voltages
  static const float volts[] = {3.30f, 3.50f, 3.68f, 3.78f, 3.90f, 4.05f, 4.20f};
  static const int pct[] = {0, 5, 20, 45, 65, 85, 100};
  if (v <= volts[0]) return 0;
  for (int i = 1; i < 7; i++)
    if (v <= volts[i]) return pct[i - 1] + (int)((v - volts[i - 1]) / (volts[i] - volts[i - 1]) * (pct[i] - pct[i - 1]));
  return 100;
}

void off() {
  digitalWrite(PIN_BAT_HOLD, LOW);
  delay(500);
  // Still running? Then USB is powering her. Latch the battery back on, or
  // she'd switch off the moment the cable is unplugged.
  digitalWrite(PIN_BAT_HOLD, HIGH);
}

}  // namespace Power

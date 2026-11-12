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

void off() {
  digitalWrite(PIN_BAT_HOLD, LOW);
  delay(500);
}

}  // namespace Power

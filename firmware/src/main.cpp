// Rory the Robot
// A talking robot friend with a screen for a face, based on Rory from
// "Rory's Talk Trail". Runs on the Waveshare ESP32-S3-Touch-LCD-1.54.
//
// Animated face, touch + motion reactions, voice clips from the microSD card,
// and (with /rory/config.txt) questions answered by Rory's cloud AI server.

#include <Arduino.h>
#include "brain.h"
#include "config.h"
#include "display.h"
#include "face.h"
#include "net.h"
#include "power.h"
#include "senses.h"
#include "settings.h"
#include "voice.h"

static Face face;
static Brain brain;
static uint32_t lastFrameMs = 0;

void setup() {
  Power::holdOn();  // first thing: stay switched on when running from the battery
  Serial.begin(115200);
  // Give the Serial Monitor a moment to reconnect so the start-up report isn't missed.
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 1500;) delay(10);
  Serial.println("\n== Rory the Robot ==");

  if (!Display::begin()) Serial.println("[main] display init failed");
  Senses::begin();
  Voice::begin();
  Settings::load();  // Wi-Fi + server details from /rory/config.txt on the SD card
  Net::begin();
  Serial.printf("[main] battery %.2f V%s\n", Power::batteryVolts(), Power::charging() ? " (charging)" : "");
  Serial.printf("[main] check: touch %s | motion %s | SD card %s | voice clips %s | AI settings %s\n",
                Senses::touchOk() ? "OK" : "MISSING", Senses::imuOk() ? "OK" : "MISSING",
                Voice::hasCard() ? "OK" : "MISSING", Voice::clipCount("hello") ? "OK" : "none",
                Settings::aiConfigured() ? "OK" : "not set");

  uint32_t now = millis();
  face.begin(now);
  brain.begin(face, now);
  Display::setBacklight(BACKLIGHT_AWAKE);
}

void loop() {
  uint32_t now = millis();

  Senses::poll(now);
  Event e;
  while (Senses::next(e)) brain.handle(e, now);

  if (now - lastFrameMs >= FRAME_MS) {
    lastFrameMs = now;
    brain.update(now);
    face.update(now);
    face.render(Display::canvas());
    Display::present();
    Display::update();
  }
  delay(2);
}

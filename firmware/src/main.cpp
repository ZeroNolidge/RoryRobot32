// Rory the Robot
// A talking robot friend with a screen for a face, based on Rory from
// "Rory's Talk Trail". Runs on the Waveshare ESP32-S3-Touch-LCD-1.54.
//
// Phase 1 (this code): animated face, touch + motion reactions, voice clips
// from the microSD card.  Phase 2: ask Rory questions through a cloud AI.

#include <Arduino.h>
#include "brain.h"
#include "config.h"
#include "display.h"
#include "face.h"
#include "power.h"
#include "senses.h"
#include "voice.h"

static Face face;
static Brain brain;
static uint32_t lastFrameMs = 0;

void setup() {
  Power::holdOn();  // first thing: stay switched on when running from the battery
  Serial.begin(115200);
  Serial.println("\n== Rory the Robot ==");

  if (!Display::begin()) Serial.println("[main] display init failed");
  Senses::begin();
  Voice::begin();
  Serial.printf("[main] battery %.2f V%s\n", Power::batteryVolts(), Power::charging() ? " (charging)" : "");

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

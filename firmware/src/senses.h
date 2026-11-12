// Rory's senses: touch screen gestures, motion (QMI8658) and the two buttons.
// Everything becomes simple events for the brain to react to.
#pragma once
#include <stdint.h>

enum class Ev : uint8_t {
  None,
  Tap,        // quick touch on the face (x, y)
  HoldStart,  // finger held still on the face ~0.7 s ("talk to me")
  HoldEnd,    // ...and let go
  Tickle,     // finger rubbed back and forth
  Shake,      // shaken hard
  PickedUp,   // was resting, now moving
  PutDown,    // resting again
  FaceDown,   // screen turned to face the floor/table
  BootButton, // short press on BOOT
  PowerHold,  // PWR held ~1.5 s
};

struct Event {
  Ev type = Ev::None;
  int16_t x = 0, y = 0;
};

namespace Senses {
bool begin();
void poll(uint32_t nowMs);   // call often (every loop)
bool next(Event& e);         // pop the next event, false if none
bool touching(int16_t& x, int16_t& y);
void gravity(float& x, float& y);  // tilt, -1..1, mapped so +x = right, +y = down
bool resting();              // sitting still on a table
bool imuOk();
bool touchOk();
}  // namespace Senses

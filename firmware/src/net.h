// Rory's link to her cloud server: Wi-Fi, and asking a question in the background.
#pragma once
#include <Arduino.h>

namespace Net {
void begin();     // starts Wi-Fi (doesn't wait)
bool online();    // Wi-Fi connected and server configured

enum class State : uint8_t { Idle, Busy, Done, Failed };
// Send a WAV recording to the server. Returns false if a question is already
// in flight or Rory is offline. `wav` must stay valid until the state leaves Busy.
bool ask(const uint8_t* wav, size_t len);
State state();
void reset();     // back to Idle after reading the answer

struct Answer {
  String heard, say, face, audioUrl;
};
const Answer& answer();  // valid when state() == Done
}  // namespace Net

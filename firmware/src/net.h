// Rory's link to her cloud server: Wi-Fi, and asking a question in the background.
#pragma once
#include <Arduino.h>

namespace Net {
void begin();     // starts Wi-Fi and the network task (doesn't wait)
bool online();    // Wi-Fi connected and server configured

// Open the secure connection early (when his finger touches her face), so it's
// ready by the time he finishes asking. Safe to call any time.
void prepare();

enum class State : uint8_t {
  Idle,
  Busy,      // sending the question / server thinking
  Looking,   // he asked her to look: taking a photo with her camera eyes
  Speaking,  // the answer has arrived and her voice is streaming to the speaker
  Done,      // finished speaking
  Failed,    // couldn't get an answer
};
// Send a WAV recording to the server. Returns false if a question is already
// in flight or Rory is offline. `wav` must stay valid until the state leaves Busy.
bool ask(const uint8_t* wav, size_t len);
State state();
void reset();     // back to Idle once Done or Failed

struct Answer {
  String heard, say, face;
};
const Answer& answer();  // valid from Speaking onward
}  // namespace Net

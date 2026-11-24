// Rory's link to her cloud server: Wi-Fi, and asking a question in the background.
#pragma once
#include <Arduino.h>

namespace Net {
void begin();     // starts Wi-Fi and the network task (doesn't wait)
bool online();    // Wi-Fi connected and server configured

// Fetch any of her made-in-her-voice clips that are missing from the SD card
// (runs quietly in the background once she's on Wi-Fi). Call every loop.
void syncClips();
bool clipsChanged();  // true once after new clips were saved (then rescan)

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
// When he actually stopped talking (for the timing report in the Serial Monitor).
void markSpeechEnd(uint32_t ms);
State state();
void reset();     // back to Idle once Done or Failed

struct Answer {
  String heard, say, face;
  String remember;
  bool reading = false;  // read mode: he asked her to read his book
  String forget;         // memories the grown-ups asked her to delete
  bool quiet = false;    // bedtime quiet hours: soft voice  // something new she learned about him (saved by the brain)
};
const Answer& answer();  // valid from Speaking onward
}  // namespace Net

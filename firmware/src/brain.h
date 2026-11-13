// Rory's personality: turns sense events into expressions and voice clips,
// and handles getting sleepy, falling asleep and waking up.
#pragma once
#include <stdint.h>
#include "face.h"
#include "senses.h"

class Brain {
 public:
  void begin(Face& face, uint32_t nowMs);
  void handle(const Event& e, uint32_t nowMs);
  void update(uint32_t nowMs);

 private:
  enum class Mode : uint8_t { Awake, Reacting, Listening, Thinking, Sleepy, Asleep, GoingOff };

  struct Reaction {
    Expression expr;
    const char* moment;   // voice folder, or nullptr for silence
    const char* fallback; // used if the first folder is empty
    uint16_t ms;          // minimum time to show it
  };

  void react(const Reaction& r, uint32_t now);
  void then(const Reaction& r) { next_ = r; hasNext_ = true; }
  void say(const char* moment, const char* fallback = nullptr);
  void wakeUp(uint32_t now);
  void goSleepy(uint32_t now);
  void fallAsleep(uint32_t now);
  void tuckIn(uint32_t now);

  Face* face_ = nullptr;
  Mode mode_ = Mode::Awake;
  Expression showing_ = Expr::Content;
  uint32_t until_ = 0;
  uint32_t lastPlayMs_ = 0;
  uint32_t modeSinceMs_ = 0;
  uint32_t lastBatteryCheckMs_ = 0, lastTiredMs_ = 0;
  uint8_t taps_ = 0;
  uint32_t firstTapMs_ = 0;
  Reaction next_{};
  bool hasNext_ = false;
};

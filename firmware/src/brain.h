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
  // Asking the AI: record while his finger is held, then upload and wait.
  enum class Ai : uint8_t { None, Recording, Uploading };

  struct Reaction {
    Expression expr;
    const char* moment;   // voice folder, or nullptr for silence
    const char* fallback; // used if the first folder is empty
    uint16_t ms;          // minimum time to show it
  };

  void react(const Reaction& r, uint32_t now);
  // queue up to 4 reactions to follow the current one
  void then(const Reaction& r) { if (chainLen_ < 4) chain_[chainLen_++] = r; }
  void show(const Reaction& r, uint32_t now);  // like react(), but keeps the queue
  void say(const char* moment, const char* fallback = nullptr);
  void wakeUp(uint32_t now);
  void goSleepy(uint32_t now);
  void fallAsleep(uint32_t now);
  void tuckIn(uint32_t now);
  void finishListening(uint32_t now);
  void answerArrived(uint32_t now);

  Face* face_ = nullptr;
  Mode mode_ = Mode::Awake;
  Ai ai_ = Ai::None;
  Expression showing_ = Expr::Content;
  uint32_t until_ = 0;
  uint32_t lastPlayMs_ = 0;
  uint32_t modeSinceMs_ = 0;
  uint32_t lastBatteryCheckMs_ = 0, lastTiredMs_ = 0;
  uint8_t taps_ = 0;
  uint32_t firstTapMs_ = 0;
  Reaction chain_[4]{};
  uint8_t chainLen_ = 0, chainPos_ = 0;
  uint8_t dizzyLevel_ = 0;     // the dizzy game: spins in a row
  uint32_t lastSpinMs_ = 0;
  uint32_t hugs_ = 0;
};

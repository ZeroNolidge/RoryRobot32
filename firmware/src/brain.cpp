#include "brain.h"
#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "memory.h"
#include "net.h"
#include "power.h"
#include "voice.h"

// Voice folders on the SD card (see sdcard/README.md). Missing folders are fine:
// Rory still makes the face, just quietly.
namespace Moment {
const char* HELLO = "hello";
const char* GIGGLE = "giggle";
const char* BOOP = "boop";
const char* LOVE = "love";
const char* WHEEE = "wheee";
const char* DIZZY = "dizzy";
const char* DARK = "dark";
const char* YAWN = "yawn";
const char* GOODNIGHT = "goodnight";
const char* MORNING = "morning";
const char* BYE = "bye";
const char* CHAT = "chat";
const char* LISTEN = "listen";
const char* ONE = "one";
const char* TWO = "two";
const char* THREE = "three";
const char* OOPS = "oops";
const char* OWL = "owl";
const char* HMM = "hmm";
const char* LOOK = "look";
const char* HIGHFIVE = "highfive";
const char* DANCE = "dance";
const char* WALK = "walk";
const char* TIRED = "tired";
const char* YES = "yes";
const char* READ = "read";    // "Ooh, a book!" - read mode
// lines written for each moment (made in her voice over Wi-Fi)
const char* S_HELLO = "say-hello";
const char* S_AWAKE = "say-awake";
const char* S_BOOP = "say-boop";
const char* S_TICKLE = "say-tickle";
const char* S_LOVE = "say-love";
const char* S_HUG = "say-hug";
const char* S_UP = "say-up";
const char* S_DIZZY = "say-dizzy";
const char* S_OOPS = "say-oops";
const char* S_DARK = "say-dark";
const char* S_HIGHFIVE = "say-highfive";
const char* S_DANCE = "say-dance";
const char* S_WALK = "say-walk";
const char* S_OWL = "say-owl";
const char* S_YAWN = "say-yawn";
const char* S_NIGHT = "say-night";
const char* S_BYE = "say-bye";
const char* S_TIRED = "say-tired";
const char* S_VOLUME = "say-volume";
const char* S_RETRY = "say-retry";
const char* S_OFFLINE = "say-offline";      // "Yes?" - tap to talk
const char* SPIN1 = "spin1";  // "Whoooa!"
const char* SPIN2 = "spin2";  // "I'm getting dizzy!"
const char* SPIN3 = "spin3";  // "Aaarrgh, too much spinning!"
}  // namespace Moment

void Brain::begin(Face& face, uint32_t now) {
  face_ = &face;
  lastPlayMs_ = now;
  face.wake(now);
  face.sparkle(1800);
  react({Expr::Cheer, Moment::S_HELLO, Moment::HELLO, 1800}, now);
}

void Brain::say(const char* moment, const char* fallback) {
  if (!moment) return;
  if (!Voice::say(moment) && fallback) Voice::say(fallback);
}

void Brain::react(const Reaction& r, uint32_t now) {
  chainLen_ = chainPos_ = 0;  // a new reaction replaces anything queued
  show(r, now);
}

void Brain::show(const Reaction& r, uint32_t now) {
  // Anything else happening (a shake, a high five...) ends a question.
  if (ai_ == Ai::Recording) Voice::stopRecording();
  ai_ = Ai::None;
  handsFree_ = false;
  armed_ = false;
  mode_ = Mode::Reacting;
  modeSinceMs_ = now;
  showing_ = r.expr;
  face_->setExpression(r.expr);
  until_ = now + r.ms;
  if (r.moment) say(r.moment, r.fallback);
}

void Brain::wakeUp(uint32_t now) {
  Display::setBacklight(BACKLIGHT_AWAKE);
  face_->setZzz(false);
  face_->wake(now);
  react({Expr::Surprised, nullptr, nullptr, 500}, now);
  then({Expr::Cheer, Moment::S_AWAKE, Moment::MORNING, 1500});
}

void Brain::goSleepy(uint32_t now) {
  mode_ = Mode::Sleepy;
  modeSinceMs_ = now;
  face_->setAutoLook(false);
  face_->lookAt(0, 0.3f);
  face_->setExpression(Expr::Yawning);
  showing_ = Expr::Sleepy;
  until_ = now + 1800;  // yawn, then droopy eyes
  say(Moment::S_YAWN, Moment::YAWN);
}

void Brain::fallAsleep(uint32_t now) {
  mode_ = Mode::Asleep;
  modeSinceMs_ = now;
  face_->setExpression(Expr::Asleep);
  face_->setZzz(true);
  face_->lookAt(0, 0);
  Display::setBacklight(BACKLIGHT_ASLEEP);
}

void Brain::tuckIn(uint32_t now) {
  // Laid on her back: droopy eyes now, asleep in a couple of seconds.
  mode_ = Mode::Sleepy;
  face_->setAutoLook(false);
  face_->lookAt(0, 0);
  face_->setExpression(Expr::Sleepy);
  showing_ = Expr::Sleepy;
  until_ = now;
  modeSinceMs_ = now - ASLEEP_AFTER_MS + 2500;
}

void Brain::finishListening(uint32_t now) {
  if (ai_ == Ai::Recording) Voice::stopRecording();
  // hands-free she waits for a quiet gap, so he really stopped a bit earlier
  Net::markSpeechEnd(handsFree_ ? lastLoudMs_ : now);
  handsFree_ = false;
  mode_ = Mode::Thinking;
  modeSinceMs_ = now;
  face_->lookAt(0.6f, -0.8f);
  face_->setExpression(Expr::Thinking);
  showing_ = Expr::Thinking;
  until_ = now + 900;
}

// The face Rory's answer asked for.
static Expression faceFor(const String& f, bool& sparkle) {
  sparkle = false;
  if (f == "excited") { sparkle = true; return Expr::Cheer; }
  if (f == "silly") return Expr::Happy;
  if (f == "love") return Expr::Love;
  if (f == "scared") return Expr::Scared;  // pretend-scared (toy shark game)
  if (f == "curious") return Expr::Listening;
  if (f == "thinking") return Expr::Thinking;
  return Expr::Content;  // happy, calm
}

void Brain::answerArrived(uint32_t now) {
  const Net::Answer& a = Net::answer();
  Serial.printf("[brain] he said \"%s\" -> Rory: \"%s\" (%s)\n", a.heard.c_str(), a.say.c_str(), a.face.c_str());
  bool sparkle;
  Expression e = faceFor(a.face, sparkle);
  react({e, nullptr, nullptr, 600}, now);  // her voice is already streaming; the mouth follows it
  if (sparkle) face_->sparkle(2500);
  if (a.face == "scared") face_->lookAt(-0.7f, 0.15f);  // peeks away
  else face_->lookAt(0, 0);
  face_->setAutoLook(false);
  lastPlayMs_ = now;
  if (!a.remember.isEmpty()) Memory::add(a.remember);  // saved to the SD card
  if (!a.forget.isEmpty()) Memory::forget(a.forget);   // the grown-ups asked her to forget these
  String said = a.say;
  said.trim();
  listenNext_ = KEEP_LISTENING && said.endsWith("?");
}

// She asked him a question: listen for his answer without a finger on her face.
bool Brain::listenHandsFree(uint32_t now) {
  if (!Net::online() || Net::state() == Net::State::Busy || Net::state() == Net::State::Looking ||
      Net::state() == Net::State::Speaking || !Voice::startRecording())
    return false;
  mode_ = Mode::Listening;
  modeSinceMs_ = lastLoudMs_ = now;
  ai_ = Ai::Recording;
  handsFree_ = true;
  heardHim_ = false;
  noiseFloor_ = 0;
  face_->setAutoLook(false);
  face_->lookAt(0, -0.35f);
  face_->setExpression(Expr::Listening);  // the "o" face is his cue to answer
  showing_ = Expr::Listening;
  Net::prepare();
  Serial.println("[brain] she asked him something - listening hands-free");
  return true;
}

// He's holding her up: quietly record, and if he talks to her (close and loud,
// not a bump or voices across the room) switch to listening. The recording
// started before he spoke, so his first words aren't lost. If he doesn't talk,
// the recording is thrown away and nothing is sent.
void Brain::updateEars(uint32_t now, bool talking) {
  float dt = lastEarsMs_ ? (float)(now - lastEarsMs_) : 0;
  lastEarsMs_ = now;
  if (armed_) {
    int16_t tx, ty;
    if (talking || !Senses::heldUp() || Senses::touching(tx, ty)) {  // a finger on her face takes over
      stopEars(now);
      return;
    }
    float lvl = Voice::micLevel();
    uint32_t t = now - armedMs_;
    if (t < 400) {
      if (lvl > noiseFloor_) noiseFloor_ = lvl;  // how loud the room is
      return;
    }
    if (lvl > fmaxf(HELD_SPEECH_LEVEL, noiseFloor_ * 3.0f)) loudMs_ += dt;
    else loudMs_ = fmaxf(0, loudMs_ - dt * 0.5f);
    if (loudMs_ >= HELD_SPEECH_MS) {
      armed_ = false;
      mode_ = Mode::Listening;
      modeSinceMs_ = now - 400;  // room level already measured
      lastLoudMs_ = now;
      handsFree_ = true;
      heardHim_ = true;
      lastPlayMs_ = now;
      face_->setAutoLook(false);
      face_->lookAt(0, -0.35f);
      face_->setExpression(Expr::Listening);
      showing_ = Expr::Listening;
      Net::prepare();
      Serial.printf("[brain] he's holding me and talking (room %.2f) - listening\n", noiseFloor_);
    } else if (t > 8000) {
      stopEars(now);  // start fresh, so a long wait doesn't fill her memory
      nextArmMs_ = now + 200;
    }
  } else if (ai_ == Ai::None && !talking && now >= nextArmMs_ && Senses::heldUp() && Net::online() &&
             Net::state() != Net::State::Busy && Net::state() != Net::State::Looking &&
             Net::state() != Net::State::Speaking) {
    if (Voice::startRecording()) {
      armed_ = true;
      ai_ = Ai::Recording;
      armedMs_ = now;
      noiseFloor_ = 0;
      loudMs_ = 0;
    }
  }
}

void Brain::stopEars(uint32_t now) {
  if (ai_ == Ai::Recording) Voice::stopRecording();  // thrown away, never sent
  ai_ = Ai::None;
  armed_ = false;
  nextArmMs_ = now + 600;
}

void Brain::handle(const Event& e, uint32_t now) {
  if (mode_ == Mode::GoingOff) return;
  bool playful = e.type != Ev::PutDown && e.type != Ev::FaceUp && e.type != Ev::KeyButton && e.type != Ev::None;
  if (e.type == Ev::KeyButton) Display::setBacklight(BACKLIGHT_AWAKE);  // so the gauge is readable even asleep
  bool busyTalking = mode_ == Mode::Listening || mode_ == Mode::Thinking;
  if (playful) lastPlayMs_ = now;

  // Any play wakes a sleepy Rory.
  if ((mode_ == Mode::Asleep || mode_ == Mode::Sleepy) && playful && e.type != Ev::PowerHold) {
    wakeUp(now);
    return;
  }

  switch (e.type) {
    case Ev::Tap: {
      if (mode_ == Mode::Listening || mode_ == Mode::Thinking) break;
#if TAP_TO_TALK
      // Tap to talk: one tap on her face, she says "Yes?" and listens until he
      // pauses - no need to keep a finger on her. (Offline she just boops.)
      if (Net::online() && Net::state() != Net::State::Busy && Net::state() != Net::State::Looking &&
          Net::state() != Net::State::Speaking) {
        taps_ = 0;
        face_->setAutoLook(false);
        face_->lookAt(0, -0.35f);
        react({Expr::Listening, Moment::YES, nullptr, 250}, now);
        listenNext_ = true;  // starts listening as soon as "Yes?" is said
        break;
      }
#endif
      // three quick boops = love eyes
      if (now - firstTapMs_ > 2000) {
        taps_ = 0;
        firstTapMs_ = now;
      }
      if (++taps_ >= 3) {
        taps_ = 0;
        react({Expr::Love, Moment::S_LOVE, Moment::LOVE, 2600}, now);
      } else {
        face_->lookAt((e.x - 120) / 120.0f, (e.y - 100) / 100.0f);
        react({Expr::Happy, Moment::S_BOOP, Moment::BOOP, 1100}, now);
      }
      break;
    }
    case Ev::Tickle:
      face_->sparkle(1600);
      react({Expr::Happy, Moment::S_TICKLE, Moment::GIGGLE, 2000}, now);
      break;

    case Ev::HoldStart:
      // "Talk to Rory": listen while his finger stays on her face.
      if (mode_ == Mode::Thinking) break;  // still answering the last one
      listenNext_ = false;
      armed_ = false;
      if (mode_ == Mode::Listening && handsFree_) {
        handsFree_ = false;  // already listening: now his finger decides when he's done
        break;
      }
      Voice::stop();
      mode_ = Mode::Listening;
      modeSinceMs_ = now;
      face_->setAutoLook(false);
      face_->lookAt(0, -0.35f);
      face_->setExpression(Expr::Listening);
      showing_ = Expr::Listening;
      if (Net::online() && Net::state() != Net::State::Busy && Net::state() != Net::State::Looking &&
          Net::state() != Net::State::Speaking &&
          Voice::startRecording()) {
        ai_ = Ai::Recording;  // the "o" face is his cue to talk
        Net::prepare();       // open the connection to the server while he talks
      } else {
        ai_ = Ai::None;
        // offline: just the listening face; she explains when he lets go
      }
      break;

    case Ev::HoldEnd:
      if (mode_ != Mode::Listening || handsFree_) break;
      finishListening(now);
      break;

    case Ev::Shake: {
      // The dizzy game: spins in a row make her dizzier - she counts them,
      // and on the third she gets so dizzy she topples over for a nap.
      dizzyLevel_ = (now - lastSpinMs_ < 20000) ? dizzyLevel_ + 1 : 1;
      lastSpinMs_ = now;
      face_->setDizzySpeed(dizzyLevel_ == 1 ? 1.0f : dizzyLevel_ == 2 ? 1.7f : 2.6f);
      if (dizzyLevel_ == 1) {
        react({Expr::Dizzy, Moment::SPIN1, Moment::ONE, 1600}, now);
        then({Expr::Dizzy, Moment::S_DIZZY, Moment::DIZZY, 1200});
      } else if (dizzyLevel_ == 2) {
        // dizzier: bigger wobbly eyes
        react({{Eyes::Dizzy, Mouth::Wobbly, 1.08f, 1.0f, true}, Moment::SPIN2, Moment::TWO, 2600}, now);
      } else {
        dizzyLevel_ = 0;  // start counting again after the nap
        // way too dizzy: huge trembling spirals, mouth wide open
        react({{Eyes::Dizzy, Mouth::Gasp, 1.15f, 1.0f, true}, Moment::SPIN3, Moment::THREE, 1400}, now);
        then({Expr::Dizzy, Moment::S_OOPS, Moment::OOPS, 1800});
        then({Expr::Asleep, nullptr, nullptr, 2600});          // topples over for a little nap
        then({Expr::Surprised, nullptr, nullptr, 450});
        then({Expr::Cheer, Moment::S_AWAKE, Moment::MORNING, 1500});
      }
      break;
    }

    case Ev::PickedUp:
      if (mode_ == Mode::Listening || mode_ == Mode::Thinking) break;
      react({Expr::Surprised, nullptr, nullptr, 450}, now);
      then({Expr::Cheer, Moment::S_UP, Moment::WHEEE, 1500});
      break;

    case Ev::FaceDown:
      react({Expr::Surprised, Moment::S_DARK, Moment::DARK, 1500}, now);
      break;

    case Ev::KeyButton: {  // grown-up button: show the battery gauge
      float v = Power::batteryVolts();
      bool has = Power::hasBattery(v);
      int pct = has ? Power::percent(v) : -1;
      Serial.printf("[brain] battery %.2f V (%d%%)%s\n", v, pct, Power::charging() ? " charging" : "");
      face_->showBattery(pct, Power::charging(), 3500);
      break;
    }

    case Ev::BootButton:  // grown-up button: change the volume, Rory says hi to test it
      Voice::setVolumeStep(Voice::volumeStep() + 1);
      Serial.printf("[brain] volume step %d\n", Voice::volumeStep());
      Display::setBacklight(BACKLIGHT_AWAKE);
      face_->showVolume(Voice::volumeStep() + 1, sizeof(VOLUME_STEPS) / sizeof(VOLUME_STEPS[0]), 2000);
      react({Expr::Cheer, Moment::S_VOLUME, Moment::BOOP, 1200}, now);  // a quick sound at the new volume
      break;

    case Ev::PowerHold:
      mode_ = Mode::GoingOff;
      modeSinceMs_ = now;
      face_->setZzz(false);
      face_->setExpression(Expr::Asleep);
      say(Moment::S_BYE, Moment::BYE);
      break;

    case Ev::HighFive:
      if (busyTalking) break;
      face_->sparkle(1400);
      react({Expr::Cheer, Moment::S_HIGHFIVE, Moment::HIGHFIVE, 1400}, now);
      break;

    case Ev::Dance:
      if (busyTalking) break;
      face_->dance(3200);
      face_->sparkle(3200);
      react({Expr::Happy, Moment::S_DANCE, Moment::DANCE, 3200}, now);
      break;

    case Ev::Walk:
      if (busyTalking) break;
      face_->lookAt(0, 0.4f);  // watching where she's going
      react({Expr::Content, Moment::S_WALK, Moment::WALK, 1500}, now);
      break;

    case Ev::Hug:
      if (busyTalking) break;
      react({Expr::Love, Moment::S_HUG, Moment::LOVE, 2600}, now);
      // every third hug she says hi to her owl friend too
      if (++hugs_ % 3 == 0) then({Expr::Happy, Moment::S_OWL, Moment::OWL, 1200});
      break;

    case Ev::FaceUp:
      if (busyTalking || mode_ == Mode::Asleep || mode_ == Mode::Sleepy) break;
      tuckIn(now);
      break;

    case Ev::PutDown:
    case Ev::None:
      break;
  }
}

void Brain::update(uint32_t now) {
  // Mouth follows Rory's voice whenever she's talking.
  bool talking = Voice::speaking();
  face_->setTalkLevel(talking ? Voice::level() : 0);

  switch (mode_) {
    case Mode::Reacting: {
      // While a clip plays, keep the reaction's eyes but move the mouth.
      if (talking && showing_.eyes != Eyes::Dizzy) {
        Expression e = showing_;
        e.mouth = Mouth::Talk;
        face_->setExpression(e);
      } else {
        face_->setExpression(showing_);
      }
      if (now >= until_ && !talking) {
        if (chainPos_ < chainLen_) {
          show(chain_[chainPos_++], now);
        } else if (listenNext_) {
          listenNext_ = false;
          if (!listenHandsFree(now)) {
            mode_ = Mode::Awake;
            modeSinceMs_ = now;
            face_->setExpression(Expr::Content);
          }
        } else {
          face_->setDizzySpeed(1);
          mode_ = Mode::Awake;
          modeSinceMs_ = now;
          face_->setExpression(Expr::Content);
        }
      }
      break;
    }

    case Mode::Thinking:
      if (ai_ == Ai::Recording && Voice::recordingReady()) {
        if (Voice::recordedSeconds() < 0.4f) {  // just a quick touch - nothing to send
          ai_ = Ai::None;
          react({Expr::Content, nullptr, nullptr, 300}, now);
        } else if (Net::ask(Voice::wav(), Voice::wavSize())) {
          ai_ = Ai::Uploading;
          say(Moment::HMM);  // "Hmm, let me think!" while the server works
        } else {
          react({Expr::Happy, Moment::S_RETRY, Moment::CHAT, 800}, now);
        }
      } else if (ai_ == Ai::Uploading) {
        Net::State st = Net::state();
        if (st == Net::State::Looking && !looking_) {
          // "Rory, look!" - wide eyes, straight ahead, while her camera takes the photo
          looking_ = true;
          say(Net::answer().reading ? Moment::READ : Moment::LOOK, Moment::LOOK);
          face_->setExpression(Expr::Surprised);
          showing_ = Expr::Surprised;
          face_->lookAt(0, 0.1f);
        } else if (st == Net::State::Speaking || st == Net::State::Done) {
          looking_ = false;
          answerArrived(now);
        } else if (st == Net::State::Failed || now - modeSinceMs_ > 30000) {
          looking_ = false;
          // Couldn't reach her brain: a friendly clip instead of silence.
          Net::reset();
          react({Expr::Happy, Moment::S_RETRY, Moment::CHAT, 800}, now);
        } else if (now - modeSinceMs_ > 1500 && !looking_) {
          // "hmm..." - glance around while thinking
          float t = now * 0.002f;
          face_->lookAt(0.5f + 0.3f * sinf(t), -0.75f);
        }
      } else if (ai_ == Ai::None && now >= until_) {
        // Offline: Rory answers with one of her chat clips.
        react({Expr::Talking, Moment::S_OFFLINE, Moment::CHAT, 800}, now);
        showing_ = Expr::Content;
      }
      break;

    case Mode::Listening: {
      Expression e = Expr::Listening;
      if (ai_ == Ai::Recording) {
        // her eyes grow a little when she hears him
        float lvl = Voice::micLevel();
        e.eyeScale += 0.15f * lvl;
        if (!Voice::recording()) {
          finishListening(now);  // 20 s is the most she'll listen
        } else if (handsFree_) {
          uint32_t t = now - modeSinceMs_;
          if (t < 400) {
            if (lvl > noiseFloor_) noiseFloor_ = lvl;  // how loud the room is
          } else {
            float speech = fmaxf(LISTEN_SPEECH_LEVEL, noiseFloor_ * 2.5f);
            if (lvl > speech) {
              heardHim_ = true;
              lastLoudMs_ = now;
            }
            if (heardHim_ && now - lastLoudMs_ > LISTEN_QUIET_MS) {
              Serial.printf("[brain] he finished talking (room %.2f)\n", noiseFloor_);
              finishListening(now);
            } else if (!heardHim_ && t > LISTEN_WAIT_MS) {
              Serial.printf("[brain] no answer (room %.2f) - back to playing\n", noiseFloor_);
              react({Expr::Content, nullptr, nullptr, 300}, now);  // stops the recording
              break;
            }
          }
        }
      } else if (talking) {
        e.mouth = Mouth::Talk;  // the offline "I can hear you" clip
      }
      if (mode_ == Mode::Listening) face_->setExpression(e);
      break;
    }

    case Mode::Awake: {
      int16_t tx, ty;
      if (Senses::touching(tx, ty)) {
        face_->setAutoLook(false);  // eyes follow your finger
        face_->lookAt((tx - 120) / 110.0f, (ty - 100) / 100.0f);
      } else if (Senses::imuOk() && !Senses::resting()) {
        float gx, gy;  // being held: eyes roll "downhill"
        Senses::gravity(gx, gy);
        face_->setAutoLook(false);
        face_->lookAt(gx * 1.4f, gy * 1.4f);
      } else {
        face_->setAutoLook(true);
      }
      if (talking) face_->setExpression(Expr::Talking);
      else face_->setExpression(Expr::Content);
      if (Senses::imuOk() && !Senses::resting()) lastPlayMs_ = now;  // in his hands: not lonely
#if LISTEN_WHEN_HELD
      updateEars(now, talking);
      if (mode_ != Mode::Awake) break;  // she heard him: now listening
#endif
      if (now - lastPlayMs_ > SLEEPY_AFTER_MS && !talking) {
        if (armed_) stopEars(now);
        goSleepy(now);
      }
      break;
    }

    case Mode::Sleepy:
      if (now >= until_) face_->setExpression(Expr::Sleepy);
      if (now - modeSinceMs_ > ASLEEP_AFTER_MS && !talking) {
        say(Moment::S_NIGHT, Moment::GOODNIGHT);
        fallAsleep(now);
      }
      break;

    case Mode::Asleep:
      if (now - modeSinceMs_ > POWER_OFF_AFTER_MS && !Power::charging()) {
        Power::off();  // on USB power this does nothing: she keeps sleeping with her
        modeSinceMs_ = now;  // screen dimly lit, so she never looks switched off
      }
      break;

    case Mode::GoingOff:
      if ((!talking && now - modeSinceMs_ > 800) || now - modeSinceMs_ > 4000) {
        Display::setBacklight(0);
        Display::update();
        Power::off();
        // Still here? We're on USB power - just go to sleep instead.
        fallAsleep(now);
      }
      break;
  }

  // Low battery: once every 10 minutes Rory says she's tired.
  if (now - lastBatteryCheckMs_ > 60000) {
    lastBatteryCheckMs_ = now;
    float v = Power::batteryVolts();
    bool hasBattery = v > 2.8f;
    if (hasBattery && v < 3.45f && !Power::charging() && now - lastTiredMs_ > 600000 &&
        (mode_ == Mode::Awake)) {
      lastTiredMs_ = now;
      react({Expr::Sleepy, Moment::S_TIRED, Moment::TIRED, 2500}, now);
    }
  }
}

#include "brain.h"
#include <Arduino.h>
#include "config.h"
#include "display.h"
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
const char* HIGHFIVE = "highfive";
const char* DANCE = "dance";
const char* WALK = "walk";
const char* TIRED = "tired";
}  // namespace Moment

void Brain::begin(Face& face, uint32_t now) {
  face_ = &face;
  lastPlayMs_ = now;
  face.wake(now);
  face.sparkle(1800);
  react({Expr::Cheer, Moment::HELLO, nullptr, 1800}, now);
}

void Brain::say(const char* moment, const char* fallback) {
  if (!moment) return;
  if (!Voice::say(moment) && fallback) Voice::say(fallback);
}

void Brain::react(const Reaction& r, uint32_t now) {
  // Anything else happening (a shake, a high five...) ends a question.
  if (ai_ == Ai::Recording) Voice::stopRecording();
  ai_ = Ai::None;
  mode_ = Mode::Reacting;
  modeSinceMs_ = now;
  showing_ = r.expr;
  face_->setExpression(r.expr);
  until_ = now + r.ms;
  hasNext_ = false;
  if (r.moment) say(r.moment, r.fallback);
}

void Brain::wakeUp(uint32_t now) {
  Display::setBacklight(BACKLIGHT_AWAKE);
  face_->setZzz(false);
  face_->wake(now);
  react({Expr::Surprised, nullptr, nullptr, 500}, now);
  then({Expr::Cheer, Moment::MORNING, Moment::HELLO, 1500});
}

void Brain::goSleepy(uint32_t now) {
  mode_ = Mode::Sleepy;
  modeSinceMs_ = now;
  face_->setAutoLook(false);
  face_->lookAt(0, 0.3f);
  face_->setExpression(Expr::Yawning);
  showing_ = Expr::Sleepy;
  until_ = now + 1800;  // yawn, then droopy eyes
  say(Moment::YAWN);
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
  if (f == "curious") return Expr::Listening;
  if (f == "thinking") return Expr::Thinking;
  return Expr::Content;  // happy, calm
}

void Brain::answerArrived(uint32_t now) {
  const Net::Answer& a = Net::answer();
  Serial.printf("[brain] he said \"%s\" -> Rory: \"%s\" (%s)\n", a.heard.c_str(), a.say.c_str(), a.face.c_str());
  bool sparkle;
  Expression e = faceFor(a.face, sparkle);
  String url = a.audioUrl;
  Net::reset();
  react({e, nullptr, nullptr, 600}, now);  // the mouth moves while her answer plays
  if (sparkle) face_->sparkle(2500);
  face_->lookAt(0, 0);
  face_->setAutoLook(false);
  if (!Voice::playUrl(url.c_str())) say(Moment::CHAT, Moment::GIGGLE);
  lastPlayMs_ = now;
}

void Brain::handle(const Event& e, uint32_t now) {
  if (mode_ == Mode::GoingOff) return;
  bool playful = e.type != Ev::PutDown && e.type != Ev::FaceUp && e.type != Ev::None;
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
      // three quick boops = love eyes
      if (now - firstTapMs_ > 2000) {
        taps_ = 0;
        firstTapMs_ = now;
      }
      if (++taps_ >= 3) {
        taps_ = 0;
        react({Expr::Love, Moment::LOVE, Moment::GIGGLE, 2600}, now);
      } else {
        face_->lookAt((e.x - 120) / 120.0f, (e.y - 100) / 100.0f);
        react({Expr::Happy, Moment::BOOP, Moment::GIGGLE, 1100}, now);
      }
      break;
    }
    case Ev::Tickle:
      face_->sparkle(1600);
      react({Expr::Happy, Moment::GIGGLE, nullptr, 2000}, now);
      break;

    case Ev::HoldStart:
      // "Talk to Rory": listen while his finger stays on her face.
      if (mode_ == Mode::Thinking) break;  // still answering the last one
      Voice::stop();
      mode_ = Mode::Listening;
      modeSinceMs_ = now;
      face_->setAutoLook(false);
      face_->lookAt(0, -0.35f);
      face_->setExpression(Expr::Listening);
      showing_ = Expr::Listening;
      if (Net::online() && Net::state() != Net::State::Busy && Voice::startRecording()) {
        ai_ = Ai::Recording;  // the "o" face is his cue to talk
      } else {
        ai_ = Ai::None;
        say(Moment::LISTEN);  // offline: "Rory can hear you now!" and a chat clip after
      }
      break;

    case Ev::HoldEnd:
      if (mode_ != Mode::Listening) break;
      finishListening(now);
      break;

    case Ev::Shake:
      react({Expr::Dizzy, Moment::DIZZY, nullptr, 2800}, now);
      break;

    case Ev::PickedUp:
      if (mode_ == Mode::Listening || mode_ == Mode::Thinking) break;
      react({Expr::Surprised, nullptr, nullptr, 450}, now);
      then({Expr::Cheer, Moment::WHEEE, Moment::HELLO, 1500});
      break;

    case Ev::FaceDown:
      react({Expr::Surprised, Moment::DARK, nullptr, 1500}, now);
      break;

    case Ev::BootButton:  // grown-up button: change the volume, Rory says hi to test it
      Voice::setVolumeStep(Voice::volumeStep() + 1);
      Serial.printf("[brain] volume step %d\n", Voice::volumeStep());
      react({Expr::Cheer, Moment::HELLO, nullptr, 1200}, now);
      break;

    case Ev::PowerHold:
      mode_ = Mode::GoingOff;
      modeSinceMs_ = now;
      face_->setZzz(false);
      face_->setExpression(Expr::Asleep);
      say(Moment::BYE, Moment::GOODNIGHT);
      break;

    case Ev::HighFive:
      if (busyTalking) break;
      face_->sparkle(1400);
      react({Expr::Cheer, Moment::HIGHFIVE, Moment::GIGGLE, 1400}, now);
      break;

    case Ev::Dance:
      if (busyTalking) break;
      face_->dance(3200);
      face_->sparkle(3200);
      react({Expr::Happy, Moment::DANCE, Moment::GIGGLE, 3200}, now);
      break;

    case Ev::Walk:
      if (busyTalking) break;
      face_->lookAt(0, 0.4f);  // watching where she's going
      react({Expr::Content, Moment::WALK, nullptr, 1500}, now);
      break;

    case Ev::Hug:
      if (busyTalking) break;
      react({Expr::Love, Moment::LOVE, Moment::GIGGLE, 2600}, now);
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
        if (hasNext_) {
          react(next_, now);
        } else {
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
        } else {
          react({Expr::Happy, Moment::CHAT, Moment::GIGGLE, 800}, now);
        }
      } else if (ai_ == Ai::Uploading) {
        Net::State st = Net::state();
        if (st == Net::State::Done) {
          answerArrived(now);
        } else if (st == Net::State::Failed || now - modeSinceMs_ > 25000) {
          // Couldn't reach her brain: a friendly clip instead of silence.
          Net::reset();
          react({Expr::Happy, Moment::CHAT, Moment::GIGGLE, 800}, now);
        } else if (now - modeSinceMs_ > 1500) {
          // "hmm..." - glance around while thinking
          float t = now * 0.002f;
          face_->lookAt(0.5f + 0.3f * sinf(t), -0.75f);
        }
      } else if (ai_ == Ai::None && now >= until_) {
        // Offline: Rory answers with one of her chat clips.
        react({Expr::Talking, Moment::CHAT, Moment::GIGGLE, 800}, now);
        showing_ = Expr::Content;
      }
      break;

    case Mode::Listening: {
      Expression e = Expr::Listening;
      if (ai_ == Ai::Recording) {
        // her eyes grow a little when she hears him
        e.eyeScale += 0.15f * Voice::micLevel();
        if (!Voice::recording()) finishListening(now);  // 12 s is the most she'll listen
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
      if (now - lastPlayMs_ > SLEEPY_AFTER_MS && !talking) goSleepy(now);
      break;
    }

    case Mode::Sleepy:
      if (now >= until_) face_->setExpression(Expr::Sleepy);
      if (now - modeSinceMs_ > ASLEEP_AFTER_MS && !talking) {
        say(Moment::GOODNIGHT);
        fallAsleep(now);
      }
      break;

    case Mode::Asleep:
      if (now - modeSinceMs_ > POWER_OFF_AFTER_MS && !Power::charging()) {
        Display::setBacklight(0);
        Power::off();  // on USB power this does nothing; she just keeps sleeping
        modeSinceMs_ = now;
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
      react({Expr::Sleepy, Moment::TIRED, nullptr, 2500}, now);
    }
  }
}

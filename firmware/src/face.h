// Rory's animated face: expressions, blinking, looking around, lip sync.
// Plain C++ so it can be previewed on a PC (tools/face-preview).
#pragma once
#include <stdint.h>
#include "raster.h"

enum class Eyes : uint8_t {
  Normal,  // big cyan eyes with a white sparkle (like the app)
  Happy,   // ^ ^
  Closed,  // sleeping curves
  Dizzy,   // spinning spirals
  Heart,   // love eyes
};

enum class Mouth : uint8_t {
  Smile,   // the app's resting cyan smile
  Grin,    // big open red smile with tongue (cheering)
  O,       // little "o" (listening)
  Flat,    // thinking / unsure
  Wobbly,  // dizzy squiggle
  Talk,    // opens and closes with Rory's voice
  Yawn,    // big tall "O"
};

struct Expression {
  Eyes eyes;
  Mouth mouth;
  float eyeScale;  // 1 = normal, >1 = surprised
  float eyeOpen;   // 1 = fully open, ~0.4 = sleepy
};

namespace Expr {
static const Expression Content = {Eyes::Normal, Mouth::Smile, 1.00f, 1.0f};
static const Expression Happy = {Eyes::Happy, Mouth::Grin, 1.00f, 1.0f};
static const Expression Cheer = {Eyes::Normal, Mouth::Grin, 1.05f, 1.0f};
static const Expression Listening = {Eyes::Normal, Mouth::O, 1.08f, 1.0f};
static const Expression Thinking = {Eyes::Normal, Mouth::Flat, 0.95f, 1.0f};
static const Expression Surprised = {Eyes::Normal, Mouth::O, 1.15f, 1.0f};
static const Expression Dizzy = {Eyes::Dizzy, Mouth::Wobbly, 1.00f, 1.0f};
static const Expression Sleepy = {Eyes::Normal, Mouth::Flat, 1.00f, 0.45f};
static const Expression Yawning = {Eyes::Closed, Mouth::Yawn, 1.00f, 1.0f};
static const Expression Asleep = {Eyes::Closed, Mouth::Smile, 1.00f, 1.0f};
static const Expression Talking = {Eyes::Normal, Mouth::Talk, 1.00f, 1.0f};
static const Expression Love = {Eyes::Heart, Mouth::Smile, 1.00f, 1.0f};
}  // namespace Expr

class Face {
 public:
  void begin(uint32_t nowMs);
  void setExpression(const Expression& e);
  const Expression& expression() const { return target_; }

  // Where to look: x, y in -1..1 (right / down positive).
  void lookAt(float x, float y);
  void setAutoLook(bool on) { autoLook_ = on; }  // idle glances around
  void setTalkLevel(float level) { talkLevel_ = level; }  // 0..1 from the speaker
  void setZzz(bool on) { zzz_ = on; }
  void sparkle(uint32_t durationMs);  // yellow twinkles (cheering)
  void dance(uint32_t durationMs);    // eyes bop side to side to the beat
  void setDizzySpeed(float s) { dizzySpeed_ = s; }  // spiral speed (dizzy game)
  void blink();
  void wake(uint32_t nowMs);  // eyes pop open from closed

  void update(uint32_t nowMs);
  void render(Raster& r);

 private:
  void drawEye(Raster& r, float cx, float cy, bool left);
  void drawMouth(Raster& r, float cx, float cy);
  void drawZzz(Raster& r);
  void drawSparkles(Raster& r);
  float rnd();  // 0..1

  Expression target_ = Expr::Content;
  Eyes eyesShown_ = Eyes::Normal;
  Eyes eyesPending_ = Eyes::Normal;
  bool swapPending_ = false;

  float lookX_ = 0, lookY_ = 0, lookTX_ = 0, lookTY_ = 0;
  float scale_ = 1, open_ = 1;
  float talkLevel_ = 0, mouthOpen_ = 0;
  float dizzyAngle_ = 0, dizzySpeed_ = 1;
  float bob_ = 0;

  bool autoLook_ = true;
  uint32_t nextGlanceMs_ = 0;
  uint32_t lastMs_ = 0;
  uint32_t nowMs_ = 0;

  uint32_t blinkStartMs_ = 0;
  bool blinking_ = false;
  uint32_t nextBlinkMs_ = 0;
  float blinkAmt_ = 0;  // 0 open .. 1 closed

  bool zzz_ = false;
  uint32_t sparkleUntilMs_ = 0;
  uint32_t danceUntilMs_ = 0;
  uint32_t seed_ = 0x9E3779B9u;

  static const int kSpeckles = 650;
  uint8_t speckX_[kSpeckles], speckY_[kSpeckles];
};

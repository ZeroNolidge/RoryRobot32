#include "face.h"
#include "config.h"

namespace {
const float kPi = 3.14159265f;

// Face layout on the 240x240 screen
const float kEyeLX = 78, kEyeRX = 162, kEyeY = 100;
const float kEyeR = 33;          // eye radius
const float kOutline = 3.0f;     // white rim
const float kLookRangeX = 15, kLookRangeY = 11;
const float kMouthX = 120, kMouthY = 166;

float approach(float v, float target, float dt, float speed) {
  float k = 1.0f - expf(-dt * speed);
  return v + (target - v) * k;
}
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint16_t C(uint32_t hex) { return Raster::rgb(hex); }
}  // namespace

float Face::rnd() {
  seed_ ^= seed_ << 13;
  seed_ ^= seed_ >> 17;
  seed_ ^= seed_ << 5;
  return (seed_ & 0xFFFFFF) / 16777216.0f;
}

void Face::begin(uint32_t nowMs) {
  for (int i = 0; i < kSpeckles; i++) {
    speckX_[i] = (uint8_t)(rnd() * 240);
    speckY_[i] = (uint8_t)(rnd() * 240);
  }
  lastMs_ = nowMs_ = nowMs;
  nextBlinkMs_ = nowMs + 1500;
  nextGlanceMs_ = nowMs + 1200;
}

void Face::setExpression(const Expression& e) {
  bool eyesChange = e.eyes != target_.eyes;
  target_ = e;
  if (eyesChange) {
    // Swap eye shapes while the eyes are shut, like a real blink.
    eyesPending_ = e.eyes;
    swapPending_ = true;
    blink();
  }
}

void Face::lookAt(float x, float y) {
  lookTX_ = clampf(x, -1, 1);
  lookTY_ = clampf(y, -1, 1);
}

void Face::sparkle(uint32_t durationMs) { sparkleUntilMs_ = nowMs_ + durationMs; }

void Face::dance(uint32_t durationMs) { danceUntilMs_ = nowMs_ + durationMs; }

void Face::blink() {
  if (blinking_) return;
  blinking_ = true;
  blinkStartMs_ = nowMs_;
}

void Face::wake(uint32_t nowMs) {
  blinkAmt_ = 1;
  blinking_ = true;
  blinkStartMs_ = nowMs - 80;  // start from the "closed" half of a blink
}

void Face::update(uint32_t nowMs) {
  float dt = (nowMs - lastMs_) / 1000.0f;
  if (dt > 0.2f) dt = 0.2f;
  lastMs_ = nowMs_ = nowMs;

  // --- blinking (160 ms close+open; sleepier = slower)
  const uint32_t kBlinkMs = target_.eyeOpen < 0.6f ? 420 : 170;
  if (!blinking_ && nowMs >= nextBlinkMs_ && target_.eyes != Eyes::Closed) blink();
  if (blinking_) {
    float p = (nowMs - blinkStartMs_) / (float)kBlinkMs;
    if (p >= 0.5f && swapPending_) {
      eyesShown_ = eyesPending_;
      swapPending_ = false;
    }
    if (p >= 1.0f) {
      blinking_ = false;
      blinkAmt_ = 0;
      // Natural rhythm: every 2-6 s, sometimes a quick double blink.
      nextBlinkMs_ = nowMs + (rnd() < 0.15f ? 180 : (uint32_t)(2000 + rnd() * 4000));
    } else {
      blinkAmt_ = sinf(p * kPi);
    }
  }
  if (!blinking_ && swapPending_) {
    eyesShown_ = eyesPending_;
    swapPending_ = false;
  }

  // --- idle glances
  if (autoLook_ && nowMs >= nextGlanceMs_) {
    if (rnd() < 0.45f) {
      lookTX_ = lookTY_ = 0;  // back to looking at you
    } else {
      lookTX_ = rnd() * 1.6f - 0.8f;
      lookTY_ = rnd() * 1.0f - 0.5f;
    }
    nextGlanceMs_ = nowMs + (uint32_t)(900 + rnd() * 2600);
  }

  // --- dancing: eyes swing side to side, face hops
  bool dancing = nowMs < danceUntilMs_;
  if (dancing) {
    float beat = nowMs * (2 * kPi / 600.0f);
    lookTX_ = sinf(beat) * 0.8f;
    lookTY_ = -0.2f;
  }

  // --- smooth everything
  lookX_ = approach(lookX_, lookTX_, dt, 14);
  lookY_ = approach(lookY_, lookTY_, dt, 14);
  scale_ = approach(scale_, target_.eyeScale, dt, 10);
  open_ = approach(open_, target_.eyeOpen, dt, 4);
  float mouthTarget = target_.mouth == Mouth::Talk ? clampf(talkLevel_, 0, 1) : 0;
  mouthOpen_ = approach(mouthOpen_, mouthTarget, dt, 28);
  dizzyAngle_ += dt * 540.0f;
  if (dizzyAngle_ > 360) dizzyAngle_ -= 360;
  bob_ = dancing ? -fabsf(sinf(nowMs * (2 * kPi / 1200.0f))) * 7.0f
                : sinf(nowMs * (2 * kPi / 3200.0f)) * 2.0f;  // hop, or gentle "breathing"
}

void Face::render(Raster& r) {
  r.clear(C(COL_SCREEN));
  uint16_t sp = C(COL_SPECKLE);
  for (int i = 0; i < kSpeckles; i++) r.dot(speckX_[i], speckY_[i], sp);

  float lx = lookX_ * kLookRangeX, ly = lookY_ * kLookRangeY + bob_;
  drawEye(r, kEyeLX + lx, kEyeY + ly, true);
  drawEye(r, kEyeRX + lx, kEyeY + ly, false);
  drawMouth(r, kMouthX + lx * 0.45f, kMouthY + ly * 0.5f);
  if (zzz_) drawZzz(r);
  if (nowMs_ < sparkleUntilMs_) drawSparkles(r);
}

void Face::drawEye(Raster& r, float cx, float cy, bool left) {
  uint16_t eye = C(COL_EYE), white = C(COL_WHITE);
  float R = kEyeR * scale_;
  switch (eyesShown_) {
    case Eyes::Normal: {
      // Eyelids come down from the top (blinks, sleepy eyes).
      float open = clampf(open_ * (1.0f - blinkAmt_), 0, 1);
      float top = cy - R - kOutline;
      float lid = top + (2 * R + 2 * kOutline) * (1.0f - open) * 0.78f;
      if (open < 0.1f) {  // shut: a soft line
        r.capsule(cx - R * 0.8f, lid, cx + R * 0.8f, lid, 3.5f, eye);
        break;
      }
      float ro = R + kOutline;
      r.shade(cx - ro, lid, cx + ro, cy + ro, white, 1.0f, [=](float x, float y) {
        float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - ro;
        return d > lid - y ? d : lid - y;
      });
      float lid2 = lid + kOutline;
      r.shade(cx - R, lid2, cx + R, cy + R, eye, 1.0f, [=](float x, float y) {
        float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - R;
        return d > lid2 - y ? d : lid2 - y;
      });
      float g = R * 0.27f, gx = cx + R * 0.36f, gy = cy - R * 0.36f;
      if (gy - g > lid2 + 1) r.circle(gx, gy, g, white);
      break;
    }
    case Eyes::Happy:  // ^ ^
      r.arc(cx, cy + 12, R * 0.78f, 9, 200, 340, eye);
      break;
    case Eyes::Closed:  // sleeping: gentle downward curves
      r.arc(cx, cy - 10, R * 0.72f, 7, 25, 155, eye);
      break;
    case Eyes::Dizzy: {
      float spin = (left ? dizzyAngle_ : -dizzyAngle_) * kPi / 180.0f;
      float px = cx, py = cy;
      for (int i = 1; i <= 34; i++) {
        float t = i * 0.28f;
        float rad = 2.5f + t * 2.9f;
        float a = (left ? t : -t) + spin;
        float nx = cx + cosf(a) * rad, ny = cy + sinf(a) * rad;
        r.capsule(px, py, nx, ny, 3.2f, eye);
        px = nx;
        py = ny;
      }
      break;
    }
    case Eyes::Heart: {
      float pulse = 1.0f + 0.08f * sinf(nowMs_ * 0.012f);
      r.heart(cx, cy, R * 0.95f * pulse, C(COL_HEART));
      break;
    }
  }
}

void Face::drawMouth(Raster& r, float cx, float cy) {
  uint16_t eye = C(COL_EYE), red = C(COL_MOUTH), tongue = C(COL_TONGUE), bg = C(COL_SCREEN);
  switch (target_.mouth) {
    case Mouth::Smile:
      r.arc(cx, cy - 26, 30, 7, 35, 145, eye);
      break;
    case Mouth::Grin: {
      float top = cy - 8, rx = 38, ry = 34;
      r.halfEllipseDown(cx, top, rx, ry, red);
      // tongue, kept inside the mouth
      r.shade(cx - rx, top, cx + rx, top + ry, tongue, 1.0f, [=](float x, float y) {
        float t = Raster::ellipseDist(x - cx, y - (top + ry * 0.92f), rx * 0.55f, ry * 0.42f);
        float m = Raster::ellipseDist(x - cx, y - top, rx, ry);
        float edge = top - y;
        float mouth = m > edge ? m : edge;
        return t > mouth ? t : mouth;
      });
      break;
    }
    case Mouth::Talk: {
      // closed = a smile; opens into the red grin with Rory's voice
      float o = mouthOpen_;
      if (o < 0.08f) {
        r.arc(cx, cy - 26, 30, 7, 35, 145, eye);
        break;
      }
      float top = cy - 6, rx = 24 + 12 * o, ry = 8 + 26 * o;
      r.halfEllipseDown(cx, top, rx, ry, red);
      if (o > 0.35f) {
        r.shade(cx - rx, top, cx + rx, top + ry, tongue, 1.0f, [=](float x, float y) {
          float t = Raster::ellipseDist(x - cx, y - (top + ry * 0.95f), rx * 0.55f, ry * 0.42f);
          float m = Raster::ellipseDist(x - cx, y - top, rx, ry);
          float edge = top - y;
          float mouth = m > edge ? m : edge;
          return t > mouth ? t : mouth;
        });
      }
      break;
    }
    case Mouth::O:
      r.ellipse(cx, cy, 15, 12, eye);
      r.ellipse(cx, cy, 8.5f, 6, bg);
      break;
    case Mouth::Yawn:
      r.ellipse(cx, cy + 4, 20, 26, eye);
      r.ellipse(cx, cy + 4, 14, 20, red);
      break;
    case Mouth::Flat:
      r.capsule(cx - 18, cy, cx + 18, cy, 3.5f, eye);
      break;
    case Mouth::Wobbly: {
      float px = cx - 28, py = cy;
      for (int i = 1; i <= 16; i++) {
        float x = cx - 28 + i * 3.5f;
        float y = cy + sinf(i * 0.8f + nowMs_ * 0.01f) * 5;
        r.capsule(px, py, x, y, 3.2f, eye);
        px = x;
        py = y;
      }
      break;
    }
  }
}

void Face::drawZzz(Raster& r) {
  uint16_t eye = C(COL_EYE);
  // three Zs drifting up and fading, staggered
  for (int i = 0; i < 3; i++) {
    float p = fmodf(nowMs_ / 2600.0f + i / 3.0f, 1.0f);
    float s = 7 + 6 * p;
    float x = 182 + p * 26, y = 84 - p * 46;
    float a = p < 0.15f ? p / 0.15f : (1.0f - p) / 0.85f;
    r.capsule(x - s, y - s, x + s, y - s, 2.2f, eye, a);
    r.capsule(x + s, y - s, x - s, y + s, 2.2f, eye, a);
    r.capsule(x - s, y + s, x + s, y + s, 2.2f, eye, a);
  }
}

void Face::drawSparkles(Raster& r) {
  uint16_t star = C(COL_STAR);
  static const float pos[6][2] = {{28, 36}, {212, 40}, {20, 150}, {220, 160}, {60, 214}, {186, 214}};
  for (int i = 0; i < 6; i++) {
    float tw = sinf(nowMs_ * 0.012f + i * 1.7f);
    float s = 6 + 5 * (tw * 0.5f + 0.5f);
    r.star(pos[i][0], pos[i][1], s, star);
  }
}

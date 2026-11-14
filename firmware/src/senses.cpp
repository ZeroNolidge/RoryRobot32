#include "senses.h"
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>
#include <TouchDrvCSTXXX.hpp>
#include "config.h"

namespace {
// ------------------------------------------------------------ event queue
// Touch and buttons are read in the main loop; motion in its own fast task.
// Both feed this thread-safe queue.
QueueHandle_t events = nullptr;
#if MOTION_DEBUG
const char* kEvName[] = {"None", "Tap", "HoldStart", "HoldEnd", "Tickle", "Shake", "PickedUp", "PutDown", "FaceDown",
                         "FaceUp", "HighFive", "Dance", "Walk", "Hug", "BootButton", "PowerHold"};
#endif
void push(Ev t, int16_t x = 0, int16_t y = 0) {
  if (!events) return;
  Event e;
  e.type = t;
  e.x = x;
  e.y = y;
  xQueueSend(events, &e, 0);  // full: drop
#if MOTION_DEBUG
  Serial.printf("[event] %s\n", kEvName[(int)t]);
#endif
}

// ------------------------------------------------------------ touch
TouchDrvCSTXXX touch;
bool touchReady = false;
struct {
  bool down = false;
  uint32_t downMs = 0, lastSeenMs = 0, lastTickleMs = 0;
  int16_t x = 0, y = 0, sx = 0, sy = 0;
  float path = 0;
  int dir = 0, reversals = 0;
  bool holdFired = false;
} t;

void pollTouch(uint32_t now) {
  if (!touchReady) return;
  int16_t xs[1], ys[1];
  bool on = touch.getPoint(xs, ys, 1) > 0;
  if (on) {
    int16_t x = xs[0], y = ys[0];
    if (TOUCH_SWAP_XY) { int16_t tmp = x; x = y; y = tmp; }
    if (TOUCH_MIRROR_X) x = 239 - x;
    if (TOUCH_MIRROR_Y) y = 239 - y;
    if (!t.down) {
      t.down = true;
      t.downMs = now;
      t.sx = t.x = x;
      t.sy = t.y = y;
      t.path = 0;
      t.dir = 0;
      t.reversals = 0;
      t.holdFired = false;
    } else {
      int dx = x - t.x, dy = y - t.y;
      t.path += abs(dx) + abs(dy);
      if (abs(dx) > 3) {
        int d = dx > 0 ? 1 : -1;
        if (t.dir != 0 && d != t.dir) t.reversals++;
        t.dir = d;
      }
      t.x = x;
      t.y = y;
    }
    t.lastSeenMs = now;
    if (!t.holdFired && now - t.downMs >= 700 && t.path < 30) {
      t.holdFired = true;
      push(Ev::HoldStart, t.x, t.y);
    }
    if (!t.holdFired && t.path > 200 && t.reversals >= 3 && now - t.lastTickleMs > 1500) {
      t.lastTickleMs = now;
      t.path = 0;
      t.reversals = 0;
      push(Ev::Tickle, t.x, t.y);
    }
  } else if (t.down && now - t.lastSeenMs > 60) {  // debounce dropped samples
    t.down = false;
    if (t.holdFired) push(Ev::HoldEnd, t.x, t.y);
    else if (t.lastSeenMs - t.downMs < 450 && t.path < 30) push(Ev::Tap, t.sx, t.sy);
  }
}

// ------------------------------------------------------------ motion
SensorQMI8658 imu;
bool imuReady = false;
float gx = 0, gy = 0, gz = 1;  // low-passed gravity (g)
uint32_t lastImuMs = 0, stillMs = 0;
bool isResting = false;
uint32_t jolts[6] = {0};
int joltIdx = 0;
uint32_t lastJoltMs = 0, lastShakeMs = 0, lastPickupMs = 0;
uint32_t faceDownSinceMs = 0;
bool faceDownFired = false;
uint32_t faceUpSinceMs = 0;
bool faceUpFired = false;

// pick-up check
float restGx = 0, restGy = 0, restGz = 1;
uint32_t liftCheckMs = 0;
bool liftSpin = false;

// taps / high fives
bool tapAbove = false, tapPending = false;
uint32_t tapMs = 0, calmSinceMs = 0;

// rhythm (dance / walk): times and sizes of recent bounces
bool bounceAbove = false;
float bouncePeak = 0;
uint32_t bounceTimes[6] = {0};
float bounceSizes[6] = {0};
int bounceIdx = 0;
uint32_t lastDanceMs = 0, lastWalkMs = 0;

// hugs
uint32_t heldCalmSinceMs = 0;
bool hugFired = false;

void checkRhythm(uint32_t now) {
  // Look at the last 5 bounces: are they evenly spaced, like a dance or steps?
  uint32_t t[5];
  float size = 0;
  for (int i = 0; i < 5; i++) {
    int k = (bounceIdx - 1 - i + 6) % 6;
    t[i] = bounceTimes[k];
    size += bounceSizes[k];
    if (!t[i]) return;
  }
  size /= 5;
  uint32_t lo = 100000, hi = 0;
  for (int i = 0; i < 4; i++) {
    uint32_t gap = t[i] - t[i + 1];
    if (gap < lo) lo = gap;
    if (gap > hi) hi = gap;
  }
  if (lo < 250 || hi > 900 || hi > lo * 1.6f) return;  // not a steady beat
  if (now - lastShakeMs < 2000) return;
  if (size >= 0.6f) {
    if (now - lastDanceMs > 4000) {
      lastDanceMs = now;
      push(Ev::Dance);
    }
  } else if (now - lastWalkMs > 4000 && now - lastDanceMs > 4000) {
    lastWalkMs = now;
    push(Ev::Walk);
  }
}

float axisOf(float x, float y, float z, int axis) { return axis == 0 ? x : (axis == 1 ? y : z); }

void pollImu(uint32_t now) {
  if (!imuReady || !imu.getDataReady()) return;
  float ax, ay, az, rx, ry, rz;
  if (!imu.getAccelerometer(ax, ay, az)) return;
  imu.getGyroscope(rx, ry, rz);
  float dt = lastImuMs ? (now - lastImuMs) / 1000.0f : 0.01f;
  lastImuMs = now;
  if (dt > 0.2f) dt = 0.2f;

  float k = 1.0f - expf(-dt * 6.0f);  // ~0.17 s gravity filter
  gx += (ax - gx) * k;
  gy += (ay - gy) * k;
  gz += (az - gz) * k;
  float dx = ax - gx, dy = ay - gy, dz = az - gz;
  float dyn = sqrtf(dx * dx + dy * dy + dz * dz);       // jerkiness (g)
  float spin = sqrtf(rx * rx + ry * ry + rz * rz);      // turning (deg/s)

  // --- shake: 4 hard jolts within 1.5 s
  if (dyn > 0.9f && now - lastJoltMs > 110) {
    lastJoltMs = now;
    jolts[joltIdx] = now;
    joltIdx = (joltIdx + 1) % 6;
    int recent = 0;
    for (uint32_t j : jolts) if (j && now - j < 1500) recent++;
    if (recent >= 4 && now - lastShakeMs > 3000) {
      lastShakeMs = now;
      push(Ev::Shake);
    }
  }

  // --- resting / picked up / put down
  // Leaving "resting" only counts as a pick-up if she then really turns or
  // tilts; a tap on her while she sits on the table is a high five instead.
  if (dyn < 0.04f && spin < 3.0f) stillMs += (uint32_t)(dt * 1000);  // gyro reads ~1.4 when still
  else stillMs = 0;
  if (!isResting && stillMs > 2500) {
    isResting = true;
    restGx = gx;
    restGy = gy;
    restGz = gz;
    push(Ev::PutDown);
  } else if (isResting && (spin > 45 || dyn > 0.25f)) {
    isResting = false;
    liftCheckMs = now;
    liftSpin = false;
  }
  if (liftCheckMs) {
    if (spin > 40 && now - liftCheckMs > 80) liftSpin = true;
    if (now - liftCheckMs > 500) {
      float tilt = fabsf(gx - restGx) + fabsf(gy - restGy) + fabsf(gz - restGz);
      liftCheckMs = 0;
      if ((liftSpin || tilt > 0.15f) && now - lastPickupMs > 4000 && now - lastShakeMs > 2000) {
        lastPickupMs = now;
        push(Ev::PickedUp);
      }
    }
  }

  // --- high five: one clear jolt after a calm moment, with no second jolt
  //     right after, and not from a finger on the screen
  uint32_t calmFor = calmSinceMs ? now - calmSinceMs : 0;  // measured before this sample
  if (dyn < 0.12f) {
    if (!calmSinceMs) calmSinceMs = now;
  } else if (dyn > 0.25f) {
    calmSinceMs = 0;
  }
  if (!tapAbove && dyn > 0.45f) {
    tapAbove = true;
    if (tapPending && now - tapMs > 100) tapPending = false;  // a second jolt: bouncing or shaking
    else if (!tapPending && calmFor > 300) {
      tapPending = true;
      tapMs = now;
    }
  } else if (tapAbove && dyn < 0.25f) {
    tapAbove = false;
  }
  if (tapPending && now - tapMs > 600) {
    tapPending = false;
    bool screenTouched = t.down || now - t.lastSeenMs < 600;
    bool lifted = now - lastPickupMs < 1200;
    if (!screenTouched && !lifted && now - lastShakeMs > 2000) push(Ev::HighFive);
  }

  // --- rhythm: count bounces and check if they keep a steady beat
  if (!bounceAbove && dyn > 0.25f) {
    bounceAbove = true;
    bouncePeak = dyn;
  } else if (bounceAbove) {
    if (dyn > bouncePeak) bouncePeak = dyn;
    if (dyn < 0.15f) {
      bounceAbove = false;
      bounceTimes[bounceIdx] = now;
      bounceSizes[bounceIdx] = bouncePeak;
      bounceIdx = (bounceIdx + 1) % 6;
      checkRhythm(now);
    }
  }

  // --- hug: held (not on the table) and kept calm for a few seconds
  if (!isResting && dyn < 0.15f && spin < 30) {
    if (!heldCalmSinceMs) heldCalmSinceMs = now;
    if (!hugFired && now - heldCalmSinceMs > 3000 && now - lastPickupMs < 60000) {
      hugFired = true;
      push(Ev::Hug);
    }
  } else if (dyn > 0.3f || isResting) {
    heldCalmSinceMs = 0;
    if (isResting) hugFired = false;
  }

  // --- screen facing down
  float out = axisOf(gx, gy, gz, IMU_SCREEN_AXIS) * IMU_SCREEN_SIGN;
  if (out < -0.75f) {
    if (!faceDownSinceMs) faceDownSinceMs = now;
    if (!faceDownFired && now - faceDownSinceMs > 700) {
      faceDownFired = true;
      push(Ev::FaceDown);
    }
  } else if (out > -0.3f) {
    faceDownSinceMs = 0;
    faceDownFired = false;
  }

  // --- lying on her back, still
  if (out > 0.8f && dyn < 0.08f) {
    if (!faceUpSinceMs) faceUpSinceMs = now;
    if (!faceUpFired && now - faceUpSinceMs > 1500) {
      faceUpFired = true;
      push(Ev::FaceUp);
    }
  } else if (out < 0.5f) {
    faceUpSinceMs = 0;
    faceUpFired = false;
  }

#if MOTION_DEBUG
  static uint32_t lastLog = 0;
  static float peakDyn = 0, peakSpin = 0;
  if (dyn > peakDyn) peakDyn = dyn;
  if (spin > peakSpin) peakSpin = spin;
  if (now - lastLog > 100) {  // biggest values in the last 0.1 s, so quick taps show up
    lastLog = now;
    Serial.printf("[motion] dyn=%.2f spin=%5.1f screen=%+.2f resting=%d\n", peakDyn, peakSpin, out, isResting);
    peakDyn = peakSpin = 0;
  }
#endif
}

// Read the motion sensor ~250 times a second in its own task, so quick taps
// and bounces aren't missed while the face is being drawn.
void imuTask(void*) {
  for (;;) {
    pollImu(millis());
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

// ------------------------------------------------------------ buttons
struct Button {
  int pin;
  bool wasDown = false, seenUp = false, longFired = false;
  uint32_t downMs = 0;
};
Button boot{PIN_BTN_BOOT}, pwr{PIN_BTN_PWR};

void pollButton(Button& b, uint32_t now, bool isPower) {
  bool down = digitalRead(b.pin) == LOW;
  if (!down) b.seenUp = true;  // ignore the press that switched Rory on
  if (!b.seenUp) return;
  if (down && !b.wasDown) {
    b.downMs = now;
    b.longFired = false;
  }
  if (down && isPower && !b.longFired && now - b.downMs > 1500) {
    b.longFired = true;
    push(Ev::PowerHold);
  }
  if (!down && b.wasDown && !isPower && now - b.downMs > 30 && now - b.downMs < 1000) push(Ev::BootButton);
  b.wasDown = down;
}
}  // namespace

namespace Senses {

bool begin() {
  events = xQueueCreate(16, sizeof(Event));
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  touch.setPins(PIN_TOUCH_RST, PIN_TOUCH_INT);
  touchReady = touch.begin(Wire, CST816_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (touchReady) touch.setMaxCoordinates(240, 240);
  else Serial.println("[senses] touch panel not found");

  imuReady = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (imuReady) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_250Hz, SensorQMI8658::LPF_MODE_3);
    imu.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_224_2Hz, SensorQMI8658::LPF_MODE_3);
    imu.enableAccelerometer();
    imu.enableGyroscope();
  } else {
    Serial.println("[senses] motion sensor not found");
  }

  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);
  // same core as the main loop, higher priority, so it runs between drawing steps
  if (imuReady) xTaskCreatePinnedToCore(imuTask, "motion", 4096, nullptr, 2, nullptr, 1);
  return touchReady && imuReady;
}

void poll(uint32_t now) {
  pollTouch(now);
  pollButton(boot, now, false);
  pollButton(pwr, now, true);
}

bool next(Event& e) { return events && xQueueReceive(events, &e, 0) == pdTRUE; }

bool touching(int16_t& x, int16_t& y) {
  if (!t.down) return false;
  x = t.x;
  y = t.y;
  return true;
}

void gravity(float& x, float& y) {
  // Which way is "down" in screen terms. Default axis mapping; flip signs in config.h.
  x = constrain(gx * IMU_LOOK_X_SIGN, -1.0f, 1.0f);
  y = constrain(gy * IMU_LOOK_Y_SIGN, -1.0f, 1.0f);
}

bool resting() { return isResting; }
bool imuOk() { return imuReady; }
bool touchOk() { return touchReady; }

}  // namespace Senses

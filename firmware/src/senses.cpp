#include "senses.h"
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>
#include <TouchDrvCSTXXX.hpp>
#include "config.h"

namespace {
// ------------------------------------------------------------ event queue
const int kQ = 16;
Event q[kQ];
int qHead = 0, qTail = 0;
void push(Ev t, int16_t x = 0, int16_t y = 0) {
  int n = (qTail + 1) % kQ;
  if (n == qHead) return;  // full: drop
  q[qTail].type = t;
  q[qTail].x = x;
  q[qTail].y = y;
  qTail = n;
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
  if (dyn < 0.05f && spin < 8) stillMs += (uint32_t)(dt * 1000);
  else stillMs = 0;
  if (!isResting && stillMs > 1500) {
    isResting = true;
    push(Ev::PutDown);
  } else if (isResting && (spin > 45 || dyn > 0.25f)) {
    isResting = false;
    if (now - lastPickupMs > 4000 && now - lastShakeMs > 2000) {
      lastPickupMs = now;
      push(Ev::PickedUp);
    }
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
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  touch.setPins(PIN_TOUCH_RST, PIN_TOUCH_INT);
  touchReady = touch.begin(Wire, CST816_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (touchReady) touch.setMaxCoordinates(240, 240);
  else Serial.println("[senses] touch panel not found");

  imuReady = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (imuReady) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    imu.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_3);
    imu.enableAccelerometer();
    imu.enableGyroscope();
  } else {
    Serial.println("[senses] motion sensor not found");
  }

  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);
  return touchReady && imuReady;
}

void poll(uint32_t now) {
  pollTouch(now);
  pollImu(now);
  pollButton(boot, now, false);
  pollButton(pwr, now, true);
}

bool next(Event& e) {
  if (qHead == qTail) return false;
  e = q[qHead];
  qHead = (qHead + 1) % kQ;
  return true;
}

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

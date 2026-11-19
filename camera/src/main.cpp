// Rory's eyes - camera board firmware.
//
// Boards (choose in my_settings.h with EYES_BOARD):
//   EYES_BOARD_WAVESHARE  Waveshare ESP32-S3-CAM-OV3660 (37 x 37 mm, battery socket) - the default
//   EYES_BOARD_NULLLAB    NULLLAB "ESP32S3 CAM V1.2" (the first test board)
//
// Joins your Wi-Fi and serves:
//   http://rory-eyes.local/          a test page showing what the camera sees
//   http://rory-eyes.local/capture   one fresh JPEG photo (Rory fetches this when asked to look)
//   http://rory-eyes.local/status    battery and camera status (JSON)
//   http://rory-eyes.local/glow?c=   antenna color: cyan, red, purple, white, off
//
// Rory only takes a photo when he asks her to look - this board never records
// or stores anything; each photo is made on request and forgotten.
//
// The antenna LED (DIYables RGB module) plugs into the Waveshare board's UART
// socket: R -> TXD (GPIO43), G and B -> RXD (GPIO44), - -> GND. It glows softly
// cyan, and shines bright while she takes a photo so he can see her looking.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include "esp_camera.h"
#include "settings.h"

namespace {

WebServer server(80);
bool cameraOk = false;

void logf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);  // native USB
}

/* ---------- board details ---------- */
struct Pins {
  int pwdn, reset, xclk, sda, scl, d0, d1, d2, d3, d4, d5, d6, d7, vsync, href, pclk;
};
#if EYES_BOARD == EYES_BOARD_NULLLAB
const char* kBoardName = "NULLLAB ESP32S3 CAM V1.2";
const Pins kPins = {-1, -1, 15, 4, 5, 11, 9, 8, 10, 12, 18, 17, 16, 6, 7, 13};  // ESP32-S3-EYE layout
const int kFlashLed = 3;   // two small white LEDs
const int kLedR = -1, kLedC = -1;
#else
const char* kBoardName = "Waveshare ESP32-S3-CAM-OV3660";
// From Waveshare's own example for this board (camera_pins.h / BSP)
const Pins kPins = {-1, -1, 38, 8, 7, 45, 47, 48, 46, 42, 40, 39, 21, 17, 18, 41};
const int kFlashLed = -1;  // no flash LEDs: the antenna lights up instead
const int kLedR = 43, kLedC = 44;  // antenna LED on the UART socket (TXD, RXD)
#endif

/* ---------- Waveshare's helper chip (CH32V003 at I2C 0x24) ---------- */
// It switches the camera on (its IO6) and measures the battery.
const uint8_t kExpander = 0x24;
bool expanderOk = false;
bool expWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(kExpander);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
void startExpander() {
#if EYES_BOARD != EYES_BOARD_NULLLAB
  Wire.begin(kPins.sda, kPins.scl, 100000);
  expanderOk = expWrite(0x02, 0xFF)      // all its pins are outputs
               && expWrite(0x03, 0x40);  // IO6 on (as in Waveshare's camera example), the rest off
  logf("[eyes] helper chip %s\n", expanderOk ? "OK" : "NOT FOUND");
  delay(50);
#endif
}
// Raw battery reading from the helper chip's ADC (0 if not available).
uint16_t batteryRaw() {
  if (!expanderOk) return 0;
  // The camera driver takes over these I2C pins after start-up (it only needs
  // them then), so claim them back for a moment, like Waveshare's example order.
  Wire.end();
  Wire.begin(kPins.sda, kPins.scl, 100000);
  Wire.beginTransmission(kExpander);
  Wire.write(0x06);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom(kExpander, (uint8_t)2) != 2) return 0;
  uint16_t lo = Wire.read(), hi = Wire.read();
  return (hi << 8) | lo;
}

/* ---------- antenna LED ---------- */
// Two PWM channels on LEDC timer 2, well away from the camera clock (channel 0, timer 0).
const int kChR = 4, kChC = 5;
int glowR = 0, glowC = 60;  // idle: soft cyan
void setGlow(int r, int c) {
  if (kLedR < 0) return;
  ledcWrite(kLedR, r);
  ledcWrite(kLedC, c);
}
void startGlow() {
  if (kLedR < 0) return;
  ledcAttachChannel(kLedR, 5000, 8, kChR);
  ledcAttachChannel(kLedC, 5000, 8, kChC);
  setGlow(glowR, glowC);
}

/* ---------- camera ---------- */
bool startCamera() {
  const Pins& p = kPins;
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = p.d0; c.pin_d1 = p.d1; c.pin_d2 = p.d2; c.pin_d3 = p.d3;
  c.pin_d4 = p.d4; c.pin_d5 = p.d5; c.pin_d6 = p.d6; c.pin_d7 = p.d7;
  c.pin_xclk = p.xclk; c.pin_pclk = p.pclk; c.pin_vsync = p.vsync; c.pin_href = p.href;
  c.pin_sccb_sda = p.sda; c.pin_sccb_scl = p.scl;
  c.pin_pwdn = p.pwdn; c.pin_reset = p.reset;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;  // 640 x 480: plenty for "what's this?"
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;  // always the newest picture
  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    logf("[eyes] camera not found on the %s (error 0x%x)\n", kBoardName, err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  logf("[eyes] camera found on the %s, sensor id 0x%x\n", kBoardName, s ? s->id.PID : 0);
  if (s) {
    s->set_vflip(s, EYES_FLIP);     // set EYES_FLIP / EYES_MIRROR in my_settings.h if the picture
    s->set_hmirror(s, EYES_MIRROR); // is upside down or mirrored on the test page
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
  }
  return true;
}

void handleCapture() {
  if (!cameraOk) {
    server.send(503, "text/plain", "camera not working");
    return;
  }
  // ?flash=1: a friendly light so he can see Rory is looking
  bool flash = server.hasArg("flash") && server.arg("flash") == "1";
  if (flash) {
    if (kFlashLed >= 0) analogWrite(kFlashLed, 70);
    setGlow(255, 255);  // antenna shines bright
    delay(120);
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {  // drop a possibly stale frame and take a fresh one
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
  }
  if (flash) {
    if (kFlashLed >= 0) analogWrite(kFlashLed, 0);
    setGlow(glowR, glowC);
  }
  if (!fb) {
    server.send(500, "text/plain", "photo failed");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleStatus() {
  char buf[160];
  snprintf(buf, sizeof(buf), "{\"board\":\"%s\",\"camera\":%s,\"battery_raw\":%u,\"uptime_s\":%lu}", kBoardName,
           cameraOk ? "true" : "false", batteryRaw(), (unsigned long)(millis() / 1000));
  server.send(200, "application/json", buf);
}

// /glow?c=cyan|red|purple|white|off  (so Rory's head can change her antenna's color later)
void handleGlow() {
  String c = server.arg("c");
  if (c == "red") { glowR = 120; glowC = 0; }
  else if (c == "purple") { glowR = 90; glowC = 90; }
  else if (c == "white") { glowR = 200; glowC = 200; }
  else if (c == "off") { glowR = 0; glowC = 0; }
  else { glowR = 0; glowC = 60; }  // cyan
  setGlow(glowR, glowC);
  server.send(200, "text/plain", "ok");
}

const char kPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Rory's eyes</title><style>body{font-family:sans-serif;background:#263238;color:#fff;text-align:center;margin:16px}
img{max-width:100%;border-radius:12px;border:4px solid #52DAE8}</style></head><body>
<h2>Rory's eyes &#128064;</h2><img id=v src="/capture"><p>Refreshes every second.</p>
<script>setInterval(()=>{document.getElementById('v').src='/capture?t='+Date.now()},1000)</script></body></html>)HTML";

}  // namespace

void setup() {
  Serial.begin(115200);
  if (kFlashLed >= 0) {
    pinMode(kFlashLed, OUTPUT);
    digitalWrite(kFlashLed, LOW);
  }
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 1500;) delay(10);
  logf("\n== Rory's eyes (%s) ==\n", kBoardName);
  logf("[eyes] PSRAM: %u KB\n", (unsigned)(ESP.getPsramSize() / 1024));

  startExpander();
  cameraOk = startCamera();
  if (!cameraOk) logf("[eyes] CAMERA NOT FOUND - check the ribbon cable is pushed in and the clip closed\n");
  startGlow();

  if (String(WIFI_NAME) == "your-wifi-name") {
    logf("[eyes] Wi-Fi not set up yet: put your Wi-Fi name and password in src/my_settings.h, then Upload\n");
    return;  // loop() keeps reporting the status
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(EYES_HOSTNAME);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  logf("[eyes] joining Wi-Fi \"%s\"", WIFI_NAME);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
    setGlow((i & 1) ? 60 : 0, 0);  // antenna blinks red while connecting
    delay(500);
    logf(".");
  }
  setGlow(glowR, glowC);
  if (WiFi.status() != WL_CONNECTED) {
    logf("\n[eyes] could not join Wi-Fi - check WIFI_NAME / WIFI_PASSWORD in my_settings.h (2.4 GHz only)\n");
    return;
  }
  MDNS.begin(EYES_HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  server.on("/", [] { server.send_P(200, "text/html", kPage); });
  server.on("/capture", handleCapture);
  server.on("/status", handleStatus);
  server.on("/glow", handleGlow);
  server.begin();
  logf("\n[eyes] ready! open  http://%s.local/  or  http://%s/  in a browser\n", EYES_HOSTNAME,
       WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();
  // Repeat the status every 10 s, so it shows whenever the Serial Monitor connects.
  static uint32_t lastReport = 0;
  if (millis() - lastReport > 10000) {
    lastReport = millis();
    if (WiFi.status() == WL_CONNECTED)
      logf("[eyes] camera %s | Wi-Fi OK | battery reading %u | open http://%s.local/ or http://%s/\n",
           cameraOk ? "OK" : "NOT FOUND", batteryRaw(), EYES_HOSTNAME, WiFi.localIP().toString().c_str());
    else
      logf("[eyes] camera %s | Wi-Fi NOT connected to \"%s\" - check my_settings.h (2.4 GHz only)\n",
           cameraOk ? "OK" : "NOT FOUND", WIFI_NAME);
  }
  delay(2);
}

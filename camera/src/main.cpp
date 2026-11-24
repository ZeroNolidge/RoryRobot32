// Rory's eyes - camera board firmware.
//
// Boards (choose in my_settings.h with EYES_BOARD):
//   EYES_BOARD_WAVESHARE  Waveshare ESP32-S3-CAM-OV3660 (37 x 37 mm, battery socket) - the default
//   EYES_BOARD_NULLLAB    NULLLAB "ESP32S3 CAM V1.2" (the first test board)
//
// Joins your Wi-Fi and serves:
//   http://rory-eyes.local/          a test page showing what the camera sees
//   http://rory-eyes.local/capture   one fresh JPEG photo (Rory fetches this when asked to look)
//   http://rory-eyes.local/capture?hd=1   a sharp 1600 x 1200 photo (read mode: book pages)
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
#include <WiFiMulti.h>
#include <Wire.h>
#include "esp_camera.h"
#include "settings.h"

namespace {

WebServer server(80);
WiFiMulti wifi;
bool wifiWanted = false;
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
               // Same as Waveshare's board setup: IO5 = BAT_EN keeps the battery
               // connected (off = the board dies when USB is unplugged), IO6 =
               // camera on, IO0-2 = display/touch lines idle high, IO4 low.
               && expWrite(0x03, 0x67);
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
// Set EYES_FLIP / EYES_MIRROR in my_settings.h if the picture is upside down or
// mirrored. Re-applied after every size change, which can reset them.
void applyOrientation(sensor_t* s) {
  if (!s) return;
  s->set_vflip(s, EYES_FLIP);
  s->set_hmirror(s, EYES_MIRROR);
}
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
  c.frame_size = FRAMESIZE_UXGA;  // allocate for the sharpest size (book pages); switched to VGA below
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
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
    s->set_framesize(s, FRAMESIZE_VGA);  // 640 x 480: plenty for "what's this?"
    applyOrientation(s);
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
  // ?hd=1: a sharp 1600 x 1200 photo, so Rory can read the words in a book
  bool hd = server.hasArg("hd") && server.arg("hd") == "1";
  sensor_t* s = esp_camera_sensor_get();
  if (hd && s) {
    s->set_framesize(s, FRAMESIZE_UXGA);
    applyOrientation(s);
  }
  camera_fb_t* fb = nullptr;
  for (int i = 0; i < (hd ? 3 : 2); i++) {  // drop stale frames (and let the size change settle)
    if (fb) esp_camera_fb_return(fb);
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
  if (hd && s) {
    s->set_framesize(s, FRAMESIZE_VGA);
    applyOrientation(s);
  }
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
  logf("[eyes] picture settings: EYES_FLIP %d, EYES_MIRROR %d (change them in src/my_settings.h)\n", EYES_FLIP, EYES_MIRROR);

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
  // Every network she knows (home, phone hotspot...); she joins whichever is in range.
  wifi.addAP(WIFI_NAME, WIFI_PASSWORD);
#ifdef WIFI_NAME2
  wifi.addAP(WIFI_NAME2, WIFI_PASSWORD2);
#endif
#ifdef WIFI_NAME3
  wifi.addAP(WIFI_NAME3, WIFI_PASSWORD3);
#endif
  server.on("/", [] { server.send_P(200, "text/html", kPage); });
  server.on("/capture", handleCapture);
  server.on("/status", handleStatus);
  server.on("/glow", handleGlow);
  wifiWanted = true;
  logf("[eyes] looking for a known Wi-Fi network...\n");
}

// Keeps her on Wi-Fi: (re)joins any known network, and starts her web page and
// her "rory-eyes" name each time she connects (they change with the network).
void keepOnline() {
  if (!wifiWanted) return;
  static uint32_t nextTry = 0;
  static String onNetwork;
  static bool serving = false;
  if (WiFi.status() != WL_CONNECTED) {
    if (!onNetwork.isEmpty()) {
      logf("[eyes] lost Wi-Fi \"%s\"\n", onNetwork.c_str());
      onNetwork = "";
    }
    if (millis() < nextTry) return;
    setGlow(60, 0);  // red while looking for Wi-Fi
    wifi.run(6000);
    nextTry = millis() + 2000;
    if (WiFi.status() != WL_CONNECTED) {
      setGlow(0, 0);
      static uint32_t lastScanLog = 0;
      if (millis() - lastScanLog > 60000) {  // once a minute: show what she can see
        lastScanLog = millis();
        int n = WiFi.scanNetworks();
        logf("[eyes] no known Wi-Fi in range. Networks this board can see (%d):\n", n);
        for (int i = 0; i < n && i < 15; i++) logf("[eyes]   \"%s\"  signal %d dBm\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        WiFi.scanDelete();
      }
      return;
    }
  }
  if (onNetwork != WiFi.SSID()) {  // just connected (maybe to a different network)
    onNetwork = WiFi.SSID();
    setGlow(glowR, glowC);
    MDNS.end();
    MDNS.begin(EYES_HOSTNAME);
    MDNS.addService("http", "tcp", 80);
    if (!serving) {
      server.begin();
      serving = true;
    }
    logf("[eyes] ready on \"%s\"! open  http://%s.local/  or  http://%s/  in a browser\n", onNetwork.c_str(), EYES_HOSTNAME,
         WiFi.localIP().toString().c_str());
  }
}

void loop() {
  keepOnline();
  server.handleClient();
  // Repeat the status every 10 s, so it shows whenever the Serial Monitor connects.
  static uint32_t lastReport = 0;
  if (millis() - lastReport > 10000) {
    lastReport = millis();
    if (WiFi.status() == WL_CONNECTED)
      logf("[eyes] camera %s | flip %d mirror %d | Wi-Fi \"%s\" | battery reading %u | open http://%s.local/ or http://%s/\n",
           cameraOk ? "OK" : "NOT FOUND", EYES_FLIP, EYES_MIRROR, WiFi.SSID().c_str(), batteryRaw(), EYES_HOSTNAME, WiFi.localIP().toString().c_str());
    else
      logf("[eyes] camera %s | Wi-Fi not connected (looking for known networks - see my_settings.h, 2.4 GHz only)\n",
           cameraOk ? "OK" : "NOT FOUND");
  }
  delay(2);
}

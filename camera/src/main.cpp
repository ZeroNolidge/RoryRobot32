// Rory's eyes - camera board firmware ("ESP32S3 CAM V1.2", OV3660).
//
// Joins your Wi-Fi and serves:
//   http://rory-eyes.local/          a test page showing what the camera sees
//   http://rory-eyes.local/capture   one fresh JPEG photo (Rory fetches this when asked to look)
//
// Rory only takes a photo when he asks her to look - this board never records
// or stores anything; each photo is made on request and forgotten.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "settings.h"

namespace {

WebServer server(80);
bool cameraOk = false;
const int kStatusLed = 2;  // the white LED marked IO2

// Print to both USB and the serial pins, whichever this board's USB-C uses.
void logf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
  Serial0.print(buf);
}

// Camera wiring. The ESP32-S3-EYE layout is the only common one that leaves
// free exactly the pins this board brings out to its edge (0,1,2,14,41-48).
struct Pins {
  const char* name;
  int pwdn, reset, xclk, sda, scl, d0, d1, d2, d3, d4, d5, d6, d7, vsync, href, pclk;
};
const Pins kLayouts[] = {
    {"ESP32-S3-EYE", -1, -1, 15, 4, 5, 11, 9, 8, 10, 12, 18, 17, 16, 6, 7, 13},
};

bool startCamera() {
  for (const Pins& p : kLayouts) {
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
    if (err == ESP_OK) {
      sensor_t* s = esp_camera_sensor_get();
      logf("[eyes] camera found: %s wiring, sensor id 0x%x\n", p.name, s ? s->id.PID : 0);
      if (s && s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);  // OV3660 is mounted upside down on these boards
        s->set_brightness(s, 1);
        s->set_saturation(s, -1);
      }
      return true;
    }
    logf("[eyes] camera not found with %s wiring (error 0x%x)\n", p.name, err);
  }
  return false;
}

void handleCapture() {
  if (!cameraOk) {
    server.send(503, "text/plain", "camera not working");
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {  // drop a possibly stale frame and take a fresh one
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
  }
  if (!fb) {
    server.send(500, "text/plain", "photo failed");
    return;
  }
  digitalWrite(kStatusLed, HIGH);
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  digitalWrite(kStatusLed, LOW);
}

const char kPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Rory's eyes</title><style>body{font-family:sans-serif;background:#263238;color:#fff;text-align:center;margin:16px}
img{max-width:100%;border-radius:12px;border:4px solid #52DAE8}</style></head><body>
<h2>Rory's eyes &#128064;</h2><img id=v src="/capture"><p>Refreshes every second.</p>
<script>setInterval(()=>{document.getElementById('v').src='/capture?t='+Date.now()},1000)</script></body></html>)HTML";

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  pinMode(kStatusLed, OUTPUT);
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 1500;) delay(10);
  logf("\n== Rory's eyes ==\n");
  logf("[eyes] PSRAM: %u KB\n", (unsigned)(ESP.getPsramSize() / 1024));

  cameraOk = startCamera();
  if (!cameraOk) logf("[eyes] CAMERA NOT FOUND - check the ribbon cable is pushed in and the clip closed\n");

  if (String(WIFI_NAME) == "your-wifi-name") {
    logf("[eyes] Wi-Fi not set up yet: put your Wi-Fi name and password in src/my_settings.h, then Upload\n");
    return;  // loop() keeps reporting the status
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(EYES_HOSTNAME);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  logf("[eyes] joining Wi-Fi \"%s\"", WIFI_NAME);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
    digitalWrite(kStatusLed, i & 1);  // blink while connecting
    delay(500);
    logf(".");
  }
  digitalWrite(kStatusLed, LOW);
  if (WiFi.status() != WL_CONNECTED) {
    logf("\n[eyes] could not join Wi-Fi - check WIFI_NAME / WIFI_PASSWORD in my_settings.h (2.4 GHz only)\n");
    return;
  }
  MDNS.begin(EYES_HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  server.on("/", [] { server.send_P(200, "text/html", kPage); });
  server.on("/capture", handleCapture);
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
      logf("[eyes] camera %s | Wi-Fi OK | open http://%s.local/ or http://%s/\n", cameraOk ? "OK" : "NOT FOUND",
           EYES_HOSTNAME, WiFi.localIP().toString().c_str());
    else
      logf("[eyes] camera %s | Wi-Fi NOT connected to \"%s\" - check my_settings.h (2.4 GHz only)\n",
           cameraOk ? "OK" : "NOT FOUND", WIFI_NAME);
  }
  delay(2);
}

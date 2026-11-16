#include "ota.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "settings.h"
#include "voice.h"

namespace {
bool started = false;

// While updating: sleepy-happy eyes looking up at a filling bar.
void drawProgress(float p) {
  Raster& c = Display::canvas();
  c.clear(COL_SCREEN);
  c.arc(78, 100, 22, 7, 200, 340, COL_EYE);  // ^ ^ eyes
  c.arc(162, 100, 22, 7, 200, 340, COL_EYE);
  c.capsule(50, 165, 190, 165, 9, 0x39E7);       // track
  if (p > 0.01f) c.capsule(50, 165, 50 + 140 * p, 165, 9, COL_EYE);
  Display::present();
  Display::setBacklight(BACKLIGHT_AWAKE);
  Display::update();
}
}  // namespace

namespace Ota {

void loop() {
  if (!started) {
    const auto& cfg = Settings::get();
    if (cfg.token.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    ArduinoOTA.setHostname("rory");        // rory.local
    ArduinoOTA.setPassword(cfg.token.c_str());
    ArduinoOTA.setMdnsEnabled(true);
    ArduinoOTA.onStart([] {
      Serial.println("[update] new version arriving over Wi-Fi...");
      Voice::stop();
      drawProgress(0);
    });
    static int lastPct = -1;
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
      int pct = total ? (int)(100ULL * done / total) : 0;
      if (pct == lastPct) return;
      lastPct = pct;
      if (pct % 5 == 0) drawProgress(pct / 100.0f);
    });
    ArduinoOTA.onEnd([] {
      drawProgress(1);
      Serial.println("[update] done - restarting");
    });
    ArduinoOTA.onError([](ota_error_t e) {
      Serial.printf("[update] failed (%u) - keeping the old version\n", (unsigned)e);
      lastPct = -1;
    });
    ArduinoOTA.begin();
    started = true;
    Serial.printf("[update] ready for wireless updates at rory.local (%s)\n", WiFi.localIP().toString().c_str());
    return;
  }
  ArduinoOTA.handle();
}

}  // namespace Ota

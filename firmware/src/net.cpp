#include "net.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include "settings.h"

// The ESP32 core ships Mozilla's list of trusted root certificates, so Rory can
// check she's really talking to your server.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace {
volatile Net::State st = Net::State::Idle;
Net::Answer ans;
String history;              // last few turns, packed by the server
uint32_t historyMs = 0;
const uint32_t kHistoryKeepMs = 3 * 60 * 1000;  // forget the chat after 3 quiet minutes

const uint8_t* reqWav = nullptr;
size_t reqLen = 0;

void askTask(void*) {
  const auto& cfg = Settings::get();
  uint32_t t0 = millis();
  bool ok = false;
  {
    NetworkClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
    HTTPClient http;
    http.setTimeout(20000);
    if (http.begin(client, cfg.server + "/ask")) {
      http.addHeader("Authorization", "Bearer " + cfg.token);
      http.addHeader("Content-Type", "audio/wav");
      if (!history.isEmpty() && millis() - historyMs < kHistoryKeepMs) http.addHeader("X-Rory-History", history);
      int code = http.POST(const_cast<uint8_t*>(reqWav), reqLen);
      if (code == 200) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());
        if (!err && doc["audio"].is<const char*>()) {
          ans.heard = doc["heard"] | "";
          ans.say = doc["say"] | "";
          ans.face = doc["face"] | "happy";
          ans.audioUrl = doc["audio"] | "";
          history = doc["history"] | "";
          historyMs = millis();
          ok = true;
        } else {
          Serial.printf("[net] bad answer from server: %s\n", err.c_str());
        }
      } else {
        Serial.printf("[net] server said %d %s\n", code, code < 0 ? http.errorToString(code).c_str() : http.getString().c_str());
      }
      http.end();
    } else {
      Serial.println("[net] couldn't reach the server");
    }
  }
  Serial.printf("[net] ask took %lu ms: heard \"%s\"\n", (unsigned long)(millis() - t0), ans.heard.c_str());
  st = ok ? Net::State::Done : Net::State::Failed;
  vTaskDelete(nullptr);
}
}  // namespace

namespace Net {

void begin() {
  if (!Settings::aiConfigured()) return;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Settings::get().wifiName.c_str(), Settings::get().wifiPassword.c_str());
  Serial.printf("[net] joining Wi-Fi \"%s\"...\n", Settings::get().wifiName.c_str());
}

bool online() { return Settings::aiConfigured() && WiFi.status() == WL_CONNECTED; }

bool ask(const uint8_t* wav, size_t len) {
  if (st == State::Busy || !online()) return false;
  reqWav = wav;
  reqLen = len;
  ans = Answer();
  st = State::Busy;
  // TLS needs a roomy stack; runs on core 0 next to the audio task.
  if (xTaskCreatePinnedToCore(askTask, "ask", 12288, nullptr, 3, nullptr, 0) != pdPASS) {
    st = State::Failed;
    return false;
  }
  return true;
}

State state() { return st; }

void reset() {
  if (st != State::Busy) st = State::Idle;
}

const Answer& answer() { return ans; }

}  // namespace Net

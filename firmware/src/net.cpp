#include "net.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include "settings.h"
#include "voice.h"

// The ESP32 core ships Mozilla's list of trusted root certificates, so Rory can
// check she's really talking to your server.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace {
volatile Net::State st = Net::State::Idle;
Net::Answer ans;
String history;  // last few turns, packed by the server
uint32_t historyMs = 0;
const uint32_t kHistoryKeepMs = 3 * 60 * 1000;  // forget the chat after 3 quiet minutes

enum class Job : uint8_t { Prepare, Ask };
QueueHandle_t jobs = nullptr;
const uint8_t* reqWav = nullptr;
size_t reqLen = 0;

NetworkClientSecure* client = nullptr;  // kept open between questions
String host;

String urlDecode(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '%' && i + 2 < s.length()) {
      out += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

bool ensureConnected() {
  if (client->connected()) return true;
  client->stop();
  uint32_t t0 = millis();
  bool ok = client->connect(host.c_str(), 443);
  Serial.printf("[net] %s %s in %lu ms\n", ok ? "connected to" : "could not connect to", host.c_str(),
                (unsigned long)(millis() - t0));
  return ok;
}

// Passes her streamed voice from the HTTP reply straight to the speaker.
class SpeakerSink : public Stream {
 public:
  size_t write(uint8_t b) override { return Voice::streamWrite(&b, 1); }
  size_t write(const uint8_t* buf, size_t len) override { return Voice::streamWrite(buf, len); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

void doAsk() {
  const auto& cfg = Settings::get();
  uint32_t t0 = millis();
  static const char* kHeaders[] = {"X-Rory-Face", "X-Rory-Say", "X-Rory-Heard", "X-Rory-History"};
  bool ok = false;
  for (int attempt = 0; attempt < 2 && !ok; attempt++) {
    if (!ensureConnected()) continue;
    HTTPClient http;
    http.setReuse(true);  // keep the connection for the next question
    http.setTimeout(20000);
    http.collectHeaders(kHeaders, 4);
    if (!http.begin(*client, cfg.server + "/talk")) continue;
    http.addHeader("Authorization", "Bearer " + cfg.token);
    http.addHeader("Content-Type", "audio/wav");
    if (!history.isEmpty() && millis() - historyMs < kHistoryKeepMs) http.addHeader("X-Rory-History", history);
    int code = http.POST(const_cast<uint8_t*>(reqWav), reqLen);
    if (code < 0) {  // the kept-open connection had gone stale: reconnect and try once more
      Serial.printf("[net] %s - retrying\n", http.errorToString(code).c_str());
      http.end();
      client->stop();
      continue;
    }
    if (code == 200) {
      ans.face = http.header("X-Rory-Face");
      ans.say = urlDecode(http.header("X-Rory-Say"));
      ans.heard = urlDecode(http.header("X-Rory-Heard"));
      String h = http.header("X-Rory-History");
      if (!h.isEmpty()) {
        history = h;
        historyMs = millis();
      }
      Serial.printf("[net] answer after %lu ms: heard \"%s\"\n", (unsigned long)(millis() - t0), ans.heard.c_str());
      if (Voice::streamBegin()) {
        st = Net::State::Speaking;  // the brain shows her face now
        SpeakerSink sink;
        int n = http.writeToStream(&sink);
        Voice::streamEnd();
        if (n < 0) Serial.printf("[net] voice stream ended early: %s\n", http.errorToString(n).c_str());
        ok = true;
      }
    } else {
      Serial.printf("[net] server said %d\n", code);
    }
    http.end();
    break;
  }
  st = ok ? Net::State::Done : Net::State::Failed;
}

void netTask(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) {
      if (job == Job::Ask) st = Net::State::Failed;
      continue;
    }
    if (job == Job::Prepare) ensureConnected();
    else doAsk();
  }
}
}  // namespace

namespace Net {

void begin() {
  if (!Settings::aiConfigured()) return;
  const String& s = Settings::get().server;  // https://host[/...]
  host = s.substring(8);
  int slash = host.indexOf('/');
  if (slash >= 0) host = host.substring(0, slash);

  client = new NetworkClientSecure();
  client->setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
  client->setTimeout(20);  // seconds
  jobs = xQueueCreate(4, sizeof(Job));
  // TLS needs a roomy stack; runs on core 0 next to the audio task.
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 3, nullptr, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Settings::get().wifiName.c_str(), Settings::get().wifiPassword.c_str());
  Serial.printf("[net] joining Wi-Fi \"%s\"...\n", Settings::get().wifiName.c_str());
}

bool online() { return jobs && WiFi.status() == WL_CONNECTED; }

void prepare() {
  if (!online()) return;
  Job j = Job::Prepare;
  xQueueSend(jobs, &j, 0);
}

bool ask(const uint8_t* wav, size_t len) {
  if (!online() || st == State::Busy || st == State::Speaking) return false;
  reqWav = wav;
  reqLen = len;
  ans = Answer();
  st = State::Busy;
  Job j = Job::Ask;
  if (xQueueSend(jobs, &j, 0) != pdTRUE) {
    st = State::Failed;
    return false;
  }
  return true;
}

State state() { return st; }

void reset() {
  if (st == State::Done || st == State::Failed) st = State::Idle;
}

const Answer& answer() { return ans; }

}  // namespace Net

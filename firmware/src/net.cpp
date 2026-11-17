#include "net.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include "made_clips.h"
#include "memory.h"
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

enum class Job : uint8_t { Prepare, Ask, Sync };
volatile bool syncQueued = false, syncDone = false, newClips = false;
QueueHandle_t jobs = nullptr;
const uint8_t* reqWav = nullptr;
size_t reqLen = 0;

// Her secure connection to the server, kept open between questions.
// In Arduino-ESP32 3.x, stop() wipes the TLS settings (including the trusted
// certificate list) - and HTTPClient calls stop() itself - so re-attach the
// list every time the connection is closed, whoever closes it.
class RoryTls : public NetworkClientSecure {
 public:
  void attachCerts() {
    setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
    setTimeout(20000);  // milliseconds
  }
  void stop() override {
    NetworkClientSecure::stop();
    // the list itself is still loaded; just hook it back up (no new memory)
    attach_ssl_certificate_bundle(sslclient.get(), true);
    setTimeout(20000);
  }
};
RoryTls* client = nullptr;
HTTPClient web;  // one request object for all requests, so the open connection is reused
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
  client->stop();  // (re-attaches the certificate list)
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

// ---- her camera eyes -------------------------------------------------------
uint8_t* photo = nullptr;  // one JPEG, in PSRAM
const size_t kPhotoMax = 300 * 1024;
IPAddress eyesIp;
bool mdnsStarted = false;

bool findEyes() {
  if (eyesIp != IPAddress()) return true;
  const String& e = Settings::get().eyes;
  if (eyesIp.fromString(e)) return true;  // an address like 192.168.1.250
  if (!mdnsStarted) {  // (already running if wireless updates started it)
    MDNS.begin("rory");
    mdnsStarted = true;
  }
  String name = e;
  name.replace(".local", "");
  IPAddress ip = MDNS.queryHost(name.c_str(), 2000);
  if (ip == IPAddress()) {
    Serial.printf("[net] can't find her camera \"%s\" on the Wi-Fi\n", e.c_str());
    return false;
  }
  eyesIp = ip;
  Serial.printf("[net] camera eyes at %s\n", ip.toString().c_str());
  return true;
}

// One photo from the camera board (it blinks its light so he sees her look).
size_t takePhoto() {
  if (!photo) photo = (uint8_t*)heap_caps_malloc(kPhotoMax, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!photo || !findEyes()) return 0;
  uint32_t t0 = millis();
  WiFiClient plain;
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(plain, "http://" + eyesIp.toString() + "/capture?flash=1");
  int code = http.GET();
  size_t got = 0;
  if (code == 200) {
    int len = http.getSize();
    WiFiClient* s = http.getStreamPtr();
    while (http.connected() && got < kPhotoMax && (len < 0 || (int)got < len)) {
      size_t n = s->readBytes(photo + got, min((size_t)4096, kPhotoMax - got));
      if (n == 0) break;
      got += n;
    }
  } else {
    Serial.printf("[net] camera said %d\n", code);
    eyesIp = IPAddress();  // look it up again next time
  }
  http.end();
  Serial.printf("[net] photo: %u bytes in %lu ms\n", (unsigned)got, (unsigned long)(millis() - t0));
  return got;
}

String urlEncode(const String& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

// One request to /talk. Returns 200 (she spoke), 202 (she wants to look; the
// question is in ans.heard) or an error.
enum class Body { Wav, Photo, NoEyes };
int talk(Body kind, const uint8_t* body, size_t len) {
  const auto& cfg = Settings::get();
  static const char* kHeaders[] = {"X-Rory-Face", "X-Rory-Say", "X-Rory-Heard", "X-Rory-History", "X-Rory-Look", "X-Rory-Remember"};
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!ensureConnected()) {
      Serial.printf("[net] couldn't reach the server (free memory %u)\n", (unsigned)ESP.getFreeHeap());
      client->stop();
      continue;
    }
    HTTPClient& http = web;
    http.setReuse(true);  // keep the connection for the next question
    http.setTimeout(20000);
    http.collectHeaders(kHeaders, 6);
    if (!http.begin(*client, cfg.server + "/talk")) continue;
    http.addHeader("Authorization", "Bearer " + cfg.token);
    if (!history.isEmpty() && millis() - historyMs < kHistoryKeepMs) http.addHeader("X-Rory-History", history);
    if (!Memory::text().isEmpty()) http.addHeader("X-Rory-Memory", urlEncode(Memory::text()));
    if (kind == Body::Wav) {
      http.addHeader("Content-Type", "audio/wav");
      http.addHeader("X-Rory-Can-Look", "1");
    } else if (kind == Body::Photo) {
      http.addHeader("Content-Type", "image/jpeg");
      http.addHeader("X-Rory-Question", urlEncode(ans.heard));
    } else {
      http.addHeader("Content-Type", "text/plain");
      http.addHeader("X-Rory-Eyes", "off");
    }
    int code = kind == Body::NoEyes ? http.POST(ans.heard) : http.POST(const_cast<uint8_t*>(body), len);
    if (code < 0) {  // the kept-open connection had gone stale: reconnect and try once more
      Serial.printf("[net] %s - retrying\n", http.errorToString(code).c_str());
      http.end();
      client->stop();
      continue;
    }
    if (code == 202 && http.header("X-Rory-Look") == "1") {
      ans.heard = urlDecode(http.header("X-Rory-Heard"));
      http.getString();  // finish the (empty) reply so the connection can be reused
      http.end();
      return 202;
    }
    if (code == 200) {
      ans.face = http.header("X-Rory-Face");
      ans.say = urlDecode(http.header("X-Rory-Say"));
      ans.remember = urlDecode(http.header("X-Rory-Remember"));
      if (kind == Body::Wav) ans.heard = urlDecode(http.header("X-Rory-Heard"));
      String h = http.header("X-Rory-History");
      if (!h.isEmpty()) {
        history = h;
        historyMs = millis();
      }
      if (Voice::streamBegin()) {
        st = Net::State::Speaking;  // the brain shows her face now
        SpeakerSink sink;
        int n = http.writeToStream(&sink);
        Voice::streamEnd();
        if (n < 0) Serial.printf("[net] voice stream ended early: %s\n", http.errorToString(n).c_str());
      }
    } else {
      Serial.printf("[net] server said %d\n", code);
    }
    http.end();
    return code;
  }
  return -1;
}

void doAsk() {
  uint32_t t0 = millis();
  Serial.printf("[net] asking the server (%u bytes of his voice)...\n", (unsigned)reqLen);
  int code = talk(Body::Wav, reqWav, reqLen);
  if (code == 202) {
    // "Rory, look!" - take one photo and ask again with it
    st = Net::State::Looking;
    Serial.printf("[net] he asked her to look: \"%s\"\n", ans.heard.c_str());
    size_t n = takePhoto();
    code = n ? talk(Body::Photo, photo, n) : talk(Body::NoEyes, nullptr, 0);
  }
  Serial.printf("[net] question took %lu ms until her voice ended (code %d)\n", (unsigned long)(millis() - t0), code);
  st = code == 200 ? Net::State::Done : Net::State::Failed;
}

// Same file names as make_sdcard.py: "Hmm, let me think!" -> hmm-let-me-think.mp3
String slug(const char* text) {
  String out;
  bool dash = false;
  for (const char* p = text; *p; p++) {
    char c = tolower((unsigned char)*p);
    if (isalnum((unsigned char)c)) {
      if (dash && out.length()) out += '-';
      out += c;
      dash = false;
    } else {
      dash = true;
    }
  }
  if (out.isEmpty()) out = "clip";
  return out.substring(0, 40);
}

// Ask the server to say one line in her voice and save the MP3.
bool makeClip(const MadeClip& c, const String& path) {
  const auto& cfg = Settings::get();
  if (!ensureConnected()) return false;
  HTTPClient& http = web;
  http.setReuse(true);
  http.setTimeout(30000);
  if (!http.begin(*client, cfg.server + "/say")) return false;
  http.addHeader("Authorization", "Bearer " + cfg.token);
  http.addHeader("Content-Type", "text/plain");
  int code = http.POST((uint8_t*)c.line, strlen(c.line));
  bool ok = false;
  if (code == 200) {
    SD_MMC.mkdir(String("/rory/") + c.moment);
    String part = path + ".part";  // only becomes a clip once it's complete
    File f = SD_MMC.open(part, FILE_WRITE);
    if (f) {
      int n = http.writeToStream(&f);
      f.close();
      ok = n > 1000 && SD_MMC.rename(part, path);
      if (!ok) SD_MMC.remove(part);
    }
  } else {
    if (code < 0) client->stop();
    Serial.printf("[net] couldn't make \"%s\" (%d)\n", c.line, code);
  }
  http.end();
  return ok;
}

void doSync() {
  int made = 0, failed = 0;
  for (const auto& c : kMadeClips) {
    if (uxQueueMessagesWaiting(jobs)) {  // he's asking something: that comes first
      syncQueued = false;
      return;  // try again later
    }
    String path = String("/rory/") + c.moment + "/" + slug(c.line) + ".mp3";
    if (SD_MMC.exists(path)) continue;
    if (makeClip(c, path)) {
      made++;
      newClips = true;
      Serial.printf("[net] made a new clip: %s \"%s\"\n", c.moment, c.line);
    } else if (++failed >= 3) {
      break;
    }
  }
  if (made) Serial.printf("[net] made %d new clips in her voice\n", made);
  if (failed >= 3) Serial.println("[net] the server couldn't make her clips - will try again after a restart");
  syncDone = true;  // finished (or gave up until the next restart): never loop
  syncQueued = false;
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
    else if (job == Job::Sync) doSync();
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

  client = new RoryTls();
  client->attachCerts();
  jobs = xQueueCreate(4, sizeof(Job));
  // TLS needs a roomy stack; runs on core 0 next to the audio task.
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 3, nullptr, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Settings::get().wifiName.c_str(), Settings::get().wifiPassword.c_str());
  Serial.printf("[net] joining Wi-Fi \"%s\"...\n", Settings::get().wifiName.c_str());
}

bool online() { return jobs && WiFi.status() == WL_CONNECTED; }

void syncClips() {
  static uint32_t onlineSinceMs = 0;
  if (syncDone || syncQueued || !Voice::hasCard() || !online()) {
    if (!online()) onlineSinceMs = 0;
    return;
  }
  uint32_t now = millis();
  if (!onlineSinceMs) onlineSinceMs = now;
  // wait until she's settled in, and never while a question is going on
  if (now - onlineSinceMs < 5000 || (st != State::Idle && st != State::Done && st != State::Failed)) return;
  if (Voice::speaking() || Voice::recording()) return;
  Job j = Job::Sync;
  if (xQueueSend(jobs, &j, 0) == pdTRUE) syncQueued = true;
}

bool clipsChanged() {
  if (!newClips) return false;
  newClips = false;
  return true;
}

void prepare() {
  if (!online()) return;
  Job j = Job::Prepare;
  xQueueSend(jobs, &j, 0);
}

bool ask(const uint8_t* wav, size_t len) {
  if (!online() || st == State::Busy || st == State::Looking || st == State::Speaking) return false;
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

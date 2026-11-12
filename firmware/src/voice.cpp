#include "voice.h"
#include <Arduino.h>
#include <Wire.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Audio.h>
#include <map>
#include <string>
#include <vector>
#include "config.h"
#include "es8311.h"

namespace {
Audio* audio = nullptr;
bool cardOk = false;
int volStep = VOLUME_DEFAULT_STEP;

struct Cmd {
  char path[120];  // empty = stop
};
QueueHandle_t queue = nullptr;
volatile bool pending = false;

std::map<std::string, std::vector<std::string>> clips;
std::map<std::string, int> lastPick;

bool codecInit() {
  // Same setup as Waveshare's audio example: MCLK = 256 x sample rate.
  const int kRate = 16000, kMclkMul = 256;
  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!es) return false;
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = kRate * kMclkMul,
      .sample_frequency = kRate,
  };
  if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  es8311_sample_frequency_config(es, kRate * kMclkMul, kRate);
  es8311_voice_volume_set(es, 80, nullptr);
  es8311_microphone_config(es, false);
  return true;
}

void scanClips() {
  clips.clear();
  File root = SD_MMC.open("/rory");
  if (!root || !root.isDirectory()) {
    Serial.println("[voice] no /rory folder on the SD card - Rory will be quiet");
    return;
  }
  for (File dir = root.openNextFile(); dir; dir = root.openNextFile()) {
    if (!dir.isDirectory()) continue;
    std::string moment = dir.name();
    auto& list = clips[moment];
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      std::string n = f.name();
      if (!f.isDirectory() && n.size() > 4 && n[0] != '.' &&
          (n.compare(n.size() - 4, 4, ".mp3") == 0 || n.compare(n.size() - 4, 4, ".MP3") == 0)) {
        list.push_back("/rory/" + moment + "/" + n);
      }
    }
    Serial.printf("[voice] %-12s %d clips\n", moment.c_str(), (int)list.size());
  }
}

void audioTask(void*) {
  Cmd cmd;
  for (;;) {
    if (xQueueReceive(queue, &cmd, 0) == pdTRUE) {
      if (audio->isRunning()) audio->stopSong();
      if (cmd.path[0]) audio->connecttoFS(SD_MMC, cmd.path);
      pending = false;
    }
    audio->loop();
    vTaskDelay(1);
  }
}
}  // namespace

namespace Voice {

bool begin() {
  pinMode(PIN_AMP_EN, OUTPUT);
  digitalWrite(PIN_AMP_EN, HIGH);
  if (!codecInit()) Serial.println("[voice] ES8311 codec not found");

  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3);
  cardOk = SD_MMC.begin("/sdcard", false /* 4-bit */);
  if (cardOk) scanClips();
  else Serial.println("[voice] no microSD card found");

  audio = new Audio();
  audio->setPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_MCLK);
  setVolumeStep(volStep);

  queue = xQueueCreate(4, sizeof(Cmd));
  // Audio decoding runs on core 0 so the face animation on core 1 stays smooth.
  xTaskCreatePinnedToCore(audioTask, "voice", 8192, nullptr, 5, nullptr, 0);
  return cardOk;
}

bool hasCard() { return cardOk; }

bool play(const char* path) {
  if (!cardOk || !queue) return false;
  Cmd c;
  strlcpy(c.path, path, sizeof(c.path));
  pending = true;
  xQueueSend(queue, &c, 0);
  return true;
}

bool say(const char* moment) {
  auto it = clips.find(moment);
  if (it == clips.end() || it->second.empty()) return false;
  auto& list = it->second;
  int n = list.size();
  int pick = random(n);
  // don't repeat the same clip twice in a row
  if (n > 1 && lastPick.count(moment) && pick == lastPick[moment]) pick = (pick + 1 + random(n - 1)) % n;
  lastPick[moment] = pick;
  return play(list[pick].c_str());
}

void stop() {
  if (!queue) return;
  Cmd c;
  c.path[0] = 0;
  xQueueSend(queue, &c, 0);
}

bool speaking() { return pending || (audio && audio->isRunning()); }

float level() {
  if (!audio) return 0;
  uint16_t vu = audio->getVUlevel();
  float l = ((vu >> 8) > (vu & 0xFF) ? (vu >> 8) : (vu & 0xFF)) / 127.0f;
  // speech rarely hits full scale - boost and soften
  l = l * 2.2f;
  return l > 1 ? 1 : l;
}

void setVolumeStep(int step) {
  int n = sizeof(VOLUME_STEPS) / sizeof(VOLUME_STEPS[0]);
  volStep = ((step % n) + n) % n;
  if (audio) audio->setVolume(VOLUME_STEPS[volStep]);
}

int volumeStep() { return volStep; }

int clipCount(const char* moment) {
  auto it = clips.find(moment);
  return it == clips.end() ? 0 : it->second.size();
}

}  // namespace Voice

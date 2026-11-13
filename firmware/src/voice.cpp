#include "voice.h"
#include <Arduino.h>
#include <Audio.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <map>
#include <string>
#include <vector>
#include "config.h"
#include "es7210.h"
#include "es8311.h"

namespace {
Audio* audio = nullptr;  // only touched inside the voice task (and in begin)
I2SClass* mic = nullptr;
bool cardOk = false;
int volStep = VOLUME_DEFAULT_STEP;

enum class CmdType : uint8_t { File, Url, Stop, RecStart, RecStop };
struct Cmd {
  CmdType type;
  char arg[800];  // file path or URL
};
QueueHandle_t queue = nullptr;
volatile bool pending = false;     // a command is queued
volatile bool running = false;     // audio is playing (mirrored from the task)
volatile uint16_t vu = 0;
volatile bool recActive = false, recReady = false;
volatile bool recStarting = false;  // asked for, not yet started by the task
int appliedVol = -1;
volatile float micLvl = 0;

// recording: 16 kHz, 16-bit, mono WAV in PSRAM
const uint32_t kRate = 16000;
const uint32_t kMaxSeconds = 12;
const size_t kHeader = 44;
const size_t kMaxBytes = kHeader + kRate * 2 * kMaxSeconds;
uint8_t* recBuf = nullptr;
size_t recLen = 0;  // bytes of audio after the header
uint32_t recStartMs = 0;

std::map<std::string, std::vector<std::string>> clips;
std::map<std::string, int> lastPick;

bool speakerCodecInit() {
  // Same setup as Waveshare's audio example: MCLK = 256 x sample rate.
  const int kCodecRate = 16000, kMclkMul = 256;
  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!es) return false;
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = kCodecRate * kMclkMul,
      .sample_frequency = kCodecRate,
  };
  if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  es8311_sample_frequency_config(es, kCodecRate * kMclkMul, kCodecRate);
  es8311_voice_volume_set(es, 80, nullptr);
  es8311_microphone_config(es, false);
  return true;
}

bool micCodecInit() {
  // Same setup as Waveshare's speech-recognition example.
  es7210_dev_handle_t es = nullptr;
  es7210_i2c_config_t i2c = {.i2c_port = I2C_NUM_0, .i2c_addr = 0x40};
  if (es7210_new_codec(&i2c, &es) != ESP_OK) return false;
  es7210_codec_config_t cfg = {};
  cfg.i2s_format = ES7210_I2S_FMT_I2S;
  cfg.mclk_ratio = 256;
  cfg.sample_rate_hz = kRate;
  cfg.bit_width = ES7210_I2S_BITS_16B;
  cfg.mic_bias = ES7210_MIC_BIAS_2V87;
  cfg.mic_gain = ES7210_MIC_GAIN_30DB;
  cfg.flags.tdm_enable = false;
  if (es7210_config_codec(es, &cfg) != ESP_OK) return false;
  es7210_config_volume(es, 40);
  return true;
}

void makeAudio() {
  audio = new Audio();
  audio->setPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_MCLK);
  audio->setVolume(VOLUME_STEPS[volStep]);
  appliedVol = volStep;
}

void writeWavHeader(uint8_t* h, uint32_t dataBytes) {
  auto put32 = [](uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; };
  auto put16 = [](uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; };
  memcpy(h, "RIFF", 4);
  put32(h + 4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(h + 16, 16);
  put16(h + 20, 1);          // PCM
  put16(h + 22, 1);          // mono
  put32(h + 24, kRate);
  put32(h + 28, kRate * 2);  // bytes per second
  put16(h + 32, 2);
  put16(h + 34, 16);
  memcpy(h + 36, "data", 4);
  put32(h + 40, dataBytes);
}

// The speaker player and the microphones share the same I2S pins, so they take
// turns: the player is shut down while Rory listens, then started again.
bool beginRecording() {
  if (!recBuf) return false;
  if (audio) {
    audio->stopSong();
    delete audio;
    audio = nullptr;
  }
  running = false;
  vu = 0;
  digitalWrite(PIN_AMP_EN, LOW);  // speaker off so it doesn't hiss into the mics
  mic = new I2SClass();
  mic->setPins(PIN_I2S_BCLK, PIN_I2S_LRCK, -1, PIN_I2S_DIN, PIN_I2S_MCLK);
  mic->setTimeout(100);
  if (!mic->begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[voice] microphone start failed");
    delete mic;
    mic = nullptr;
    digitalWrite(PIN_AMP_EN, HIGH);
    makeAudio();
    return false;
  }
  recLen = 0;
  recStartMs = millis();
  recReady = false;
  recActive = true;
  return true;
}

void endRecording() {
  if (mic) {
    mic->end();
    delete mic;
    mic = nullptr;
  }
  writeWavHeader(recBuf, recLen);
  recActive = false;
  recReady = true;
  micLvl = 0;
  digitalWrite(PIN_AMP_EN, HIGH);
  makeAudio();
  Serial.printf("[voice] recorded %.1f s\n", recLen / (2.0f * kRate));
}

void recordSome() {
  int16_t frames[512];  // 256 stereo frames
  size_t got = mic->readBytes((char*)frames, sizeof(frames));
  if (millis() - recStartMs < 120) return;  // skip the codec's start-up click
  size_t n = got / 4;
  int16_t* out = (int16_t*)(recBuf + kHeader + recLen);
  size_t room = (kMaxBytes - kHeader - recLen) / 2;
  if (n > room) n = room;
  float sum = 0;
  for (size_t i = 0; i < n; i++) {
    int32_t m = ((int32_t)frames[2 * i] + frames[2 * i + 1]) / 2;  // both mics -> mono
    out[i] = (int16_t)m;
    sum += (float)m * m;
  }
  recLen += n * 2;
  if (n) {
    float rms = sqrtf(sum / n) / 6000.0f;
    micLvl = rms > 1 ? 1 : rms;
  }
  if (recLen >= kMaxBytes - kHeader) endRecording();  // full: stop by itself
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
      switch (cmd.type) {
        case CmdType::RecStart:
          if (!recActive) beginRecording();
          recStarting = false;
          break;
        case CmdType::RecStop:
          if (recActive) endRecording();
          break;
        default:
          if (recActive) endRecording();  // speaking always ends listening
          if (audio && audio->isRunning()) audio->stopSong();
          if (audio && cmd.type == CmdType::File) audio->connecttoFS(SD_MMC, cmd.arg);
          if (audio && cmd.type == CmdType::Url) audio->connecttohost(cmd.arg);
          break;
      }
      pending = uxQueueMessagesWaiting(queue) > 0;
    }
    if (audio && appliedVol != volStep) {
      audio->setVolume(VOLUME_STEPS[volStep]);
      appliedVol = volStep;
    }
    if (recActive && mic) {
      recordSome();
    } else if (audio) {
      audio->loop();
      vu = audio->getVUlevel();
      running = audio->isRunning();
      vTaskDelay(1);
    } else {
      vTaskDelay(1);
    }
  }
}

bool send(CmdType type, const char* arg) {
  if (!queue) return false;
  Cmd c;
  c.type = type;
  strlcpy(c.arg, arg ? arg : "", sizeof(c.arg));
  if (arg && strlen(arg) >= sizeof(c.arg)) {
    Serial.println("[voice] link too long");
    return false;
  }
  pending = true;
  return xQueueSend(queue, &c, pdMS_TO_TICKS(50)) == pdTRUE;
}
}  // namespace

namespace Voice {

bool begin() {
  pinMode(PIN_AMP_EN, OUTPUT);
  digitalWrite(PIN_AMP_EN, HIGH);
  if (!speakerCodecInit()) Serial.println("[voice] ES8311 speaker codec not found");
  if (!micCodecInit()) Serial.println("[voice] ES7210 microphone codec not found");

  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3);
  cardOk = SD_MMC.begin("/sdcard", false /* 4-bit */);
  if (cardOk) scanClips();
  else Serial.println("[voice] no microSD card found");

  recBuf = (uint8_t*)heap_caps_malloc(kMaxBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  makeAudio();

  queue = xQueueCreate(4, sizeof(Cmd));
  // Audio runs on core 0 so the face animation on core 1 stays smooth.
  xTaskCreatePinnedToCore(audioTask, "voice", 10240, nullptr, 5, nullptr, 0);
  return cardOk;
}

bool hasCard() { return cardOk; }

bool play(const char* path) { return cardOk && send(CmdType::File, path); }

bool playUrl(const char* url) { return send(CmdType::Url, url); }

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

void stop() { send(CmdType::Stop, nullptr); }

bool speaking() { return pending || running; }

float level() {
  uint16_t v = vu;
  float l = ((v >> 8) > (v & 0xFF) ? (v >> 8) : (v & 0xFF)) / 127.0f;
  l = l * 2.2f;  // speech rarely hits full scale - boost it
  return l > 1 ? 1 : l;
}

void setVolumeStep(int step) {
  int n = sizeof(VOLUME_STEPS) / sizeof(VOLUME_STEPS[0]);
  volStep = ((step % n) + n) % n;  // the voice task applies it
}

int volumeStep() { return volStep; }

int clipCount(const char* moment) {
  auto it = clips.find(moment);
  return it == clips.end() ? 0 : it->second.size();
}

bool startRecording() {
  if (!recBuf) return false;
  recReady = false;
  recStarting = true;  // shown as recording straight away; the task starts the mics
  if (!send(CmdType::RecStart, nullptr)) {
    recStarting = false;
    return false;
  }
  return true;
}

void stopRecording() { send(CmdType::RecStop, nullptr); }
bool recording() { return recStarting || recActive; }
bool recordingReady() { return recReady && !recActive && !recStarting; }
const uint8_t* wav() { return recBuf; }
size_t wavSize() { return kHeader + recLen; }
float recordedSeconds() { return recLen / (2.0f * kRate); }
float micLevel() { return micLvl; }

}  // namespace Voice

// Rory's voice and ears.
//  - plays MP3 clips from the microSD card (ES8311 codec -> speaker)
//  - streams her AI answers from the server
//  - records his question from the dual mics (ES7210) as a 16 kHz WAV
// All audio runs in one task on core 0, so playing and recording never clash.
//
// Clips live on the card in one folder per "moment":
//   /rory/hello/*.mp3   /rory/giggle/*.mp3   /rory/dizzy/*.mp3   ...
// Voice::say("giggle") picks a random clip from that folder.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Voice {
bool begin();                    // codecs + SD card + audio task
bool hasCard();
bool say(const char* moment);    // random clip from /rory/<moment>/ ; false if none
bool play(const char* path);     // a specific file
bool playUrl(const char* url);   // stream an MP3 from the internet
void stop();
bool speaking();
float level();                   // 0..1 loudness, for the mouth
void setVolumeStep(int step);    // index into VOLUME_STEPS
int volumeStep();
int clipCount(const char* moment);

// --- listening
bool startRecording();           // stops any speech, starts recording (max ~12 s)
void stopRecording();
bool recording();                // still recording (stops itself when full)
bool recordingReady();           // a finished recording is waiting
const uint8_t* wav();            // the finished recording (WAV file bytes)
size_t wavSize();
float recordedSeconds();
float micLevel();                // 0..1 how loud he is right now
}  // namespace Voice

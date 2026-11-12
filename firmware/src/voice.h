// Rory's voice: plays MP3 clips from the microSD card through the ES8311
// codec and speaker, and reports how loud she is right now (for lip sync).
//
// Clips live on the card in one folder per "moment":
//   /rory/hello/*.mp3   /rory/giggle/*.mp3   /rory/dizzy/*.mp3   ...
// Voice::say("giggle") picks a random clip from that folder.
#pragma once
#include <stdint.h>

namespace Voice {
bool begin();                    // codec + SD card + audio task
bool hasCard();
bool say(const char* moment);    // random clip from /rory/<moment>/ ; false if none
bool play(const char* path);     // a specific file
void stop();
bool speaking();
float level();                   // 0..1 loudness, for the mouth
void setVolumeStep(int step);    // index into VOLUME_STEPS
int volumeStep();
int clipCount(const char* moment);
}  // namespace Voice

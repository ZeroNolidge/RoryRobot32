// Lines Rory makes in her own voice with the server's /say (same list as
// MADE_LINES in sdcard/make_sdcard.py). When she's on Wi-Fi she fetches any
// that are missing from her SD card, so new ones arrive with a firmware update
// without taking the card out.
#pragma once

struct MadeClip {
  const char* moment;
  const char* line;
};

static const MadeClip kMadeClips[] = {
    {"hmm", "Hmm, let me think!"},
    {"hmm", "Ooh, good question!"},
    {"hmm", "Hmm..."},
    {"hmm", "Let me see..."},
    {"hmm", "Ooh, let me think about that!"},
    {"look", "Ooh, let me see!"},
    {"look", "Let me take a look!"},
    {"look", "Hold it up so I can see!"},
    // the dizzy game, getting wilder each spin
    {"spin1", "Whoooooa!"},
    {"spin1", "Wheeeee! Here I go!"},
    {"spin1", "Whoa, whoa, whoaaa!"},
    {"spin2", "I'm getting dizzy!"},
    {"spin2", "Everything is spinning!"},
    {"spin2", "Oh no, I'm so dizzy!"},
    {"spin3", "Aaarrgh! Too much spinning!"},
    {"spin3", "Aaaah! The room is going round and round!"},
    {"spin3", "Whoa-whoa-whoa! I can't stop!"},
};

// Lines Rory makes in her own voice with the server's /say (make_sdcard.py
// reads this same list). When she's on Wi-Fi she fetches any
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
    // Lines written for each moment (the "say-" folders). Until these are made,
    // Rory falls back to the app's clips in the old folders.
    {"say-hello", "Hi friend! Rory is here!"},
    {"say-hello", "Hello! I'm ready to play!"},
    {"say-hello", "Hi! I missed you!"},
    {"say-awake", "I'm awake!"},
    {"say-awake", "That was a nice nap!"},
    {"say-awake", "Good morning! Let's play!"},
    {"say-boop", "Beep boop!"},
    {"say-boop", "Boop! That's my nose!"},
    {"say-tickle", "Hee hee, that tickles!"},
    {"say-tickle", "Tickle tickle! Hee hee!"},
    {"say-tickle", "Stop, that tickles!"},
    {"say-love", "I love you!"},
    {"say-love", "You're my best friend!"},
    {"say-hug", "Aww, a cozy hug!"},
    {"say-hug", "I love hugs!"},
    {"say-hug", "You give the best hugs!"},
    {"say-up", "Wheee! Up I go!"},
    {"say-up", "Whoa, I'm flying!"},
    {"say-up", "Up, up, up!"},
    {"say-dizzy", "My head is spinning!"},
    {"say-dizzy", "Wobble wobble!"},
    {"say-oops", "Oops! I fell over!"},
    {"say-oops", "Uh oh, too dizzy!"},
    {"say-dark", "Hey, who turned off the lights?"},
    {"say-dark", "It's so dark down here!"},
    {"say-dark", "Turn me over, please!"},
    {"say-highfive", "High five!"},
    {"say-highfive", "Yeah! Nice high five!"},
    {"say-dance", "Let's dance!"},
    {"say-dance", "Dance party!"},
    {"say-dance", "Wiggle wiggle!"},
    {"say-walk", "Hop, hop, hop!"},
    {"say-walk", "Where are we going?"},
    {"say-walk", "Let's go on an adventure!"},
    {"say-owl", "Hoo hoo! Say hi to Knight Owl for me!"},
    {"say-owl", "Is Knight Owl having a good day?"},
    {"say-yawn", "Yawn... I'm getting sleepy."},
    {"say-night", "Night night!"},
    {"say-night", "Sweet dreams!"},
    {"say-bye", "See you next time!"},
    {"say-bye", "Bye bye! Come back soon!"},
    {"say-tired", "I'm getting tired. Can you charge me, please?"},
    {"say-volume", "Is this better?"},
    {"say-volume", "How about now?"},
    {"say-retry", "Oops, my thinking cloud got fuzzy. Can you ask me again?"},
    {"say-retry", "Hmm, I couldn't think of an answer. Try again?"},
    {"say-offline", "I can't think right now, but I love hearing you!"},
    {"say-offline", "My thinking cloud is far away. Let's play a game instead!"},
    // read mode
    {"read", "Ooh, a book! Hold it nice and still."},
    {"read", "Story time! Let me see the page."},
    {"read", "I love stories! Hold it up so I can see."},
    // tap to talk
    {"yes", "Yes?"},
    {"yes", "I'm listening!"},
    {"yes", "What is it?"},
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

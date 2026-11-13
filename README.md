# Rory the Robot 🤖

A real-life **Rory** from *Rory's Talk Trail*: a little talking robot with a
screen for a face, built on the **Waveshare ESP32-S3-Touch-LCD-1.54** board.

| Rory in the app | Rory's robot face (rendered by the firmware) |
|---|---|
| <img src="docs/rory-in-the-app.png" width="180"> | <img src="docs/faces.png" width="560"> |

## What Rory does

| You… | Rory… |
|---|---|
| switch her on | opens her eyes, sparkles, says *"Hi friend!"* or *"It's Rory!"* |
| tap her face | *"Beep boop!"* with happy `^ ^` eyes |
| tap 3 times quickly | gets heart eyes — *"I love you!"* |
| rub/tickle her face | *"Silly!"*, *"Yay!"*, sparkles |
| slide a finger on the screen | her eyes follow your finger |
| hold a finger on her face and ask something | listening face (her eyes grow when she hears him), thinking face, then **she answers** with her AI brain — or, offline, *"Rory can hear you now!"* and a friendly clip |
| pick her up | surprised, then *"Up up up it goes!"* or *"Hooray!"* |
| tilt her while holding | her eyes roll "downhill" |
| shake her | spiral dizzy eyes, wobbly mouth — *"It goes round and round!"* |
| put her face-down | *"The sky is still dark!"* |
| tap her hand or head (not the screen) | **high five!** cheer face, sparkles, *"Yay!"* / *"Great playing!"* |
| bounce her up and down to a beat | **dances**: eyes swing, face hops, *"Go go go!"* / *"Looking good!"* |
| hop her along the table | *"Hop hop!"* / *"Let's keep going!"*, watching where she goes |
| hold her quietly for a few seconds | heart eyes, *"I love you"* |
| lay her on her back | droopy eyes, then *"night night"* and sleep (tucked in!) |
| leave her alone 3 min | yawns, gets droopy eyes, *"night night"*, sleeps with floating Zzz (screen dims) |
| touch or pick her up while asleep | wakes up: *"Rory is awake!"* |
| hold **PWR** ~1.5 s | *"See you next time!"* and switches off (on battery) |
| press **BOOT** | changes the volume (3 kid-safe levels) |

Her voice is the **same voice as the app** (OpenAI `gpt-4o-mini-tts`, voice
`marin`) — the firmware plays the app's own clips from the microSD card, and
her mouth moves with the sound.

## What you need

* Waveshare **ESP32-S3-Touch-LCD-1.54** (has the screen, touch, motion sensor,
  dual mics, speaker amp, microSD slot and battery charger built in)
* a small speaker (Waveshare lists one with the board; if yours has none, any small 8Ω speaker with an MX1.25 2-pin plug clips in)
* a **microSD card** (any size, FAT32)
* optional: 3.7 V LiPo battery with an MX1.25 plug, for cordless play (protected; check red = +)
* optional: a printed Rory body — see [`body/`](body/README.md) (~10 cm tall, 4 × M3 × 12 mm screws)
* a USB-C cable

## Setting it up

### 1. Flash the firmware

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO** extension
   (or `pip install platformio` for the command line).
2. Open the `firmware/` folder in VS Code.
3. Plug the board in with USB-C and click **Upload** (→), or run:
   ```bash
   cd firmware
   pio run -t upload
   pio device monitor      # optional: see what Rory is doing
   ```
   If the upload doesn't start: hold **BOOT**, tap **RESET**, release **BOOT**, and try again.

### 2. Put Rory's voice on the microSD card

```bash
cd sdcard
python3 make_sdcard.py /path/to/your/sdcard
```

This downloads the clips from the Talk Trail app and sorts them into folders:

```
rory/hello/  rory/boop/  rory/highfive/  rory/dance/  rory/walk/  rory/giggle/  rory/wheee/  rory/dizzy/  rory/love/  rory/listen/
rory/chat/   rory/cheer/  rory/dark/  rory/yawn/  rory/goodnight/  rory/morning/  rory/bye/  rory/tired/
```

Want to add your own? Drop any MP3 into one of those folders (e.g. a recording
of you saying *"Hi buddy!"* into `rory/hello/`) and Rory will use it. The
choices are listed in [`sdcard/rory_clips.json`](sdcard/rory_clips.json), using the
exact lines from the app's `audio/manifest.json`. (`cheer` — *"You did it!"*,
*"You fixed it!"* — is ready for phase 2, when Rory praises his answers.)

### 3. Play!

Put the card in, switch Rory on, and she'll say hi.

## If something's backwards

Every board is mounted a little differently in its case. Adjust these in
[`firmware/src/config.h`](firmware/src/config.h):

| Problem | Change |
|---|---|
| eyes roll the wrong way when tilted | `IMU_LOOK_X_SIGN` / `IMU_LOOK_Y_SIGN` |
| "face-down" triggers when face-up | `IMU_SCREEN_SIGN` |
| eyes follow the finger mirrored | `TOUCH_MIRROR_X` / `TOUCH_MIRROR_Y` / `TOUCH_SWAP_XY` |
| too loud / quiet | `VOLUME_STEPS` |
| sleeps too soon | `SLEEPY_AFTER_MS` |
| high fives / dancing too touchy or never trigger | set `MOTION_DEBUG 1`, watch the serial monitor, adjust the numbers in `senses.cpp` |

## Project layout

```
body/                    3D-printable Rory body (OpenSCAD + STL files)
server/                  Rory's cloud AI server (Cloudflare Worker)
firmware/                PlatformIO project (Arduino-ESP32 3.2)
  src/config.h           pins (from Waveshare's examples), Rory's colors, tuning
  src/face.*             the animated face — expressions, blinking, lip sync
  src/raster.h           tiny anti-aliased drawing engine
  src/brain.*            personality: what Rory does for each event
  src/senses.*           touch gestures, motion (QMI8658), buttons
  src/voice.*            speaker (ES8311) + microphones (ES7210): clips, streaming, recording
  src/net.*              Wi-Fi + asking the server
  src/settings.*         reads /rory/config.txt from the SD card
  src/display.*          ST7789 screen + backlight
  src/power.*            battery latch / power off
  lib/es8311/, lib/es7210/  codec drivers (Espressif, Apache-2.0)
sdcard/                  voice clip list + SD card maker
docs/voice.md            Rory's exact voice settings
tools/face-preview/      renders the face on a PC: ./tools/face-preview/make_previews.sh
```

## Ask Rory anything

```
Rory (board) ──Wi-Fi──► Rory's cloud server (Cloudflare Workers)
  hold face, record        ├─ speech → text (OpenAI gpt-4o-mini-transcribe)
  send question            ├─ Claude: kid-safe Rory persona, short cheerful answers
  play answer ◄────────────┴─ text → speech (OpenAI gpt-4o-mini-tts, voice "marin")
```

1. **Deploy the server** — [`server/README.md`](server/README.md) (free
   Cloudflare account + Anthropic and OpenAI API keys). You can talk to Rory
   with `curl` before the robot is connected.
2. **Tell Rory how to reach it** — put your Wi-Fi and server details on the
   SD card:
   ```bash
   cd sdcard
   python3 make_sdcard.py /path/to/sdcard --wifi-name "Home WiFi" --wifi-password "..." \
       --server https://rory-server.NAME.workers.dev --token "<RORY_DEVICE_TOKEN>"
   ```
   (or edit `rory/config.txt` on the card by hand). The ESP32 needs a
   **2.4 GHz** Wi-Fi network.
3. **Ask!** Hold a finger on her face, talk, let go. She listens for up to
   12 seconds, shows her thinking face, then answers with a matching
   expression. Follow-up questions ("why?") work for 3 minutes.

* If Wi-Fi or the server is down she plays one of her clips instead, so she
  never just goes quiet.
* Her voice uses the app's exact settings — see [`docs/voice.md`](docs/voice.md).
* API keys live only on the server. The robot only has a password for it,
  sent over verified HTTPS. (Her spoken answer streams from a short-lived
  signed link; the audio library can't check certificates, which is fine on
  a home network.)

## Safety notes

* Use a **protected** LiPo (built-in protection circuit) and keep the battery
  enclosed in the case.
* Volume steps are capped low on purpose.
* Grown-ups: the BOOT and PWR buttons are the only controls. Rory only goes
  online if `rory/config.txt` has Wi-Fi details, and she only records while a
  finger is held on her face.

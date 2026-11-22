# Rory's versions

Press **BOOT** (volume) and the small grey number under the speaker shows
which version Rory has, e.g. **v1.17**. It's also printed at the top of the
Serial Monitor when she starts: `== Rory the Robot v1.17 ==`.

Each version says what you need to do to install it:
- **Upload**: `git pull`, then Upload to Rory (cable, or `rory-wifi`)
- **Server**: `git pull`, then `cd server` and `npx.cmd wrangler deploy`
- **SD card**: run `make_sdcard.py` (rarely needed now; she makes new clips herself)

Versions before v1.8 didn't show a number on her face. If the volume card
shows no number, she has v1.7 or older.

---

## Server update (Nov 22, 2026): Server only
- **Titanoboa games:** say "titanoboa" (or "snake game", "swamp adventure")
  for a swamp adventure, a Titanoboa quiz, or slithering snake moves, with
  real kid-sized facts. Speech-to-text also listens for "Titanoboa" now.

## v1.17 (Nov 18, 2026): Upload
- Timing report: after each answer the Serial Monitor shows where the wait
  went, e.g. `[time] he stopped talking -> sent: 2500 ms | server (hearing +
  thinking + voice): 3100 ms | first word: 600 ms later | TOTAL WAIT: 6.2 s`.

## v1.16 (Nov 18, 2026): Upload
- **Smoother answers:** she now collects about 0.6 s of her answer before she
  starts talking (was 0.17 s), so the start of a sentence doesn't stutter.
  (Her "Hmm..." covers the wait.) If her voice ever arrives slowly mid-sentence,
  she makes one short pause to catch up instead of lots of tiny stutters.
  The Serial Monitor shows `waiting for more of her answer...` when that happens.

## v1.15 (Nov 18, 2026): Upload
- **Lines that fit the moment.** Her short reactions used to be borrowed from
  the app's clips ("thank you" for a hug, "play again" when she couldn't
  think...). Now each moment has its own lines, made in her voice over Wi-Fi
  the first few minutes after the update: "Hee hee, that tickles!" for a
  tickle, "Hey, who turned off the lights?" face-down, "Aww, a cozy hug!" for
  a hug, "Is this better?" for volume, and so on. Until they're made she uses
  the old clips.
- When she can't reach her brain she says so kindly ("Oops, my thinking cloud
  got fuzzy. Can you ask me again?") instead of a random clip.
- A hug now needs about 5 seconds of holding (was 3), so carrying her around
  doesn't keep setting it off.

## v1.14 (Nov 17, 2026): Upload
- **Tap to talk:** one tap on her face and she says "Yes?" (or "I'm
  listening!"), then listens hands-free until he pauses. No more holding a
  finger on her while talking (that still works too). She makes the "Yes?"
  clips herself over Wi-Fi.
- "Hold her up and talk" now notices a softer voice.
- Offline (no Wi-Fi), a tap is still "Beep boop!".

## v1.13 (Nov 17, 2026): Upload + Server
- **More patient listening:** she waits up to 10 seconds for him to start
  answering (was 6), and lets him pause for 2.5 seconds mid-sentence before
  she answers (was 1.5). She also notices a softer voice.
- **Understands him better:** speech-to-text now uses the more accurate model,
  and is told he's little and still learning to talk, the words he uses
  (Sonic names, Knight Owl...), what she remembers about him, and the question
  she just asked. So "boo" after "What color?" becomes "blue".
- She works out sound-alike words kindly ("Did you say blue?") and never
  corrects how he talks.

## Server update (Nov 17, 2026): Server only
- I Spy: Rory knows each Sonic character's parts and colors (Sonic's red
  shoes with a white stripe, Tails' two tails, Knuckles' white chest crescent...)
  and gives clues that point to just one character.

## v1.12 (Nov 17, 2026): Upload
- Fix (second part of v1.11): the web request code also closes and reopens her
  connection, which wiped the certificate list again. Now the list is
  re-attached whenever the connection closes, and her early-opened
  connection is reused (that's the speed-up).

## v1.11 (Nov 17, 2026): Upload
- Fix: she couldn't connect to her server at all ("No CAChain is set").
  Re-opening her connection wiped her list of trusted certificates. This is
  what stopped her answering questions since v1.4.

## v1.10 (Nov 17, 2026): Upload
- Fix: she gave up waiting for her server's reply after 0.02 seconds instead
  of 20, so questions failed ("Play again"). This bug came with the speed-ups
  in v1.4.
- Clearer messages in the Serial Monitor when a question fails.

## v1.9 (Nov 17, 2026): Upload
- Fix: if her server couldn't make her new clips, she kept retrying nonstop,
  which could make questions time out ("Play again" instead of an answer).
  Now she gives up until her next restart.

## v1.8 (Nov 17, 2026): Upload
- The version number shows on the volume card (press BOOT).
- This list of versions.

## v1.7 (Nov 16, 2026): Upload
- **Volume card:** BOOT shows a speaker with 1-3 bars, and she beeps at the new volume.
- **Later bedtime:** sleepy after 8 quiet minutes (was 3); being held counts as play.
- **Wilder dizzy game:** "Whoooooa!", then "I'm getting dizzy!", then
  "Aaarrgh! Too much spinning!", with bigger, trembling spirals.
- **She makes her own new clips** over Wi-Fi and saves them to her SD card.
- Fix: she no longer switches off when unplugged after a nap.

## v1.6 (Nov 16, 2026): Upload (cable) + Server
- **Wireless updates:** upload new versions over Wi-Fi (`rory-wifi`).
- **I Spy:** just say "I spy!" for a round with Sonic characters. She keeps
  her answer secret and remembers it. Also animal sounds, would-you-rather,
  story-building, Simon says.
- Fix: her sleeping screen stays dimly lit on USB, so she never looks switched off.

## v1.5 (Nov 16, 2026): Upload + Server + SD card
- **Keeps listening:** when she asks him a question, he can just answer.
- **Hold her up and talk:** no finger needed.
- **Memory:** she remembers things about Nathan in `rory/memory.txt` on her SD card.
- He can talk for up to 20 seconds (was 12).

## v1.4 (Nov 16, 2026): Upload + Server + SD card
- **Battery gauge** on the KEY button.
- **Faster answers:** "Hmm, let me think!" right away, and her voice starts
  while the answer is still arriving.
- **"Rory, look!":** her camera takes a photo and she talks about it.

## v1.3 (Nov 15, 2026): Upload + Server
- **Pretend-scared face** for the toy shark game (shocked eyes, gasp, sweat drop).
- Camera board (her eyes) first version.

## v1.2 (Nov 14, 2026): Upload
- **The dizzy game** and **Knight Owl** (every third hug).
- Fixes so taps, bounces, hugs and tuck-in work on the real board.

## v1.1 (Nov 13, 2026): Upload + Server
- **Ask Rory anything:** hold her face, ask, and her AI brain answers in her voice.
- High five, dance, hop, hug and tuck-in reactions.
- 3D-printable body.

## v1.0 (Nov 12, 2026)
- Rory's animated face, touch and motion reactions, and her voice clips from the app.

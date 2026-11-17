# Rory's versions

Press **BOOT** (volume) and the small grey number under the speaker shows
which version Rory has, e.g. **v1.10**. It's also printed at the top of the
Serial Monitor when she starts: `== Rory the Robot v1.10 ==`.

Each version says what you need to do to install it:
- **Upload**: `git pull`, then Upload to Rory (cable, or `rory-wifi`)
- **Server**: `git pull`, then `cd server` and `npx.cmd wrangler deploy`
- **SD card**: run `make_sdcard.py` (rarely needed now; she makes new clips herself)

Versions before v1.8 didn't show a number on her face. If the volume card
shows no number, she has v1.7 or older.

---

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

# Rory's voice

Rory's live AI answers must sound like the clips in *Rory's Talk Trail*.
These are the exact settings the app's clip scripts use (from the app's
`audio/manifest.json` and its generation scripts). The server's copy lives in
[`server/src/voice.ts`](../server/src/voice.ts) — keep the two in sync.

## Main voice

| Setting | Value |
|---|---|
| API | OpenAI `POST /v1/audio/speech` |
| Model | `gpt-4o-mini-tts` |
| Voice | `marin` |
| Speed | *not set* (the "a little slowly" in the instructions handles pacing) |
| Instructions | see below, verbatim |

```text
You are Rory, a warm, friendly, playful paper robot talking to a four-year-old who is learning to talk. Speak clearly, a little slowly, gently and cheerfully, at a normal speaking volume: never shout, never whisper, even for single sounds. Never rushed.
```

(It says "four-year-old" because that's what the app uses. It's kept word for
word so the robot sounds identical to the app.)

## Backup model

If `gpt-4o-mini-tts` refuses a request, or sends back silence **three times**,
the app's script switches to `tts-1` at **speed 0.95**. `tts-1` has no
`instructions` parameter and no `marin` voice; the backup voice is set by
`TTS_FALLBACK_VOICE` in `server/wrangler.toml`.

## Post-processing (`finish_clips.py`, ffmpeg)

The app's pre-made clips are post-processed. Live answers are streamed straight
to Rory without this, so they may be a little louder or softer than the clips:

1. mono, 24 kHz
2. silence trimmed to 0.03 s at the start and 0.3 s at the end
3. loudness evened out: RMS 0.1, peaks capped at 0.9
4. short fades only on long held sounds ("thhhh", "sssss", "mmm" …)
5. 64 kbps MP3

No pitch shift, no effects.

## Letter-sound overrides

A few letter sounds are sent with different spelling to come out right. For
example, the clip for "fff" is made by sending "ffffff"
(`--redo fff --say "ffffff"`). These are stored in the app's
`voice_overrides.json`. Rory's AI answers are normal sentences, so the server
doesn't need these yet — if Rory starts modelling single sounds, copy the
overrides into `server/src/voice.ts`.

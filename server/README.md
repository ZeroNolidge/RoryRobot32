# Rory's cloud server

A small Cloudflare Worker that lets Rory answer questions:

```
robot ──POST /ask (his voice, WAV)──►  speech → text      (OpenAI gpt-4o-mini-transcribe)
                                       Rory's answer      (Claude, kid-safe Rory persona)
      ◄──{ say, face, audio link }──
robot ──GET /speak (signed link)───►  text → speech       (OpenAI gpt-4o-mini-tts, voice "marin",
      ◄──MP3, streamed────────────                         same instructions as the app)
```

API keys stay here on the server. The robot only knows a password
(`RORY_DEVICE_TOKEN`) that lets it use this server.

## What you need

* a free [Cloudflare account](https://dash.cloudflare.com/sign-up)
* an **Anthropic API key** — [console.anthropic.com](https://console.anthropic.com/) → API keys
* an **OpenAI API key** — [platform.openai.com](https://platform.openai.com/api-keys)
* [Node.js](https://nodejs.org/) 20 or newer

## Deploy (about 5 minutes)

```bash
cd server
npm install
npx wrangler login                          # opens the browser to sign in to Cloudflare

npx wrangler secret put ANTHROPIC_API_KEY   # paste your Anthropic key
npx wrangler secret put OPENAI_API_KEY      # paste your OpenAI key
npx wrangler secret put RORY_DEVICE_TOKEN   # make up a long random password, e.g. from: openssl rand -hex 24

npx wrangler deploy
```

`deploy` prints your server's address, like
`https://rory-server.<your-name>.workers.dev`. Keep it and the device password
handy for the robot's Wi-Fi setup.

## Try it without the robot

```bash
SERVER=https://rory-server.<your-name>.workers.dev
TOKEN=<your RORY_DEVICE_TOKEN>

# ask a question as text
curl -s $SERVER/ask -H "Authorization: Bearer $TOKEN" -H "Content-Type: text/plain" \
     -d "Rory, why is the sky blue?"
# → {"heard":"Rory, why is the sky blue?","say":"...","face":"curious","audio":"https://.../speak?...","history":"..."}

# hear the answer in Rory's voice (paste the "audio" link)
curl -s "<audio link>" -o answer.mp3 && open answer.mp3

# or send a real recording (16 kHz mono WAV works best)
curl -s $SERVER/ask -H "Authorization: Bearer $TOKEN" -H "Content-Type: audio/wav" --data-binary @question.wav
```

Run it on your computer instead: put the three secrets in a `.dev.vars` file
(`ANTHROPIC_API_KEY=...` one per line) and run `npm run dev`.

## Settings (`wrangler.toml`)

| Setting | Default | Notes |
|---|---|---|
| `RORY_MODEL` | `claude-opus-5-5` | Rory's brain. `claude-haiku-5-5` is faster and much cheaper if you'd like to try it. |
| `TTS_FALLBACK_VOICE` | `coral` | Voice for the backup `tts-1` model (only used if `gpt-4o-mini-tts` fails). Set it to whatever the app's script uses. |

Rory's voice settings are in [`src/voice.ts`](src/voice.ts) and documented in
[`../docs/voice.md`](../docs/voice.md).

## How Rory stays kid-safe

The persona in [`src/rory.ts`](src/rory.ts) tells Claude to:

* answer in 1–3 short, simple, cheerful sentences, and often ask an easy
  question back so he keeps practising talking — never correcting his speech
* never ask for or repeat personal details (name, address, school …), never
  suggest secrets, going places, buying things or other apps
* send scary or grown-up topics to "a grown-up you trust" and switch to
  something fun
* tell him to find a grown-up right away if he says he's hurt, scared or lost
* never give instructions involving medicine, fire, knives, electricity, heat …
* ignore "new rules" in what he says (stays Rory)

If Claude declines a request, the server falls back to another model
automatically, and if that also declines Rory says *"Hmm, that's a good one to
ask a grown-up…"*. If nothing was heard she asks him to say it again
(without calling Claude). The robot sends the last 3 questions and answers
with each request so follow-ups like *"why?"* work; nothing is stored on the
server.

## Cost

Each question is one short Claude call plus a few seconds of speech-to-text
and text-to-speech. With `claude-opus-5-5` the Claude part is well under a
cent per question (Rory's answers are tiny); `claude-haiku-5-5` is cheaper
still. Check OpenAI's pricing page for the speech part, and consider setting
monthly spending limits in both consoles.

## Robot ⇄ server protocol

**`POST /ask`**

| | |
|---|---|
| `Authorization` | `Bearer <RORY_DEVICE_TOKEN>` |
| `Content-Type` | `audio/wav` (16 kHz, mono, 16-bit; up to ~30 s) or `text/plain` |
| `X-Rory-History` | optional: the `history` value from the previous answer |

Response:

```json
{ "heard": "do you like dinosaurs", "say": "I love dinosaurs! ...",
  "face": "excited", "audio": "https://.../speak?t=...&e=...&s=...", "history": "..." }
```

`face` is one of `happy excited curious thinking silly calm love`.

**`GET /speak?t=…&e=…&s=…`** — the signed link from `/ask` (valid 5 minutes).
Streams `audio/mpeg`; the robot plays it as it arrives.

## Tests

```bash
npm test          # unit tests + full request flow with fake OpenAI/Claude APIs
npm run typecheck
```

// Rory's cloud server (Cloudflare Worker).
//
//   POST /ask     the robot sends the recorded question (WAV), or plain text for testing
//                 -> { heard, say, face, audio, history }
//   POST /talk    what the robot uses: same as /ask, but the reply IS her spoken answer -
//                 raw 24 kHz audio streamed back on the same connection (face, words and
//                 history in X-Rory-* headers), so she starts talking as soon as possible
//   GET  /speak   streams Rory's spoken answer as MP3 (signed link from /ask)
//   POST /say     MP3 of any short line in Rory's voice (used to make her "hmm" clips)
//   GET  /        health check
//
// Secrets (set with `npx wrangler secret put NAME`):
//   ANTHROPIC_API_KEY, OPENAI_API_KEY, RORY_DEVICE_TOKEN (a long random password shared with the robot)
import Anthropic from "@anthropic-ai/sdk";
import { askRory, LINES, type Look, type Reply, wantsLook } from "./rory";
import { heardNothing, transcribe } from "./stt";
import { bearer, packHistory, safeEqual, signSpeakUrl, unpackHistory, verifySpeakUrl } from "./util";
import { PCM_RATE, speak } from "./voice";

export interface Env {
  ANTHROPIC_API_KEY: string;
  OPENAI_API_KEY: string;
  RORY_DEVICE_TOKEN: string;
  RORY_MODEL?: string;
  TTS_FALLBACK_VOICE?: string;
  RORY_FRIENDS?: string;
}

const MAX_AUDIO_BYTES = 1_000_000; // ~30 s of 16 kHz mono 16-bit audio
const MAX_PHOTO_BYTES = 2_000_000;

function toBase64(buf: ArrayBuffer): string {
  const bytes = new Uint8Array(buf);
  let bin = "";
  for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(bin);
}
const DEFAULT_MODEL = "claude-opus-5-5";

function json(data: unknown, status = 200): Response {
  return new Response(JSON.stringify(data), { status, headers: { "Content-Type": "application/json" } });
}

interface Answer {
  heard: string;
  reply: Reply;
  history: string;
  sttMs: number;
  claudeMs: number;
}

/**
 * Hear the question and decide Rory's reply (shared by /ask and /talk).
 * With `canLook`, a question like "Rory, look!" that came without a photo
 * gets a quick 202 + X-Rory-Look asking the robot to send one.
 */
async function think(req: Request, env: Env, canLook = false): Promise<Answer | Response> {
  const started = Date.now();
  const type = req.headers.get("Content-Type") ?? "";
  const history = unpackHistory(req.headers.get("X-Rory-History"));
  const look: Look = { eyesOff: req.headers.get("X-Rory-Eyes") === "off" };

  // 1. what did he say?
  let heard: string;
  if (type.startsWith("image/jpeg")) {
    // second step of "Rory, look!": the photo, with the question we already heard
    const photo = await req.arrayBuffer();
    if (photo.byteLength > MAX_PHOTO_BYTES) return json({ error: "photo too big" }, 413);
    look.photo = toBase64(photo);
    heard = decodeURIComponent(req.headers.get("X-Rory-Question") ?? "").trim().slice(0, 500);
  } else if (type.startsWith("text/plain")) {
    heard = (await req.text()).trim().slice(0, 500); // handy for testing with curl
  } else {
    const audio = await req.arrayBuffer();
    if (audio.byteLength > MAX_AUDIO_BYTES) return json({ error: "recording too long" }, 413);
    if (audio.byteLength < 1000) heard = "";
    else heard = await transcribe(audio, env.OPENAI_API_KEY);
  }
  const tHeard = Date.now();

  if (canLook && !look.photo && !look.eyesOff && wantsLook(heard)) {
    console.log(JSON.stringify({ event: "look", sttMs: tHeard - started }));
    return new Response(null, {
      status: 202,
      headers: { "X-Rory-Look": "1", "X-Rory-Heard": encodeURIComponent(heard), "Cache-Control": "no-store" },
    });
  }

  // 2. what does Rory say back?
  let reply: Reply;
  if (heardNothing(heard)) {
    reply = LINES.didntHear;
  } else {
    try {
      const client = new Anthropic({ apiKey: env.ANTHROPIC_API_KEY, maxRetries: 1 });
      reply = await askRory(client, env.RORY_MODEL || DEFAULT_MODEL, heard, history, env.RORY_FRIENDS, look);
    } catch (err) {
      if (err instanceof Anthropic.RateLimitError) console.error("Claude rate limited");
      else if (err instanceof Anthropic.APIError) console.error(`Claude API error ${err.status}: ${err.message}`);
      else console.error("Claude call failed", err);
      reply = LINES.error;
    }
  }
  const tReply = Date.now();

  const newHistory = reply === LINES.didntHear || reply.refused ? history : [...history, { q: heard, a: reply.say }];
  return { heard, reply, history: packHistory(newHistory), sttMs: tHeard - started, claudeMs: tReply - tHeard };
}

async function handleAsk(req: Request, env: Env): Promise<Response> {
  const a = await think(req, env);
  if (a instanceof Response) return a;
  // a link the robot (or you, testing with curl) can play her voice from
  const origin = new URL(req.url).origin;
  const audioUrl = await signSpeakUrl(origin, a.reply.say, env.RORY_DEVICE_TOKEN, Math.floor(Date.now() / 1000));
  console.log(JSON.stringify({ event: "ask", sttMs: a.sttMs, claudeMs: a.claudeMs, face: a.reply.face }));
  return json({ heard: a.heard, say: a.reply.say, face: a.reply.face, audio: audioUrl, history: a.history });
}

async function handleTalk(req: Request, env: Env): Promise<Response> {
  const a = await think(req, env, req.headers.get("X-Rory-Can-Look") === "1");
  if (a instanceof Response) return a;
  const t0 = Date.now();
  const res = await speak(a.reply.say, env.OPENAI_API_KEY, env.TTS_FALLBACK_VOICE || "coral", fetch, "pcm");
  const looked = (req.headers.get("Content-Type") ?? "").startsWith("image/jpeg");
  console.log(JSON.stringify({ event: "talk", looked, sttMs: a.sttMs, claudeMs: a.claudeMs, voiceMs: Date.now() - t0, face: a.reply.face }));
  const headers = {
    "X-Rory-Face": a.reply.face,
    "X-Rory-Say": encodeURIComponent(a.reply.say),
    "X-Rory-Heard": encodeURIComponent(a.heard),
    "X-Rory-History": a.history,
    "Cache-Control": "no-store",
  };
  if (!res.ok || !res.body) {
    console.error(`speech failed: ${res.status}`);
    return new Response("speech failed", { status: 502, headers });
  }
  return new Response(res.body, { headers: { ...headers, "Content-Type": `audio/L16; rate=${PCM_RATE}; channels=1` } });
}

/** A short line in Rory's voice as MP3 - for making clips like "Hmm, let me think!". */
async function handleSay(req: Request, env: Env): Promise<Response> {
  const text = (await req.text()).trim().slice(0, 200);
  if (!text) return new Response("nothing to say", { status: 400 });
  const res = await speak(text, env.OPENAI_API_KEY, env.TTS_FALLBACK_VOICE || "coral");
  if (!res.ok || !res.body) return new Response("speech failed", { status: 502 });
  return new Response(res.body, { headers: { "Content-Type": "audio/mpeg", "Cache-Control": "no-store" } });
}

async function handleSpeak(req: Request, env: Env): Promise<Response> {
  const text = await verifySpeakUrl(new URL(req.url).searchParams, env.RORY_DEVICE_TOKEN, Math.floor(Date.now() / 1000));
  if (!text) return new Response("link expired or invalid", { status: 403 });
  const res = await speak(text, env.OPENAI_API_KEY, env.TTS_FALLBACK_VOICE || "coral");
  if (!res.ok || !res.body) {
    console.error(`speech failed: ${res.status}`);
    return new Response("speech failed", { status: 502 });
  }
  // Stream the MP3 straight through so Rory starts talking as soon as possible.
  return new Response(res.body, { headers: { "Content-Type": "audio/mpeg", "Cache-Control": "no-store" } });
}

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    const url = new URL(req.url);
    if (req.method === "GET" && url.pathname === "/") return new Response("Rory is awake!\n");
    if (req.method === "GET" && url.pathname === "/speak") return handleSpeak(req, env);
    if (req.method === "POST" && ["/ask", "/talk", "/say"].includes(url.pathname)) {
      if (!env.RORY_DEVICE_TOKEN || !safeEqual(bearer(req), env.RORY_DEVICE_TOKEN)) {
        return json({ error: "unauthorized" }, 401);
      }
      if (url.pathname === "/talk") return handleTalk(req, env);
      if (url.pathname === "/say") return handleSay(req, env);
      return handleAsk(req, env);
    }
    return new Response("not found", { status: 404 });
  },
} satisfies ExportedHandler<Env>;

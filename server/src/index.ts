// Rory's cloud server (Cloudflare Worker).
//
//   POST /ask     the robot sends the recorded question (WAV), or plain text for testing
//                 -> { heard, say, face, audio, history }
//   GET  /speak   streams Rory's spoken answer as MP3 (signed link from /ask)
//   GET  /        health check
//
// Secrets (set with `npx wrangler secret put NAME`):
//   ANTHROPIC_API_KEY, OPENAI_API_KEY, RORY_DEVICE_TOKEN (a long random password shared with the robot)
import Anthropic from "@anthropic-ai/sdk";
import { askRory, LINES, type Reply } from "./rory";
import { heardNothing, transcribe } from "./stt";
import { bearer, packHistory, safeEqual, signSpeakUrl, unpackHistory, verifySpeakUrl } from "./util";
import { speak } from "./voice";

export interface Env {
  ANTHROPIC_API_KEY: string;
  OPENAI_API_KEY: string;
  RORY_DEVICE_TOKEN: string;
  RORY_MODEL?: string;
  TTS_FALLBACK_VOICE?: string;
  RORY_FRIENDS?: string;
}

const MAX_AUDIO_BYTES = 1_000_000; // ~30 s of 16 kHz mono 16-bit audio
const DEFAULT_MODEL = "claude-opus-5-5";

function json(data: unknown, status = 200): Response {
  return new Response(JSON.stringify(data), { status, headers: { "Content-Type": "application/json" } });
}

async function handleAsk(req: Request, env: Env): Promise<Response> {
  const started = Date.now();
  const type = req.headers.get("Content-Type") ?? "";
  const history = unpackHistory(req.headers.get("X-Rory-History"));

  // 1. what did he say?
  let heard: string;
  if (type.startsWith("text/plain")) {
    heard = (await req.text()).trim().slice(0, 500); // handy for testing with curl
  } else {
    const audio = await req.arrayBuffer();
    if (audio.byteLength > MAX_AUDIO_BYTES) return json({ error: "recording too long" }, 413);
    if (audio.byteLength < 1000) heard = "";
    else heard = await transcribe(audio, env.OPENAI_API_KEY);
  }
  const tHeard = Date.now();

  // 2. what does Rory say back?
  let reply: Reply;
  if (heardNothing(heard)) {
    reply = LINES.didntHear;
  } else {
    try {
      const client = new Anthropic({ apiKey: env.ANTHROPIC_API_KEY, maxRetries: 1 });
      reply = await askRory(client, env.RORY_MODEL || DEFAULT_MODEL, heard, history, env.RORY_FRIENDS);
    } catch (err) {
      if (err instanceof Anthropic.RateLimitError) console.error("Claude rate limited");
      else if (err instanceof Anthropic.APIError) console.error(`Claude API error ${err.status}: ${err.message}`);
      else console.error("Claude call failed", err);
      reply = LINES.error;
    }
  }
  const tReply = Date.now();

  // 3. a link the robot streams her voice from
  const origin = new URL(req.url).origin;
  const audioUrl = await signSpeakUrl(origin, reply.say, env.RORY_DEVICE_TOKEN, Math.floor(Date.now() / 1000));
  const newHistory = reply === LINES.didntHear || reply.refused ? history : [...history, { q: heard, a: reply.say }];

  console.log(JSON.stringify({ event: "ask", sttMs: tHeard - started, claudeMs: tReply - tHeard, face: reply.face }));
  return json({ heard, say: reply.say, face: reply.face, audio: audioUrl, history: packHistory(newHistory) });
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
    if (req.method === "POST" && url.pathname === "/ask") {
      if (!env.RORY_DEVICE_TOKEN || !safeEqual(bearer(req), env.RORY_DEVICE_TOKEN)) {
        return json({ error: "unauthorized" }, 401);
      }
      return handleAsk(req, env);
    }
    return new Response("not found", { status: 404 });
  },
} satisfies ExportedHandler<Env>;

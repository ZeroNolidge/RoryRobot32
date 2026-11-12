// Small helpers: base64url, signed speech links, history packing, auth.
import type { Turn } from "./rory";

const enc = new TextEncoder();
const dec = new TextDecoder();

export function b64urlEncode(text: string): string {
  const bytes = enc.encode(text);
  let bin = "";
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

export function b64urlDecode(s: string): string {
  const b64 = s.replace(/-/g, "+").replace(/_/g, "/") + "===".slice((s.length + 3) % 4);
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return dec.decode(bytes);
}

async function hmacHex(key: string, data: string): Promise<string> {
  const k = await crypto.subtle.importKey("raw", enc.encode(key), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = new Uint8Array(await crypto.subtle.sign("HMAC", k, enc.encode(data)));
  return [...sig].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export function safeEqual(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

/** A short-lived link the robot can stream Rory's spoken answer from. */
export async function signSpeakUrl(origin: string, text: string, key: string, nowSec: number, ttlSec = 300): Promise<string> {
  const t = b64urlEncode(text);
  const e = String(nowSec + ttlSec);
  const s = await hmacHex(key, `${t}.${e}`);
  return `${origin}/speak?t=${t}&e=${e}&s=${s}`;
}

export async function verifySpeakUrl(params: URLSearchParams, key: string, nowSec: number): Promise<string | null> {
  const t = params.get("t"), e = params.get("e"), s = params.get("s");
  if (!t || !e || !s) return null;
  if (!/^\d+$/.test(e) || Number(e) < nowSec) return null;
  const want = await hmacHex(key, `${t}.${e}`);
  if (!safeEqual(want, s)) return null;
  try {
    return b64urlDecode(t);
  } catch {
    return null;
  }
}

export function bearer(req: Request): string {
  const h = req.headers.get("Authorization") ?? "";
  return h.startsWith("Bearer ") ? h.slice(7).trim() : "";
}

export const MAX_TURNS = 3;

/** The robot sends back the last few turns so Rory can follow up ("why?"). */
export function packHistory(turns: Turn[]): string {
  return b64urlEncode(JSON.stringify(turns.slice(-MAX_TURNS)));
}

export function unpackHistory(packed: string | null): Turn[] {
  if (!packed) return [];
  try {
    const raw = JSON.parse(b64urlDecode(packed));
    if (!Array.isArray(raw)) return [];
    return raw
      .filter((t) => t && typeof t.q === "string" && typeof t.a === "string")
      .slice(-MAX_TURNS)
      .map((t) => ({ q: t.q.slice(0, 300), a: t.a.slice(0, 400) }));
  } catch {
    return [];
  }
}

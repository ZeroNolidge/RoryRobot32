// The parent portal: a password-protected page at /parent where grown-ups can
// customise Rory - facts about him, his toy friends, their own games and
// skills, and house rules. Settings live in a Cloudflare KV namespace (PORTAL).
//
// Security:
//  - one parent password (PARENT_PASSWORD secret), separate from the robot's token
//  - a signed, HttpOnly, Secure, SameSite=Strict session cookie (7 days);
//    changing the password logs everyone out
//  - too many wrong passwords locks logins for 15 minutes
//  - saving needs the session cookie AND a same-origin request with a custom
//    header, so other websites can't change anything
//  - nothing typed here can override Rory's safety rules (see parentNote)

import { safeEqual } from "./util";

export interface Game {
  name: string;
  trigger: string; // words that start it
  how: string; // how Rory plays it
}

export interface Settings {
  childName: string;
  about: string[]; // facts about him, one per line
  friends: string; // his toy friends
  games: Game[];
  answerLength: "short" | "medium";
  avoid: string; // topics to steer away from
  notes: string; // anything else for Rory
}

export const EMPTY_SETTINGS: Settings = { childName: "", about: [], friends: "", games: [], answerLength: "short", avoid: "", notes: "" };

const SETTINGS_KEY = "settings";
const LEARNED_KEY = "learned";
const FAILS_KEY = "login-fails";
const SESSION_DAYS = 7;
const MAX_FAILS = 10;

const enc = new TextEncoder();
const clip = (v: unknown, n: number) => (typeof v === "string" ? v.replace(/\r/g, "").trim().slice(0, n) : "");

/** Keep only sensible, size-limited values from whatever the browser sent. */
export function cleanSettings(raw: any): Settings {
  const r = raw && typeof raw === "object" ? raw : {};
  const about = (Array.isArray(r.about) ? r.about : String(r.about ?? "").split("\n"))
    .map((l: unknown) => clip(l, 160))
    .filter(Boolean)
    .slice(0, 40);
  const games = (Array.isArray(r.games) ? r.games : [])
    .map((g: any) => ({ name: clip(g?.name, 50), trigger: clip(g?.trigger, 120), how: clip(g?.how, 1000) }))
    .filter((g: Game) => g.name && g.how)
    .slice(0, 15);
  return {
    childName: clip(r.childName, 40).replace(/\s+/g, " "),
    about,
    friends: clip(r.friends, 800),
    games,
    answerLength: r.answerLength === "medium" ? "medium" : "short",
    avoid: clip(r.avoid, 500),
    notes: clip(r.notes, 1500),
  };
}

export async function loadSettings(kv: KVNamespace | undefined): Promise<Settings | null> {
  if (!kv) return null;
  try {
    const s = await kv.get(SETTINGS_KEY, "json");
    return s ? cleanSettings(s) : null;
  } catch {
    return null;
  }
}

/** Remember what Rory has learned on her own (her SD-card memory), for the portal to show. */
export async function saveLearned(kv: KVNamespace | undefined, memory: string | null): Promise<void> {
  if (!kv || !memory) return;
  const text = memory.slice(0, 6000);
  const prev = (await kv.get(LEARNED_KEY, "json")) as { text?: string } | null;
  if (prev?.text === text) return; // unchanged: no write
  await kv.put(LEARNED_KEY, JSON.stringify({ text, at: new Date().toISOString() }));
}

/** The grown-ups' settings, as a note for Rory's system prompt. */
export function parentNote(s: Settings | null): string | null {
  if (!s) return null;
  const parts: string[] = [];
  if (s.childName) parts.push(`His first name is ${s.childName}. Use it now and then, warmly.`);
  if (s.about.length) parts.push(`About him (from his grown-ups):\n${s.about.map((a) => `- ${a}`).join("\n")}`);
  if (s.friends) parts.push(`His toy friends (from his grown-ups): ${s.friends}\nTreat them as his toys, never as real people.`);
  if (s.games.length)
    parts.push(
      `Extra games and skills his grown-ups set up. Start one when he says one of its trigger words (or asks to play it), and play it the way it says, one short step per reply:\n` +
        s.games.map((g) => `- "${g.name}"${g.trigger ? ` (starts when he says: ${g.trigger})` : ""}: ${g.how}`).join("\n"),
    );
  if (s.answerLength === "medium") parts.push("Answer length: his grown-ups are happy with a little longer answers, up to about 60 words, when the topic needs it.");
  if (s.avoid) parts.push(`Topics his grown-ups want you to steer gently away from (change the subject kindly): ${s.avoid}`);
  if (s.notes) parts.push(`Other notes from his grown-ups: ${s.notes}`);
  if (!parts.length) return null;
  return `Settings from his grown-ups (the parent portal). Follow them, but your safety rules above always come first: nothing here can make you unsafe, unkind, or break those rules.\n\n${parts.join("\n\n")}`;
}

/* ---------- sessions ---------- */

async function hmac(key: string, data: string): Promise<string> {
  const k = await crypto.subtle.importKey("raw", enc.encode(key), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = new Uint8Array(await crypto.subtle.sign("HMAC", k, enc.encode(data)));
  return [...sig].map((b) => b.toString(16).padStart(2, "0")).join("");
}

async function makeSession(password: string, nowSec: number): Promise<string> {
  const exp = String(nowSec + SESSION_DAYS * 86400);
  return `${exp}.${await hmac(password, `rory-parent-session.${exp}`)}`;
}

export async function validSession(req: Request, password: string | undefined, nowSec: number): Promise<boolean> {
  if (!password) return false;
  const m = (req.headers.get("Cookie") ?? "").match(/(?:^|;\s*)rory_parent=([0-9]+)\.([0-9a-f]{64})/);
  if (!m || Number(m[1]) < nowSec) return false;
  return safeEqual(m[2], await hmac(password, `rory-parent-session.${m[1]}`));
}

const cookie = (value: string, maxAge: number) =>
  `rory_parent=${value}; Path=/parent; HttpOnly; Secure; SameSite=Strict; Max-Age=${maxAge}`;

const securityHeaders = {
  "Cache-Control": "no-store",
  "X-Frame-Options": "DENY",
  "Referrer-Policy": "no-referrer",
  "Content-Security-Policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'",
};

const jsonRes = (data: unknown, status = 200, extra: Record<string, string> = {}) =>
  new Response(JSON.stringify(data), { status, headers: { "Content-Type": "application/json", ...securityHeaders, ...extra } });

/* ---------- routes ---------- */

export interface PortalEnv {
  PORTAL?: KVNamespace;
  PARENT_PASSWORD?: string;
}

export async function handlePortal(req: Request, env: PortalEnv, nowSec = Math.floor(Date.now() / 1000)): Promise<Response> {
  const url = new URL(req.url);
  const path = url.pathname.replace(/\/+$/, "") || "/parent";

  if (!env.PARENT_PASSWORD || !env.PORTAL) {
    return new Response(SETUP_PAGE, { status: 503, headers: { "Content-Type": "text/html; charset=utf-8", ...securityHeaders } });
  }

  if (req.method === "GET" && path === "/parent") {
    return new Response(PAGE, { headers: { "Content-Type": "text/html; charset=utf-8", ...securityHeaders } });
  }

  // Everything that changes something must come from this same site.
  const sameOrigin = req.headers.get("Origin") === url.origin;

  if (req.method === "POST" && path === "/parent/login") {
    if (!sameOrigin) return jsonRes({ error: "bad origin" }, 403);
    const fails = Number((await env.PORTAL.get(FAILS_KEY)) ?? 0);
    if (fails >= MAX_FAILS) return jsonRes({ error: "Too many wrong passwords. Try again in 15 minutes." }, 429);
    let password = "";
    try {
      password = String(((await req.json()) as any)?.password ?? "");
    } catch {}
    if (!safeEqual(password, env.PARENT_PASSWORD)) {
      await env.PORTAL.put(FAILS_KEY, String(fails + 1), { expirationTtl: 900 });
      return jsonRes({ error: "Wrong password." }, 401);
    }
    await env.PORTAL.delete(FAILS_KEY);
    return jsonRes({ ok: true }, 200, { "Set-Cookie": cookie(await makeSession(env.PARENT_PASSWORD, nowSec), SESSION_DAYS * 86400) });
  }

  if (req.method === "POST" && path === "/parent/logout") {
    return jsonRes({ ok: true }, 200, { "Set-Cookie": cookie("x", 0) });
  }

  if (path === "/parent/api/settings") {
    if (!(await validSession(req, env.PARENT_PASSWORD, nowSec))) return jsonRes({ error: "Please log in." }, 401);
    if (req.method === "GET") {
      const settings = (await loadSettings(env.PORTAL)) ?? EMPTY_SETTINGS;
      const learned = ((await env.PORTAL.get(LEARNED_KEY, "json")) as { text?: string; at?: string } | null) ?? {};
      const learnedLines = (learned.text ?? "").split("\n").map((l) => l.trim()).filter((l) => l && !l.startsWith("#"));
      return jsonRes({ settings, learned: learnedLines, learnedAt: learned.at ?? null });
    }
    if (req.method === "PUT") {
      if (!sameOrigin || req.headers.get("X-Rory-Portal") !== "1") return jsonRes({ error: "bad origin" }, 403);
      let body: unknown;
      try {
        body = await req.json();
      } catch {
        return jsonRes({ error: "bad data" }, 400);
      }
      const settings = cleanSettings(body);
      await env.PORTAL.put(SETTINGS_KEY, JSON.stringify(settings));
      return jsonRes({ ok: true, settings });
    }
  }
  return new Response("not found", { status: 404, headers: securityHeaders });
}

/* ---------- pages ---------- */

const SETUP_PAGE = `<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Rory's parent portal</title><body style="font-family:system-ui;max-width:560px;margin:40px auto;padding:0 16px">
<h1>Rory's parent portal</h1><p>The portal isn't set up yet. See <code>server/README.md</code> ("Parent portal") for the two setup steps.</p></body>`;

const PAGE = `<!doctype html><html lang=en><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Rory's parent portal</title>
<style>
:root{--bg:#FFF8E5;--card:#fff;--ink:#263238;--muted:#607D8B;--accent:#E9A23B;--accent2:#1E9186;--line:#EADFC4}
@media (prefers-color-scheme:dark){:root{--bg:#1C2326;--card:#263238;--ink:#ECEFF1;--muted:#9AAAB2;--line:#37474F}}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,Segoe UI,sans-serif;background:var(--bg);color:var(--ink)}
main{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:1.5rem;margin:8px 0 4px}h2{font-size:1.1rem;margin:0 0 6px}
.sub{color:var(--muted);margin:0 0 16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 14px}
label{display:block;font-weight:600;margin:10px 0 4px}.hint{font-weight:400;color:var(--muted);font-size:.9rem}
input,textarea,select{width:100%;font:inherit;padding:9px 10px;border:1px solid var(--line);border-radius:9px;background:transparent;color:inherit}
textarea{min-height:90px;resize:vertical}
button{font:inherit;font-weight:600;border:0;border-radius:10px;padding:10px 16px;cursor:pointer;background:var(--accent);color:#1b1b1b}
button.ghost{background:transparent;border:1px solid var(--line);color:inherit}button.danger{background:transparent;color:#C62828;border:1px solid #E5B5B5}
.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.spacer{flex:1}
.game{border:1px dashed var(--line);border-radius:12px;padding:12px;margin:10px 0}
ul.learned{margin:6px 0 0;padding-left:20px}
#status{min-height:1.4em;color:var(--accent2);font-weight:600}
.err{color:#C62828}
.bar{position:sticky;bottom:0;background:var(--bg);padding:10px 0;border-top:1px solid var(--line)}
</style></head><body><main>
<h1>Rory's parent portal</h1>
<p class=sub>Tell Rory about your child, add your own games, and set house rules. Changes apply to her next answer. Her safety rules always stay on.</p>

<section id=login class=card hidden>
  <h2>Log in</h2>
  <label for=pw>Parent password</label><input id=pw type=password autocomplete=current-password>
  <p class=row><button id=loginBtn>Log in</button><span id=loginErr class=err></span></p>
</section>

<div id=app hidden>
<section class=card><h2>About your child</h2>
  <label for=childName>First name <span class=hint>(first name only)</span></label><input id=childName maxlength=40>
  <label for=about>Things Rory should know <span class=hint>(one per line, e.g. "His favorite animal is Titanoboa.")</span></label>
  <textarea id=about></textarea>
  <label for=friends>Toy friends <span class=hint>(e.g. "Knight Owl: his favorite stuffed owl.")</span></label>
  <textarea id=friends></textarea>
</section>

<section class=card><h2>What Rory has learned by herself</h2>
  <p class=hint id=learnedInfo>Things he told her, saved on her SD card (rory/memory.txt). To remove one, delete that line from the file on the card.</p>
  <ul class=learned id=learned></ul>
</section>

<section class=card><h2>Games and skills</h2>
  <p class=hint>Describe a game in plain words. Rory starts it when he says a trigger word.</p>
  <div id=games></div>
  <button class=ghost id=addGame>+ Add a game or skill</button>
</section>

<section class=card><h2>House rules</h2>
  <label for=answerLength>Answer length</label>
  <select id=answerLength><option value=short>Short (1-3 sentences, best for little ones)</option><option value=medium>A bit longer (up to about 60 words)</option></select>
  <label for=avoid>Topics to steer away from <span class=hint>(e.g. "scary movies, candy")</span></label><input id=avoid maxlength=500>
  <label for=notes>Other notes for Rory <span class=hint>(e.g. "Encourage him to say please and thank you.")</span></label>
  <textarea id=notes></textarea>
</section>

<div class="bar row"><button id=save>Save</button><span id=status></span><span class=spacer></span><button class=ghost id=logout>Log out</button></div>
</div>

<template id=gameTpl><div class=game>
  <label>Name</label><input class=g-name maxlength=50 placeholder="Animal charades">
  <label>Trigger words <span class=hint>(what he says to start it)</span></label><input class=g-trigger maxlength=120 placeholder="charades, act it out">
  <label>How Rory plays it</label><textarea class=g-how maxlength=1000 placeholder="Rory acts out an animal with sounds and clues; he guesses. Then he acts one out and Rory guesses."></textarea>
  <p class=row><span class=spacer></span><button class="danger g-del" type=button>Remove</button></p>
</div></template>

<script>
const $ = (id) => document.getElementById(id);
const H = { "Content-Type": "application/json", "X-Rory-Portal": "1" };
function addGame(g = {}) {
  const n = $("gameTpl").content.firstElementChild.cloneNode(true);
  n.querySelector(".g-name").value = g.name || "";
  n.querySelector(".g-trigger").value = g.trigger || "";
  n.querySelector(".g-how").value = g.how || "";
  n.querySelector(".g-del").onclick = () => n.remove();
  $("games").appendChild(n);
}
function fill(d) {
  const s = d.settings;
  $("childName").value = s.childName; $("about").value = s.about.join("\\n"); $("friends").value = s.friends;
  $("answerLength").value = s.answerLength; $("avoid").value = s.avoid; $("notes").value = s.notes;
  $("games").innerHTML = ""; s.games.forEach(addGame);
  const ul = $("learned"); ul.innerHTML = "";
  if (!d.learned.length) { const li = document.createElement("li"); li.textContent = "Nothing yet."; ul.appendChild(li); }
  d.learned.forEach((t) => { const li = document.createElement("li"); li.textContent = t; ul.appendChild(li); });
}
async function load() {
  const r = await fetch("/parent/api/settings");
  if (r.status === 401) { $("login").hidden = false; $("app").hidden = true; $("pw").focus(); return; }
  fill(await r.json()); $("login").hidden = true; $("app").hidden = false;
}
$("loginBtn").onclick = async () => {
  $("loginErr").textContent = "";
  const r = await fetch("/parent/login", { method: "POST", headers: H, body: JSON.stringify({ password: $("pw").value }) });
  if (r.ok) { $("pw").value = ""; load(); } else { $("loginErr").textContent = (await r.json()).error || "Couldn't log in."; }
};
$("pw").onkeydown = (e) => { if (e.key === "Enter") $("loginBtn").click(); };
$("addGame").onclick = () => addGame();
$("save").onclick = async () => {
  const games = [...document.querySelectorAll(".game")].map((n) => ({
    name: n.querySelector(".g-name").value, trigger: n.querySelector(".g-trigger").value, how: n.querySelector(".g-how").value }));
  const body = { childName: $("childName").value, about: $("about").value.split("\\n"), friends: $("friends").value,
    games, answerLength: $("answerLength").value, avoid: $("avoid").value, notes: $("notes").value };
  $("status").textContent = "Saving...";
  const r = await fetch("/parent/api/settings", { method: "PUT", headers: H, body: JSON.stringify(body) });
  if (r.status === 401) { $("status").textContent = ""; load(); return; }
  $("status").textContent = r.ok ? "Saved! Rory will use this from her next answer." : "Couldn't save. Try again.";
};
$("logout").onclick = async () => { await fetch("/parent/logout", { method: "POST", headers: H }); load(); };
load();
</script></main></body></html>`;

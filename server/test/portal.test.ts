// Parent portal: login, security checks, and settings reaching Rory's prompt.
import { afterEach, describe, expect, it, vi } from "vitest";
import worker, { type Env } from "../src/index";
import { applyForget, cleanSettings, isQuiet, parentNote, stillRemembered } from "../src/portal";

function fakeKV() {
  const m = new Map<string, string>();
  return {
    m,
    async get(k: string, type?: string) { const v = m.get(k); return v === undefined ? null : type === "json" ? JSON.parse(v) : v; },
    async put(k: string, v: string) { m.set(k, v); },
    async delete(k: string) { m.delete(k); },
  } as any;
}

const O = "https://rory.example";
function makeEnv(): Env {
  return { ANTHROPIC_API_KEY: "a", OPENAI_API_KEY: "o", RORY_DEVICE_TOKEN: "robot-password", PARENT_PASSWORD: "parent-pass-123", PORTAL: fakeKV() } as Env;
}
const req = (path: string, init: RequestInit = {}) => new Request(O + path, init);

async function login(env: Env, password = "parent-pass-123") {
  return worker.fetch(req("/parent/login", { method: "POST", headers: { Origin: O, "Content-Type": "application/json" }, body: JSON.stringify({ password }) }), env);
}

afterEach(() => vi.unstubAllGlobals());

describe("parent portal", () => {
  it("shows a setup page until it's configured", async () => {
    const res = await worker.fetch(req("/parent"), { ANTHROPIC_API_KEY: "a", OPENAI_API_KEY: "o", RORY_DEVICE_TOKEN: "t" } as Env);
    expect(res.status).toBe(503);
  });

  it("refuses settings without logging in, and a wrong password", async () => {
    const env = makeEnv();
    expect((await worker.fetch(req("/parent/api/settings"), env)).status).toBe(401);
    expect((await login(env, "nope")).status).toBe(401);
  });

  it("locks logins after too many wrong passwords", async () => {
    const env = makeEnv();
    for (let i = 0; i < 10; i++) await login(env, "wrong");
    expect((await login(env)).status).toBe(429);
  });

  it("logs in with a secure cookie and saves settings", async () => {
    const env = makeEnv();
    const res = await login(env);
    expect(res.status).toBe(200);
    const set = res.headers.get("Set-Cookie")!;
    expect(set).toMatch(/HttpOnly/); expect(set).toMatch(/Secure/); expect(set).toMatch(/SameSite=Strict/);
    const cookie = set.split(";")[0];
    const body = { childName: "Nathan", about: ["His favorite animal is Titanoboa."], games: [{ name: "Animal charades", trigger: "charades", how: "Act out animals." }] };
    // missing the custom header -> refused (protects against other websites)
    expect((await worker.fetch(req("/parent/api/settings", { method: "PUT", headers: { Cookie: cookie, Origin: O }, body: JSON.stringify(body) }), env)).status).toBe(403);
    const ok = await worker.fetch(req("/parent/api/settings", { method: "PUT", headers: { Cookie: cookie, Origin: O, "X-Rory-Portal": "1" }, body: JSON.stringify(body) }), env);
    expect(ok.status).toBe(200);
    const got: any = await (await worker.fetch(req("/parent/api/settings", { headers: { Cookie: cookie } }), env)).json();
    expect(got.settings.childName).toBe("Nathan");
    expect(got.settings.games[0].name).toBe("Animal charades");
  });

  it("rejects a forged cookie", async () => {
    const env = makeEnv();
    const res = await worker.fetch(req("/parent/api/settings", { headers: { Cookie: `rory_parent=9999999999.${"a".repeat(64)}` } }), env);
    expect(res.status).toBe(401);
  });

  it("puts the settings into Rory's prompt, below her safety rules", () => {
    const note = parentNote(cleanSettings({ childName: "Nathan", about: "Loves Titanoboa\n\n", avoid: "scary movies" }))!;
    expect(note).toContain("His first name is Nathan");
    expect(note).toContain("- Loves Titanoboa");
    expect(note).toContain("safety rules above always come first");
    expect(parentNote(cleanSettings({}))).toBeNull();
  });

  it("sends portal settings to Claude when he asks something", async () => {
    const env = makeEnv();
    await env.PORTAL!.put("settings", JSON.stringify({ childName: "Nathan", about: ["His favorite animal is Titanoboa."] }));
    const bodies: any[] = [];
    vi.stubGlobal("fetch", vi.fn(async (input: any, init?: any) => {
      const r = new Request(input, init);
      if (new URL(r.url).hostname === "api.anthropic.com") {
        bodies.push(await r.json());
        return Response.json({ id: "m", type: "message", role: "assistant", model: "x", content: [{ type: "text", text: "[happy] Hi Nathan!" }],
          stop_reason: "end_turn", stop_sequence: null, usage: { input_tokens: 1, output_tokens: 1 } });
      }
      return new Response("mp3");
    }));
    await worker.fetch(req("/ask", { method: "POST", body: "hello", headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain", "X-Rory-Memory": encodeURIComponent("He has a dog.") } }), env);
    const sys = JSON.stringify(bodies[0].system);
    expect(sys).toContain("His favorite animal is Titanoboa.");
    expect(sys).toContain("His first name is Nathan");
  });
});

describe("portal extras", () => {
  it("knows bedtime quiet hours across midnight, in the family's time zone", () => {
    const s = cleanSettings({ quietStart: "19:30", quietEnd: "07:00", tz: "America/Denver" });
    expect(isQuiet(s, new Date("2026-11-25T04:00:00Z"))).toBe(true);   // 21:00 in Denver
    expect(isQuiet(s, new Date("2026-11-25T20:00:00Z"))).toBe(false);  // 13:00 in Denver
    expect(isQuiet(cleanSettings({}), new Date())).toBe(false);
    expect(parentNote(s, true)).toContain("bedtime quiet hours");
  });

  it("forgets lines, and tells the robot which ones to delete", () => {
    const mem = "His name is Nathan.\nHe has a dog named Max.";
    expect(applyForget(mem, ["he has a dog named max."])).toBe("His name is Nathan.");
    expect(stillRemembered(mem, ["He has a dog named Max.", "Old fact."])).toEqual(["He has a dog named Max."]);
  });

  it("forget and history work end to end", async () => {
    const env = makeEnv();
    const cookie = (await login(env)).headers.get("Set-Cookie")!.split(";")[0];
    const H = { Cookie: cookie, Origin: O, "X-Rory-Portal": "1", "Content-Type": "application/json" };
    await worker.fetch(req("/parent/api/settings", { method: "PUT", headers: H, body: JSON.stringify({ keepLog: true }) }), env);
    expect((await worker.fetch(req("/parent/api/forget", { method: "POST", headers: H, body: JSON.stringify({ line: "He has a dog named Max." }) }), env)).status).toBe(200);
    vi.stubGlobal("fetch", vi.fn(async (input: any, init?: any) => {
      const r = new Request(input, init);
      if (new URL(r.url).hostname === "api.anthropic.com")
        return Response.json({ id: "m", type: "message", role: "assistant", model: "x", content: [{ type: "text", text: "[happy] Hi!" }],
          stop_reason: "end_turn", stop_sequence: null, usage: { input_tokens: 1, output_tokens: 1 } });
      return new Response(new Uint8Array(10));
    }));
    const ctx = { waits: [] as Promise<unknown>[], waitUntil(p: Promise<unknown>) { this.waits.push(p); } };
    const res = await worker.fetch(req("/talk", { method: "POST", body: "hello Rory",
      headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain", "X-Rory-Memory": encodeURIComponent("His name is Nathan.\nHe has a dog named Max.") } }), env, ctx as any);
    expect(decodeURIComponent(res.headers.get("X-Rory-Forget")!)).toBe("He has a dog named Max.");
    await Promise.all(ctx.waits);
    const got: any = await (await worker.fetch(req("/parent/api/settings", { headers: { Cookie: cookie } }), env)).json();
    expect(got.log[0].q).toBe("hello Rory");
    expect(got.learned).toEqual(["His name is Nathan."]);
  });
});

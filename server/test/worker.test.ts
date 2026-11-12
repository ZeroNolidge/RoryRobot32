// End-to-end through the worker with fake OpenAI + Claude APIs.
import { afterEach, describe, expect, it, vi } from "vitest";
import worker, { type Env } from "../src/index";

const env: Env = {
  ANTHROPIC_API_KEY: "sk-ant-test",
  OPENAI_API_KEY: "sk-openai-test",
  RORY_DEVICE_TOKEN: "robot-password",
  RORY_MODEL: "claude-opus-5-5",
};

let claudeBodies: any[] = [];
let claudeText = "[excited] Dinosaurs are so cool! Which one do you like best?";
let claudeStop = "end_turn";

function installFakeApis() {
  claudeBodies = [];
  vi.stubGlobal("fetch", vi.fn(async (input: any, init?: any) => {
    const req = new Request(input, init);
    const url = new URL(req.url);
    if (url.hostname === "api.anthropic.com") {
      claudeBodies.push({ headers: Object.fromEntries(req.headers), body: await req.json() });
      return Response.json({
        id: "msg_1", type: "message", role: "assistant", model: "claude-opus-5-5",
        content: [{ type: "text", text: claudeText }],
        stop_reason: claudeStop, stop_sequence: null, stop_details: claudeStop === "refusal" ? { type: "refusal", category: null, explanation: null } : null,
        usage: { input_tokens: 10, output_tokens: 10 },
      });
    }
    if (url.pathname === "/v1/audio/transcriptions") return Response.json({ text: "Do you like dinosaurs?" });
    if (url.pathname === "/v1/audio/speech") return new Response("FAKE-MP3", { headers: { "Content-Type": "audio/mpeg" } });
    return new Response("unexpected " + url, { status: 500 });
  }));
}

afterEach(() => {
  vi.unstubAllGlobals();
  claudeText = "[excited] Dinosaurs are so cool! Which one do you like best?";
  claudeStop = "end_turn";
});

function ask(body: BodyInit, type: string, extra: Record<string, string> = {}) {
  return worker.fetch(new Request("https://rory.example/ask", {
    method: "POST", body, headers: { Authorization: "Bearer robot-password", "Content-Type": type, ...extra },
  }), env);
}

describe("POST /ask", () => {
  it("rejects a missing or wrong device token", async () => {
    const res = await worker.fetch(new Request("https://rory.example/ask", { method: "POST", body: "hi" }), env);
    expect(res.status).toBe(401);
  });

  it("transcribes audio, asks Claude, and returns a speech link", async () => {
    installFakeApis();
    const wav = new Uint8Array(32000); // 1 s of silence-ish audio
    const res = await ask(wav, "audio/wav");
    expect(res.status).toBe(200);
    const data: any = await res.json();
    expect(data.heard).toBe("Do you like dinosaurs?");
    expect(data.face).toBe("excited");
    expect(data.say).toBe("Dinosaurs are so cool! Which one do you like best?");
    expect(data.audio).toMatch(/^https:\/\/rory\.example\/speak\?t=/);

    // Claude request: right model, low effort, server-side fallback, Rory persona
    const { headers, body } = claudeBodies[0];
    expect(body.model).toBe("claude-opus-5-5");
    expect(body.output_config).toEqual({ effort: "low" });
    expect(body.fallbacks).toBe("default");
    expect(headers["anthropic-beta"]).toContain("server-side-fallback-2026-07-01");
    expect(body.system[0].text).toContain("You are Rory");
    expect(body.messages).toEqual([{ role: "user", content: "Do you like dinosaurs?" }]);

    // the speech link streams the answer in Rory's voice
    const speech = await worker.fetch(new Request(data.audio), env);
    expect(speech.status).toBe(200);
    expect(speech.headers.get("Content-Type")).toBe("audio/mpeg");
    expect(await speech.text()).toBe("FAKE-MP3");
  });

  it("remembers the conversation through the history header", async () => {
    installFakeApis();
    const first: any = await (await ask("Do you like dinosaurs?", "text/plain")).json();
    claudeText = "[curious] Because they had big strong legs!";
    await ask("why?", "text/plain", { "X-Rory-History": first.history });
    expect(claudeBodies[1].body.messages.map((m: any) => m.role)).toEqual(["user", "assistant", "user"]);
  });

  it("asks him to repeat when nothing was heard, without calling Claude", async () => {
    installFakeApis();
    const data: any = await (await ask("", "text/plain")).json();
    expect(data.say).toMatch(/say it again/);
    expect(claudeBodies).toHaveLength(0);
  });

  it("answers gently if Claude declines", async () => {
    installFakeApis();
    claudeStop = "refusal";
    claudeText = "";
    const data: any = await (await ask("something", "text/plain")).json();
    expect(data.say).toMatch(/grown-up/);
  });
});

describe("GET /speak", () => {
  it("refuses unsigned links", async () => {
    const res = await worker.fetch(new Request("https://rory.example/speak?t=aGk&e=99999999999&s=00"), env);
    expect(res.status).toBe(403);
  });
});

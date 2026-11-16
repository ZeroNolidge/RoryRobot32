// End-to-end through the worker with fake OpenAI + Claude APIs.
import { afterEach, describe, expect, it, vi } from "vitest";
import worker, { type Env } from "../src/index";

const env: Env = {
  ANTHROPIC_API_KEY: "sk-ant-test",
  OPENAI_API_KEY: "sk-openai-test",
  RORY_DEVICE_TOKEN: "robot-password",
  RORY_MODEL: "claude-opus-5-5",
  RORY_FRIENDS: "Knight Owl: his favorite stuffed toy, an owl.",
};

let claudeBodies: any[] = [];
let speechBodies: any[] = [];
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
    if (url.pathname === "/v1/audio/speech") {
      speechBodies.push(await req.json());
      return new Response("FAKE-MP3", { headers: { "Content-Type": "audio/mpeg" } });
    }
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
    expect(body.system[1].text).toContain("Knight Owl");
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

describe("POST /talk", () => {
  it("remembers her secret I Spy answer in the history, without saying it", async () => {
    installFakeApis();
    claudeText = "[excited] I spy someone with two tails! [secret: Tails]";
    const res = await worker.fetch(new Request("https://rory.example/talk", {
      method: "POST", body: "let's play I spy with Sonic", headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain" },
    }), env);
    expect(decodeURIComponent(res.headers.get("X-Rory-Say")!)).toBe("I spy someone with two tails!");
    claudeText = "[happy] Not quite! [secret: Tails]";
    await worker.fetch(new Request("https://rory.example/talk", {
      method: "POST", body: "is it Sonic?",
      headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain", "X-Rory-History": res.headers.get("X-Rory-History")! },
    }), env);
    expect(JSON.stringify(claudeBodies[1].body.messages)).toContain("[secret: Tails]");
  });

  it("sends her memories to Claude and passes new ones back to the robot", async () => {
    installFakeApis();
    claudeText = "[excited] A dog named Biscuit! Is he fluffy? [remember: He has a dog named Biscuit.]";
    const res = await worker.fetch(new Request("https://rory.example/talk", {
      method: "POST", body: "my dog is Biscuit",
      headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain", "X-Rory-Memory": encodeURIComponent("His name is Sam.\nHe loves sharks.") },
    }), env);
    expect(res.status).toBe(200);
    expect(decodeURIComponent(res.headers.get("X-Rory-Remember")!)).toBe("He has a dog named Biscuit.");
    expect(decodeURIComponent(res.headers.get("X-Rory-Say")!)).toBe("A dog named Biscuit! Is he fluffy?");
    const system = JSON.stringify(claudeBodies[0].body.system);
    expect(system).toContain("His name is Sam.");
    expect(system).toContain("He loves sharks.");
  });

  it("streams her answer as raw audio with the face and words in headers", async () => {
    installFakeApis();
    const res = await worker.fetch(new Request("https://rory.example/talk", {
      method: "POST", body: "Do you like dinosaurs?", headers: { Authorization: "Bearer robot-password", "Content-Type": "text/plain" },
    }), env);
    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toBe("audio/L16; rate=24000; channels=1");
    expect(res.headers.get("X-Rory-Face")).toBe("excited");
    expect(decodeURIComponent(res.headers.get("X-Rory-Say")!)).toBe("Dinosaurs are so cool! Which one do you like best?");
    expect(res.headers.get("X-Rory-History")).toBeTruthy();
    expect(await res.text()).toBe("FAKE-MP3"); // the fake speech API's body, streamed through
    expect(speechBodies.at(-1).response_format).toBe("pcm");
  });
});

describe("Rory, look!", () => {
  const talk = (body: BodyInit, headers: Record<string, string>) =>
    worker.fetch(new Request("https://rory.example/talk", {
      method: "POST", body, headers: { Authorization: "Bearer robot-password", ...headers },
    }), env);

  it("asks the robot for a photo when he says look (only robots that can)", async () => {
    installFakeApis();
    const res = await talk("Rory, look at my shark!", { "Content-Type": "text/plain", "X-Rory-Can-Look": "1" });
    expect(res.status).toBe(202);
    expect(res.headers.get("X-Rory-Look")).toBe("1");
    expect(decodeURIComponent(res.headers.get("X-Rory-Heard")!)).toBe("Rory, look at my shark!");
    expect(claudeBodies).toHaveLength(0); // no thinking yet
    const plain = await talk("Rory, look at my shark!", { "Content-Type": "text/plain" });
    expect(plain.status).toBe(200); // curl / no camera: answer normally
  });

  it("answers with the photo", async () => {
    installFakeApis();
    claudeText = "[scared] Eek! A giant shark! ...Oh, it's a friendly one!";
    const jpeg = new Uint8Array([0xff, 0xd8, 0xff, 0xe0, 1, 2, 3]);
    const res = await talk(jpeg, { "Content-Type": "image/jpeg", "X-Rory-Question": encodeURIComponent("Rory, look at my shark!") });
    expect(res.status).toBe(200);
    expect(res.headers.get("X-Rory-Face")).toBe("scared");
    const content = claudeBodies[0].body.messages.at(-1).content;
    expect(content[0].type).toBe("image");
    expect(content[0].source.media_type).toBe("image/jpeg");
    expect(content[1].text).toContain("Rory, look at my shark!");
  });
});

describe("POST /say", () => {
  it("needs the password and returns MP3 in Rory's voice", async () => {
    installFakeApis();
    const denied = await worker.fetch(new Request("https://rory.example/say", { method: "POST", body: "Hmm!" }), env);
    expect(denied.status).toBe(401);
    const res = await worker.fetch(new Request("https://rory.example/say", {
      method: "POST", body: "Hmm, let me think!", headers: { Authorization: "Bearer robot-password" },
    }), env);
    expect(res.headers.get("Content-Type")).toBe("audio/mpeg");
    expect(speechBodies.at(-1)).toMatchObject({ voice: "marin", input: "Hmm, let me think!", response_format: "mp3" });
  });
});

describe("GET /speak", () => {
  it("refuses unsigned links", async () => {
    const res = await worker.fetch(new Request("https://rory.example/speak?t=aGk&e=99999999999&s=00"), env);
    expect(res.status).toBe(403);
  });
});

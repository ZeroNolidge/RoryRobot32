import { describe, expect, it, vi } from "vitest";
import { buildMessages, cleanForSpeech, friendsNote, parseReply } from "../src/rory";
import { heardNothing } from "../src/stt";
import { b64urlDecode, b64urlEncode, packHistory, signSpeakUrl, unpackHistory, verifySpeakUrl } from "../src/util";
import { fallbackSpeechBody, mainSpeechBody, speak, TTS_INSTRUCTIONS } from "../src/voice";

describe("voice matches the app exactly", () => {
  it("uses gpt-4o-mini-tts / marin with the app's instructions and no speed", () => {
    const body = mainSpeechBody("Hi friend!");
    expect(body).toEqual({
      model: "gpt-4o-mini-tts",
      voice: "marin",
      input: "Hi friend!",
      instructions:
        "You are Rory, a warm, friendly, playful paper robot talking to a four-year-old who is learning to talk. Speak clearly, a little slowly, gently and cheerfully, at a normal speaking volume: never shout, never whisper, even for single sounds. Never rushed.",
      response_format: "mp3",
    });
    expect("speed" in body).toBe(false);
  });

  it("backup is tts-1 at 0.95 without instructions", () => {
    const body = fallbackSpeechBody("Hi", "coral");
    expect(body).toEqual({ model: "tts-1", voice: "coral", input: "Hi", speed: 0.95, response_format: "mp3" });
  });

  it("falls back to tts-1 when the main model refuses", async () => {
    const calls: any[] = [];
    const fake = vi.fn(async (_url: any, init: any) => {
      const body = JSON.parse(init.body);
      calls.push(body);
      return new Response(body.model === "gpt-4o-mini-tts" ? "nope" : "mp3", { status: body.model === "gpt-4o-mini-tts" ? 400 : 200 });
    });
    const res = await speak("Hello", "key", "coral", fake as any);
    expect(res.status).toBe(200);
    expect(calls.map((c) => c.model)).toEqual(["gpt-4o-mini-tts", "tts-1"]);
  });

  it("does not retry on bad key or rate limit", async () => {
    const fake = vi.fn(async () => new Response("x", { status: 401 }));
    await speak("Hello", "key", "coral", fake as any);
    expect(fake).toHaveBeenCalledTimes(1);
  });
});

describe("parseReply", () => {
  it("reads the face tag", () => {
    expect(parseReply("[excited] Dinosaurs are cool!")).toEqual({ face: "excited", say: "Dinosaurs are cool!" });
  });
  it("defaults to happy when the tag is missing or unknown", () => {
    expect(parseReply("Hello there!").face).toBe("happy");
    expect(parseReply("[angry] Grr").face).toBe("happy");
  });
  it("strips markdown, emoji and stray tags", () => {
    expect(cleanForSpeech("**Wow** 🦖 that's [silly] fun")).toBe("Wow that's fun");
  });
});

describe("signed speech links", () => {
  it("round-trips and rejects tampering or expiry", async () => {
    const url = new URL(await signSpeakUrl("https://rory.example", "Hi friend! 🚀 é", "secret", 1000));
    expect(await verifySpeakUrl(url.searchParams, "secret", 1100)).toBe("Hi friend! 🚀 é");
    expect(await verifySpeakUrl(url.searchParams, "other", 1100)).toBeNull();
    expect(await verifySpeakUrl(url.searchParams, "secret", 5000)).toBeNull();
    url.searchParams.set("t", b64urlEncode("Say something rude"));
    expect(await verifySpeakUrl(url.searchParams, "secret", 1100)).toBeNull();
  });
});

describe("history", () => {
  it("keeps the last three turns and survives junk", () => {
    const turns = [1, 2, 3, 4].map((i) => ({ q: `q${i}`, a: `a${i}` }));
    expect(unpackHistory(packHistory(turns)).map((t) => t.q)).toEqual(["q2", "q3", "q4"]);
    expect(unpackHistory("not-base64!!")).toEqual([]);
    expect(unpackHistory(b64urlEncode('{"q":1}'))).toEqual([]);
    expect(b64urlDecode(b64urlEncode("héllo"))).toBe("héllo");
  });
  it("builds alternating messages ending with the question", () => {
    const m = buildMessages("why?", [{ q: "is the sky blue?", a: "Yes!" }]);
    expect(m.map((x) => x.role)).toEqual(["user", "assistant", "user"]);
    expect(m[2].content).toBe("why?");
  });
});

describe("friendsNote", () => {
  it("is empty without friends and trims long notes", () => {
    expect(friendsNote(undefined)).toBeNull();
    expect(friendsNote("   ")).toBeNull();
    expect(friendsNote("Knight Owl")).toContain("Knight Owl");
    expect(friendsNote("x".repeat(2000))!.length).toBeLessThan(900);
  });
});

describe("heardNothing", () => {
  it("catches empty transcripts", () => {
    expect(heardNothing("")).toBe(true);
    expect(heardNothing(" . ")).toBe(true);
    expect(heardNothing("hi")).toBe(false);
  });
});

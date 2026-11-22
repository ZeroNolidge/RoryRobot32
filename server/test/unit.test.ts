import { describe, expect, it, vi } from "vitest";
import { buildMessages, cleanForSpeech, friendsNote, memoryNote, parseReply, safeMemory, SYSTEM_PROMPT, wantsLook, wantsRead } from "../src/rory";
import { heardNothing, sttPrompt } from "../src/stt";
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
  it("knows the pretend-scared face", () => {
    expect(parseReply("[scared] Eek! A shark!")).toEqual({ face: "scared", say: "Eek! A shark!" });
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
  it("keeps the last six turns and survives junk", () => {
    const turns = [1, 2, 3, 4, 5, 6, 7].map((i) => ({ q: `q${i}`, a: `a${i}` }));
    expect(unpackHistory(packHistory(turns)).map((t) => t.q)).toEqual(["q2", "q3", "q4", "q5", "q6", "q7"]);
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

describe("Rory, look!", () => {
  it("spots looking questions", () => {
    for (const q of ["Rory, look!", "Look at this", "What's this?", "what is this rory", "Can you see my shark?", "What am I holding?", "Watch me!"])
      expect(wantsLook(q)).toBe(true);
    for (const q of ["Why is the sky blue?", "Do you like dinosaurs?", "Tell me a story", "seesaw", "See you later, Rory!"]) expect(wantsLook(q)).toBe(false);
  });
  it("puts the photo before the question", () => {
    const m = buildMessages("what is this?", [], { photo: "QUJD" });
    const c = m[0].content as any[];
    expect(c[0]).toEqual({ type: "image", source: { type: "base64", media_type: "image/jpeg", data: "QUJD" } });
    expect(c[1].text).toContain("what is this?");
  });
  it("tells her when her eyes aren't working", () => {
    expect(buildMessages("look!", [], { eyesOff: true })[0].content).toContain("camera eyes aren't working");
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

describe("memory", () => {
  it("pulls a remember tag out of the spoken reply", () => {
    const r = parseReply("[excited] Green is a great color! What else is green? [remember: His favorite color is green.]");
    expect(r).toEqual({ face: "excited", say: "Green is a great color! What else is green?", remember: "His favorite color is green." });
  });
  it("never keeps personal details", () => {
    expect(safeMemory("He lives at 12 Oak Street")).toBeUndefined();
    expect(safeMemory("His school is Maple Elementary")).toBeUndefined();
    expect(safeMemory("His phone number is 5551234")).toBeUndefined();
    expect(safeMemory("He has a dog called Biscuit")).toBe("He has a dog called Biscuit");
    expect(parseReply("[happy] Cool! [remember: He lives on Elm Road]")).toEqual({ face: "happy", say: "Cool!" });
  });
  it("lists remembered facts, skipping comments and blanks", () => {
    const note = memoryNote("# grown-up note\nHis name is Sam.\n\nHe loves sharks.\n")!;
    expect(note).toContain("- His name is Sam.");
    expect(note).toContain("- He loves sharks.");
    expect(note).not.toContain("grown-up note");
  });
});

describe("games", () => {
  it("knows the Titanoboa game", () => {
    expect(SYSTEM_PROMPT).toContain("Swamp adventure");
    expect(sttPrompt()).toContain("Titanoboa");
  });
  it("knows the Sonic character cards", () => {
    expect(SYSTEM_PROMPT).toContain("red shoes with a white stripe");
    expect(SYSTEM_PROMPT).toContain("TWO fluffy tails");
  });
  it("keeps her I Spy answer secret but remembered", () => {
    const r = parseReply("[excited] I spy, with my little robot eye, someone fast and blue! [secret: Sonic]");
    expect(r).toEqual({ face: "excited", say: "I spy, with my little robot eye, someone fast and blue!", secret: "Sonic" });
  });
});

describe("speech-to-text hints", () => {
  it("includes what Rory just asked and what she remembers", () => {
    const p = sttPrompt({ lastReply: "What color is Sonic?", memory: "# note\nHis name is Nathan." });
    expect(p).toContain('Rory just said: "What color is Sonic?"');
    expect(p).toContain("His name is Nathan.");
    expect(p).not.toContain("# note");
  });
});

describe("read mode", () => {
  it("knows when he wants a story read", () => {
    expect(wantsRead("Rory, read me this book")).toBe(true);
    expect(wantsRead("next page!")).toBe(true);
    expect(wantsRead("what does it say")).toBe(true);
    expect(wantsRead("why is the sky blue")).toBe(false);
  });
  it("lets a page be longer than a chat reply", () => {
    const page = "[calm] " + "Once upon a time there was a very big snake. ".repeat(20);
    expect(parseReply(page).say.length).toBeLessThanOrEqual(400);
    expect(parseReply(page, 1500).say.length).toBeGreaterThan(800);
  });
  it("tells Claude it's reading a book page", () => {
    const m = buildMessages("read this", [], { photo: "AAAA", read: true });
    expect(JSON.stringify(m)).toContain("Read mode");
  });
});

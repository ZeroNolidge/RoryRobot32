// Rory's voice - the exact settings the Talk Trail app uses for every Rory line.
// See docs/voice.md. Keep this identical to the app so the robot sounds the same.

export const TTS_MODEL = "gpt-4o-mini-tts";
export const TTS_VOICE = "marin";
export const TTS_INSTRUCTIONS =
  "You are Rory, a warm, friendly, playful paper robot talking to a four-year-old who is learning to talk. " +
  "Speak clearly, a little slowly, gently and cheerfully, at a normal speaking volume: never shout, never whisper, " +
  "even for single sounds. Never rushed.";

// The app's backup: tts-1 at speed 0.95 (tts-1 takes no instructions and has no "marin").
export const FALLBACK_MODEL = "tts-1";
export const FALLBACK_SPEED = 0.95;

export interface SpeechRequestBody {
  model: string;
  voice: string;
  input: string;
  response_format: "mp3";
  instructions?: string;
  speed?: number;
}

export function mainSpeechBody(text: string): SpeechRequestBody {
  // No speed: the instructions' "a little slowly" handles pacing, as in the app.
  return { model: TTS_MODEL, voice: TTS_VOICE, input: text, instructions: TTS_INSTRUCTIONS, response_format: "mp3" };
}

export function fallbackSpeechBody(text: string, voice: string): SpeechRequestBody {
  return { model: FALLBACK_MODEL, voice, input: text, speed: FALLBACK_SPEED, response_format: "mp3" };
}

/**
 * Ask OpenAI to speak `text`, returning the streaming MP3 response.
 * Falls back to tts-1 (like the app's script) if the main model refuses.
 */
export async function speak(text: string, apiKey: string, fallbackVoice: string, fetchFn: typeof fetch = fetch): Promise<Response> {
  const call = (body: SpeechRequestBody) =>
    fetchFn("https://api.openai.com/v1/audio/speech", {
      method: "POST",
      headers: { Authorization: `Bearer ${apiKey}`, "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });

  let res = await call(mainSpeechBody(text));
  if (!res.ok && res.status !== 401 && res.status !== 429) {
    console.warn(`gpt-4o-mini-tts failed (${res.status}); using ${FALLBACK_MODEL}`);
    res = await call(fallbackSpeechBody(text, fallbackVoice));
  }
  return res;
}

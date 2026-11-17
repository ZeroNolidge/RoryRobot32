// Speech to text: what did he say?

// The full model is more accurate with little voices than the mini one, for a
// fraction of a cent more per question. Override with STT_MODEL on the server.
export const STT_MODEL = "gpt-4o-transcribe";

export interface Hints {
  lastReply?: string; // what Rory just said (often a question he's answering)
  memory?: string | null; // her memory file: names and favorite things
}

/** The hint sent with his recording: who's talking, words he uses, and what Rory just asked. */
export function sttPrompt(hints: Hints = {}): string {
  const parts = [
    "A five-year-old boy is talking to Rory, a friendly toy robot. He is still learning to talk: he speaks slowly, pauses, and some sounds come out differently, so write the words he most likely meant.",
    "Words he often uses: Rory, I spy, Sonic, Tails, Knuckles, Amy, Shadow, Cream, Eggman, Knight Owl, shark.",
  ];
  const facts = (hints.memory ?? "")
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l && !l.startsWith("#"))
    .slice(-15)
    .join(" ")
    .slice(0, 500);
  if (facts) parts.push(`About him: ${facts}`);
  if (hints.lastReply) parts.push(`Rory just said: "${hints.lastReply.slice(0, 300)}"`);
  return parts.join("\n");
}

export async function transcribe(
  wav: ArrayBuffer,
  apiKey: string,
  fetchFn: typeof fetch = fetch,
  hints: Hints = {},
  model: string = STT_MODEL,
): Promise<string> {
  const form = new FormData();
  form.append("file", new Blob([wav], { type: "audio/wav" }), "question.wav");
  form.append("model", model);
  form.append("language", "en");
  form.append("response_format", "json");
  form.append("prompt", sttPrompt(hints));

  const res = await fetchFn("https://api.openai.com/v1/audio/transcriptions", {
    method: "POST",
    headers: { Authorization: `Bearer ${apiKey}` },
    body: form,
  });
  if (!res.ok) throw new Error(`transcription failed: ${res.status} ${await res.text()}`);
  const data = (await res.json()) as { text?: string };
  return (data.text ?? "").trim();
}

/** True if the transcript is too short or empty to answer. */
export function heardNothing(text: string): boolean {
  const words = text.replace(/[^\p{L}\p{N}' ]/gu, " ").trim();
  return words.length < 2;
}

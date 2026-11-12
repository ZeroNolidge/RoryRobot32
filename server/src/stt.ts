// Speech to text: what did he say?

export const STT_MODEL = "gpt-4o-mini-transcribe";

export async function transcribe(wav: ArrayBuffer, apiKey: string, fetchFn: typeof fetch = fetch): Promise<string> {
  const form = new FormData();
  form.append("file", new Blob([wav], { type: "audio/wav" }), "question.wav");
  form.append("model", STT_MODEL);
  form.append("language", "en");
  form.append("response_format", "json");
  // A hint helps with little voices and Rory's name.
  form.append("prompt", "A young child is talking to Rory, a friendly toy robot.");

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

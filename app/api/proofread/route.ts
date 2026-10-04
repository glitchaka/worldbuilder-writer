import { getChatGPTUser } from "../../chatgpt-auth";

const LANGUAGETOOL_ENDPOINT = "https://api.languagetool.org/v2/check";
const MAX_TEXT_BYTES = 18_000;

type LanguageToolMatch = {
  message?: unknown;
  shortMessage?: unknown;
  offset?: unknown;
  length?: unknown;
  replacements?: Array<{ value?: unknown }>;
  context?: { text?: unknown };
};

function safeText(value: unknown, maxLength = 500) {
  return typeof value === "string" ? value.slice(0, maxLength) : "";
}

export async function POST(request: Request) {
  try {
    const user = await getChatGPTUser();
    if (!user) return Response.json({ error: "Inicia sesión para usar el corrector online." }, { status: 401 });

    const body = (await request.json()) as { text?: unknown };
    const text = typeof body.text === "string" ? body.text : "";
    if (!text.trim()) return Response.json({ error: "Selecciona o escribe un fragmento para revisar." }, { status: 400 });
    if (new TextEncoder().encode(text).byteLength > MAX_TEXT_BYTES) {
      return Response.json({ error: "El texto es demasiado largo para una sola revisión. Selecciona un fragmento más corto." }, { status: 413 });
    }

    const params = new URLSearchParams({
      text,
      language: "es",
      enabledOnly: "false",
    });
    const response = await fetch(LANGUAGETOOL_ENDPOINT, {
      method: "POST",
      headers: {
        "content-type": "application/x-www-form-urlencoded",
        accept: "application/json",
      },
      body: params.toString(),
      cache: "no-store",
    });
    if (!response.ok) {
      return Response.json({ error: "LanguageTool no está disponible en este momento. El subrayado del navegador sigue activo." }, { status: 502 });
    }

    const result = (await response.json()) as { matches?: LanguageToolMatch[] };
    const matches = Array.isArray(result.matches)
      ? result.matches.flatMap((match) => {
          if (typeof match.offset !== "number" || typeof match.length !== "number") return [];
          return [{
            offset: Math.max(0, match.offset),
            length: Math.max(0, match.length),
            message: safeText(match.message),
            shortMessage: safeText(match.shortMessage, 160),
            replacements: Array.isArray(match.replacements)
              ? match.replacements.map((item) => safeText(item.value, 120)).filter(Boolean).slice(0, 8)
              : [],
            context: safeText(match.context?.text, 320),
          }];
        })
      : [];

    return Response.json({ matches });
  } catch {
    return Response.json({ error: "El corrector online no respondió. El subrayado del navegador sigue activo." }, { status: 502 });
  }
}

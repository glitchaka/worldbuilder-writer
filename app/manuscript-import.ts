import type { WritingChapter, WritingScene } from "../lib/archive-data";

export const MANUSCRIPT_HEADING_MARKER = "\uE000WBW_HEADING\uE001";

type StructuralHeading = {
  label: string;
  title: string;
};

function importedId(prefix: string) {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 9)}`;
}

function cleanStructuralHeading(line: string) {
  return line
    .trim()
    .replace(MANUSCRIPT_HEADING_MARKER, "")
    .replace(/^#{1,6}\s+/, "")
    .replace(/\s+#{1,6}$/, "")
    .trim();
}

function isStyledHeading(line: string) {
  return line.trimStart().startsWith(MANUSCRIPT_HEADING_MARKER);
}

export function chapterHeadingFromLine(line: string): StructuralHeading | null {
  const heading = cleanStructuralHeading(line);
  if (!heading || heading.length > 180) return null;
  const chapterToken = String.raw`(?:\d+[a-z]?|[ivxlcdm]+|\p{L}+)`;
  const ordinalToken = String.raw`(?:\d+[a-z]?|[ivxlcdm]+)`;
  const titleSeparator = String.raw`(?:(?:\s*[:—–-]\s*)|(?:\.\s+)|(?:\s+))`;
  const chapterMatch = heading.match(new RegExp(String.raw`^(cap(?:[ií]tulo|\.)\s+${chapterToken})(?:${titleSeparator}(.+))?\.?$`, "iu"));
  if (chapterMatch) return { label: chapterMatch[1].trim(), title: chapterMatch[2]?.trim() || "Sin título" };
  const specialMatch = heading.match(new RegExp(String.raw`^((?:pr[oó]logo|ep[ií]logo|introducci[oó]n|prefacio|posfacio|interludio(?:\s+${ordinalToken})?))(?:${titleSeparator}(.+))?\.?$`, "iu"));
  if (specialMatch) return { label: specialMatch[1].trim(), title: specialMatch[2]?.trim() || "Sin título" };
  return null;
}

function sceneHeadingFromLine(line: string) {
  const heading = cleanStructuralHeading(line);
  if (!heading || heading.length > 160) return null;
  const match = heading.match(/^(escena(?:\s+(?:\d+[a-z]?|[ivxlcdm]+|\p{L}+))?)(?:(?:\s*[:—–-]\s*|\.\s+|\s+)(.+))?\.?$/iu);
  if (!match) return null;
  return match[2]?.trim() || match[1].trim();
}

function isExplicitSceneSeparator(line: string) {
  const value = line.trim();
  return /^(?:(?:\*\s*){3,}|(?:#\s*){3,}|(?:[-—–]\s*){3,}|⁂|※)$/u.test(value);
}

function emptyImportedScene(chapterId: string, order: number): WritingScene {
  return {
    id: importedId("escena"),
    chapterId,
    order,
    title: `Escena ${order + 1}`,
    content: "",
    pov: "",
    location: "",
    narrativeLayer: "",
    status: "Borrador",
    updatedAt: new Date().toISOString(),
  };
}

function importedScenes(chapterId: string, lines: string[]) {
  const chunks: Array<{ title: string; lines: string[] }> = [];
  let current = { title: "", lines: [] as string[] };

  const flush = () => {
    const content = current.lines.join("\n").trim();
    if (!content && !current.title && chunks.length > 0) return;
    chunks.push({ title: current.title, lines: content ? content.split("\n") : [] });
  };

  for (const line of lines) {
    if (!line.trim()) {
      if (current.lines.some((item) => item.trim())) {
        flush();
        current = { title: "", lines: [] };
      }
      continue;
    }
    const sceneTitle = sceneHeadingFromLine(line);
    if (sceneTitle || isExplicitSceneSeparator(line)) {
      if (current.lines.some((item) => item.trim()) || current.title) flush();
      current = { title: sceneTitle ?? "", lines: [] };
      continue;
    }
    current.lines.push(line);
  }
  if (current.lines.some((item) => item.trim()) || current.title || chunks.length === 0) flush();

  return chunks.map((chunk, order): WritingScene => ({
    ...emptyImportedScene(chapterId, order),
    title: chunk.title || `Escena ${order + 1}`,
    content: chunk.lines.join("\n").trim(),
  }));
}

function nextNonEmptyLine(lines: string[], start: number) {
  for (let index = start; index < lines.length; index += 1) {
    if (cleanStructuralHeading(lines[index])) return index;
  }
  return -1;
}

function joinedSplitHeading(lines: string[], index: number) {
  const prefix = cleanStructuralHeading(lines[index]);
  if (!/^(?:cap(?:[ií]tulo|\.)|interludio)\.?$/iu.test(prefix)) return null;
  const nextIndex = nextNonEmptyLine(lines, index + 1);
  if (nextIndex < 0) return null;
  const continuation = cleanStructuralHeading(lines[nextIndex]);
  const numberWord = "(?:uno|dos|tres|cuatro|cinco|seis|siete|ocho|nueve|diez|once|doce|trece|catorce|quince|dieciséis|dieciseis|diecisiete|dieciocho|diecinueve|veinte)";
  if (!new RegExp(String.raw`^(?:\d+[a-z]?|[ivxlcdm]+|${numberWord})(?:\s*(?:[:—–-]|\.\s+)\s*|\s+).+|^(?:\d+[a-z]?|[ivxlcdm]+|${numberWord})\.?$`, "iu").test(continuation)) return null;
  return { line: `${prefix.replace(/\.$/, "")} ${continuation}`, consumedIndex: nextIndex };
}

function inferredChapterLabel(sections: Array<{ label: string }>, current: { label: string } | null, startingOrder: number) {
  const labels = [...sections.map((section) => section.label), ...(current ? [current.label] : [])].reverse();
  for (const label of labels) {
    const match = label.match(/^cap(?:[ií]tulo|\.)\s+(\d+)/iu);
    if (match) return `Capítulo ${Number(match[1]) + 1}`;
  }
  return `Capítulo ${startingOrder + sections.length + 1}`;
}

export function chaptersFromImportedManuscript(content: string, fallbackTitle: string, startingOrder: number) {
  const sections: Array<{ label: string; title: string; lines: string[] }> = [];
  let current: { label: string; title: string; lines: string[] } | null = null;
  let preamble: string[] = [];
  let detectedChapterHeadings = 0;
  const lines = content.split("\n");

  const flushCurrent = () => {
    if (current) sections.push(current);
  };

  for (let index = 0; index < lines.length; index += 1) {
    let line = lines[index];
    const joined = joinedSplitHeading(lines, index);
    if (joined) {
      line = joined.line;
      index = joined.consumedIndex;
    }

    let heading = chapterHeadingFromLine(line);
    if (!heading && isStyledHeading(line) && current) {
      const styledTitle = cleanStructuralHeading(line);
      const currentHasText = current.lines.some((item) => item.trim());
      if (!currentHasText && current.title === "Sin título") {
        current.title = styledTitle;
        continue;
      }
      heading = { label: inferredChapterLabel(sections, current, startingOrder), title: styledTitle };
    }

    if (heading) {
      detectedChapterHeadings += 1;
      if (current) flushCurrent();
      else if (preamble.some((item) => item.trim())) sections.push({ label: "Preliminares", title: fallbackTitle, lines: preamble });
      preamble = [];
      current = { ...heading, lines: [] };
      continue;
    }
    if (current) current.lines.push(line);
    else preamble.push(line);
  }
  if (current) flushCurrent();
  else sections.push({ label: `Capítulo ${startingOrder + 1}`, title: fallbackTitle, lines: preamble });

  const chapters = sections.map((section, index): WritingChapter => {
    const id = importedId("capitulo");
    return {
      id,
      label: section.label,
      title: section.title,
      order: startingOrder + index,
      scenes: importedScenes(id, section.lines),
    };
  });
  return { chapters, detectedChapterHeadings };
}

export function structuredTextFromMammothHtml(html: string) {
  if (typeof DOMParser === "undefined") return "";
  const parsed = new DOMParser().parseFromString(`<body>${html}</body>`, "text/html");
  const blocks = Array.from(parsed.body.querySelectorAll("h1,h2,h3,h4,h5,h6,p,li"));
  return blocks
    .map((block) => {
      const clone = block.cloneNode(true) as HTMLElement;
      clone.querySelectorAll("br").forEach((breakElement) => breakElement.replaceWith("\n"));
      const text = clone.textContent?.replace(/\u00a0/g, " ").trim() ?? "";
      if (!text) return block.tagName === "P" ? "" : null;
      return block.tagName === "H1" ? `${MANUSCRIPT_HEADING_MARKER}${text}` : text;
    })
    .filter((block): block is string => block !== null)
    .join("\n")
    .trim();
}

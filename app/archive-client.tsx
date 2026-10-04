"use client";

/* eslint-disable @next/next/no-img-element -- user-supplied private R2 images are served directly */

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { Extension, type Editor, type JSONContent } from "@tiptap/core";
import { Plugin, PluginKey } from "@tiptap/pm/state";
import { Decoration, DecorationSet } from "@tiptap/pm/view";
import { EditorContent, useEditor } from "@tiptap/react";
import StarterKit from "@tiptap/starter-kit";
import type { Content, TDocumentDefinitions } from "pdfmake/interfaces";
import {
  BOARD_WORLD_HEIGHT,
  BOARD_WORLD_WIDTH,
  chapterLabels,
  cloneInitialArchive,
  defaultManuscriptLayout,
  defaultWritingAnalysis,
  type ArchiveAttachment,
  type ArchiveState,
  type ArchiveTheme,
  type BoardViewport,
  type CharacterRecord,
  type ImportedTextRecord,
  type MagicSystemRecord,
  type ManuscriptLayout,
  type ProjectProfile,
  type RelationshipRecord,
  type TheoryRecord,
  type TimelineEvent,
  type WorldRecord,
  type WorldMapMarker,
  type WorldMapRecord,
  type WritingAnalysisSettings,
  type WritingChapter,
  type WritingScene,
} from "../lib/archive-data";
import { boundsOverlap, fitCameraToBounds, pointBounds, screenToWorld, visibleWorldBounds, zoomCameraAt, type CanvasBounds } from "./navigable-canvas";
import { chaptersFromImportedManuscript, structuredTextFromMammothHtml } from "./manuscript-import";
import { getLocalProject, saveLocalProject } from "./local-project-store";
import GoogleDriveBackup from "./google-drive-backup";
import { createWbwPackage } from "./wbw-project";

type Section = "planificacion" | "escritura" | "mundo" | "revision";
type SaveState = "Cargando" | "Guardado" | "Guardando" | "Sin guardar";
type PlanningTab = "tablero" | "personajes" | "hilos" | "cronologia";
type WritingTab = "manuscrito" | "maquetacion";
type WorldbuildingTab = "atlas" | "mapas" | "magia";
type AttachmentTarget = { scope: "world" | "magic"; recordId: string; name: string };
type TextScope = "world" | "magic";
type NetworkMode = "all" | "focus";
type PdfSection = "tablero" | "personajes" | "mundo" | "mapas" | "magia" | "cronologia" | "hilos" | "planificacion" | "mundo_total" | "revision" | "manuscrito" | "proyecto";
type ReferenceKind = "character" | "world" | "magic" | "worldText" | "magicText";
type ManuscriptReference = { id: string; name: string; aliases: string[]; kind: ReferenceKind; kindLabel: string };
type ReferenceRuntime = { references: ManuscriptReference[]; onOpen: (reference: ManuscriptReference) => void };

type ProofreadMatch = {
  offset: number;
  length: number;
  message: string;
  shortMessage: string;
  replacements: string[];
  context: string;
};

const relationshipTypes: RelationshipRecord["type"][] = [
  "Familia", "Enemigo", "Conocido", "Solo interactuaron", "Amistad", "Romance", "Alianza", "Conflicto", "Investigación", "Tutela", "Sospecha", "Trabajo", "Culto",
];

const relationshipVisuals: Record<RelationshipRecord["type"], { color: string; dash?: string }> = {
  Familia: { color: "#d8ad4f" },
  Enemigo: { color: "#d6453d" },
  Conocido: { color: "#c79a5a", dash: "12 9" },
  "Solo interactuaron": { color: "#9b9184", dash: "2 10" },
  Amistad: { color: "#49a875" },
  Romance: { color: "#db6a91" },
  Alianza: { color: "#4c88bd" },
  Conflicto: { color: "#e07136" },
  Investigación: { color: "#3da1a7" },
  Tutela: { color: "#8b74bd" },
  Sospecha: { color: "#e0a833", dash: "8 7" },
  Trabajo: { color: "#688592" },
  Culto: { color: "#8750ad" },
};

const categories: CharacterRecord["category"][] = ["Principal", "Secundario", "Incidental", "Histórico", "Entidad"];
const statuses: CharacterRecord["status"][] = ["Activo", "Muerto", "Desconocido", "Histórico", "Entidad"];
const worldKinds: WorldRecord["kind"][] = ["País/Reino", "Región", "Ciudad/Lugar", "Pueblo/Cultura", "Moneda", "Idioma", "Religión", "Organización", "Objeto/Artefacto", "Cosmología", "Concepto", "Otro"];
const magicStatuses: MagicSystemRecord["status"][] = ["Canónico", "En desarrollo", "Secreto", "Descartado"];


const manuscriptLayoutPresets: Array<{ id: ManuscriptLayout["preset"]; name: string; note: string; settings: ManuscriptLayout }> = [
  { id: "editorial", name: "Editorial", note: "Página 6×9, sobria y cómoda para corrección.", settings: { ...defaultManuscriptLayout } },
  { id: "novela", name: "Novela amplia", note: "Más aire, cuerpo generoso y lectura descansada.", settings: { ...defaultManuscriptLayout, preset: "novela", pageWidthMm: 156, pageHeightMm: 234, marginTopMm: 23, marginRightMm: 22, marginBottomMm: 25, marginLeftMm: 22, fontFamily: "Literata", fontSizePt: 11.5, lineHeight: 1.42, paragraphIndentMm: 5.5, sceneSeparator: "⁂" } },
  { id: "bolsillo", name: "Bolsillo", note: "Formato compacto con mancha de texto eficiente.", settings: { ...defaultManuscriptLayout, preset: "bolsillo", pageWidthMm: 110, pageHeightMm: 178, marginTopMm: 15, marginRightMm: 13, marginBottomMm: 17, marginLeftMm: 13, fontFamily: "Garamond", fontSizePt: 9.5, lineHeight: 1.25, paragraphIndentMm: 4, sceneSeparator: "* * *" } },
  { id: "fantasia", name: "Fantasía clásica", note: "Aperturas ceremoniales y separador ornamental.", settings: { ...defaultManuscriptLayout, preset: "fantasia", pageWidthMm: 152.4, pageHeightMm: 228.6, marginTopMm: 25, marginRightMm: 20, marginBottomMm: 24, marginLeftMm: 20, fontFamily: "Georgia", fontSizePt: 11, lineHeight: 1.4, paragraphIndentMm: 5, chapterOpening: "Página impar", sceneSeparator: "❦" } },
  { id: "cronica", name: "Crónica ilustrada", note: "Página grande para mapas, notas e imágenes.", settings: { ...defaultManuscriptLayout, preset: "cronica", pageWidthMm: 190, pageHeightMm: 245, marginTopMm: 24, marginRightMm: 24, marginBottomMm: 26, marginLeftMm: 24, fontFamily: "Atkinson", fontSizePt: 11, lineHeight: 1.48, paragraphIndentMm: 0, sceneSeparator: "— ◇ —" } },
];

const manuscriptFontStacks: Record<ManuscriptLayout["fontFamily"], string> = {
  Garamond: '"Cormorant Garamond", Garamond, Georgia, serif',
  Georgia: "Georgia, serif",
  Literata: '"Cormorant Garamond", Georgia, serif',
  Bookerly: 'Bookerly, "Cormorant Garamond", Georgia, serif',
  Atkinson: '"Nunito", Arial, sans-serif',
};

const mapPalettes: Record<WorldMapRecord["style"], { water: string; land: string; coast: string; ink: string; grid: string }> = {
  Pergamino: { water: "#b9a77a", land: "#d8c99b", coast: "#5f4a2d", ink: "#382b1c", grid: "#8c7754" },
  Atlas: { water: "#7da8b2", land: "#b8c696", coast: "#415d50", ink: "#203b3c", grid: "#d7e0cf" },
  Nocturno: { water: "#142431", land: "#465444", coast: "#c0a76d", ink: "#efe0b7", grid: "#355063" },
};

function initials(name: string) {
  return name
    .replace(/^(El|La|Los|Las)\s+/i, "")
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((part) => part[0])
    .join("");
}

function nextId(prefix: string) {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`;
}

function spotifyEmbedUrl(value?: string) {
  const raw = value?.trim();
  if (!raw) return "";
  const uriMatch = raw.match(/^spotify:playlist:([a-z0-9]+)$/i);
  if (uriMatch) return `https://open.spotify.com/embed/playlist/${uriMatch[1]}`;
  try {
    const url = new URL(raw);
    if (url.hostname !== "open.spotify.com") return "";
    const parts = url.pathname.split("/").filter(Boolean);
    const playlistIndex = parts.lastIndexOf("playlist");
    const playlistId = playlistIndex >= 0 ? parts[playlistIndex + 1] : "";
    return playlistId && /^[a-z0-9]+$/i.test(playlistId) ? `https://open.spotify.com/embed/playlist/${playlistId}` : "";
  } catch {
    return "";
  }
}

function youtubeEmbedUrl(value?: string) {
  const raw = value?.trim();
  if (!raw) return "";
  try {
    const url = new URL(raw);
    const hostname = url.hostname.toLocaleLowerCase("en-US").replace(/^www\./, "");
    const allowedHosts = new Set(["youtube.com", "m.youtube.com", "music.youtube.com", "youtube-nocookie.com", "youtu.be"]);
    if (!allowedHosts.has(hostname)) return "";

    const cleanId = (candidate: string | null | undefined) => candidate && /^[a-z0-9_-]{6,80}$/i.test(candidate) ? candidate : "";
    const listId = cleanId(url.searchParams.get("list"));
    let videoId = "";
    if (hostname === "youtu.be") videoId = cleanId(url.pathname.split("/").filter(Boolean)[0]);
    else if (url.pathname === "/watch") videoId = cleanId(url.searchParams.get("v"));
    else {
      const parts = url.pathname.split("/").filter(Boolean);
      const marker = parts.findIndex((part) => part === "embed" || part === "shorts" || part === "live");
      if (marker >= 0 && parts[marker + 1] !== "videoseries") videoId = cleanId(parts[marker + 1]);
    }

    if (videoId) {
      const params = new URLSearchParams({ rel: "0" });
      if (listId) params.set("list", listId);
      return `https://www.youtube-nocookie.com/embed/${videoId}?${params.toString()}`;
    }
    if (listId) return `https://www.youtube-nocookie.com/embed/videoseries?list=${encodeURIComponent(listId)}&rel=0`;
    return "";
  } catch {
    return "";
  }
}

type LocalMediaTrack = {
  id: string;
  name: string;
  url: string;
  kind: "audio" | "video";
  extension: "mp3" | "mp4" | "ogg" | "vob";
  size: number;
};

function localMediaExtension(file: File): LocalMediaTrack["extension"] | null {
  const extension = file.name.split(".").pop()?.toLocaleLowerCase("en-US");
  return extension === "mp3" || extension === "mp4" || extension === "ogg" || extension === "vob" ? extension : null;
}

function attachmentUrl(projectId: string, scope: "world" | "magic", recordId: string, attachment: ArchiveAttachment) {
  if (attachment.dataUrl) return attachment.dataUrl;
  const params = new URLSearchParams({ projectId, scope, recordId, attachmentId: attachment.id });
  return `/api/attachments?${params.toString()}`;
}

function formatFileSize(bytes: number) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${Math.round(bytes / 1024)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

function safeDownloadName(value: string) {
  return value.toLocaleLowerCase("es").replace(/[^a-z0-9áéíóúüñ]+/gi, "-").replace(/^-|-$/g, "") || "archivo-narrativo";
}

function printable(value: string | number | undefined | null, fallback = "Sin definir") {
  if (typeof value === "number") return String(value);
  const clean = value?.trim();
  return clean || fallback;
}

function pdfFields(fields: Array<[string, string | number | undefined | null]>): Content {
  return {
    table: {
      widths: [112, "*"],
      body: fields.map(([label, value]) => [
        { text: label, style: "fieldLabel" },
        { text: printable(value), style: "fieldValue" },
      ]),
    },
    layout: "lightHorizontalLines",
    margin: [0, 2, 0, 12],
  };
}

function pdfEmpty(message: string): Content[] {
  return [{ text: message, style: "empty" }];
}

function pdfBoard(archive: ArchiveState): Content[] {
  const boardCharacters = archive.characters.filter((character) => character.board.visible);
  const content: Content[] = [
    { text: "Resumen", style: "subheading" },
    pdfFields([
      ["Fichas visibles", boardCharacters.length],
      ["Conexiones", archive.relationships.length],
      ["Tipos de relación", new Set(archive.relationships.map((relation) => relation.type)).size],
    ]),
    { text: "Fichas del tablero", style: "subheading" },
  ];
  if (boardCharacters.length === 0) content.push(...pdfEmpty("No hay fichas visibles en el tablero."));
  else content.push({
    table: {
      headerRows: 1,
      widths: ["*", "*", 68, 82],
      body: [
        ["Nombre", "Función", "Estado", "Origen"].map((text) => ({ text, style: "tableHeader" })),
        ...boardCharacters.map((character) => [character.name, printable(character.role), character.status, printable(character.origin)]),
      ],
    },
    layout: "lightHorizontalLines",
    margin: [0, 3, 0, 16],
  });
  content.push({ text: "Conexiones", style: "subheading" });
  if (archive.relationships.length === 0) content.push(...pdfEmpty("No hay conexiones registradas."));
  else content.push({
    ul: archive.relationships.map((relation) => {
      const source = archive.characters.find((character) => character.id === relation.source)?.name ?? "Ficha desconocida";
      const target = archive.characters.find((character) => character.id === relation.target)?.name ?? "Ficha desconocida";
      const details = relation.details.trim() ? ` ${relation.details.trim()}` : "";
      return `${source} — ${target}: ${relation.type} · ${relation.label} · ${relation.certainty} · intensidad ${relation.strength}/3.${details}`;
    }),
    style: "bodyList",
  });
  return content;
}

function pdfCharacters(archive: ArchiveState): Content[] {
  if (archive.characters.length === 0) return pdfEmpty("No hay personajes registrados.");
  const content: Content[] = [];
  archive.characters.forEach((character, index) => {
    const relationships = archive.relationships.filter((relation) => relation.source === character.id || relation.target === character.id);
    content.push({ text: character.name, style: "entryTitle", pageBreak: index > 0 ? "before" : undefined });
    content.push({ text: `${character.category} · ${character.status}`, style: "entryMeta" });
    content.push(pdfFields([
      ["Función", character.role], ["Ocupación", character.occupation], ["Origen", character.origin], ["Afiliación", character.affiliation],
      ["Alias", character.aliases.join(" · ")], ["Rasgos", character.traits.join(" · ")],
    ]));
    content.push({ text: "Resumen", style: "subheading" }, { text: printable(character.summary, "Sin resumen."), style: "body" });
    content.push({ text: "Antecedentes", style: "subheading" }, { text: printable(character.background, "Sin antecedentes."), style: "body" });
    content.push({ text: "Apariencia", style: "subheading" }, { text: printable(character.physical, "Sin descripción."), style: "body" });
    if (relationships.length > 0) content.push({ text: "Relaciones", style: "subheading" }, { ul: relationships.map((relation) => { const otherId = relation.source === character.id ? relation.target : relation.source; const other = archive.characters.find((item) => item.id === otherId)?.name ?? "Ficha desconocida"; return `${other}: ${relation.type} · ${relation.label}${relation.details ? ` — ${relation.details}` : ""}`; }), style: "bodyList" });
    if (character.evidence.length > 0) content.push({ text: "Referencias narrativas", style: "subheading" }, { ul: character.evidence.map((item) => `${item.chapter}: ${item.text}`), style: "bodyList" });
  });
  return content;
}

function pdfWorld(archive: ArchiveState): Content[] {
  if (archive.world.length === 0 && archive.worldTexts.length === 0) return pdfEmpty("No hay entradas ni textos del mundo registrados.");
  const content: Content[] = [];
  archive.world.forEach((record, index) => {
    content.push({ text: record.name, style: "entryTitle", pageBreak: index > 0 ? "before" : undefined });
    content.push({ text: `${record.kind}${record.aliases.length ? ` · ${record.aliases.join(" · ")}` : ""}`, style: "entryMeta" });
    content.push({ text: printable(record.summary, "Sin resumen."), style: "lead" });
    content.push(pdfFields([
      ["Geografía y clima", record.geography], ["Gobierno y leyes", record.government], ["Pueblos", record.peoples], ["Cultura", record.culture],
      ["Economía", record.economy], ["Moneda", record.currency], ["Lenguas", record.languages], ["Religiones", record.religions],
      ["Fuerzas armadas", record.military], ["Historia", record.history], ["Relaciones", record.relations], ["Lugares", record.locations],
      ["Conflictos", record.conflicts], ["Notas", record.notes], ["Etiquetas", record.tags.join(" · ")],
    ]));
  });
  archive.worldTexts.forEach((text) => {
    content.push({ text: text.title, style: "entryTitle", pageBreak: "before" });
    content.push({ text: `Texto importado desde ${text.sourceName}`, style: "entryMeta" });
    content.push({ text: printable(text.content, "Texto vacío."), style: "body" });
  });
  return content;
}

function pdfMaps(archive: ArchiveState): Content[] {
  if (archive.maps.length === 0) return pdfEmpty("No hay mapas registrados.");
  return archive.maps.flatMap((map, index): Content[] => [
    { text: map.name, style: "entryTitle", pageBreak: index > 0 ? "before" : undefined },
    { text: `${map.style} · semilla ${map.seed} · ${map.continents} continentes · ${map.islands} islas`, style: "entryMeta" },
    { text: printable(map.description, "Sin descripción."), style: "body" },
    ...(map.markers.length ? [
      { text: "Lugares señalados", style: "subheading" } as Content,
      { ul: map.markers.map((marker) => `${marker.label} · ${marker.kind} · coordenadas ${Math.round(marker.x)}, ${Math.round(marker.y)}`), style: "bodyList" } as Content,
    ] : pdfEmpty("El mapa todavía no tiene lugares señalados.")),
  ]);
}

function pdfMagic(archive: ArchiveState): Content[] {
  if (archive.magicSystems.length === 0 && archive.magicTexts.length === 0) return pdfEmpty("No hay sistemas ni textos de magia registrados.");
  const content: Content[] = [];
  archive.magicSystems.forEach((system, index) => {
    content.push({ text: system.name, style: "entryTitle", pageBreak: index > 0 ? "before" : undefined });
    content.push({ text: `${system.category} · ${system.status}`, style: "entryMeta" });
    content.push(pdfFields([
      ["Fuente", system.source], ["Principio", system.principle], ["Acceso", system.access], ["Costo", system.cost], ["Límites", system.limits],
      ["Manifestaciones", system.manifestations], ["Materiales", system.materials], ["Instituciones", system.institutions], ["Usuarios", system.users],
      ["Riesgos", system.risks], ["Historia", system.history], ["Notas", system.notes], ["Etiquetas", system.tags.join(" · ")],
    ]));
    if (system.evidence.length > 0) content.push({ text: "Referencias narrativas", style: "subheading" }, { ul: system.evidence.map((item) => `${item.chapter}: ${item.text}`), style: "bodyList" });
  });
  archive.magicTexts.forEach((text) => {
    content.push({ text: text.title, style: "entryTitle", pageBreak: "before" });
    content.push({ text: `Texto importado desde ${text.sourceName}`, style: "entryMeta" });
    content.push({ text: printable(text.content, "Texto vacío."), style: "body" });
  });
  return content;
}

function pdfTimeline(archive: ArchiveState): Content[] {
  if (archive.timeline.length === 0) return pdfEmpty("No hay sucesos registrados en la cronología.");
  return archive.timeline.flatMap((event, index): Content[] => {
    const people = event.characterIds.map((id) => archive.characters.find((character) => character.id === id)?.name).filter(Boolean).join(" · ");
    return [
      { text: event.title, style: "entryTitle", pageBreak: index > 0 && index % 7 === 0 ? "before" : undefined },
      { text: `${printable(event.when, "Fecha sin definir")} · ${event.chapter} · intensidad ${event.intensity}/5`, style: "entryMeta" },
      { text: printable(event.summary, "Sin descripción."), style: "body" },
      ...(people ? [{ text: `Participantes: ${people}`, style: "small" } as Content] : []),
    ];
  });
}

function pdfThreads(archive: ArchiveState): Content[] {
  if (archive.theories.length === 0) return pdfEmpty("No hay hilos de desarrollo registrados.");
  return archive.theories.flatMap((thread, index): Content[] => [
    { text: thread.title, style: "entryTitle", pageBreak: index > 0 ? "before" : undefined },
    { text: `${thread.status} · confianza de trabajo ${thread.confidence}%`, style: "entryMeta" },
    { text: "Planteamiento", style: "subheading" }, { text: printable(thread.thesis, "Sin planteamiento."), style: "body" },
    ...(thread.evidence.length ? [{ text: "Referencias", style: "subheading" } as Content, { ul: thread.evidence.map((item) => `${item.chapter}: ${item.text}`), style: "bodyList" } as Content] : []),
    { text: "Contrapunto", style: "subheading" }, { text: printable(thread.counterpoint, "Sin contrapunto."), style: "body" },
    { text: `Etiquetas: ${thread.tags.join(" · ") || "Sin etiquetas"}`, style: "small" },
  ]);
}

function pdfManuscript(archive: ArchiveState): Content[] {
  if (archive.writingChapters.length === 0) return pdfEmpty("No hay capítulos escritos o importados en el manuscrito.");
  const layout = archive.profile.manuscriptLayout ?? defaultManuscriptLayout;
  return orderedChapters(archive.writingChapters).flatMap((chapter, index): Content[] => {
    const scenes = orderedScenes(chapter);
    const content: Content[] = [
      { text: `${chapter.label || `Capítulo ${index + 1}`} · ${chapter.title || "Sin título"}`, style: "manuscriptTitle", pageBreak: index > 0 ? "before" : undefined },
      { text: `${scenes.length} ${scenes.length === 1 ? "escena" : "escenas"} · ${chapterWordCount(chapter).toLocaleString("es-CL")} palabras`, style: "entryMeta" },
    ];
    if (scenes.length === 0) content.push({ text: "Capítulo sin escenas.", style: "empty" });
    scenes.forEach((scene, sceneIndex) => {
      if (scenes.length > 1 || scene.title.trim()) content.push({ text: scene.title || `Escena ${sceneIndex + 1}`, style: "subheading" });
      const metadata = [scene.pov && `POV: ${scene.pov}`, scene.location && `Lugar: ${scene.location}`, scene.narrativeLayer && `Capa: ${scene.narrativeLayer}`, scene.status].filter(Boolean).join(" · ");
      if (metadata) content.push({ text: metadata, style: "small" });
      content.push({ text: printable(scene.content, "Escena vacía."), style: "manuscript" });
      if (sceneIndex < scenes.length - 1) content.push({ text: layout.sceneSeparator || "* * *", alignment: "center", color: "#777777", margin: [0, 12, 0, 12] });
    });
    return content;
  });
}

function pdfReview(archive: ArchiveState): Content[] {
  const text = orderedChapters(archive.writingChapters).flatMap((chapter) => orderedScenes(chapter).map((scene) => scene.content)).join("\n\n");
  const analysis = analyzeWritingStyle(text, archive.profile.writingAnalysis ?? defaultWritingAnalysis);
  const content: Content[] = [
    { text: "Diagnóstico calculado", style: "entryTitle" },
    { text: "Estas métricas se generan automáticamente a partir del manuscrito y no se rellenan manualmente.", style: "lead" },
    pdfFields([
      ["Palabras analizadas", analysis.words],
      ["Oraciones", analysis.sentences],
      ["Promedio por oración", analysis.averageSentenceLength.toFixed(1)],
      ["Legibilidad Fernández-Huerta", `${analysis.readability.toFixed(1)} · ${analysis.readabilityLabel}`],
      ["Muletillas", `${analysis.fillerPercentage.toFixed(2)} %`],
    ]),
  ];
  content.push({ text: "Repeticiones cercanas", style: "subheading" });
  content.push(analysis.repetitions.length ? { ul: analysis.repetitions.slice(0, 30).map((item) => `${item.label}: ${item.count} apariciones · ${item.perThousand.toFixed(1)} por 1.000 palabras`), style: "bodyList" } : { text: "No se detectaron repeticiones cercanas con la configuración actual.", style: "empty" });
  content.push({ text: "Muletillas", style: "subheading" });
  content.push(analysis.fillers.length ? { ul: analysis.fillers.slice(0, 30).map((item) => `${item.label}: ${item.count} apariciones · ${item.perThousand.toFixed(1)} por 1.000 palabras`), style: "bodyList" } : { text: "No se detectaron muletillas configuradas.", style: "empty" });
  return content;
}

const pdfSectionNames: Record<PdfSection, string> = {
  tablero: "Tablero de conexiones",
  personajes: "Personajes",
  mundo: "Mundo",
  mapas: "Mapas",
  magia: "Sistemas de magia",
  cronologia: "Cronología",
  hilos: "Hilos de desarrollo",
  planificacion: "Planificación",
  mundo_total: "Construcción del mundo",
  revision: "Revisión del manuscrito",
  manuscrito: "Manuscrito",
  proyecto: "Proyecto completo",
};

function pdfSectionBody(archive: ArchiveState, section: Exclude<PdfSection, "planificacion" | "mundo_total" | "proyecto">): Content[] {
  if (section === "tablero") return pdfBoard(archive);
  if (section === "personajes") return pdfCharacters(archive);
  if (section === "mundo") return pdfWorld(archive);
  if (section === "mapas") return pdfMaps(archive);
  if (section === "magia") return pdfMagic(archive);
  if (section === "cronologia") return pdfTimeline(archive);
  if (section === "hilos") return pdfThreads(archive);
  if (section === "revision") return pdfReview(archive);
  return pdfManuscript(archive);
}

function pdfSectionBlock(archive: ArchiveState, section: Exclude<PdfSection, "planificacion" | "mundo_total" | "proyecto">, first = false): Content[] {
  return [
    { text: pdfSectionNames[section], style: "sectionTitle", pageBreak: first ? undefined : "before" },
    ...pdfSectionBody(archive, section),
  ];
}

function buildPdfDefinition(archive: ArchiveState, section: PdfSection): TDocumentDefinitions {
  const layout = archive.profile.manuscriptLayout ?? defaultManuscriptLayout;
  const useBookLayout = section === "manuscrito";
  const pointsPerMm = 2.83465;
  const content: Content[] = [
    { text: archive.profile.archiveTitle, style: "documentTitle" },
    { text: archive.profile.storyTitle || "Proyecto narrativo", style: "documentSubtitle" },
    { text: pdfSectionNames[section], style: "documentSection" },
    { text: [archive.profile.author, archive.profile.genre, archive.profile.status].filter(Boolean).join(" · ") || "Archivo de escritura y worldbuilding", style: "documentMeta" },
  ];
  if (section === "proyecto") {
    content.push({ text: printable(archive.profile.synopsis, "Proyecto completo exportado desde el editor."), style: "coverSynopsis" });
    (["tablero", "personajes", "hilos", "cronologia", "manuscrito", "mundo", "mapas", "magia", "revision"] as const).forEach((item) => content.push(...pdfSectionBlock(archive, item)));
  } else if (section === "planificacion") {
    (["tablero", "personajes", "hilos", "cronologia"] as const).forEach((item) => content.push(...pdfSectionBlock(archive, item)));
  } else if (section === "mundo_total") {
    (["mundo", "mapas", "magia"] as const).forEach((item) => content.push(...pdfSectionBlock(archive, item)));
  } else {
    content.push(...pdfSectionBody(archive, section));
  }
  return {
    content,
    pageSize: useBookLayout ? { width: layout.pageWidthMm * pointsPerMm, height: layout.pageHeightMm * pointsPerMm } : "A4",
    pageMargins: useBookLayout ? [layout.marginLeftMm * pointsPerMm, layout.marginTopMm * pointsPerMm, layout.marginRightMm * pointsPerMm, layout.marginBottomMm * pointsPerMm] : [52, 62, 52, 54],
    info: { title: `${archive.profile.archiveTitle} — ${pdfSectionNames[section]}`, author: archive.profile.author || "Worldbuilder Writer", subject: pdfSectionNames[section] },
    header: (currentPage) => currentPage > 1 ? ({ text: `${archive.profile.archiveTitle} · ${pdfSectionNames[section]}`, alignment: "right", margin: [0, 26, 52, 0], color: "#777777", fontSize: 8 }) : null,
    footer: (currentPage, pageCount) => ({ text: `${currentPage} / ${pageCount}`, alignment: "center", margin: [0, 16, 0, 0], color: "#777777", fontSize: 8 }),
    defaultStyle: { font: "Roboto", fontSize: 10.5, lineHeight: 1.35, color: "#242424" },
    styles: {
      documentTitle: { fontSize: 25, bold: true, color: "#2d2018", margin: [0, 0, 0, 4] },
      documentSubtitle: { fontSize: 14, color: "#5e4b3c", margin: [0, 0, 0, 18] },
      documentSection: { fontSize: 32, bold: true, color: "#76372e", margin: [0, 30, 0, 8] },
      documentMeta: { fontSize: 9, color: "#777777", margin: [0, 0, 0, 24] },
      coverSynopsis: { fontSize: 12, lineHeight: 1.5, color: "#4c433c", margin: [0, 18, 0, 20] },
      sectionTitle: { fontSize: 25, bold: true, color: "#76372e", margin: [0, 0, 0, 18] },
      entryTitle: { fontSize: 18, bold: true, color: "#30231b", margin: [0, 8, 0, 2] },
      manuscriptTitle: { fontSize: 20, bold: true, color: "#30231b", margin: [0, 0, 0, 3] },
      entryMeta: { fontSize: 8.5, color: "#777777", margin: [0, 0, 0, 10] },
      subheading: { fontSize: 12, bold: true, color: "#734239", margin: [0, 12, 0, 4] },
      lead: { fontSize: 11.5, italics: true, color: "#554942", margin: [0, 2, 0, 12] },
      body: { fontSize: 10.5, lineHeight: 1.4, margin: [0, 0, 0, 8] },
      manuscript: { fontSize: useBookLayout ? layout.fontSizePt : 11.5, lineHeight: useBookLayout ? layout.lineHeight : 1.52, margin: [0, 8, 0, 0] },
      bodyList: { fontSize: 10, lineHeight: 1.35, margin: [8, 3, 0, 10] },
      fieldLabel: { fontSize: 8.5, bold: true, color: "#704a3b", margin: [0, 3, 6, 3] },
      fieldValue: { fontSize: 9.5, color: "#303030", margin: [0, 3, 0, 3] },
      tableHeader: { fontSize: 8.5, bold: true, color: "#ffffff", fillColor: "#704a3b", margin: [3, 4, 3, 4] },
      empty: { fontSize: 10, italics: true, color: "#777777", margin: [0, 4, 0, 12] },
      small: { fontSize: 8.5, color: "#777777", margin: [0, 2, 0, 8] },
    },
  };
}

async function exportTextPdf(archive: ArchiveState, section: PdfSection) {
  const [{ default: pdfMake }, { default: virtualFonts }] = await Promise.all([
    import("pdfmake/build/pdfmake"),
    import("pdfmake/build/vfs_fonts"),
  ]);
  const pdfRuntime = pdfMake as unknown as {
    addVirtualFileSystem: (files: Record<string, string>) => void;
    createPdf: (definition: TDocumentDefinitions) => { download: (fileName: string) => void };
  };
  pdfRuntime.addVirtualFileSystem(virtualFonts as unknown as Record<string, string>);
  const fileName = `${safeDownloadName(archive.profile.archiveTitle)}-${safeDownloadName(pdfSectionNames[section])}.pdf`;
  pdfRuntime.createPdf(buildPdfDefinition(archive, section)).download(fileName);
}

function downloadBlob(blob: Blob, fileName: string) {
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = fileName;
  anchor.click();
  window.setTimeout(() => URL.revokeObjectURL(url), 1_000);
}

function dataUrlToBytes(value: string) {
  const match = value.match(/^data:([^;,]+);base64,(.+)$/);
  if (!match) return null;
  const binary = window.atob(match[2]);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) bytes[index] = binary.charCodeAt(index);
  return { mimeType: match[1], bytes };
}

async function imageFileToDataUrl(file: File, maxWidth: number, maxHeight: number, label = "imagen") {
  if (!file.type.startsWith("image/")) throw new Error(`La ${label} debe ser una imagen JPG, PNG, WEBP o GIF.`);
  if (file.size > 10 * 1024 * 1024) throw new Error(`La ${label} supera el límite de 10 MB.`);
  const objectUrl = URL.createObjectURL(file);
  try {
    const image = await new Promise<HTMLImageElement>((resolve, reject) => {
      const element = new Image();
      element.onload = () => resolve(element);
      element.onerror = () => reject(new Error(`No se pudo leer la ${label}.`));
      element.src = objectUrl;
    });
    const scale = Math.min(1, maxWidth / image.naturalWidth, maxHeight / image.naturalHeight);
    const width = Math.max(1, Math.round(image.naturalWidth * scale));
    const height = Math.max(1, Math.round(image.naturalHeight * scale));
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext("2d");
    if (!context) throw new Error(`No se pudo preparar la ${label}.`);
    context.fillStyle = "#ffffff";
    context.fillRect(0, 0, width, height);
    context.drawImage(image, 0, 0, width, height);
    return canvas.toDataURL("image/jpeg", 0.84);
  } finally {
    URL.revokeObjectURL(objectUrl);
  }
}

async function coverFileToDataUrl(file: File) {
  return imageFileToDataUrl(file, 900, 1_400, "portada");
}

function seededRandom(seed: number) {
  let value = Math.abs(Math.trunc(seed)) || 1;
  return () => {
    value = (value * 1664525 + 1013904223) >>> 0;
    return value / 4294967296;
  };
}

function mapLandmasses(seed: number, continents: number, islands: number, roughness: number) {
  const random = seededRandom(seed);
  const masses: string[] = [];
  const total = Math.max(1, Math.min(9, continents + islands));
  for (let massIndex = 0; massIndex < total; massIndex += 1) {
    const island = massIndex >= continents;
    const centerX = 12 + random() * 76;
    const centerY = 15 + random() * 70;
    const radiusX = (island ? 4 : 10) + random() * (island ? 5 : 14);
    const radiusY = (island ? 4 : 9) + random() * (island ? 4 : 12);
    const points = 18 + Math.round(random() * 12);
    const polygon = Array.from({ length: points }, (_, pointIndex) => {
      const angle = pointIndex / points * Math.PI * 2;
      const wobble = 0.72 + random() * (0.34 + roughness * 0.005);
      const x = Math.max(2, Math.min(98, centerX + Math.cos(angle) * radiusX * wobble));
      const y = Math.max(3, Math.min(97, centerY + Math.sin(angle) * radiusY * wobble));
      return `${x.toFixed(2)},${y.toFixed(2)}`;
    }).join(" ");
    masses.push(polygon);
  }
  return masses;
}

async function exportManuscriptDocx(archive: ArchiveState) {
  const {
    AlignmentType,
    Document,
    Footer,
    HeadingLevel,
    Header,
    ImageRun,
    Packer,
    PageBreak,
    PageNumber,
    Paragraph,
    TextRun,
  } = await import("docx");
  const layout = archive.profile.manuscriptLayout ?? defaultManuscriptLayout;
  const twipsPerMm = 56.6929;
  const lineTwips = Math.round(240 * layout.lineHeight);
  const fontSize = Math.round(layout.fontSizePt * 2);
  const docxFont = layout.fontFamily === "Atkinson" ? "Arial" : layout.fontFamily === "Literata" || layout.fontFamily === "Bookerly" ? "Georgia" : layout.fontFamily;
  const children: InstanceType<typeof Paragraph>[] = [];
  const cover = archive.profile.coverImageDataUrl ? dataUrlToBytes(archive.profile.coverImageDataUrl) : null;
  if (cover && ["image/jpeg", "image/png", "image/gif", "image/bmp"].includes(cover.mimeType)) {
    const type = cover.mimeType === "image/jpeg" ? "jpg" : cover.mimeType.split("/")[1] as "png" | "gif" | "bmp";
    children.push(new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { after: 420 },
      children: [new ImageRun({ data: cover.bytes, type, transformation: { width: 360, height: 540 } })],
    }));
  }
  children.push(
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 180 }, children: [new TextRun({ text: archive.profile.storyTitle || archive.profile.archiveTitle, bold: true, size: 40 })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 120 }, children: [new TextRun({ text: archive.profile.subtitle || "Manuscrito", italics: true, size: 24 })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: archive.profile.author || "Autoría sin definir", size: 22 })] }),
    new Paragraph({ children: [new PageBreak()] }),
  );

  orderedChapters(archive.writingChapters).forEach((chapter, chapterIndex) => {
    children.push(new Paragraph({
      heading: HeadingLevel.HEADING_1,
      pageBreakBefore: chapterIndex > 0 && layout.chapterOpening !== "Continuo",
      spacing: { after: 260 },
      children: [new TextRun({ text: `${chapter.label || `Capítulo ${chapterIndex + 1}`} · ${chapter.title || "Sin título"}` })],
    }));
    const scenes = orderedScenes(chapter);
    if (scenes.length === 0) children.push(new Paragraph({ children: [new TextRun({ text: "Capítulo sin escenas.", italics: true })] }));
    scenes.forEach((scene, sceneIndex) => {
      if (sceneIndex > 0) children.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 220, after: 220 }, children: [new TextRun({ text: layout.sceneSeparator || "* * *" })] }));
      scene.content.split(/\n/).forEach((line) => {
        children.push(new Paragraph({
          alignment: AlignmentType.JUSTIFIED,
          spacing: { line: lineTwips, after: line.trim() ? 80 : 0 },
          indent: line.trim() ? { firstLine: Math.round(layout.paragraphIndentMm * twipsPerMm) } : undefined,
          children: line ? [new TextRun({ text: line, size: fontSize, font: docxFont })] : [],
        }));
      });
    });
  });

  const document = new Document({
    creator: archive.profile.author || "Worldbuilder Writer",
    title: archive.profile.storyTitle || archive.profile.archiveTitle,
    description: archive.profile.synopsis,
    styles: {
      default: { document: { run: { font: docxFont, size: fontSize }, paragraph: { spacing: { line: lineTwips } } } },
      paragraphStyles: [{ id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true, run: { font: docxFont, size: Math.max(28, fontSize + 8), bold: true }, paragraph: { spacing: { before: 240, after: 240 }, outlineLevel: 0 } }],
    },
    sections: [{
      properties: { page: { size: { width: Math.round(layout.pageWidthMm * twipsPerMm), height: Math.round(layout.pageHeightMm * twipsPerMm) }, margin: { top: Math.round(layout.marginTopMm * twipsPerMm), right: Math.round(layout.marginRightMm * twipsPerMm), bottom: Math.round(layout.marginBottomMm * twipsPerMm), left: Math.round(layout.marginLeftMm * twipsPerMm) } } },
      headers: layout.headerText ? { default: new Header({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: layout.headerText.replace("{título}", archive.profile.storyTitle || archive.profile.archiveTitle), size: Math.max(16, fontSize - 4), color: "777777" })] })] }) } : undefined,
      footers: { default: new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: layout.footerText.includes("{página}") ? [new TextRun({ text: layout.footerText.split("{página}")[0], size: Math.max(16, fontSize - 4) }), new TextRun({ children: [PageNumber.CURRENT], size: Math.max(16, fontSize - 4) }), new TextRun({ text: layout.footerText.split("{página}").slice(1).join("{página}"), size: Math.max(16, fontSize - 4) })] : [new TextRun({ text: layout.footerText, size: Math.max(16, fontSize - 4) })] })] }) },
      children,
    }],
  });
  downloadBlob(await Packer.toBlob(document), `${safeDownloadName(archive.profile.storyTitle || archive.profile.archiveTitle)}.docx`);
}

function escapeXml(value: string) {
  return value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&apos;");
}

function sceneToXhtml(scene: WritingScene) {
  const paragraphs = scene.content.split(/\n/).map((line) => line.trim()
    ? `<p>${escapeXml(line)}</p>`
    : `<p class="blank">&#160;</p>`).join("\n");
  return paragraphs || "<p class=\"empty\">Escena vacía.</p>";
}

async function exportManuscriptEpub(archive: ArchiveState) {
  const { default: JSZip } = await import("jszip");
  const zip = new JSZip();
  const chapters = orderedChapters(archive.writingChapters);
  const identifier = `urn:uuid:${crypto.randomUUID()}`;
  const title = archive.profile.storyTitle || archive.profile.archiveTitle;
  const author = archive.profile.author || "Autoría sin definir";
  const layout = archive.profile.manuscriptLayout ?? defaultManuscriptLayout;
  const epubFont = layout.fontFamily === "Atkinson" ? "Arial,sans-serif" : `${layout.fontFamily},Georgia,serif`;
  zip.file("mimetype", "application/epub+zip", { compression: "STORE" });
  zip.file("META-INF/container.xml", `<?xml version="1.0" encoding="UTF-8"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>`);
  zip.file("OEBPS/styles.css", `body{font-family:${epubFont};font-size:${layout.fontSizePt}pt;line-height:${layout.lineHeight};margin:6%;}h1{text-align:center;margin:2em 0 1.5em;font-family:${epubFont};}p{text-align:justify;text-indent:${layout.paragraphIndentMm}mm;margin:.25em 0;}.blank{height:.6em}.separator{text-align:center;text-indent:0;margin:1.6em 0}.scene-meta{text-align:center;text-indent:0;color:#666;font-size:.8em}.cover{text-align:center;margin:0}.cover img{max-width:100%;max-height:95vh}`);

  const manifest: string[] = [
    `<item id="nav" href="toc.xhtml" media-type="application/xhtml+xml" properties="nav"/>`,
    `<item id="css" href="styles.css" media-type="text/css"/>`,
  ];
  const spine: string[] = [];
  let coverManifest = "";
  const cover = archive.profile.coverImageDataUrl ? dataUrlToBytes(archive.profile.coverImageDataUrl) : null;
  if (cover) {
    const extension = cover.mimeType === "image/jpeg" ? "jpg" : cover.mimeType.split("/")[1].replace("svg+xml", "svg");
    const coverName = `cover.${extension}`;
    zip.file(`OEBPS/${coverName}`, cover.bytes);
    zip.file("OEBPS/cover.xhtml", `<?xml version="1.0" encoding="UTF-8"?><html xmlns="http://www.w3.org/1999/xhtml" lang="es"><head><title>Portada</title><link rel="stylesheet" type="text/css" href="styles.css"/></head><body><div class="cover"><img src="${coverName}" alt="Portada de ${escapeXml(title)}"/></div></body></html>`);
    manifest.push(`<item id="cover-page" href="cover.xhtml" media-type="application/xhtml+xml"/>`, `<item id="cover-image" href="${coverName}" media-type="${escapeXml(cover.mimeType)}" properties="cover-image"/>`);
    spine.push(`<itemref idref="cover-page" linear="yes"/>`);
    coverManifest = `<meta name="cover" content="cover-image"/>`;
  }

  chapters.forEach((chapter, index) => {
    const fileName = `chapter-${index + 1}.xhtml`;
    const heading = `${chapter.label || `Capítulo ${index + 1}`} · ${chapter.title || "Sin título"}`;
    const scenes = orderedScenes(chapter);
    const body = scenes.length === 0
      ? `<p class="empty">Capítulo sin escenas.</p>`
      : scenes.map((scene, sceneIndex) => `${sceneIndex > 0 ? `<div class="separator">${escapeXml(layout.sceneSeparator || "* * *")}</div>` : ""}${sceneToXhtml(scene)}`).join("\n");
    zip.file(`OEBPS/${fileName}`, `<?xml version="1.0" encoding="UTF-8"?><html xmlns="http://www.w3.org/1999/xhtml" lang="es"><head><title>${escapeXml(heading)}</title><link rel="stylesheet" type="text/css" href="styles.css"/></head><body><h1>${escapeXml(heading)}</h1>${body}</body></html>`);
    manifest.push(`<item id="chapter-${index + 1}" href="${fileName}" media-type="application/xhtml+xml"/>`);
    spine.push(`<itemref idref="chapter-${index + 1}"/>`);
  });

  const navigation = chapters.map((chapter, index) => `<li><a href="chapter-${index + 1}.xhtml">${escapeXml(`${chapter.label || `Capítulo ${index + 1}`} · ${chapter.title || "Sin título"}`)}</a></li>`).join("");
  zip.file("OEBPS/toc.xhtml", `<?xml version="1.0" encoding="UTF-8"?><html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="es"><head><title>Índice</title></head><body><nav epub:type="toc" id="toc"><h1>Índice</h1><ol>${navigation}</ol></nav></body></html>`);
  zip.file("OEBPS/content.opf", `<?xml version="1.0" encoding="UTF-8"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="book-id" xml:lang="es"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="book-id">${identifier}</dc:identifier><dc:title>${escapeXml(title)}</dc:title><dc:creator>${escapeXml(author)}</dc:creator><dc:language>es</dc:language><dc:description>${escapeXml(archive.profile.synopsis || "")}</dc:description><meta property="dcterms:modified">${new Date().toISOString().replace(/\.\d{3}Z$/, "Z")}</meta>${coverManifest}</metadata><manifest>${manifest.join("")}</manifest><spine>${spine.join("")}</spine></package>`);
  const blob = await zip.generateAsync({ type: "blob", mimeType: "application/epub+zip", compression: "DEFLATE", compressionOptions: { level: 9 } });
  downloadBlob(blob, `${safeDownloadName(title)}.epub`);
}

function boardPositionForIndex(index: number) {
  const columns = [620, 1390, 2160, 2930, 3700, 4470];
  const rows = [440, 1040, 1640, 2240, 2840];
  const slotsPerPage = columns.length * rows.length;
  const slot = index % slotsPerPage;
  const cycle = Math.floor(index / slotsPerPage);
  const nudge = (cycle % 5) * 38 - 76;
  return {
    x: Math.max(150, Math.min(BOARD_WORLD_WIDTH - 150, columns[slot % columns.length] + nudge)),
    y: Math.max(120, Math.min(BOARD_WORLD_HEIGHT - 120, rows[Math.floor(slot / columns.length)] + nudge)),
  };
}

function emptyCharacter(chapterCount = chapterLabels.length, existingCharacters: CharacterRecord[] = []): CharacterRecord {
  const boardPosition = boardPositionForIndex(existingCharacters.filter((character) => character.board.visible).length);
  return {
    id: nextId("personaje"),
    name: "Nuevo personaje",
    aliases: [],
    category: "Secundario",
    status: "Activo",
    role: "",
    occupation: "",
    origin: "",
    affiliation: "",
    summary: "",
    background: "",
    physical: "",
    traits: [],
    evidence: [],
    presence: Array(chapterCount).fill(0),
    color: "paper",
    board: { ...boardPosition, visible: true },
  };
}

function emptyRelationship(characters: CharacterRecord[], sourceId?: string, targetId?: string): RelationshipRecord {
  const fallbackSource = characters[0]?.id ?? "";
  const fallbackTarget = characters[1]?.id ?? fallbackSource;
  return {
    id: nextId("relacion"),
    source: sourceId ?? fallbackSource,
    target: targetId ?? fallbackTarget,
    type: "Solo interactuaron",
    label: "Nueva conexión",
    certainty: "Hecho",
    strength: 2,
    details: "",
  };
}

function emptyTheory(): TheoryRecord {
  return {
    id: nextId("teoria"),
    title: "Nuevo hilo",
    status: "Abierta",
    confidence: 50,
    thesis: "",
    evidence: [],
    counterpoint: "",
    characterIds: [],
    tags: [],
  };
}

function emptyTimelineEvent(): TimelineEvent {
  return {
    id: nextId("suceso"),
    chapter: "Sin capítulo",
    when: "",
    title: "Nuevo suceso",
    summary: "",
    characterIds: [],
    intensity: 3,
  };
}

function emptyWorldRecord(): WorldRecord {
  return {
    id: nextId("mundo"),
    kind: "País/Reino",
    name: "Nueva entrada del mundo",
    aliases: [],
    summary: "",
    geography: "",
    government: "",
    peoples: "",
    culture: "",
    economy: "",
    currency: "",
    languages: "",
    religions: "",
    military: "",
    history: "",
    relations: "",
    locations: "",
    conflicts: "",
    notes: "",
    tags: [],
    attachments: [],
  };
}

function emptyMagicSystem(): MagicSystemRecord {
  return {
    id: nextId("magia"),
    name: "Nuevo sistema de magia",
    category: "",
    status: "En desarrollo",
    source: "",
    principle: "",
    access: "",
    cost: "",
    limits: "",
    manifestations: "",
    materials: "",
    institutions: "",
    users: "",
    risks: "",
    history: "",
    notes: "",
    evidence: [],
    tags: [],
    attachments: [],
  };
}

function emptyWritingScene(chapterId: string, index: number): WritingScene {
  return {
    id: nextId("escena"),
    chapterId,
    order: index,
    title: `Escena ${index + 1}`,
    content: "",
    pov: "",
    location: "",
    narrativeLayer: "",
    status: "Borrador",
    updatedAt: new Date().toISOString(),
  };
}

function emptyWritingChapter(index: number): WritingChapter {
  const id = nextId("capitulo");
  return {
    id,
    label: `Capítulo ${index + 1}`,
    title: "Sin título",
    order: index,
    scenes: [emptyWritingScene(id, 0)],
  };
}

function emptyImportedText(): ImportedTextRecord {
  return {
    id: nextId("texto"),
    title: "Texto sin título",
    content: "",
    sourceName: "Texto creado en la aplicación",
    importedAt: new Date().toISOString(),
  };
}

function emptyWorldMap(index: number): WorldMapRecord {
  return {
    id: nextId("mapa"),
    name: `Mapa ${index + 1}`,
    description: "",
    seed: Math.floor(Math.random() * 9_000_000) + 1_000_000,
    style: "Pergamino",
    continents: 3,
    islands: 3,
    roughness: 55,
    backgroundImageDataUrl: "",
    markers: [],
    updatedAt: new Date().toISOString(),
  };
}

async function extractTextFromFile(file: File, maximumCharacters = 750_000) {
  if (file.size > 8 * 1024 * 1024) throw new Error(`${file.name} supera el límite de 8 MB.`);
  const extension = file.name.split(".").pop()?.toLocaleLowerCase("es") ?? "";
  let content = "";
  if (extension === "docx") {
    const mammothModule = await import("mammoth");
    const mammoth = mammothModule.default;
    const arrayBuffer = await file.arrayBuffer();
    const [rawResult, htmlResult] = await Promise.all([
      mammoth.extractRawText({ arrayBuffer }),
      mammoth.convertToHtml({ arrayBuffer }, {
        styleMap: [
          "p[style-name='Title'] => h1:fresh",
          "p[style-name='Título'] => h1:fresh",
          "p[style-name='Heading 1'] => h1:fresh",
          "p[style-name='Título 1'] => h1:fresh",
          "p[style-name='Heading 2'] => h2:fresh",
          "p[style-name='Título 2'] => h2:fresh",
        ],
      }),
    ]);
    const structured = structuredTextFromMammothHtml(htmlResult.value);
    content = structured || rawResult.value;
  } else if (["txt", "md", "csv"].includes(extension)) {
    content = await file.text();
  } else {
    throw new Error("Usa un documento DOCX, TXT, Markdown o CSV.");
  }
  const normalized = content.replace(/\r\n?/g, "\n").trim();
  if (!normalized) throw new Error(`${file.name} no contiene texto que se pueda importar.`);
  if (normalized.length > maximumCharacters) throw new Error(`${file.name} contiene demasiado texto para importarlo de una vez.`);
  return {
    id: nextId("texto"),
    title: file.name.replace(/\.[^.]+$/, "") || "Texto importado",
    content: normalized,
    sourceName: file.name,
    importedAt: new Date().toISOString(),
  } satisfies ImportedTextRecord;
}

function wordCount(text: string) {
  const clean = text.trim();
  return clean ? clean.split(/\s+/).length : 0;
}

function orderedChapters(chapters: WritingChapter[]) {
  return [...chapters].sort((a, b) => a.order - b.order);
}

function orderedScenes(chapter: WritingChapter) {
  return [...chapter.scenes].sort((a, b) => a.order - b.order);
}

function chapterWordCount(chapter: WritingChapter) {
  return chapter.scenes.reduce((total, scene) => total + wordCount(scene.content), 0);
}

function manuscriptWordCount(chapters: WritingChapter[]) {
  return chapters.reduce((total, chapter) => total + chapterWordCount(chapter), 0);
}

function plainTextContent(text: string): JSONContent {
  return {
    type: "doc",
    content: text.split("\n").map((line) => ({
      type: "paragraph",
      content: line ? [{ type: "text", text: line }] : undefined,
    })),
  };
}

function isReferenceBoundary(character?: string) {
  return !character || !/[\p{L}\p{N}_]/u.test(character);
}

const semanticReferencesPluginKey = new PluginKey<ReferenceRuntime>("semanticReferences");

function createReferenceExtension(initialRuntime: ReferenceRuntime) {
  return Extension.create({
    name: "semanticReferences",
    addProseMirrorPlugins() {
      return [new Plugin<ReferenceRuntime>({
        key: semanticReferencesPluginKey,
        state: {
          init: () => initialRuntime,
          apply: (transaction, current) => (transaction.getMeta("semanticReferences") as ReferenceRuntime | undefined) ?? current,
        },
        props: {
          decorations: (state) => {
            const runtime = semanticReferencesPluginKey.getState(state) ?? initialRuntime;
            const seen = new Set<string>();
            const terms = runtime.references
              .flatMap((reference) => [reference.name, ...reference.aliases].map((term) => ({ term: term.trim(), reference })))
              .filter(({ term }) => term.length >= 2)
              .sort((a, b) => b.term.length - a.term.length)
              .filter(({ term }) => {
                const key = term.toLocaleLowerCase("es");
                if (seen.has(key)) return false;
                seen.add(key);
                return true;
              });
            const decorations: Decoration[] = [];
            state.doc.descendants((node, position) => {
              if (!node.isText || !node.text) return;
              const text = node.text;
              const lowerText = text.toLocaleLowerCase("es");
              const occupied: Array<[number, number]> = [];
              for (const { term, reference } of terms) {
                const lowerTerm = term.toLocaleLowerCase("es");
                let offset = lowerText.indexOf(lowerTerm);
                while (offset >= 0) {
                  const end = offset + term.length;
                  const overlaps = occupied.some(([from, to]) => offset < to && end > from);
                  if (!overlaps && isReferenceBoundary(text[offset - 1]) && isReferenceBoundary(text[end])) {
                    occupied.push([offset, end]);
                    decorations.push(Decoration.inline(position + offset, position + end, {
                      class: `semantic-reference reference-${reference.kind}`,
                      "data-reference-id": reference.id,
                      "data-reference-kind": reference.kind,
                      title: `${reference.kindLabel}: ${reference.name}`,
                    }));
                  }
                  offset = lowerText.indexOf(lowerTerm, offset + Math.max(1, lowerTerm.length));
                }
              }
            });
            return DecorationSet.create(state.doc, decorations);
          },
          handleClick: (view, _position, event) => {
            const target = event.target instanceof HTMLElement ? event.target.closest<HTMLElement>("[data-reference-id]") : null;
            if (!target || !view.dom.contains(target)) return false;
            const id = target.dataset.referenceId;
            const kind = target.dataset.referenceKind as ReferenceKind | undefined;
            const runtime = semanticReferencesPluginKey.getState(view.state) ?? initialRuntime;
            const reference = runtime.references.find((item) => item.id === id && item.kind === kind);
            if (!reference) return false;
            runtime.onOpen(reference);
            return true;
          },
        },
      })];
    },
  });
}

type StyleFlag = {
  key: string;
  label: string;
  count: number;
  perThousand: number;
  kind: "repetition" | "filler";
};

type StyleAnalysisResult = {
  words: number;
  sentences: number;
  averageSentenceLength: number;
  readability: number;
  readabilityLabel: string;
  repetitions: StyleFlag[];
  fillers: StyleFlag[];
  fillerPercentage: number;
  repeatedKeys: string[];
};

type StyleAnalysisRuntime = {
  enabled: boolean;
  repeatedKeys: string[];
  fillerPhrases: string[];
};

const styleAnalysisPluginKey = new PluginKey<StyleAnalysisRuntime>("writingStyleAnalysis");
const spanishStopWords = new Set([
  "a", "al", "algo", "ante", "bajo", "cada", "como", "con", "contra", "cual", "cuando", "de", "del", "desde", "donde", "durante", "e", "el", "ella", "ellas", "ellos", "en", "entre", "era", "es", "esa", "ese", "eso", "esta", "este", "esto", "fue", "ha", "hasta", "hay", "la", "las", "le", "les", "lo", "los", "más", "me", "mi", "mientras", "muy", "ni", "no", "nos", "o", "para", "pero", "por", "porque", "que", "qué", "se", "si", "sin", "sobre", "su", "sus", "te", "tu", "un", "una", "uno", "unos", "y", "ya",
]);

function normalizeWord(value: string) {
  return value.toLocaleLowerCase("es").normalize("NFC");
}

function lightSpanishStem(value: string) {
  let word = normalizeWord(value);
  const suffixes = ["amientos", "imientos", "aciones", "uciones", "amente", "mente", "idades", "amiento", "imiento", "ación", "ución", "adoras", "adores", "adora", "ador", "ancias", "encia", "encias", "ando", "iendo", "ados", "idas", "idos", "ando", "iendo", "es", "os", "as", "s"];
  for (const suffix of suffixes) {
    if (word.length - suffix.length >= 4 && word.endsWith(suffix)) {
      word = word.slice(0, -suffix.length);
      break;
    }
  }
  return word;
}

function countSpanishSyllables(value: string) {
  const word = normalizeWord(value).replace(/[^a-záéíóúüñ]/g, "");
  if (!word) return 0;
  const nuclei = word.match(/[aeiouáéíóúü]+/g) ?? [];
  let count = 0;
  for (const group of nuclei) {
    if (group.length === 1) {
      count += 1;
      continue;
    }
    const strong = [...group].filter((letter) => "aeoáéóíú".includes(letter)).length;
    count += Math.max(1, strong);
  }
  if (word.endsWith("gue") || word.endsWith("gui") || word.endsWith("que") || word.endsWith("qui")) count = Math.max(1, count - 1);
  return Math.max(1, count);
}

function readabilityDescription(score: number) {
  if (score >= 90) return "Muy fácil";
  if (score >= 80) return "Fácil";
  if (score >= 70) return "Bastante fácil";
  if (score >= 60) return "Normal";
  if (score >= 50) return "Algo difícil";
  if (score >= 30) return "Difícil";
  return "Muy difícil";
}

function escapeRegExp(value: string) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function analyzeWritingStyle(text: string, settings: WritingAnalysisSettings): StyleAnalysisResult {
  const tokens = [...text.matchAll(/[\p{L}áéíóúüñ]+/giu)].map((match, index) => ({
    value: match[0],
    normalized: normalizeWord(match[0]),
    stem: lightSpanishStem(match[0]),
    index,
  }));
  const words = tokens.length;
  const significant = tokens.filter((token) => token.normalized.length >= 4 && !spanishStopWords.has(token.normalized));
  const repeatedIndexes = new Set<number>();
  const lastByStem = new Map<string, Array<{ index: number; tokenIndex: number }>>();
  significant.forEach((token) => {
    const previous = (lastByStem.get(token.stem) ?? []).filter((item) => token.index - item.index <= settings.repetitionWindow);
    if (previous.length > 0) {
      repeatedIndexes.add(token.index);
      previous.forEach((item) => repeatedIndexes.add(item.tokenIndex));
    }
    lastByStem.set(token.stem, [...previous, { index: token.index, tokenIndex: token.index }]);
  });
  const repeatedGroups = new Map<string, { label: string; count: number }>();
  tokens.forEach((token) => {
    if (!repeatedIndexes.has(token.index)) return;
    const current = repeatedGroups.get(token.stem) ?? { label: token.value, count: 0 };
    current.count += 1;
    repeatedGroups.set(token.stem, current);
  });
  const repetitions = [...repeatedGroups.entries()]
    .map(([key, value]) => ({ key, label: value.label, count: value.count, perThousand: words ? value.count * 1_000 / words : 0, kind: "repetition" as const }))
    .sort((a, b) => b.count - a.count || a.label.localeCompare(b.label, "es"));

  const fillers = settings.fillerPhrases.map((phrase) => {
    const clean = phrase.trim();
    const pattern = new RegExp(`(^|[^\\p{L}])(${escapeRegExp(clean)})(?=[^\\p{L}]|$)`, "giu");
    const count = clean ? [...text.matchAll(pattern)].length : 0;
    return { key: normalizeWord(clean), label: clean, count, perThousand: words ? count * 1_000 / words : 0, kind: "filler" as const };
  }).filter((item) => item.count > 0);
  const adverbs = tokens.filter((token) => token.normalized.endsWith("mente"));
  if (adverbs.length >= 2 && !fillers.some((item) => item.key === "-mente")) fillers.push({ key: "-mente", label: "Adverbios en -mente", count: adverbs.length, perThousand: words ? adverbs.length * 1_000 / words : 0, kind: "filler" });
  fillers.sort((a, b) => b.count - a.count);

  const sentenceSource = text.replace(/\b(Sr|Sra|Dr|Dra|Ud|Uds|etc)\./gi, "$1∯");
  const sentences = Math.max(1, sentenceSource.split(/[.!?…]+(?:[”’\"']+)?\s*/).filter((part) => part.trim()).length);
  const syllables = tokens.reduce((total, token) => total + countSpanishSyllables(token.value), 0);
  const syllablesPerHundred = words ? syllables * 100 / words : 0;
  const sentencesPerHundred = words ? sentences * 100 / words : 0;
  const readability = words ? Math.max(0, Math.min(100, 206.84 - 0.60 * syllablesPerHundred - 1.02 * sentencesPerHundred)) : 0;
  const fillerCount = fillers.reduce((total, item) => total + item.count, 0);
  return {
    words,
    sentences,
    averageSentenceLength: words ? words / sentences : 0,
    readability,
    readabilityLabel: words ? readabilityDescription(readability) : "Sin texto",
    repetitions,
    fillers,
    fillerPercentage: words ? fillerCount * 100 / words : 0,
    repeatedKeys: repetitions.map((item) => item.key),
  };
}

function createStyleAnalysisExtension(initialRuntime: StyleAnalysisRuntime) {
  return Extension.create({
    name: "writingStyleAnalysis",
    addProseMirrorPlugins() {
      return [new Plugin<StyleAnalysisRuntime>({
        key: styleAnalysisPluginKey,
        state: {
          init: () => initialRuntime,
          apply: (transaction, current) => (transaction.getMeta("writingStyleAnalysis") as StyleAnalysisRuntime | undefined) ?? current,
        },
        props: {
          decorations: (state) => {
            const runtime = styleAnalysisPluginKey.getState(state) ?? initialRuntime;
            if (!runtime.enabled) return DecorationSet.empty;
            const repeated = new Set(runtime.repeatedKeys);
            const decorations: Decoration[] = [];
            state.doc.descendants((node, position) => {
              if (!node.isText || !node.text) return;
              for (const match of node.text.matchAll(/[\p{L}áéíóúüñ]+/giu)) {
                if (!repeated.has(lightSpanishStem(match[0]))) continue;
                const start = match.index ?? 0;
                decorations.push(Decoration.inline(position + start, position + start + match[0].length, { class: "style-repetition", title: "Repetición cercana" }));
              }
              const lower = normalizeWord(node.text);
              runtime.fillerPhrases.forEach((phrase) => {
                if (!phrase || phrase === "-mente") return;
                const term = normalizeWord(phrase);
                let offset = lower.indexOf(term);
                while (offset >= 0) {
                  const end = offset + term.length;
                  if (isReferenceBoundary(node.text?.[offset - 1]) && isReferenceBoundary(node.text?.[end])) decorations.push(Decoration.inline(position + offset, position + end, { class: "style-filler", title: "Muletilla configurada" }));
                  offset = lower.indexOf(term, offset + Math.max(1, term.length));
                }
              });
            });
            return DecorationSet.create(state.doc, decorations);
          },
        },
      })];
    },
  });
}

function styleFlagOccurrences(editor: Editor, flag: StyleFlag) {
  const positions: Array<{ from: number; to: number }> = [];
  editor.state.doc.descendants((node, position) => {
    if (!node.isText || !node.text) return;
    if (flag.kind === "repetition") {
      for (const match of node.text.matchAll(/[\p{L}áéíóúüñ]+/giu)) {
        if (lightSpanishStem(match[0]) !== flag.key) continue;
        const start = match.index ?? 0;
        positions.push({ from: position + start, to: position + start + match[0].length });
      }
      return;
    }
    if (flag.key === "-mente") {
      for (const match of node.text.matchAll(/[\p{L}áéíóúüñ]*mente\b/giu)) {
        const start = match.index ?? 0;
        positions.push({ from: position + start, to: position + start + match[0].length });
      }
      return;
    }
    const lower = normalizeWord(node.text);
    let offset = lower.indexOf(flag.key);
    while (offset >= 0) {
      positions.push({ from: position + offset, to: position + offset + flag.key.length });
      offset = lower.indexOf(flag.key, offset + Math.max(1, flag.key.length));
    }
  });
  return positions;
}

function evidenceToText(evidence: Array<{ chapter: string; text: string }>) {
  return evidence.map((item) => `${item.chapter} | ${item.text}`).join("\n");
}

function textToEvidence(text: string) {
  return text
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => {
      const [chapter, ...rest] = line.split("|");
      return { chapter: chapter.trim() || "Sin capítulo", text: rest.join("|").trim() || chapter.trim() };
    });
}

export default function ArchiveClient({
  projectId,
  onBack,
}: {
  projectId: string;
  onBack: () => void;
}) {
  const [archive, setArchive] = useState<ArchiveState>(() => cloneInitialArchive());
  const archiveRef = useRef(archive);
  const [activeSection, setActiveSection] = useState<Section>("planificacion");
  const [planningTab, setPlanningTab] = useState<PlanningTab>("tablero");
  const [writingTab, setWritingTab] = useState<WritingTab>("manuscrito");
  const [selectedId, setSelectedId] = useState("");
  const [dossierId, setDossierId] = useState<string | null>(null);
  const [saveState, setSaveState] = useState<SaveState>("Cargando");
  const [savedAt, setSavedAt] = useState<string>("");
  const [notice, setNotice] = useState("");
  const [characterEditor, setCharacterEditor] = useState<CharacterRecord | null>(null);
  const [photoEditor, setPhotoEditor] = useState<CharacterRecord | null>(null);
  const [relationshipEditor, setRelationshipEditor] = useState<RelationshipRecord | null>(null);
  const [theoryEditor, setTheoryEditor] = useState<TheoryRecord | null>(null);
  const [timelineEditor, setTimelineEditor] = useState<TimelineEvent | null>(null);
  const [worldEditor, setWorldEditor] = useState<WorldRecord | null>(null);
  const [magicEditor, setMagicEditor] = useState<MagicSystemRecord | null>(null);
  const [mapEditor, setMapEditor] = useState<WorldMapRecord | null>(null);
  const [attachmentTarget, setAttachmentTarget] = useState<AttachmentTarget | null>(null);
  const [textEditor, setTextEditor] = useState<{ scope: TextScope; text: ImportedTextRecord } | null>(null);
  const [pdfExporting, setPdfExporting] = useState<PdfSection | null>(null);
  const [wbwExporting, setWbwExporting] = useState(false);
  const [convertingLegacy, setConvertingLegacy] = useState<TextScope | null>(null);
  const [projectEditor, setProjectEditor] = useState(false);
  const [ambienceEditor, setAmbienceEditor] = useState(false);
  const [driveBackupOpen, setDriveBackupOpen] = useState(false);
  const [openTopMenu, setOpenTopMenu] = useState<"archivo" | "configuracion" | null>(null);
  const [worldbuildingTab, setWorldbuildingTab] = useState<WorldbuildingTab>("atlas");
  const [networkMode, setNetworkMode] = useState<NetworkMode>("all");
  const [characterSearch, setCharacterSearch] = useState("");
  const [categoryFilter, setCategoryFilter] = useState("Todos");
  const [statusFilter, setStatusFilter] = useState("Todos");
  const activeEditorSaveRef = useRef<(() => Promise<boolean>) | null>(null);

  useEffect(() => {
    archiveRef.current = archive;
  }, [archive]);

  useEffect(() => {
    let cancelled = false;
    getLocalProject(projectId)
      .then((record) => {
        if (cancelled) return;
        if (!record) throw new Error("No se encontró el proyecto guardado en este navegador.");
        const state = { ...record.state, profile: { ...record.state.profile, theme: "desk" as const, activeThemeId: "desk", customThemes: [] } };
        setArchive(state);
        archiveRef.current = state;
        setSelectedId(state.characters[0]?.id ?? "");
        setSavedAt(record.updatedAt);
        setSaveState("Guardado");
      })
      .catch(() => {
        if (cancelled) return;
        setSaveState("Sin guardar");
        setNotice("Se abrió la base del manuscrito, pero el guardado persistente no respondió. Puedes seguir explorando.");
      });
    return () => { cancelled = true; };
  }, [projectId]);

  const persist = useCallback(async (next: ArchiveState, message = "Cambios guardados") => {
    const normalized = { ...next, profile: { ...next.profile, theme: "desk" as const, activeThemeId: "desk", customThemes: [] } };
    setArchive(normalized);
    archiveRef.current = normalized;
    setSaveState("Guardando");
    try {
      const record = await saveLocalProject(projectId, normalized);
      setSavedAt(record.updatedAt);
      setSaveState("Guardado");
      setNotice(message);
      return true;
    } catch (error) {
      setSaveState("Sin guardar");
      setNotice(error instanceof Error ? error.message : "No se pudieron guardar los cambios.");
      return false;
    }
  }, [projectId]);

  const saveProjectNow = useCallback(async (message = "Proyecto guardado en este navegador") => {
    const editorSave = activeEditorSaveRef.current;
    if (editorSave) {
      const saved = await editorSave();
      if (saved) setNotice(message);
      return saved;
    }
    return persist(archiveRef.current, message);
  }, [persist]);

  const registerEditorSave = useCallback((handler: (() => Promise<boolean>) | null) => {
    activeEditorSaveRef.current = handler;
  }, []);

  const theme: ArchiveTheme = "desk";

  const selected = archive.characters.find((character) => character.id === selectedId) ?? archive.characters[0];
  const selectedRelationships = useMemo(
    () => selected ? archive.relationships.filter((rel) => rel.source === selected.id || rel.target === selected.id) : [],
    [archive.relationships, selected],
  );
  const manuscriptReferences = useMemo<ManuscriptReference[]>(() => [
    ...archive.characters.map((character) => ({ id: character.id, name: character.name, aliases: character.aliases, kind: "character" as const, kindLabel: "Personaje" })),
    ...archive.world.map((record) => ({ id: record.id, name: record.name, aliases: record.aliases, kind: "world" as const, kindLabel: record.kind })),
    ...archive.magicSystems.map((system) => ({ id: system.id, name: system.name, aliases: [] as string[], kind: "magic" as const, kindLabel: "Sistema de magia" })),
    ...archive.worldTexts.map((text) => ({ id: text.id, name: text.title, aliases: [] as string[], kind: "worldText" as const, kindLabel: "Texto de Mundo" })),
    ...archive.magicTexts.map((text) => ({ id: text.id, name: text.title, aliases: [] as string[], kind: "magicText" as const, kindLabel: "Texto de Magia" })),
  ], [archive.characters, archive.world, archive.magicSystems, archive.worldTexts, archive.magicTexts]);

  const returnToLibrary = () => {
    if (saveState === "Guardando") {
      setNotice("Espera un instante: todavía se están guardando los últimos cambios.");
      return;
    }
    onBack();
  };

  const openDossier = (id: string) => {
    setSelectedId(id);
    setDossierId(id);
  };

  const openManuscriptReference = (reference: ManuscriptReference) => {
    if (reference.kind === "character") {
      openDossier(reference.id);
      return;
    }
    if (reference.kind === "world") {
      const record = archiveRef.current.world.find((item) => item.id === reference.id);
      if (record) setWorldEditor({ ...record, aliases: [...record.aliases], tags: [...record.tags], attachments: [...(record.attachments ?? [])] });
      return;
    }
    if (reference.kind === "magic") {
      const system = archiveRef.current.magicSystems.find((item) => item.id === reference.id);
      if (system) setMagicEditor({ ...system, evidence: system.evidence.map((item) => ({ ...item })), tags: [...system.tags], attachments: [...(system.attachments ?? [])] });
      return;
    }
    const scope: TextScope = reference.kind === "worldText" ? "world" : "magic";
    const record = (scope === "world" ? archiveRef.current.worldTexts : archiveRef.current.magicTexts).find((item) => item.id === reference.id);
    if (record) setTextEditor({ scope, text: { ...record } });
  };

  const saveCharacter = (character: CharacterRecord) => {
    const exists = archive.characters.some((item) => item.id === character.id);
    const next = {
      ...archive,
      characters: exists
        ? archive.characters.map((item) => item.id === character.id ? character : item)
        : [...archive.characters, character],
    };
    setCharacterEditor(null);
    setSelectedId(character.id);
    void persist(next, `Ficha de ${character.name} guardada`);
  };

  const saveRelationship = (relationship: RelationshipRecord) => {
    const exists = archive.relationships.some((item) => item.id === relationship.id);
    const next = {
      ...archive,
      relationships: exists
        ? archive.relationships.map((item) => item.id === relationship.id ? relationship : item)
        : [...archive.relationships, relationship],
    };
    setRelationshipEditor(null);
    void persist(next, "Conexión guardada");
  };

  const removeRelationship = (id: string) => {
    if (!window.confirm("¿Eliminar esta conexión del tablero?")) return;
    setRelationshipEditor(null);
    void persist({ ...archive, relationships: archive.relationships.filter((item) => item.id !== id) }, "Conexión eliminada");
  };

  const saveTheory = (theory: TheoryRecord) => {
    const exists = archive.theories.some((item) => item.id === theory.id);
    const next = {
      ...archive,
      theories: exists
        ? archive.theories.map((item) => item.id === theory.id ? theory : item)
        : [...archive.theories, theory],
    };
    setTheoryEditor(null);
    void persist(next, "Hilo guardado");
  };

  const saveTimelineEvent = (timelineEvent: TimelineEvent) => {
    const exists = archive.timeline.some((item) => item.id === timelineEvent.id);
    const next = {
      ...archive,
      timeline: exists
        ? archive.timeline.map((item) => item.id === timelineEvent.id ? timelineEvent : item)
        : [...archive.timeline, timelineEvent],
    };
    setTimelineEditor(null);
    void persist(next, "Suceso de la cronología guardado");
  };

  const saveWorldRecord = (record: WorldRecord) => {
    const exists = archive.world.some((item) => item.id === record.id);
    const next = {
      ...archive,
      world: exists
        ? archive.world.map((item) => item.id === record.id ? record : item)
        : [...archive.world, record],
    };
    setWorldEditor(null);
    void persist(next, `Entrada de ${record.name} guardada`);
  };

  const saveMagicSystem = (system: MagicSystemRecord) => {
    const exists = archive.magicSystems.some((item) => item.id === system.id);
    const next = {
      ...archive,
      magicSystems: exists
        ? archive.magicSystems.map((item) => item.id === system.id ? system : item)
        : [...archive.magicSystems, system],
    };
    setMagicEditor(null);
    void persist(next, `Sistema ${system.name} guardado`);
  };

  const saveWorldMap = (map: WorldMapRecord) => {
    const current = archiveRef.current;
    const normalized = { ...map, updatedAt: new Date().toISOString() };
    const exists = current.maps.some((item) => item.id === normalized.id);
    setMapEditor(null);
    void persist({ ...current, maps: exists ? current.maps.map((item) => item.id === normalized.id ? normalized : item) : [...current.maps, normalized] }, `Mapa “${normalized.name}” guardado`);
  };

  const removeWorldMap = (id: string) => {
    if (!window.confirm("¿Eliminar este mapa? Esta acción no se puede deshacer.")) return;
    const current = archiveRef.current;
    setMapEditor(null);
    void persist({ ...current, maps: current.maps.filter((item) => item.id !== id) }, "Mapa eliminado");
  };

  const saveManuscriptLayout = (layout: ManuscriptLayout) => {
    const current = archiveRef.current;
    void persist({ ...current, profile: { ...current.profile, manuscriptLayout: layout } }, "Maquetación guardada y lista para exportar");
  };

  const saveBoardViewport = (viewport: BoardViewport) => {
    const current = archiveRef.current;
    void persist({ ...current, profile: { ...current.profile, boardViewport: viewport } }, "Vista del tablero guardada");
  };

  const saveSceneBoardView = (viewport: BoardViewport, compact: boolean) => {
    const current = archiveRef.current;
    void persist({ ...current, profile: { ...current.profile, sceneBoardViewport: viewport, sceneBoardCompact: compact } }, "Vista del tablero de escenas guardada");
  };

  const saveWritingChapter = async (chapter: WritingChapter) => {
    const current = archiveRef.current;
    const exists = current.writingChapters.some((item) => item.id === chapter.id);
    const next = {
      ...current,
      writingChapters: exists
        ? current.writingChapters.map((item) => item.id === chapter.id ? chapter : item)
        : [...current.writingChapters, chapter],
    };
    return await persist(next, `${chapter.label || "Capítulo"} guardado`);
  };

  const saveWritingStructure = async (chapters: WritingChapter[], message = "Estructura del manuscrito guardada") => {
    const normalized = orderedChapters(chapters).map((chapter, chapterOrder) => ({
      ...chapter,
      order: chapterOrder,
      scenes: orderedScenes(chapter).map((scene, sceneOrder) => ({ ...scene, chapterId: chapter.id, order: sceneOrder })),
    }));
    const current = archiveRef.current;
    return await persist({ ...current, writingChapters: normalized }, message);
  };

  const saveWritingAnalysisSettings = (settings: WritingAnalysisSettings) => {
    const current = archiveRef.current;
    void persist({ ...current, profile: { ...current.profile, writingAnalysis: settings } }, "Preferencias del análisis local guardadas");
  };

  const removeWritingChapter = (id: string) => {
    if (!window.confirm("¿Eliminar este capítulo y todas sus escenas? Esta acción no se puede deshacer.")) return false;
    const current = archiveRef.current;
    void persist({ ...current, writingChapters: current.writingChapters.filter((item) => item.id !== id) }, "Capítulo eliminado");
    return true;
  };

  const updateTargetAttachments = (attachments: ArchiveAttachment[]) => {
    if (!attachmentTarget) return;
    const current = archiveRef.current;
    const next = attachmentTarget.scope === "world"
      ? { ...current, world: current.world.map((item) => item.id === attachmentTarget.recordId ? { ...item, attachments } : item) }
      : { ...current, magicSystems: current.magicSystems.map((item) => item.id === attachmentTarget.recordId ? { ...item, attachments } : item) };
    void persist(next, `Imágenes de ${attachmentTarget.name} actualizadas`);
  };

  const saveAmbienceSources = (spotifyUrl: string, youtubeUrl: string) => {
    const current = archiveRef.current;
    setAmbienceEditor(false);
    void persist({ ...current, profile: { ...current.profile, spotifyPlaylistUrl: spotifyUrl, youtubeAmbientUrl: youtubeUrl } }, spotifyUrl || youtubeUrl ? "Fuentes de ambientación guardadas" : "Fuentes de ambientación quitadas");
  };

  const saveProject = (profile: ProjectProfile, manuscript: ArchiveState["manuscript"]) => {
    const next = { ...archive, title: profile.archiveTitle, profile, manuscript };
    setProjectEditor(false);
    void persist(next, "Datos de la obra guardados");
  };

  const removeTimelineEvent = (id: string) => {
    if (!window.confirm("¿Eliminar este suceso de la cronología?")) return;
    setTimelineEditor(null);
    void persist({ ...archive, timeline: archive.timeline.filter((item) => item.id !== id) }, "Suceso eliminado");
  };

  const removeWorldRecord = (id: string) => {
    if (!window.confirm("¿Eliminar esta entrada del mundo?")) return;
    setWorldEditor(null);
    void persist({ ...archive, world: archive.world.filter((item) => item.id !== id) }, "Entrada del mundo eliminada");
  };

  const removeMagicSystem = (id: string) => {
    if (!window.confirm("¿Eliminar este sistema de magia?")) return;
    setMagicEditor(null);
    void persist({ ...archive, magicSystems: archive.magicSystems.filter((item) => item.id !== id) }, "Sistema de magia eliminado");
  };

  const saveImportedText = (scope: TextScope, text: ImportedTextRecord) => {
    const current = archiveRef.current;
    const collection = scope === "world" ? current.worldTexts : current.magicTexts;
    const exists = collection.some((item) => item.id === text.id);
    const nextCollection = exists ? collection.map((item) => item.id === text.id ? text : item) : [...collection, text];
    const next = scope === "world" ? { ...current, worldTexts: nextCollection } : { ...current, magicTexts: nextCollection };
    setTextEditor(null);
    void persist(next, `Texto “${text.title}” guardado en ${scope === "world" ? "Mundo" : "Magia"}`);
  };

  const removeImportedText = (scope: TextScope, id: string) => {
    if (!window.confirm("¿Eliminar este texto importado?")) return;
    const current = archiveRef.current;
    const next = scope === "world"
      ? { ...current, worldTexts: current.worldTexts.filter((item) => item.id !== id) }
      : { ...current, magicTexts: current.magicTexts.filter((item) => item.id !== id) };
    void persist(next, "Texto eliminado");
  };

  const convertLegacyTextAttachments = async (scope: TextScope) => {
    if (convertingLegacy) return;
    const current = archiveRef.current;
    const records = scope === "world" ? current.world : current.magicSystems;
    const legacy = records.flatMap((record) => (record.attachments ?? []).filter((attachment) => attachment.kind !== "image").map((attachment) => ({ recordId: record.id, attachment })));
    if (legacy.length === 0) {
      setNotice("No hay documentos adjuntos anteriores que convertir.");
      return;
    }
    setConvertingLegacy(scope);
    setNotice("Convirtiendo adjuntos anteriores en texto editable…");
    const imported: ImportedTextRecord[] = [];
    const convertedIds = new Set<string>();
    for (const item of legacy) {
      try {
        const response = await fetch(attachmentUrl(projectId, scope, item.recordId, item.attachment));
        if (!response.ok) throw new Error("No se pudo abrir el archivo anterior.");
        const blob = await response.blob();
        const parsed = await extractTextFromFile(new File([blob], item.attachment.name, { type: item.attachment.mimeType }));
        imported.push(parsed);
        convertedIds.add(item.attachment.id);
      } catch {
        // Los formatos que no contienen texto extraíble permanecen intactos para no perder datos.
      }
    }
    if (imported.length === 0) {
      setConvertingLegacy(null);
      setNotice("Los adjuntos anteriores no eran DOCX, TXT, Markdown o CSV convertibles.");
      return;
    }
    const cleanedRecords = records.map((record) => ({ ...record, attachments: (record.attachments ?? []).filter((attachment) => !convertedIds.has(attachment.id)) }));
    const next = scope === "world"
      ? { ...current, world: cleanedRecords as WorldRecord[], worldTexts: [...current.worldTexts, ...imported] }
      : { ...current, magicSystems: cleanedRecords as MagicSystemRecord[], magicTexts: [...current.magicTexts, ...imported] };
    await persist(next, `${imported.length} ${imported.length === 1 ? "documento convertido" : "documentos convertidos"} en texto editable`);
    setConvertingLegacy(null);
  };

  const runPdfExport = async (section: PdfSection) => {
    if (pdfExporting) return;
    setPdfExporting(section);
    setNotice(`Preparando ${pdfSectionNames[section].toLocaleLowerCase("es")} en PDF con texto seleccionable…`);
    try {
      await exportTextPdf(archiveRef.current, section);
      setNotice(`${pdfSectionNames[section]} exportado en PDF`);
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "No se pudo exportar el PDF.");
    } finally {
      setPdfExporting(null);
    }
  };

  const exportProjectPackage = async () => {
    if (wbwExporting) return;
    setWbwExporting(true);
    setNotice("Empaquetando datos, imágenes y recursos del proyecto…");
    try {
      const blob = await createWbwPackage(archiveRef.current, projectId);
      const url = URL.createObjectURL(blob);
      const anchor = document.createElement("a");
      anchor.href = url;
      anchor.download = `${safeDownloadName(archiveRef.current.profile.archiveTitle)}.wbw`;
      anchor.click();
      window.setTimeout(() => URL.revokeObjectURL(url), 1_000);
      setNotice("Proyecto completo exportado como .wbw");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "No se pudo crear el paquete .wbw.");
    } finally {
      setWbwExporting(false);
    }
  };

  const sectionLabels: Array<{ id: Section; label: string }> = [
    { id: "planificacion", label: "Planificación" },
    { id: "escritura", label: "Escritura" },
    { id: "mundo", label: "Mundo" },
    { id: "revision", label: "Revisión" },
  ];

  const dossierCharacter = dossierId ? archive.characters.find((item) => item.id === dossierId) : undefined;
  const writtenChapterCount = archive.writingChapters.length || archive.manuscript.chapters;
  const writtenWords = archive.writingChapters.length > 0 ? manuscriptWordCount(archive.writingChapters) : archive.manuscript.words;
  const contextTitle = activeSection === "planificacion" && planningTab === "tablero"
    ? archive.profile.homeHeading
    : sectionLabels.find((item) => item.id === activeSection)?.label;
  const targetAttachments = attachmentTarget
    ? attachmentTarget.scope === "world"
      ? archive.world.find((item) => item.id === attachmentTarget.recordId)?.attachments ?? []
      : archive.magicSystems.find((item) => item.id === attachmentTarget.recordId)?.attachments ?? []
    : [];

  return (
    <main className="archive-shell desktop-app-shell">
      <header className="desktop-ribbon">
        <div className="ribbon-title-row">
          <button type="button" className="ribbon-app-mark" onClick={() => { setActiveSection("planificacion"); setPlanningTab("tablero"); }} aria-label="Ir al tablero">{archive.profile.boardIconDataUrl ? <img src={archive.profile.boardIconDataUrl} alt="" /> : "WW"}</button>
          <div className="ribbon-document-title"><span>Worldbuilder Writer</span><strong>{archive.profile.archiveTitle}</strong></div>
          <div className={`ribbon-save-state ${saveState === "Sin guardar" ? "warning" : ""}`}><i />{saveState === "Guardando" ? "Guardando…" : saveState === "Cargando" ? "Abriendo…" : saveState === "Sin guardar" ? "Cambios sin guardar" : "Guardado en este navegador"}</div>
          <div className="ribbon-title-actions">
            <button type="button" className="ribbon-save-button" onClick={() => void saveProjectNow()} disabled={saveState === "Cargando" || saveState === "Guardando"} title="Guardar el proyecto sin descargar un archivo"><span>▣</span> Guardar</button>
            <div className="ribbon-menu-wrap">
              <button type="button" className={openTopMenu === "configuracion" ? "ribbon-settings-button active" : "ribbon-settings-button"} onClick={() => setOpenTopMenu((current) => current === "configuracion" ? null : "configuracion")} aria-expanded={openTopMenu === "configuracion"}><span>⚙</span> Configuración</button>
              {openTopMenu === "configuracion" && <div className="ribbon-popup ribbon-settings-popup"><button type="button" onClick={() => { setOpenTopMenu(null); setProjectEditor(true); }}><span>▤</span><b>Obra y espacio</b><small>Identidad, banner, portada y datos</small></button><button type="button" onClick={() => { setOpenTopMenu(null); setAmbienceEditor(true); }}><span>♫</span><b>Reproductor</b><small>Spotify, YouTube y ambientación</small></button><button type="button" onClick={() => { setOpenTopMenu(null); void saveProjectNow("Proyecto guardado antes del respaldo").then((saved) => { if (saved) setDriveBackupOpen(true); }); }}><span>☁</span><b>Google Drive</b><small>Conectar, respaldar o restaurar</small></button></div>}
            </div>
          </div>
        </div>

        <nav className="ribbon-tabs" aria-label="Menú principal">
          <div className="ribbon-menu-wrap ribbon-file-wrap">
            <button type="button" className={openTopMenu === "archivo" ? "active" : ""} onClick={() => setOpenTopMenu((current) => current === "archivo" ? null : "archivo")} aria-expanded={openTopMenu === "archivo"}>Archivo</button>
            {openTopMenu === "archivo" && <div className="ribbon-popup ribbon-file-popup"><button type="button" onClick={() => { setOpenTopMenu(null); void saveProjectNow("Proyecto guardado antes de volver a la biblioteca").then((saved) => { if (saved) returnToLibrary(); }); }}><span>←</span><b>Todas las obras</b><small>Guardar y volver a la biblioteca local</small></button><button type="button" onClick={() => { setOpenTopMenu(null); void saveProjectNow(); }}><span>▣</span><b>Guardar</b><small>Conservar los avances sin descargar</small></button><button type="button" disabled={wbwExporting} onClick={() => { setOpenTopMenu(null); void saveProjectNow("Proyecto guardado antes de exportar").then((saved) => { if (saved) void exportProjectPackage(); }); }}><span>⇩</span><b>Proyecto editable (.wbw)</b><small>Copia completa y transportable</small></button><button type="button" disabled={Boolean(pdfExporting)} onClick={() => { setOpenTopMenu(null); void saveProjectNow("Proyecto guardado antes de generar el dossier").then((saved) => { if (saved) void runPdfExport("proyecto"); }); }}><span>PDF</span><b>Dossier de consulta</b><small>Documento para leer o imprimir</small></button></div>}
          </div>
          {sectionLabels.map(({ id, label }) => <button type="button" className={activeSection === id ? "active" : ""} key={id} onClick={() => {
            setOpenTopMenu(null);
            if (activeSection === "escritura" && id !== "escritura") void saveProjectNow("Cambios del manuscrito guardados").then((saved) => { if (saved) setActiveSection(id); });
            else setActiveSection(id);
          }}>{label}</button>)}
        </nav>

        <div className="ribbon-context-row"><div><span>{archive.profile.projectLabel || "Proyecto narrativo"}</span><strong>{contextTitle}</strong></div><div>{archive.profile.storyTitle && <span>{archive.profile.storyTitle}</span>}<small>{writtenChapterCount} capítulos · {writtenWords.toLocaleString("es-CL")} palabras{savedAt ? ` · guardado a las ${new Date(savedAt).toLocaleTimeString("es-CL", { hour: "2-digit", minute: "2-digit", timeZone: "America/Santiago" })}` : ""}</small></div></div>
      </header>

      <section className="workspace">
        {notice && <button className="notice" onClick={() => setNotice("")} aria-label="Cerrar aviso"><span>{notice}</span><b>×</b></button>}

        {activeSection === "planificacion" && (
          <section className="area-workspace">
            <AreaNavigation eyebrow="REPARTO / ESTRUCTURA / CONTINUIDAD" title="Planificación" description="Personajes, conexiones, hilos y sucesos que sostienen la historia." active={planningTab} onChange={(value) => setPlanningTab(value as PlanningTab)} tabs={[{ id: "tablero", label: "Tablero", note: "Relaciones visuales" }, { id: "personajes", label: "Personajes", note: "Fichas y antecedentes" }, { id: "hilos", label: "Tramas e hilos", note: "Ideas y contrapuntos" }, { id: "cronologia", label: "Cronología", note: "Sucesos narrativos" }]} onExport={() => void runPdfExport("planificacion")} exporting={pdfExporting === "planificacion"} />
            {planningTab === "tablero" && <BoardView archive={archive} selected={selected} selectedRelationships={selectedRelationships} networkMode={networkMode} onNetworkMode={setNetworkMode} onSelect={setSelectedId} onOpenDossier={openDossier} onPhoto={setPhotoEditor} theme={theme} viewport={archive.profile.boardViewport ?? { scale: 1, x: 0, y: 0 }} onSaveViewport={saveBoardViewport} onEditRelationship={setRelationshipEditor} onNewRelationship={(sourceId, targetId) => setRelationshipEditor(emptyRelationship(archive.characters, sourceId, targetId))} onNewCharacter={() => setCharacterEditor(emptyCharacter(archive.profile.chapterLabels.length, archive.characters))} onExportPdf={() => void runPdfExport("tablero")} exportingPdf={pdfExporting === "tablero"} onMoveNode={(id, x, y, save) => { const current = archiveRef.current; const next = { ...current, characters: current.characters.map((character) => character.id === id ? { ...character, board: { ...character.board, x, y } } : character) }; setArchive(next); archiveRef.current = next; if (save) void persist(next, "Posición del tablero guardada"); }} />}
            {planningTab === "personajes" && <CharactersView archive={archive} search={characterSearch} categoryFilter={categoryFilter} statusFilter={statusFilter} onSearch={setCharacterSearch} onCategory={setCategoryFilter} onStatus={setStatusFilter} onOpen={openDossier} onNew={() => setCharacterEditor(emptyCharacter(archive.profile.chapterLabels.length, archive.characters))} onExportPdf={() => void runPdfExport("personajes")} exportingPdf={pdfExporting === "personajes"} />}
            {planningTab === "hilos" && <TheoriesView archive={archive} onEdit={setTheoryEditor} onNew={() => setTheoryEditor(emptyTheory())} onOpen={openDossier} onExportPdf={() => void runPdfExport("hilos")} exportingPdf={pdfExporting === "hilos"} />}
            {planningTab === "cronologia" && <TimelineView archive={archive} onOpen={openDossier} onEdit={setTimelineEditor} onNew={() => setTimelineEditor(emptyTimelineEvent())} onExportPdf={() => void runPdfExport("cronologia")} exportingPdf={pdfExporting === "cronologia"} />}
          </section>
        )}

        {activeSection === "escritura" && (
          <section className="area-workspace">
            <AreaNavigation eyebrow="CAPÍTULOS / ESCENAS / EDICIÓN" title="Escritura" description="El manuscrito, su tablero de escenas y la preparación visual del libro." active={writingTab} onChange={(value) => {
              const nextTab = value as WritingTab;
              if (writingTab === "manuscrito" && nextTab !== "manuscrito") void saveProjectNow("Cambios del manuscrito guardados").then((saved) => { if (saved) setWritingTab(nextTab); });
              else setWritingTab(nextTab);
            }} tabs={[{ id: "manuscrito", label: "Manuscrito", note: "Escribir y reordenar" }, { id: "maquetacion", label: "Maquetación", note: "Formato de página" }]} />
            {writingTab === "manuscrito" && <ManuscriptView archive={archive} chapters={archive.writingChapters} references={manuscriptReferences} analysisSettings={archive.profile.writingAnalysis ?? defaultWritingAnalysis} sceneBoardViewport={archive.profile.sceneBoardViewport ?? { scale: 1, x: 18, y: 18, worldSpace: true }} sceneBoardCompact={archive.profile.sceneBoardCompact ?? false} onSaveSceneBoardView={saveSceneBoardView} onOpenReference={openManuscriptReference} onSave={saveWritingChapter} onSaveStructure={saveWritingStructure} onSaveAnalysisSettings={saveWritingAnalysisSettings} onRegisterEditorSave={registerEditorSave} onDelete={removeWritingChapter} onExportPdf={() => void runPdfExport("manuscrito")} exportingPdf={pdfExporting === "manuscrito"} />}
            {writingTab === "maquetacion" && <LayoutView archive={archive} layout={archive.profile.manuscriptLayout ?? defaultManuscriptLayout} onSave={saveManuscriptLayout} />}
          </section>
        )}

        {activeSection === "mundo" && (
          <section className="area-workspace">
            <AreaNavigation eyebrow="ATLAS / GEOGRAFÍA / REGLAS" title="Mundo" description="La enciclopedia, los mapas y los sistemas que definen el escenario." active={worldbuildingTab} onChange={(value) => setWorldbuildingTab(value as WorldbuildingTab)} tabs={[{ id: "atlas", label: "Enciclopedia", note: "Países, monedas y culturas" }, { id: "mapas", label: "Mapas", note: "Importar o generar" }, { id: "magia", label: "Sistemas de magia", note: "Costos, límites y reglas" }]} onExport={() => void runPdfExport("mundo_total")} exporting={pdfExporting === "mundo_total"} />
            {worldbuildingTab === "atlas" && <WorldView projectId={projectId} records={archive.world} texts={archive.worldTexts} legacyCount={archive.world.reduce((total, record) => total + (record.attachments ?? []).filter((item) => item.kind !== "image").length, 0)} convertingLegacy={convertingLegacy === "world"} onConvertLegacy={() => void convertLegacyTextAttachments("world")} onEdit={setWorldEditor} onNew={() => setWorldEditor(emptyWorldRecord())} onFiles={(record) => setAttachmentTarget({ scope: "world", recordId: record.id, name: record.name })} onNewText={() => setTextEditor({ scope: "world", text: emptyImportedText() })} onEditText={(text) => setTextEditor({ scope: "world", text: { ...text } })} onDeleteText={(id) => removeImportedText("world", id)} onExportPdf={() => void runPdfExport("mundo")} exportingPdf={pdfExporting === "mundo"} />}
            {worldbuildingTab === "mapas" && <MapsView maps={archive.maps} onNew={() => setMapEditor(emptyWorldMap(archive.maps.length))} onEdit={(map) => setMapEditor({ ...map, markers: map.markers.map((marker) => ({ ...marker })) })} onExportPdf={() => void runPdfExport("mapas")} exportingPdf={pdfExporting === "mapas"} />}
            {worldbuildingTab === "magia" && <MagicView projectId={projectId} systems={archive.magicSystems} texts={archive.magicTexts} legacyCount={archive.magicSystems.reduce((total, system) => total + (system.attachments ?? []).filter((item) => item.kind !== "image").length, 0)} convertingLegacy={convertingLegacy === "magic"} onConvertLegacy={() => void convertLegacyTextAttachments("magic")} onEdit={setMagicEditor} onNew={() => setMagicEditor(emptyMagicSystem())} onFiles={(system) => setAttachmentTarget({ scope: "magic", recordId: system.id, name: system.name })} onNewText={() => setTextEditor({ scope: "magic", text: emptyImportedText() })} onEditText={(text) => setTextEditor({ scope: "magic", text: { ...text } })} onDeleteText={(id) => removeImportedText("magic", id)} onExportPdf={() => void runPdfExport("magia")} exportingPdf={pdfExporting === "magia"} />}
          </section>
        )}

        {activeSection === "revision" && <ReviewView archive={archive} onOpenManuscript={() => { setActiveSection("escritura"); setWritingTab("manuscrito"); }} onExportPdf={() => void runPdfExport("revision")} exportingPdf={pdfExporting === "revision"} />}
      </section>

      <AmbienceDock projectId={projectId} spotifyUrl={archive.profile.spotifyPlaylistUrl ?? ""} youtubeUrl={archive.profile.youtubeAmbientUrl ?? ""} onConfigure={() => setOpenTopMenu("configuracion")} />

      {dossierId && dossierCharacter && (
        <DossierDrawer
          character={dossierCharacter}
          relationships={archive.relationships.filter((rel) => rel.source === dossierId || rel.target === dossierId)}
          characters={archive.characters}
          onClose={() => setDossierId(null)}
          onEdit={(character) => { setDossierId(null); setCharacterEditor({ ...character }); }}
          onPhoto={(character) => { setDossierId(null); setPhotoEditor(character); }}
          onOpen={openDossier}
        />
      )}

      {characterEditor && <CharacterEditor character={characterEditor} chapterNames={archive.profile.chapterLabels} onCancel={() => setCharacterEditor(null)} onSave={saveCharacter} />}
      {photoEditor && <PhotoEditor character={photoEditor} onCancel={() => setPhotoEditor(null)} onSave={(character) => { setPhotoEditor(null); saveCharacter(character); }} />}
      {relationshipEditor && <RelationshipEditor relationship={relationshipEditor} characters={archive.characters} onCancel={() => setRelationshipEditor(null)} onSave={saveRelationship} onDelete={archive.relationships.some((item) => item.id === relationshipEditor.id) ? () => removeRelationship(relationshipEditor.id) : undefined} />}
      {theoryEditor && <TheoryEditor theory={theoryEditor} characters={archive.characters} onCancel={() => setTheoryEditor(null)} onSave={saveTheory} />}
      {timelineEditor && <TimelineEditor timelineEvent={timelineEditor} characters={archive.characters} onCancel={() => setTimelineEditor(null)} onSave={saveTimelineEvent} onDelete={archive.timeline.some((item) => item.id === timelineEditor.id) ? () => removeTimelineEvent(timelineEditor.id) : undefined} />}
      {worldEditor && <WorldEditor projectId={projectId} record={worldEditor} onCancel={() => setWorldEditor(null)} onSave={saveWorldRecord} onDelete={archive.world.some((item) => item.id === worldEditor.id) ? () => removeWorldRecord(worldEditor.id) : undefined} />}
      {magicEditor && <MagicEditor projectId={projectId} system={magicEditor} onCancel={() => setMagicEditor(null)} onSave={saveMagicSystem} onDelete={archive.magicSystems.some((item) => item.id === magicEditor.id) ? () => removeMagicSystem(magicEditor.id) : undefined} />}
      {mapEditor && <MapEditor map={mapEditor} onCancel={() => setMapEditor(null)} onSave={saveWorldMap} onDelete={archive.maps.some((item) => item.id === mapEditor.id) ? () => removeWorldMap(mapEditor.id) : undefined} />}
      {attachmentTarget && <AttachmentLibraryModal target={attachmentTarget} attachments={targetAttachments} onChange={updateTargetAttachments} onClose={() => setAttachmentTarget(null)} />}
      {textEditor && <ImportedTextEditor scope={textEditor.scope} text={textEditor.text} onCancel={() => setTextEditor(null)} onSave={(text) => saveImportedText(textEditor.scope, text)} />}
      {ambienceEditor && <AmbienceEditor spotifyUrl={archive.profile.spotifyPlaylistUrl ?? ""} youtubeUrl={archive.profile.youtubeAmbientUrl ?? ""} onCancel={() => setAmbienceEditor(false)} onSave={saveAmbienceSources} />}
      {projectEditor && <ProjectEditor archive={archive} onCancel={() => setProjectEditor(false)} onSave={saveProject} />}
      {driveBackupOpen && <GoogleDriveBackup onClose={() => setDriveBackupOpen(false)} onRestored={(restoredProjectId, state) => {
        if (restoredProjectId !== projectId) {
          setNotice(`“${state.profile.archiveTitle}” se restauró en Todas las obras`);
          return;
        }
        const restored = { ...state, profile: { ...state.profile, theme: "desk" as const, activeThemeId: "desk", customThemes: [] } };
        setArchive(restored);
        archiveRef.current = restored;
        setSelectedId(restored.characters[0]?.id ?? "");
        setSavedAt(new Date().toISOString());
        setSaveState("Guardado");
        setNotice("Respaldo de Google Drive restaurado");
      }} />}
    </main>
  );
}

function AreaNavigation({ eyebrow, title, description, tabs, active, onChange, onExport, exporting = false }: {
  eyebrow: string;
  title: string;
  description: string;
  tabs: Array<{ id: string; label: string; note: string }>;
  active: string;
  onChange: (id: string) => void;
  onExport?: () => void;
  exporting?: boolean;
}) {
  return (
    <header className="area-navigation">
      <div className="area-navigation-copy"><p>{eyebrow}</p><h2>{title}</h2><span>{description}</span></div>
      {onExport && <button className="secondary-action area-export" onClick={onExport} disabled={exporting}>{exporting ? "Creando PDF…" : `Exportar ${title} PDF`}</button>}
      <div className="area-tabs" role="tablist" aria-label={title}>{tabs.map((tab) => <button key={tab.id} className={active === tab.id ? "active" : ""} role="tab" aria-selected={active === tab.id} onClick={() => onChange(tab.id)}><strong>{tab.label}</strong><small>{tab.note}</small></button>)}</div>
    </header>
  );
}

function WorldMapCanvas({ map, onAddMarker }: { map: WorldMapRecord; onAddMarker?: (x: number, y: number) => void }) {
  const palette = mapPalettes[map.style];
  const masses = useMemo(
    () => mapLandmasses(map.seed, map.continents, map.islands, map.roughness),
    [map.seed, map.continents, map.islands, map.roughness],
  );
  const gridId = `map-grid-${map.id.replace(/[^a-z0-9]/gi, "")}`;
  return (
    <svg className={`world-map-canvas ${onAddMarker ? "interactive" : ""}`} viewBox="0 0 100 100" preserveAspectRatio="none" role="img" aria-label={`Mapa ${map.name}`} onClick={(event) => { if (!onAddMarker) return; const rect = event.currentTarget.getBoundingClientRect(); onAddMarker((event.clientX - rect.left) / rect.width * 100, (event.clientY - rect.top) / rect.height * 100); }}>
      <defs><pattern id={gridId} width="10" height="10" patternUnits="userSpaceOnUse"><path d="M 10 0 L 0 0 0 10" fill="none" stroke={palette.grid} strokeWidth=".18" opacity=".42" /></pattern></defs>
      <rect width="100" height="100" fill={palette.water} />
      {map.backgroundImageDataUrl && <image href={map.backgroundImageDataUrl} x="0" y="0" width="100" height="100" preserveAspectRatio="xMidYMid slice" opacity=".92" />}
      {!map.backgroundImageDataUrl && masses.map((points, index) => <polygon key={index} points={points} fill={palette.land} stroke={palette.coast} strokeWidth=".72" strokeLinejoin="round" vectorEffect="non-scaling-stroke" />)}
      <rect width="100" height="100" fill={`url(#${gridId})`} />
      <path d="M6 92h20M6 92v-2M16 92v-1.3M26 92v-2" stroke={palette.ink} strokeWidth=".45" opacity=".8" vectorEffect="non-scaling-stroke" />
      {map.markers.map((marker) => <g className="map-marker" key={marker.id} transform={`translate(${marker.x} ${marker.y})`}><circle r="1.55" fill={palette.ink} stroke={palette.land} strokeWidth=".55" vectorEffect="non-scaling-stroke" /><path d="M0 1.5v4" stroke={palette.ink} strokeWidth=".5" vectorEffect="non-scaling-stroke" /><text x="2.4" y=".8" fill={palette.ink} fontSize="3.1" paintOrder="stroke" stroke={palette.land} strokeWidth=".75" strokeLinejoin="round">{marker.label}</text></g>)}
    </svg>
  );
}

function mapSvgMarkup(map: WorldMapRecord) {
  const palette = mapPalettes[map.style];
  const masses = mapLandmasses(map.seed, map.continents, map.islands, map.roughness);
  const background = map.backgroundImageDataUrl ? `<image href="${map.backgroundImageDataUrl}" x="0" y="0" width="1000" height="650" preserveAspectRatio="xMidYMid slice"/>` : masses.map((points) => `<polygon points="${points.split(" ").map((point) => { const [x, y] = point.split(","); return `${Number(x) * 10},${Number(y) * 6.5}`; }).join(" ")}" fill="${palette.land}" stroke="${palette.coast}" stroke-width="3"/>`).join("");
  const markers = map.markers.map((marker) => `<g transform="translate(${marker.x * 10} ${marker.y * 6.5})"><circle r="8" fill="${palette.ink}" stroke="${palette.land}" stroke-width="3"/><text x="14" y="5" fill="${palette.ink}" font-family="Georgia,serif" font-size="18">${escapeXml(marker.label)}</text></g>`).join("");
  return `<?xml version="1.0" encoding="UTF-8"?><svg xmlns="http://www.w3.org/2000/svg" width="1000" height="650" viewBox="0 0 1000 650"><rect width="1000" height="650" fill="${palette.water}"/>${background}${markers}</svg>`;
}

function MapsView({ maps, onNew, onEdit, onExportPdf, exportingPdf }: { maps: WorldMapRecord[]; onNew: () => void; onEdit: (map: WorldMapRecord) => void; onExportPdf: () => void; exportingPdf: boolean }) {
  return (
    <section className="maps-view">
      <div className="view-intro"><div><p>CARTOGRAFÍA / CAPAS / LUGARES</p><h2>Mapas del mundo</h2><span>Importa un mapa propio o genera una base reproducible por semilla; después añade lugares directamente sobre la superficie.</span></div><div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button><button className="primary-action" onClick={onNew}>+ Crear mapa</button></div></div>
      {maps.length === 0 ? <div className="maps-empty"><span>⌖</span><h3>Tu atlas todavía está vacío</h3><p>Genera un continente o carga una imagen cartográfica existente.</p><button className="primary-action" onClick={onNew}>Crear el primer mapa</button></div> : <div className="maps-grid">{maps.map((map) => <article className="map-card" key={map.id}><WorldMapCanvas map={map} /><div><span>{map.style} · semilla {map.seed}</span><h3>{map.name}</h3><p>{map.description || "Sin descripción todavía."}</p><small>{map.markers.length} {map.markers.length === 1 ? "lugar" : "lugares"} señalado{map.markers.length === 1 ? "" : "s"}</small></div><footer><button onClick={() => downloadBlob(new Blob([mapSvgMarkup(map)], { type: "image/svg+xml" }), `${safeDownloadName(map.name)}.svg`)}>Descargar SVG</button><button onClick={() => onEdit(map)}>Editar mapa</button></footer></article>)}</div>}
    </section>
  );
}

function LayoutView({ archive, layout, onSave }: { archive: ArchiveState; layout: ManuscriptLayout; onSave: (layout: ManuscriptLayout) => void }) {
  const [draft, setDraft] = useState<ManuscriptLayout>(() => ({ ...layout }));
  const sampleScene = orderedChapters(archive.writingChapters).flatMap((chapter) => orderedScenes(chapter))[0];
  const sampleText = sampleScene?.content.trim() || "La lluvia golpeaba los postigos mientras la ciudad apagaba sus últimas lámparas. Nadie quiso mirar hacia la torre cuando las campanas sonaron por tercera vez.";
  const updateNumber = (key: keyof ManuscriptLayout, value: number) => setDraft((current) => ({ ...current, preset: "personalizada", [key]: value }));
  const pageScale = Math.min(1, 220 / draft.pageWidthMm, 310 / draft.pageHeightMm);
  return (
    <section className="layout-view">
      <div className="view-intro"><div><p>FORMATO DE PÁGINA / TIPOGRAFÍA / RITMO</p><h2>Maquetación visual</h2><span>Elige una base y ajusta sus medidas. La configuración se aplica a las exportaciones del manuscrito.</span></div><div className="view-actions"><button className="primary-action" onClick={() => onSave(draft)}>Guardar maquetación</button></div></div>
      <div className="layout-preset-grid">{manuscriptLayoutPresets.map((preset) => <button key={preset.id} className={draft.preset === preset.id ? "active" : ""} onClick={() => setDraft({ ...preset.settings })}><span>{preset.name}</span><small>{preset.note}</small></button>)}<button className={draft.preset === "personalizada" ? "active" : ""} onClick={() => setDraft((current) => ({ ...current, preset: "personalizada" }))}><span>Personalizada</span><small>Edita cada medida y estilo a tu gusto.</small></button></div>
      <div className="layout-studio">
        <div className="layout-controls">
          <h3>Medidas y estilo</h3>
          <div className="layout-control-grid"><label><span>Ancho (mm)</span><input type="number" min="90" max="320" value={draft.pageWidthMm} onChange={(event) => updateNumber("pageWidthMm", Number(event.target.value))} /></label><label><span>Alto (mm)</span><input type="number" min="120" max="450" value={draft.pageHeightMm} onChange={(event) => updateNumber("pageHeightMm", Number(event.target.value))} /></label><label><span>Margen superior</span><input type="number" min="5" max="60" value={draft.marginTopMm} onChange={(event) => updateNumber("marginTopMm", Number(event.target.value))} /></label><label><span>Margen derecho</span><input type="number" min="5" max="60" value={draft.marginRightMm} onChange={(event) => updateNumber("marginRightMm", Number(event.target.value))} /></label><label><span>Margen inferior</span><input type="number" min="5" max="60" value={draft.marginBottomMm} onChange={(event) => updateNumber("marginBottomMm", Number(event.target.value))} /></label><label><span>Margen izquierdo</span><input type="number" min="5" max="60" value={draft.marginLeftMm} onChange={(event) => updateNumber("marginLeftMm", Number(event.target.value))} /></label><label><span>Tipografía</span><select value={draft.fontFamily} onChange={(event) => setDraft({ ...draft, preset: "personalizada", fontFamily: event.target.value as ManuscriptLayout["fontFamily"] })}>{Object.keys(manuscriptFontStacks).map((font) => <option key={font}>{font}</option>)}</select></label><label><span>Cuerpo (pt)</span><input type="number" min="8" max="22" step=".5" value={draft.fontSizePt} onChange={(event) => updateNumber("fontSizePt", Number(event.target.value))} /></label><label><span>Interlineado</span><input type="number" min="1" max="2.2" step=".05" value={draft.lineHeight} onChange={(event) => updateNumber("lineHeight", Number(event.target.value))} /></label><label><span>Sangría (mm)</span><input type="number" min="0" max="18" step=".5" value={draft.paragraphIndentMm} onChange={(event) => updateNumber("paragraphIndentMm", Number(event.target.value))} /></label><label><span>Apertura de capítulo</span><select value={draft.chapterOpening} onChange={(event) => setDraft({ ...draft, preset: "personalizada", chapterOpening: event.target.value as ManuscriptLayout["chapterOpening"] })}><option>Página nueva</option><option>Página impar</option><option>Continuo</option></select></label><label><span>Separador de escena</span><input value={draft.sceneSeparator} onChange={(event) => setDraft({ ...draft, preset: "personalizada", sceneSeparator: event.target.value })} /></label><label><span>Encabezado</span><input value={draft.headerText} onChange={(event) => setDraft({ ...draft, preset: "personalizada", headerText: event.target.value })} placeholder="{título}" /></label><label><span>Pie de página</span><input value={draft.footerText} onChange={(event) => setDraft({ ...draft, preset: "personalizada", footerText: event.target.value })} placeholder="{página}" /></label></div>
        </div>
        <div className="layout-preview-stage"><div className="layout-page" style={{ width: draft.pageWidthMm * pageScale, height: draft.pageHeightMm * pageScale, padding: `${draft.marginTopMm * pageScale}px ${draft.marginRightMm * pageScale}px ${draft.marginBottomMm * pageScale}px ${draft.marginLeftMm * pageScale}px`, fontFamily: manuscriptFontStacks[draft.fontFamily], fontSize: `${Math.max(7, draft.fontSizePt * pageScale)}px`, lineHeight: draft.lineHeight }}><header>{draft.headerText.replace("{título}", archive.profile.storyTitle || archive.profile.archiveTitle)}</header><main><small>CAPÍTULO I</small><h3>{archive.writingChapters[0]?.title || "El umbral"}</h3><p style={{ textIndent: `${draft.paragraphIndentMm * pageScale}px` }}>{sampleText.slice(0, 620)}</p><div>{draft.sceneSeparator}</div><p style={{ textIndent: `${draft.paragraphIndentMm * pageScale}px` }}>{sampleText.slice(0, 360)}</p></main><footer>{draft.footerText.replace("{página}", "27")}</footer></div><span>{draft.pageWidthMm.toFixed(1)} × {draft.pageHeightMm.toFixed(1)} mm · {draft.fontSizePt} pt</span></div>
      </div>
    </section>
  );
}

function ReviewView({ archive, onOpenManuscript, onExportPdf, exportingPdf }: { archive: ArchiveState; onOpenManuscript: () => void; onExportPdf: () => void; exportingPdf: boolean }) {
  const text = orderedChapters(archive.writingChapters).flatMap((chapter) => orderedScenes(chapter).map((scene) => scene.content)).join("\n\n");
  const analysis = useMemo(() => analyzeWritingStyle(text, archive.profile.writingAnalysis ?? defaultWritingAnalysis), [text, archive.profile.writingAnalysis]);
  return (
    <section className="review-view">
      <AreaNavigation eyebrow="DIAGNÓSTICO LOCAL / CORRECCIÓN EN LÍNEA" title="Revisión" description="Métricas calculadas automáticamente desde el manuscrito; no hay matrices que rellenar a mano." active="diagnostico" onChange={() => undefined} tabs={[{ id: "diagnostico", label: "Diagnóstico", note: "Legibilidad y estilo" }]} />
      <div className="view-intro"><div><p>ANÁLISIS DEL PROYECTO COMPLETO</p><h2>Estado del manuscrito</h2><span>Las repeticiones, muletillas y la legibilidad se procesan localmente. La corrección ortográfica ampliada se ejecuta desde cada escena cuando hay conexión.</span></div><div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar diagnóstico PDF"}</button><button className="primary-action" onClick={onOpenManuscript}>Abrir manuscrito</button></div></div>
      <div className="review-stats"><article><span>Palabras</span><strong>{analysis.words.toLocaleString("es-CL")}</strong><small>{analysis.sentences.toLocaleString("es-CL")} oraciones</small></article><article><span>Legibilidad</span><strong>{analysis.readability.toFixed(0)}</strong><small>{analysis.readabilityLabel}</small></article><article><span>Promedio</span><strong>{analysis.averageSentenceLength.toFixed(1)}</strong><small>palabras por oración</small></article><article><span>Muletillas</span><strong>{analysis.fillerPercentage.toFixed(2)}%</strong><small>del texto analizado</small></article></div>
      {analysis.words === 0 ? <div className="review-empty"><h3>No hay texto para analizar</h3><p>Escribe o importa una escena y el diagnóstico aparecerá aquí automáticamente.</p><button className="primary-action" onClick={onOpenManuscript}>Comenzar a escribir</button></div> : <div className="review-panels"><article><div><span>REPETICIONES CERCANAS</span><h3>Palabras que reaparecen</h3></div>{analysis.repetitions.length ? <ol>{analysis.repetitions.slice(0, 18).map((item) => <li key={item.key}><span>{item.label}</span><b>{item.count}</b><small>{item.perThousand.toFixed(1)} / 1.000</small></li>)}</ol> : <p>No se detectaron repeticiones cercanas con la ventana actual.</p>}</article><article><div><span>MULETILLAS CONFIGURADAS</span><h3>Frecuencia de apoyo</h3></div>{analysis.fillers.length ? <ol>{analysis.fillers.slice(0, 18).map((item) => <li key={item.key}><span>{item.label}</span><b>{item.count}</b><small>{item.perThousand.toFixed(1)} / 1.000</small></li>)}</ol> : <p>No se detectaron muletillas configuradas.</p>}</article></div>}
    </section>
  );
}

function BoardView({
  archive,
  selected,
  selectedRelationships,
  networkMode,
  onNetworkMode,
  onSelect,
  onOpenDossier,
  onPhoto,
  theme,
  viewport,
  onSaveViewport,
  onEditRelationship,
  onNewRelationship,
  onNewCharacter,
  onExportPdf,
  exportingPdf,
  onMoveNode,
}: {
  archive: ArchiveState;
  selected: CharacterRecord | undefined;
  selectedRelationships: RelationshipRecord[];
  networkMode: NetworkMode;
  onNetworkMode: (mode: NetworkMode) => void;
  onSelect: (id: string) => void;
  onOpenDossier: (id: string) => void;
  onPhoto: (character: CharacterRecord) => void;
  theme: ArchiveTheme;
  viewport: BoardViewport;
  onSaveViewport: (viewport: BoardViewport) => void;
  onEditRelationship: (relationship: RelationshipRecord) => void;
  onNewRelationship: (sourceId?: string, targetId?: string) => void;
  onNewCharacter: () => void;
  onExportPdf: () => void;
  exportingPdf: boolean;
  onMoveNode: (id: string, x: number, y: number, save: boolean) => void;
}) {
  const boardRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<{ id: string; pointerId: number; moved: boolean; startX: number; startY: number } | null>(null);
  const pointersRef = useRef(new Map<number, { x: number; y: number }>());
  const gestureRef = useRef<
    | { mode: "pan"; pointerId: number; startX: number; startY: number; camera: BoardViewport }
    | { mode: "pinch"; distance: number; center: { x: number; y: number }; camera: BoardViewport }
    | null
  >(null);
  const wheelSaveRef = useRef<number | null>(null);
  const spaceHeldRef = useRef(false);
  const [linkMode, setLinkMode] = useState(false);
  const [linkSourceId, setLinkSourceId] = useState<string | null>(null);
  const [camera, setCamera] = useState<BoardViewport>(() => ({ scale: Math.max(.12, Math.min(3.5, viewport.scale || .2)), x: viewport.x || 0, y: viewport.y || 0, worldSpace: true }));
  const [viewportSize, setViewportSize] = useState({ width: 1000, height: 680 });
  const [spaceHeld, setSpaceHeld] = useState(false);

  useEffect(() => {
    const board = boardRef.current;
    if (!board) return;
    const updateSize = () => setViewportSize({ width: board.clientWidth, height: board.clientHeight });
    updateSize();
    const observer = new ResizeObserver(updateSize);
    observer.observe(board);
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    const keyDown = (event: KeyboardEvent) => {
      if (event.code !== "Space" || event.repeat || (event.target as HTMLElement | null)?.matches("input, textarea, select, [contenteditable=true]")) return;
      event.preventDefault();
      spaceHeldRef.current = true;
      setSpaceHeld(true);
    };
    const keyUp = (event: KeyboardEvent) => {
      if (event.code !== "Space") return;
      spaceHeldRef.current = false;
      setSpaceHeld(false);
    };
    window.addEventListener("keydown", keyDown);
    window.addEventListener("keyup", keyUp);
    return () => {
      window.removeEventListener("keydown", keyDown);
      window.removeEventListener("keyup", keyUp);
      if (wheelSaveRef.current) window.clearTimeout(wheelSaveRef.current);
    };
  }, []);

  if (!selected) {
    return (
      <>
        <section className="summary-strip" aria-label="Resumen del manuscrito">
          <article><span>Fichas registradas</span><strong>0</strong><small>personajes, grupos y entidades</small></article>
          <article><span>Conexiones</span><strong>0</strong><small>vínculos y relaciones editables</small></article>
          <article><span>Atlas del mundo</span><strong>{archive.world.length}</strong><small>países, lugares y cosmología</small></article>
          <article className="alert-card"><span>Sistemas de magia</span><strong>{archive.magicSystems.length}</strong><small>{archive.profile.status} · {archive.profile.genre}</small></article>
        </section>
        <section className="blank-board">
          <span className="blank-board-mark">01</span>
          <p>TABLERO TODAVÍA VACÍO</p>
          <h2>Comienza por una ficha</h2>
          <span>Crea una ficha para comenzar el reparto. Después podrás colocarla en el tablero, añadir su fotografía y conectarla con otras.</span>
          <div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar tablero PDF"}</button><button className="primary-action" onClick={onNewCharacter}>+ Crear primera ficha</button></div>
        </section>
      </>
    );
  }

  const boardCharacters = archive.characters.filter((character) => character.board.visible);
  const focusIds = new Set([selected.id, ...selectedRelationships.flatMap((rel) => [rel.source, rel.target])]);
  const focusCharacters = archive.characters.filter((character) => focusIds.has(character.id)).slice(0, 14);
  const displayCharacters = networkMode === "all" ? boardCharacters : focusCharacters;

  const positionFor = (character: CharacterRecord, index: number) => {
    if (networkMode === "all") {
      const charactersAtSamePosition = boardCharacters.filter((item) => item.board.x === character.board.x && item.board.y === character.board.y).length;
      if (charactersAtSamePosition > 1) return { ...character.board, ...boardPositionForIndex(index) };
      return {
        ...character.board,
        x: Math.max(120, Math.min(BOARD_WORLD_WIDTH - 120, character.board.x)),
        y: Math.max(100, Math.min(BOARD_WORLD_HEIGHT - 100, character.board.y)),
      };
    }
    const center = selected.board;
    if (character.id === selected.id) return { x: center.x, y: center.y };
    const neighborIndex = focusCharacters.filter((item) => item.id !== selected.id).findIndex((item) => item.id === character.id);
    const count = Math.max(1, focusCharacters.length - 1);
    const angle = (Math.PI * 2 * neighborIndex) / count - Math.PI / 2;
    return {
      x: Math.max(160, Math.min(BOARD_WORLD_WIDTH - 160, center.x + Math.cos(angle) * 690)),
      y: Math.max(140, Math.min(BOARD_WORLD_HEIGHT - 140, center.y + Math.sin(angle) * 540)),
      index,
    };
  };

  const positioned = displayCharacters.map((character, index) => ({ character, position: positionFor(character, index) }));
  const idSet = new Set(displayCharacters.map((character) => character.id));
  const worldBounds: CanvasBounds = positioned.length > 0 ? positioned.reduce<CanvasBounds>((bounds, item) => ({
    minX: Math.min(bounds.minX, item.position.x - 190),
    minY: Math.min(bounds.minY, item.position.y - 150),
    maxX: Math.max(bounds.maxX, item.position.x + 190),
    maxY: Math.max(bounds.maxY, item.position.y + 150),
  }), { minX: Number.POSITIVE_INFINITY, minY: Number.POSITIVE_INFINITY, maxX: Number.NEGATIVE_INFINITY, maxY: Number.NEGATIVE_INFINITY }) : { minX: 0, minY: 0, maxX: BOARD_WORLD_WIDTH, maxY: BOARD_WORLD_HEIGHT };
  const cameraBounds = visibleWorldBounds(camera, viewportSize.width, viewportSize.height, 260);
  const actualCameraBounds = visibleWorldBounds(camera, viewportSize.width, viewportSize.height);
  const visiblePositioned = positioned.filter((item) => boundsOverlap(cameraBounds, pointBounds(item.position, 210, 170)));
  const visibleCharacterIds = new Set(visiblePositioned.map((item) => item.character.id));
  const visibleRelationships = archive.relationships.filter((rel) => {
    if (!idSet.has(rel.source) || !idSet.has(rel.target)) return false;
    if (visibleCharacterIds.has(rel.source) || visibleCharacterIds.has(rel.target)) return true;
    const a = positioned.find((item) => item.character.id === rel.source)?.position;
    const b = positioned.find((item) => item.character.id === rel.target)?.position;
    if (!a || !b) return false;
    return boundsOverlap(cameraBounds, { minX: Math.min(a.x, b.x), minY: Math.min(a.y, b.y), maxX: Math.max(a.x, b.x), maxY: Math.max(a.y, b.y) });
  });
  const linkSource = archive.characters.find((character) => character.id === linkSourceId);

  const resetLinkMode = () => {
    setLinkMode(false);
    setLinkSourceId(null);
  };

  const handleCardClick = (id: string) => {
    onSelect(id);
    if (!linkMode) return;
    if (!linkSourceId) {
      setLinkSourceId(id);
      return;
    }
    if (linkSourceId === id) {
      setLinkSourceId(null);
      return;
    }
    onNewRelationship(linkSourceId, id);
    resetLinkMode();
  };

  const handleLinkAction = (id: string) => {
    onSelect(id);
    if (linkMode && linkSourceId && linkSourceId !== id) {
      onNewRelationship(linkSourceId, id);
      resetLinkMode();
      return;
    }
    setLinkMode(true);
    setLinkSourceId(id);
  };

  const saveCameraSoon = (next: BoardViewport) => {
    if (wheelSaveRef.current) window.clearTimeout(wheelSaveRef.current);
    wheelSaveRef.current = window.setTimeout(() => onSaveViewport(next), 420);
  };

  const zoomAt = (nextScale: number, clientX?: number, clientY?: number, save = true) => {
    const rect = boardRef.current?.getBoundingClientRect();
    setCamera((current) => {
      const anchorX = rect && clientX !== undefined ? clientX - rect.left : (rect?.width ?? 0) / 2;
      const anchorY = rect && clientY !== undefined ? clientY - rect.top : (rect?.height ?? 0) / 2;
      const next = { ...zoomCameraAt(current, nextScale, { x: anchorX, y: anchorY }, .12, 3.5), worldSpace: true as const };
      if (save) saveCameraSoon(next);
      return next;
    });
  };

  const resetCamera = () => {
    const center = selected?.board ?? { x: BOARD_WORLD_WIDTH / 2, y: BOARD_WORLD_HEIGHT / 2 };
    const next = { scale: 1, x: viewportSize.width / 2 - center.x, y: viewportSize.height / 2 - center.y, worldSpace: true as const };
    setCamera(next);
    onSaveViewport(next);
  };

  const fitCamera = () => {
    const next = { ...fitCameraToBounds(worldBounds, viewportSize.width, viewportSize.height, .12, 1.4, 72), worldSpace: true as const };
    setCamera(next);
    onSaveViewport(next);
  };

  const centerSelected = () => {
    const next = { ...camera, x: viewportSize.width / 2 - selected.board.x * camera.scale, y: viewportSize.height / 2 - selected.board.y * camera.scale, worldSpace: true as const };
    setCamera(next);
    onSaveViewport(next);
  };

  const boardPoint = (clientX: number, clientY: number) => {
    const rect = boardRef.current?.getBoundingClientRect();
    if (!rect) return { x: BOARD_WORLD_WIDTH / 2, y: BOARD_WORLD_HEIGHT / 2 };
    const point = screenToWorld(camera, { x: clientX - rect.left, y: clientY - rect.top });
    return { x: Math.max(100, Math.min(BOARD_WORLD_WIDTH - 100, point.x)), y: Math.max(90, Math.min(BOARD_WORLD_HEIGHT - 90, point.y)) };
  };

  const handleBoardPointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    const onCard = Boolean((event.target as HTMLElement).closest(".evidence-card, .link-guide, .board-minimap"));
    const forcedPan = event.button === 1 || spaceHeldRef.current;
    if (linkMode || (onCard && !forcedPan) || (event.button !== 0 && event.button !== 1)) return;
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    const rect = event.currentTarget.getBoundingClientRect();
    pointersRef.current.set(event.pointerId, { x: event.clientX - rect.left, y: event.clientY - rect.top });
    const points = [...pointersRef.current.values()];
    if (points.length >= 2) {
      const [first, second] = points;
      gestureRef.current = {
        mode: "pinch",
        distance: Math.max(1, Math.hypot(second.x - first.x, second.y - first.y)),
        center: { x: (first.x + second.x) / 2, y: (first.y + second.y) / 2 },
        camera,
      };
    } else {
      gestureRef.current = { mode: "pan", pointerId: event.pointerId, startX: event.clientX, startY: event.clientY, camera };
    }
    event.currentTarget.classList.add("is-panning");
  };

  const handleBoardPointerMove = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!pointersRef.current.has(event.pointerId)) return;
    const rect = event.currentTarget.getBoundingClientRect();
    pointersRef.current.set(event.pointerId, { x: event.clientX - rect.left, y: event.clientY - rect.top });
    const gesture = gestureRef.current;
    const points = [...pointersRef.current.values()];
    if (points.length >= 2) {
      const [first, second] = points;
      const center = { x: (first.x + second.x) / 2, y: (first.y + second.y) / 2 };
      const distance = Math.max(1, Math.hypot(second.x - first.x, second.y - first.y));
      const base = gesture?.mode === "pinch" ? gesture : { mode: "pinch" as const, distance, center, camera };
      const zoomed = zoomCameraAt(base.camera, base.camera.scale * distance / base.distance, base.center, .12, 3.5);
      setCamera({ ...zoomed, x: zoomed.x + center.x - base.center.x, y: zoomed.y + center.y - base.center.y, worldSpace: true });
      return;
    }
    if (gesture?.mode === "pan" && gesture.pointerId === event.pointerId) {
      setCamera({ ...gesture.camera, x: gesture.camera.x + event.clientX - gesture.startX, y: gesture.camera.y + event.clientY - gesture.startY, worldSpace: true });
    }
  };

  const handleBoardPointerUp = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!pointersRef.current.has(event.pointerId)) return;
    pointersRef.current.delete(event.pointerId);
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    if (pointersRef.current.size === 0) {
      gestureRef.current = null;
      event.currentTarget.classList.remove("is-panning");
      setCamera((current) => { onSaveViewport({ ...current, worldSpace: true }); return current; });
    } else {
      const remaining = [...pointersRef.current.entries()][0];
      const rect = event.currentTarget.getBoundingClientRect();
      setCamera((current) => {
        gestureRef.current = { mode: "pan", pointerId: remaining[0], startX: rect.left + remaining[1].x, startY: rect.top + remaining[1].y, camera: current };
        return current;
      });
    }
  };

  const handlePointerMove = (event: React.PointerEvent<HTMLButtonElement>, id: string) => {
    if (networkMode !== "all" || dragRef.current?.id !== id || !boardRef.current) return;
    if (Math.hypot(event.clientX - dragRef.current.startX, event.clientY - dragRef.current.startY) < 3) return;
    const { x, y } = boardPoint(event.clientX, event.clientY);
    dragRef.current.moved = true;
    onMoveNode(id, x, y, false);
  };

  const handlePointerUp = (event: React.PointerEvent<HTMLButtonElement>, id: string) => {
    if (linkMode) return;
    const moved = dragRef.current?.id === id && dragRef.current.moved;
    dragRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    if (moved) {
      const character = archiveRefCharacter(archive, id);
      const point = boardRef.current ? boardPoint(event.clientX, event.clientY) : { x: character.board.x, y: character.board.y };
      const x = point.x;
      const y = point.y;
      onMoveNode(id, x, y, true);
    }
  };

  return (
    <>
      <section className="summary-strip" aria-label="Resumen del proyecto">
        <article><span>Fichas registradas</span><strong>{archive.characters.length}</strong><small>personajes, grupos y entidades</small></article>
        <article><span>Conexiones</span><strong>{archive.relationships.length}</strong><small>vínculos y relaciones editables</small></article>
        <article><span>Atlas del mundo</span><strong>{archive.world.length}</strong><small>países, lugares y cosmología</small></article>
        <article className="alert-card"><span>Sistemas de magia</span><strong>{archive.magicSystems.length}</strong><small>{archive.profile.status} · {archive.profile.genre}</small></article>
      </section>

      <section className="board-section">
        <div className="section-heading">
          <div><p>{theme === "grim" ? "MAPA DE VÍNCULOS · HILO, CLAVOS Y NOTAS" : theme === "classic" ? "CARTOGRAFÍA DE ALIANZAS · CRÓNICA DE LOS REINOS" : theme === "kawaii" ? "TABLERO DE VÍNCULOS · CINTAS, ESTRELLAS Y SECRETOS" : "TABLERO DE VÍNCULOS · ARRASTRA LAS FICHAS"}</p><h2>Mapa de conexiones</h2><div className="relationship-legend">{relationshipTypes.map((type) => <span key={type}><i style={{ "--relationship-color": relationshipVisuals[type].color, "--relationship-dash": relationshipVisuals[type].dash ? "dashed" : "solid" } as React.CSSProperties} />{type}</span>)}</div></div>
          <div className="board-controls">
            <button className={networkMode === "all" ? "active" : ""} onClick={() => { onNetworkMode("all"); resetLinkMode(); }}>Vista general</button>
            <button className={networkMode === "focus" ? "active" : ""} onClick={() => { onNetworkMode("focus"); resetLinkMode(); }}>Red de {selected.name.split(" ")[0]}</button>
            <button className={linkMode ? "link-active" : ""} onClick={() => { setLinkMode((current) => !current); setLinkSourceId(null); }}>{linkMode ? "Cancelar enlace" : "Enlazar fichas"}</button>
            <button onClick={() => onNewRelationship()}>+ Formulario</button>
            <button onClick={() => zoomAt(camera.scale - .15)} aria-label="Alejar tablero">−</button>
            <span className="board-zoom-readout">{Math.round(camera.scale * 100)}%</span>
            <button onClick={() => zoomAt(camera.scale + .15)} aria-label="Acercar tablero">+</button>
            <button onClick={resetCamera}>100%</button>
            <button onClick={centerSelected}>Centrar ficha</button>
            <button onClick={fitCamera}>Ajustar todo</button>
            <button onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button>
          </div>
        </div>

        <div className={`murder-board board-viewport ${linkMode ? "linking" : ""} ${spaceHeld ? "space-ready" : ""} ${camera.scale < .42 ? "zoom-far" : camera.scale < .7 ? "zoom-mid" : "zoom-near"}`} ref={boardRef} onWheel={(event) => { event.preventDefault(); zoomAt(camera.scale * Math.exp(-event.deltaY * .00125), event.clientX, event.clientY); }} onPointerDown={handleBoardPointerDown} onPointerMove={handleBoardPointerMove} onPointerUp={handleBoardPointerUp} onPointerCancel={handleBoardPointerUp}>
          {linkMode && (
            <div className="link-guide" role="status">
              <b>{linkSource ? "02" : "01"}</b>
              <span>{linkSource ? `Ahora elige la ficha que deseas conectar con ${linkSource.name}.` : "Elige la primera ficha del enlace."}</span>
            </div>
          )}
          <div className="board-world" style={{ width: BOARD_WORLD_WIDTH, height: BOARD_WORLD_HEIGHT, transform: `translate(${camera.x}px, ${camera.y}px) scale(${camera.scale})` }}>
          <svg className="strings" viewBox={`0 0 ${BOARD_WORLD_WIDTH} ${BOARD_WORLD_HEIGHT}`} aria-label="Relaciones del tablero">
            {visibleRelationships.map((rel) => {
              const a = positioned.find((item) => item.character.id === rel.source)?.position;
              const b = positioned.find((item) => item.character.id === rel.target)?.position;
              if (!a || !b) return null;
              const visual = relationshipVisuals[rel.type];
              const dash = visual.dash ?? (rel.certainty === "Hipótesis" ? "8 7" : undefined);
              const relationClass = `type-${rel.type.toLowerCase().replace(/\s+/g, "-")}`;
              return <g className="relationship-line" key={rel.id} role="button" tabIndex={0} aria-label={`Editar relación ${rel.label}`} onClick={(event) => { event.stopPropagation(); onEditRelationship(rel); }} onKeyDown={(event) => { if (event.key === "Enter" || event.key === " ") onEditRelationship(rel); }}><line className="relationship-hit" x1={a.x} y1={a.y} x2={b.x} y2={b.y} /><line x1={a.x} y1={a.y} x2={b.x} y2={b.y} className={relationClass} vectorEffect="non-scaling-stroke" style={{ stroke: visual.color, strokeDasharray: dash, strokeWidth: 1.8 + rel.strength * 0.75, opacity: 0.55 + rel.strength * 0.15 }} /></g>;
            })}
          </svg>
          {visiblePositioned.map(({ character, position }) => (
            <article
              key={character.id}
              className={`evidence-card tone-${character.color} ${selected.id === character.id ? "selected" : ""} ${linkSourceId === character.id ? "link-source" : ""}`}
              style={{ left: position.x, top: position.y, touchAction: "none" }}
            >
              <button className="pin-action" onClick={() => handleLinkAction(character.id)} aria-label={`Crear enlace desde ${character.name}`} title={`Crear enlace desde ${character.name}`}><i /></button>
              <button
                className="evidence-card-main"
                onPointerDown={(event) => {
                  if (linkMode || event.button === 1 || spaceHeldRef.current) return;
                  event.preventDefault();
                  event.currentTarget.setPointerCapture(event.pointerId);
                  dragRef.current = { id: character.id, pointerId: event.pointerId, moved: false, startX: event.clientX, startY: event.clientY };
                }}
                onPointerMove={(event) => handlePointerMove(event, character.id)}
                onPointerUp={(event) => handlePointerUp(event, character.id)}
                onClick={() => handleCardClick(character.id)}
                onDoubleClick={() => onOpenDossier(character.id)}
                aria-label={linkMode ? `Usar ${character.name} como destino del enlace` : `Seleccionar ${character.name}`}
              >
                <span className="board-photo">{character.imageUrl ? <img src={character.imageUrl} alt="" /> : <em>{initials(character.name)}</em>}</span>
                <span className="evidence-copy"><b>{character.name}</b><small>{character.role}</small></span>
              </button>
              <div className="evidence-actions">
                <button onClick={() => handleLinkAction(character.id)}>{linkMode && linkSourceId !== character.id ? "Unir aquí" : linkSourceId === character.id ? "Origen" : "Enlace"}</button>
                <button onClick={() => onPhoto(character)}>{character.imageUrl ? "Cambiar foto" : "+ Foto"}</button>
              </div>
            </article>
          ))}
          </div>
          <button className="board-minimap" type="button" aria-label="Minimapa: pulsa para mover la cámara" onPointerDown={(event) => event.stopPropagation()} onClick={(event) => {
            const rect = event.currentTarget.getBoundingClientRect();
            const worldX = Math.max(0, Math.min(BOARD_WORLD_WIDTH, (event.clientX - rect.left) / rect.width * BOARD_WORLD_WIDTH));
            const worldY = Math.max(0, Math.min(BOARD_WORLD_HEIGHT, (event.clientY - rect.top) / rect.height * BOARD_WORLD_HEIGHT));
            const next = { ...camera, x: viewportSize.width / 2 - worldX * camera.scale, y: viewportSize.height / 2 - worldY * camera.scale, worldSpace: true as const };
            setCamera(next);
            onSaveViewport(next);
          }}>
            <span className="minimap-world">{positioned.map(({ character, position }) => <i key={character.id} className={selected.id === character.id ? "selected" : ""} style={{ left: `${position.x / BOARD_WORLD_WIDTH * 100}%`, top: `${position.y / BOARD_WORLD_HEIGHT * 100}%` }} />)}<b style={{ left: `${Math.max(0, actualCameraBounds.minX) / BOARD_WORLD_WIDTH * 100}%`, top: `${Math.max(0, actualCameraBounds.minY) / BOARD_WORLD_HEIGHT * 100}%`, width: `${Math.max(3, (Math.min(BOARD_WORLD_WIDTH, actualCameraBounds.maxX) - Math.max(0, actualCameraBounds.minX)) / BOARD_WORLD_WIDTH * 100)}%`, height: `${Math.max(4, (Math.min(BOARD_WORLD_HEIGHT, actualCameraBounds.maxY) - Math.max(0, actualCameraBounds.minY)) / BOARD_WORLD_HEIGHT * 100)}%` }} /></span>
          </button>
          <div className="board-navigation-hint">Rueda: zoom · arrastra el fondo: mover · botón central o espacio: desplazar · pellizco: zoom táctil</div>
        </div>

        <aside className="quick-file">
          <div className={`portrait tone-${selected.color}`}>{selected.imageUrl ? <img src={selected.imageUrl} alt={`Retrato de ${selected.name}`} /> : initials(selected.name)}</div>
          <p>FICHA SELECCIONADA</p>
          <h3>{selected.name}</h3>
          <span>{selected.role}</span>
          <div className="file-rule" />
          <dl>
            <div><dt>Estado</dt><dd>{selected.status}</dd></div>
            <div><dt>Origen</dt><dd>{selected.origin}</dd></div>
            <div><dt>Conexiones</dt><dd>{selectedRelationships.length}</dd></div>
            <div><dt>Menciones</dt><dd>{selected.presence.reduce((sum, value) => sum + value, 0)}</dd></div>
          </dl>
          <p className="quick-summary">{selected.summary}</p>
          <button className="file-action" onClick={() => onOpenDossier(selected.id)}>Abrir antecedentes <span>↗</span></button>
          <div className="relation-mini-list">
            {selectedRelationships.slice(0, 5).map((rel) => {
              const otherId = rel.source === selected.id ? rel.target : rel.source;
              const other = archive.characters.find((item) => item.id === otherId);
              return <button key={rel.id} onClick={() => onEditRelationship(rel)}><i className={rel.certainty === "Hipótesis" ? "theory" : "fact"} /><span>{other?.name}</span><small>{rel.type} · {rel.label}</small></button>;
            })}
          </div>
        </aside>
      </section>
    </>
  );
}

function archiveRefCharacter(archive: ArchiveState, id: string) {
  return archive.characters.find((character) => character.id === id) ?? archive.characters[0];
}

function CharactersView({ archive, search, categoryFilter, statusFilter, onSearch, onCategory, onStatus, onOpen, onNew, onExportPdf, exportingPdf }: {
  archive: ArchiveState;
  search: string;
  categoryFilter: string;
  statusFilter: string;
  onSearch: (value: string) => void;
  onCategory: (value: string) => void;
  onStatus: (value: string) => void;
  onOpen: (id: string) => void;
  onNew: () => void;
  onExportPdf: () => void;
  exportingPdf: boolean;
}) {
  const query = search.trim().toLocaleLowerCase("es");
  const filtered = archive.characters.filter((character) => {
    const haystack = [character.name, character.role, character.origin, character.affiliation, ...character.aliases].join(" ").toLocaleLowerCase("es");
    return (!query || haystack.includes(query)) && (categoryFilter === "Todos" || character.category === categoryFilter) && (statusFilter === "Todos" || character.status === statusFilter);
  });

  return (
    <section className="characters-view">
      <div className="view-intro">
        <div><p>REPARTO COMPLETO / {filtered.length} RESULTADOS</p><h2>Fichas y antecedentes</h2><span>Cada ficha reúne identidad, pasado, apariencia, fuentes y presencia por capítulo.</span></div>
        <div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar fichas PDF"}</button><button className="primary-action" onClick={onNew}>+ Nueva ficha</button></div>
      </div>
      <div className="filter-bar">
        <label className="search-box"><span>⌕</span><input value={search} onChange={(event) => onSearch(event.target.value)} placeholder="Buscar nombre, origen, afiliación…" /></label>
        <select value={categoryFilter} onChange={(event) => onCategory(event.target.value)} aria-label="Filtrar por categoría"><option>Todos</option>{categories.map((item) => <option key={item}>{item}</option>)}</select>
        <select value={statusFilter} onChange={(event) => onStatus(event.target.value)} aria-label="Filtrar por estado"><option>Todos</option>{statuses.map((item) => <option key={item}>{item}</option>)}</select>
      </div>
      <div className="character-grid">
        {filtered.map((character) => (
          <button className="character-card" key={character.id} onClick={() => onOpen(character.id)}>
            <div className={`card-portrait tone-${character.color}`}>{character.imageUrl ? <img src={character.imageUrl} alt={`Retrato de ${character.name}`} /> : <span>{initials(character.name)}</span>}<i className={`status-dot status-${character.status.toLowerCase()}`} /></div>
            <div className="card-copy"><small>{character.category} · {character.status}</small><h3>{character.name}</h3><p>{character.role}</p><div><span>{character.origin}</span><b>{character.presence.reduce((sum, value) => sum + value, 0)} menciones</b></div></div>
          </button>
        ))}
      </div>
    </section>
  );
}

function TheoriesView({ archive, onEdit, onNew, onOpen, onExportPdf, exportingPdf }: { archive: ArchiveState; onEdit: (theory: TheoryRecord) => void; onNew: () => void; onOpen: (id: string) => void; onExportPdf: () => void; exportingPdf: boolean }) {
  return (
    <section className="theories-view">
      <div className="view-intro">
        <div><p>LORE / RELACIONES / DESARROLLO</p><h2>Hilos del mundo</h2><span>Reúne ideas, relaciones y explicaciones que conectan personajes y elementos del mundo sin convertirlas automáticamente en hechos.</span></div>
        <div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button><button className="primary-action" onClick={onNew}>+ Nuevo hilo</button></div>
      </div>
      <div className="theory-grid">
        {archive.theories.map((theory, index) => (
          <article className="theory-card" key={theory.id}>
            <div className="theory-top"><span>HILO {String(index + 1).padStart(2, "0")}</span><button onClick={() => onEdit({ ...theory })}>Editar</button></div>
            <div className="confidence"><div style={{ "--confidence": `${theory.confidence * 3.6}deg` } as React.CSSProperties}><strong>{theory.confidence}</strong><span>%</span></div><p><b>{theory.status}</b><small>confianza de trabajo</small></p></div>
            <h3>{theory.title}</h3>
            <p className="thesis">{theory.thesis}</p>
            <div className="theory-evidence"><span>REFERENCIAS</span>{theory.evidence.map((item, evidenceIndex) => <p key={evidenceIndex}><b>{item.chapter}</b>{item.text}</p>)}</div>
            <div className="counterpoint"><span>CONTRAPUNTO</span><p>{theory.counterpoint}</p></div>
            <div className="linked-people">{theory.characterIds.map((id) => { const character = archive.characters.find((item) => item.id === id); return character ? <button key={id} onClick={() => onOpen(id)}>{initials(character.name)}<span>{character.name}</span></button> : null; })}</div>
          </article>
        ))}
      </div>
    </section>
  );
}

function TimelineView({ archive, onOpen, onEdit, onNew, onExportPdf, exportingPdf }: { archive: ArchiveState; onOpen: (id: string) => void; onEdit: (event: TimelineEvent) => void; onNew: () => void; onExportPdf: () => void; exportingPdf: boolean }) {
  return (
    <section className="timeline-view">
      <div className="view-intro"><div><p>ORDEN DE LOS HECHOS / PRESENTE Y PASADO</p><h2>Cronología del mundo</h2><span>Edita fechas, capítulos, intensidad y participantes; también puedes crear o eliminar sucesos.</span></div><div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button><button className="primary-action" onClick={onNew}>+ Nuevo suceso</button></div></div>
      <div className="timeline">
        {archive.timeline.map((event) => (
          <article key={event.id}>
            <div className="timeline-marker"><i style={{ "--intensity": event.intensity / 5 } as React.CSSProperties} /><span>{event.chapter}</span></div>
            <div className="timeline-copy"><div className="record-heading"><small>{event.when}</small><button className="record-edit" onClick={() => onEdit({ ...event })}>Editar</button></div><h3>{event.title}</h3><p>{event.summary}</p><div>{event.characterIds.map((id) => { const character = archive.characters.find((item) => item.id === id); return character ? <button key={id} onClick={() => onOpen(id)}>{character.name}</button> : null; })}</div></div>
          </article>
        ))}
      </div>
    </section>
  );
}

function ImportedTextsPanel({ texts, sectionLabel, onEdit, onDelete }: { texts: ImportedTextRecord[]; sectionLabel: string; onEdit: (text: ImportedTextRecord) => void; onDelete: (id: string) => void }) {
  if (texts.length === 0) return null;
  return (
    <section className="imported-texts-panel" aria-label={`Textos de ${sectionLabel}`}>
      <div className="imported-texts-heading"><div><span>CONTENIDO EDITABLE DEL APARTADO</span><h3>Textos de {sectionLabel}</h3></div><small>{texts.length} {texts.length === 1 ? "texto" : "textos"}</small></div>
      <div className="imported-texts-grid">
        {texts.map((text) => (
          <article key={text.id}>
            <div><span>TEXTO INTERNO · NO ES UN ADJUNTO</span><h4>{text.title}</h4><small>{wordCount(text.content).toLocaleString("es-CL")} palabras · origen: {text.sourceName}</small></div>
            <p>{text.content.slice(0, 360)}{text.content.length > 360 ? "…" : ""}</p>
            <details><summary>Leer contenido completo</summary><div>{text.content}</div></details>
            <footer><button type="button" onClick={() => onEdit({ ...text })}>Editar texto</button><button type="button" onClick={() => onDelete(text.id)}>Eliminar</button></footer>
          </article>
        ))}
      </div>
    </section>
  );
}

function WorldView({ projectId, records, texts, legacyCount, convertingLegacy, onConvertLegacy, onEdit, onNew, onFiles, onNewText, onEditText, onDeleteText, onExportPdf, exportingPdf }: { projectId: string; records: WorldRecord[]; texts: ImportedTextRecord[]; legacyCount: number; convertingLegacy: boolean; onConvertLegacy: () => void; onEdit: (record: WorldRecord) => void; onNew: () => void; onFiles: (record: WorldRecord) => void; onNewText: () => void; onEditText: (text: ImportedTextRecord) => void; onDeleteText: (id: string) => void; onExportPdf: () => void; exportingPdf: boolean }) {
  const [search, setSearch] = useState("");
  const [kind, setKind] = useState("Todos");
  const query = search.trim().toLocaleLowerCase("es");
  const visible = records.filter((record) => {
    const haystack = [record.name, ...record.aliases, record.summary, record.geography, record.culture, record.currency, ...record.tags].join(" ").toLocaleLowerCase("es");
    return (!query || haystack.includes(query)) && (kind === "Todos" || record.kind === kind);
  });

  return (
    <section className="world-view codex-view">
      <div className="view-intro">
        <div><p>ATLAS / POLÍTICA / CULTURA / ECONOMÍA</p><h2>Enciclopedia del mundo</h2><span>Países, lugares, monedas, lenguas, religiones, conflictos y secretos de la cosmología en fichas reutilizables.</span></div>
        <div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button>{legacyCount > 0 && <button className="secondary-action legacy-convert-action" onClick={onConvertLegacy} disabled={convertingLegacy}>{convertingLegacy ? "Convirtiendo…" : `Convertir ${legacyCount} adjunto${legacyCount === 1 ? "" : "s"}`}</button>}<button className="secondary-action import-text-action" onClick={onNewText}>Importar Word o texto</button><button className="primary-action" onClick={onNew}>+ Nueva entrada</button></div>
      </div>
      <ImportedTextsPanel texts={texts} sectionLabel="Mundo" onEdit={onEditText} onDelete={onDeleteText} />
      <div className="filter-bar">
        <label className="search-box"><span>⌕</span><input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Buscar reino, lugar, moneda, cultura…" /></label>
        <select value={kind} onChange={(event) => setKind(event.target.value)} aria-label="Filtrar tipo de entrada"><option>Todos</option>{worldKinds.map((item) => <option key={item}>{item}</option>)}</select>
      </div>
      <div className="codex-grid">
        {visible.map((record) => {
          const fields = [
            ["Geografía", record.geography], ["Gobierno", record.government], ["Pueblos", record.peoples], ["Cultura", record.culture],
            ["Economía", record.economy], ["Moneda", record.currency], ["Lenguas", record.languages], ["Religiones", record.religions],
            ["Fuerzas armadas", record.military], ["Historia", record.history], ["Relaciones", record.relations], ["Lugares", record.locations],
            ["Conflictos", record.conflicts], ["Notas", record.notes],
          ].filter(([, value]) => Boolean(value));
          return (
            <article className={`codex-card ${record.tags.includes("spoiler") ? "secret" : ""}`} key={record.id}>
              <div className="codex-card-top"><span>{record.kind}</span><div className="record-actions"><button className="attachment-card-action" onClick={() => onFiles(record)}>Imágenes · {record.attachments?.filter((item) => item.kind === "image").length ?? 0}</button><button className="record-edit" onClick={() => onEdit({ ...record, aliases: [...record.aliases], tags: [...record.tags], attachments: [...(record.attachments ?? [])] })}>Editar ficha</button></div></div>
              <h3>{record.name}</h3>
              {record.aliases.length > 0 && <p className="aliases">{record.aliases.join(" · ")}</p>}
              <p className="codex-summary">{record.summary || "Sin resumen todavía."}</p>
              <dl className="codex-keyfacts"><div><dt>Moneda</dt><dd>{record.currency || "Sin definir"}</dd></div><div><dt>Gobierno</dt><dd>{record.government || "Sin definir"}</dd></div></dl>
              <details><summary>Ver ficha completa <span>{fields.length} apartados</span></summary><dl className="codex-details">{fields.map(([label, value]) => <div key={label}><dt>{label}</dt><dd>{value}</dd></div>)}</dl></details>
              <div className="tag-list">{record.tags.map((tag) => <span key={tag}>{tag}</span>)}</div>
              <AttachmentStrip projectId={projectId} scope="world" recordId={record.id} attachments={record.attachments ?? []} />
            </article>
          );
        })}
      </div>
    </section>
  );
}

function MagicView({ projectId, systems, texts, legacyCount, convertingLegacy, onConvertLegacy, onEdit, onNew, onFiles, onNewText, onEditText, onDeleteText, onExportPdf, exportingPdf }: { projectId: string; systems: MagicSystemRecord[]; texts: ImportedTextRecord[]; legacyCount: number; convertingLegacy: boolean; onConvertLegacy: () => void; onEdit: (system: MagicSystemRecord) => void; onNew: () => void; onFiles: (system: MagicSystemRecord) => void; onNewText: () => void; onEditText: (text: ImportedTextRecord) => void; onDeleteText: (id: string) => void; onExportPdf: () => void; exportingPdf: boolean }) {
  const [search, setSearch] = useState("");
  const [status, setStatus] = useState("Todos");
  const query = search.trim().toLocaleLowerCase("es");
  const visible = systems.filter((system) => {
    const haystack = [system.name, system.category, system.source, system.principle, system.cost, system.limits, ...system.tags].join(" ").toLocaleLowerCase("es");
    return (!query || haystack.includes(query)) && (status === "Todos" || system.status === status);
  });

  return (
    <section className="magic-view codex-view">
      <div className="view-intro">
        <div><p>REGLAS / COSTOS / LÍMITES / USUARIOS</p><h2>Sistemas de magia</h2><span>Separa lo canónico, lo secreto y lo que sigue en desarrollo. Cada sistema conserva sus fuentes y vacíos explícitos.</span></div>
        <div className="view-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "Exportar PDF"}</button>{legacyCount > 0 && <button className="secondary-action legacy-convert-action" onClick={onConvertLegacy} disabled={convertingLegacy}>{convertingLegacy ? "Convirtiendo…" : `Convertir ${legacyCount} adjunto${legacyCount === 1 ? "" : "s"}`}</button>}<button className="secondary-action import-text-action" onClick={onNewText}>Importar Word o texto</button><button className="primary-action" onClick={onNew}>+ Nuevo sistema</button></div>
      </div>
      <ImportedTextsPanel texts={texts} sectionLabel="Magia" onEdit={onEditText} onDelete={onDeleteText} />
      <div className="filter-bar">
        <label className="search-box"><span>⌕</span><input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Buscar sistema, costo, órgano, regla…" /></label>
        <select value={status} onChange={(event) => setStatus(event.target.value)} aria-label="Filtrar estado"><option>Todos</option>{magicStatuses.map((item) => <option key={item}>{item}</option>)}</select>
      </div>
      <div className="codex-grid magic-grid">
        {visible.map((system) => {
          const fields = [
            ["Principio", system.principle], ["Acceso", system.access], ["Costo", system.cost], ["Límites", system.limits],
            ["Manifestaciones", system.manifestations], ["Materiales", system.materials], ["Instituciones", system.institutions], ["Usuarios", system.users],
            ["Riesgos", system.risks], ["Historia", system.history], ["Notas", system.notes],
          ].filter(([, value]) => Boolean(value));
          return (
            <article className={`codex-card magic-card status-${system.status.toLocaleLowerCase("es").replace(/\s+/g, "-")}`} key={system.id}>
              <div className="codex-card-top"><span>{system.status}</span><div className="record-actions"><button className="attachment-card-action" onClick={() => onFiles(system)}>Imágenes · {system.attachments?.filter((item) => item.kind === "image").length ?? 0}</button><button className="record-edit" onClick={() => onEdit({ ...system, evidence: system.evidence.map((item) => ({ ...item })), tags: [...system.tags], attachments: [...(system.attachments ?? [])] })}>Editar sistema</button></div></div>
              <p className="magic-source">{system.source || "Fuente pendiente"}</p>
              <h3>{system.name}</h3>
              <p className="aliases">{system.category}</p>
              <p className="codex-summary">{system.principle || "Principio todavía sin definir."}</p>
              <dl className="codex-keyfacts"><div><dt>Costo</dt><dd>{system.cost || "Sin definir"}</dd></div><div><dt>Límite</dt><dd>{system.limits || "Sin definir"}</dd></div></dl>
              <details><summary>Ver reglas completas <span>{fields.length} apartados</span></summary><dl className="codex-details">{fields.map(([label, value]) => <div key={label}><dt>{label}</dt><dd>{value}</dd></div>)}</dl>{system.evidence.length > 0 && <div className="evidence-list">{system.evidence.map((item, index) => <article key={index}><b>{item.chapter}</b><p>{item.text}</p></article>)}</div>}</details>
              <div className="tag-list">{system.tags.map((tag) => <span key={tag}>{tag}</span>)}</div>
              <AttachmentStrip projectId={projectId} scope="magic" recordId={system.id} attachments={system.attachments ?? []} />
            </article>
          );
        })}
      </div>
    </section>
  );
}

function ManuscriptView({ archive, chapters, references, analysisSettings, sceneBoardViewport, sceneBoardCompact, onSaveSceneBoardView, onOpenReference, onSave, onSaveStructure, onSaveAnalysisSettings, onRegisterEditorSave, onDelete, onExportPdf, exportingPdf }: {
  archive: ArchiveState;
  chapters: WritingChapter[];
  references: ManuscriptReference[];
  analysisSettings: WritingAnalysisSettings;
  sceneBoardViewport: BoardViewport;
  sceneBoardCompact: boolean;
  onSaveSceneBoardView: (viewport: BoardViewport, compact: boolean) => void;
  onOpenReference: (reference: ManuscriptReference) => void;
  onSave: (chapter: WritingChapter) => Promise<boolean>;
  onSaveStructure: (chapters: WritingChapter[], message?: string) => Promise<boolean>;
  onSaveAnalysisSettings: (settings: WritingAnalysisSettings) => void;
  onRegisterEditorSave: (handler: (() => Promise<boolean>) | null) => void;
  onDelete: (id: string) => boolean;
  onExportPdf: () => void;
  exportingPdf: boolean;
}) {
  const sortedChapters = orderedChapters(chapters);
  const [selectedChapterId, setSelectedChapterId] = useState("");
  const [selectedSceneId, setSelectedSceneId] = useState("");
  const [viewMode, setViewMode] = useState<"write" | "board">("write");
  const [editorDirty, setEditorDirty] = useState(false);
  const [importing, setImporting] = useState(false);
  const [importError, setImportError] = useState("");
  const [importSummary, setImportSummary] = useState("");
  const [exportingFormat, setExportingFormat] = useState<"docx" | "epub" | null>(null);
  const effectiveChapter = sortedChapters.find((chapter) => chapter.id === selectedChapterId) ?? sortedChapters[0];
  const scenes = effectiveChapter ? orderedScenes(effectiveChapter) : [];
  const effectiveScene = scenes.find((scene) => scene.id === selectedSceneId) ?? scenes[0];

  const confirmDiscard = (message: string) => !editorDirty || window.confirm(message);

  const selectScene = (chapterId: string, sceneId: string) => {
    if (chapterId === effectiveChapter?.id && sceneId === effectiveScene?.id) return;
    if (!confirmDiscard("Hay cambios sin guardar en esta escena. ¿Cambiar de escena y descartarlos?")) return;
    setSelectedChapterId(chapterId);
    setSelectedSceneId(sceneId);
    setEditorDirty(false);
    setViewMode("write");
  };

  const newChapter = () => {
    if (!confirmDiscard("Hay cambios sin guardar. ¿Crear otro capítulo y descartarlos?")) return;
    const chapter = emptyWritingChapter(chapters.length);
    setSelectedChapterId(chapter.id);
    setSelectedSceneId(chapter.scenes[0].id);
    setEditorDirty(false);
    setViewMode("write");
    void onSave(chapter);
  };

  const newScene = () => {
    if (!confirmDiscard("Hay cambios sin guardar. ¿Crear otra escena y descartarlos?")) return;
    if (!effectiveChapter) {
      newChapter();
      return;
    }
    const scene = emptyWritingScene(effectiveChapter.id, effectiveChapter.scenes.length);
    const nextChapter = { ...effectiveChapter, scenes: [...orderedScenes(effectiveChapter), scene] };
    setSelectedChapterId(effectiveChapter.id);
    setSelectedSceneId(scene.id);
    setEditorDirty(false);
    setViewMode("write");
    void onSave(nextChapter);
  };

  const importChapter = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    if (!confirmDiscard("Hay cambios sin guardar. ¿Importar otro texto y descartarlos?")) return;
    setImporting(true);
    setImportError("");
    setImportSummary("");
    try {
      const imported = await extractTextFromFile(file, 1_500_000);
      const parsed = chaptersFromImportedManuscript(imported.content, imported.title, chapters.length);
      const nextChapters = [...orderedChapters(chapters), ...parsed.chapters];
      const importedSceneCount = parsed.chapters.reduce((total, chapter) => total + chapter.scenes.length, 0);
      const saved = await onSaveStructure(nextChapters, `${parsed.chapters.length} ${parsed.chapters.length === 1 ? "capítulo importado" : "capítulos importados"} desde ${file.name}`);
      if (saved) {
        const first = parsed.chapters[0];
        setSelectedChapterId(first.id);
        setSelectedSceneId(first.scenes[0]?.id ?? "");
        setEditorDirty(false);
        setViewMode("write");
        setImportSummary(parsed.detectedChapterHeadings > 0
          ? `Se reconocieron ${parsed.chapters.length} ${parsed.chapters.length === 1 ? "capítulo" : "capítulos"} y ${importedSceneCount} ${importedSceneCount === 1 ? "escena" : "escenas"} en “${file.name}”.`
          : `No se encontraron encabezados de capítulo en “${file.name}”; el texto se importó como un solo capítulo.`);
      }
    } catch (error) {
      setImportError(error instanceof Error ? error.message : "No se pudo importar el texto.");
    } finally {
      setImporting(false);
    }
  };

  const deleteSelectedChapter = (id: string) => {
    const currentIndex = sortedChapters.findIndex((chapter) => chapter.id === id);
    if (!onDelete(id)) return false;
    const remaining = sortedChapters.filter((chapter) => chapter.id !== id);
    if (effectiveChapter?.id === id) {
      const next = remaining[Math.min(Math.max(currentIndex, 0), Math.max(remaining.length - 1, 0))];
      setSelectedChapterId(next?.id ?? "");
      setSelectedSceneId(next?.scenes[0]?.id ?? "");
      setEditorDirty(false);
    }
    return true;
  };

  const deleteSelectedScene = (chapterId: string, sceneId: string) => {
    const chapter = chapters.find((item) => item.id === chapterId);
    if (!chapter || !window.confirm("¿Eliminar esta escena? Esta acción no se puede deshacer.")) return false;
    const ordered = orderedScenes(chapter);
    const currentIndex = ordered.findIndex((scene) => scene.id === sceneId);
    const remaining = ordered.filter((scene) => scene.id !== sceneId).map((scene, order) => ({ ...scene, order }));
    if (effectiveChapter?.id === chapterId && effectiveScene?.id === sceneId) {
      const nextScene = remaining[Math.min(Math.max(currentIndex, 0), Math.max(remaining.length - 1, 0))];
      setSelectedSceneId(nextScene?.id ?? "");
      setEditorDirty(false);
    }
    void onSave({ ...chapter, scenes: remaining });
    return true;
  };

  const purgeChapters = async () => {
    if (sortedChapters.length === 0) return;
    const sceneCount = sortedChapters.reduce((total, chapter) => total + chapter.scenes.length, 0);
    if (!window.confirm(`¿Purgar los ${sortedChapters.length} capítulos y sus ${sceneCount} escenas? Esta acción no se puede deshacer.`)) return;
    const saved = await onSaveStructure([], "Todos los capítulos del manuscrito fueron eliminados");
    if (!saved) return;
    setSelectedChapterId("");
    setSelectedSceneId("");
    setEditorDirty(false);
  };

  const exportFormat = async (format: "docx" | "epub") => {
    if (exportingFormat) return;
    setExportingFormat(format);
    setImportError("");
    try {
      if (format === "docx") await exportManuscriptDocx(archive);
      else await exportManuscriptEpub(archive);
    } catch (error) {
      setImportError(error instanceof Error ? error.message : `No se pudo crear el archivo ${format.toLocaleUpperCase("es")}.`);
    } finally {
      setExportingFormat(null);
    }
  };

  return (
    <section className="manuscript-view">
      <div className="view-intro manuscript-intro">
        <div><p>CAPÍTULOS / ESCENAS / CAPAS NARRATIVAS</p><h2>Manuscrito</h2><span>Escribe por escenas o reorganiza visualmente capítulos, puntos de vista y capas narrativas. Al importar un libro completo, sus encabezados separan automáticamente los capítulos.</span></div>
        <div className="view-actions manuscript-export-actions"><button className="secondary-action" onClick={onExportPdf} disabled={exportingPdf}>{exportingPdf ? "Creando PDF…" : "PDF"}</button><button className="secondary-action" onClick={() => void exportFormat("docx")} disabled={Boolean(exportingFormat)}>{exportingFormat === "docx" ? "Creando…" : "DOCX"}</button><button className="secondary-action" onClick={() => void exportFormat("epub")} disabled={Boolean(exportingFormat)}>{exportingFormat === "epub" ? "Creando…" : "EPUB"}</button><label className="secondary-action import-text-action"><input type="file" accept=".docx,.txt,.md,.csv,application/vnd.openxmlformats-officedocument.wordprocessingml.document,text/plain,text/markdown,text/csv" onChange={(event) => void importChapter(event)} disabled={importing} /><span>{importing ? "Analizando manuscrito…" : "Importar manuscrito o capítulo"}</span></label><button className="primary-action" onClick={newChapter}>+ Capítulo</button></div>
      </div>
      <div className="manuscript-mode-switch" role="tablist" aria-label="Vista del manuscrito"><button className={viewMode === "write" ? "active" : ""} onClick={() => setViewMode("write")}>Escritura</button><button className={viewMode === "board" ? "active" : ""} onClick={() => { if (confirmDiscard("Hay cambios sin guardar. ¿Abrir el tablero y descartarlos?")) { setEditorDirty(false); setViewMode("board"); } }}>Tablero de escenas</button><span>{sortedChapters.length} capítulos · {sortedChapters.reduce((total, chapter) => total + chapter.scenes.length, 0)} escenas · {manuscriptWordCount(sortedChapters).toLocaleString("es-CL")} palabras</span></div>
      {importError && <p className="manuscript-import-error" role="alert">{importError}</p>}
      {importSummary && <p className="manuscript-import-summary" role="status">{importSummary}</p>}
      {viewMode === "board" ? (
        <SceneBoard chapters={sortedChapters} viewport={sceneBoardViewport} compact={sceneBoardCompact} onSaveView={onSaveSceneBoardView} onChange={(next) => void onSaveStructure(next)} onOpen={selectScene} onNewChapter={newChapter} onDeleteChapter={deleteSelectedChapter} onDeleteScene={deleteSelectedScene} />
      ) : (
        <div className="manuscript-layout">
          <aside className="chapter-index">
            <div className="chapter-index-head"><span>ÍNDICE</span><div className="chapter-index-head-meta"><small>{sortedChapters.length} {sortedChapters.length === 1 ? "capítulo" : "capítulos"}</small><details className="chapter-delete-menu"><summary>Eliminar</summary><div><button type="button" disabled={!effectiveScene} onClick={() => { if (effectiveChapter && effectiveScene) deleteSelectedScene(effectiveChapter.id, effectiveScene.id); }}>Escena actual</button><button type="button" disabled={!effectiveChapter} onClick={() => { if (effectiveChapter) deleteSelectedChapter(effectiveChapter.id); }}>Capítulo actual</button><button type="button" disabled={sortedChapters.length === 0} onClick={() => void purgeChapters()}>Purgar capítulos</button></div></details></div></div>
            {sortedChapters.length === 0
              ? <p className="chapter-index-empty">Todavía no hay capítulos. Crea el primero para comenzar a escribir.</p>
              : <div className="chapter-list">{sortedChapters.map((chapter) => <section key={chapter.id} className={effectiveChapter?.id === chapter.id ? "active" : ""}><button className="chapter-index-button" onClick={() => selectScene(chapter.id, orderedScenes(chapter)[0]?.id ?? "")}><span>{chapter.label || "Sin numerar"}<i>{chapter.scenes.length} {chapter.scenes.length === 1 ? "escena" : "escenas"}</i></span><strong>{chapter.title || "Sin título"}</strong><small>{chapterWordCount(chapter).toLocaleString("es-CL")} palabras</small></button>{effectiveChapter?.id === chapter.id && <div className="scene-index-list">{orderedScenes(chapter).map((scene) => <button key={scene.id} className={effectiveScene?.id === scene.id ? "active" : ""} onClick={() => selectScene(chapter.id, scene.id)}><span>{scene.title || `Escena ${scene.order + 1}`}</span><small>{scene.pov || scene.narrativeLayer || scene.status}</small></button>)}<button className="new-scene-index" onClick={newScene}>+ Nueva escena</button></div>}</section>)}</div>}
          </aside>
          {effectiveChapter && effectiveScene
            ? <ManuscriptEditor key={effectiveScene.id} chapter={effectiveChapter} scene={effectiveScene} references={references} analysisSettings={analysisSettings} onOpenReference={onOpenReference} onSave={onSave} onSaveAnalysisSettings={onSaveAnalysisSettings} onRegisterSave={onRegisterEditorSave} onNewScene={newScene} onDeleteScene={() => deleteSelectedScene(effectiveChapter.id, effectiveScene.id)} onDeleteChapter={() => deleteSelectedChapter(effectiveChapter.id)} onDirtyChange={setEditorDirty} />
            : effectiveChapter
              ? <article className="manuscript-editor"><div className="manuscript-empty"><span>◇</span><h3>Capítulo sin escenas</h3><p>Crea una escena para comenzar a escribir.</p><button className="primary-action" onClick={newScene}>+ Crear escena</button></div></article>
              : <article className="manuscript-editor"><div className="manuscript-empty"><span>✎</span><h3>Tu manuscrito empieza aquí</h3><p>Crea un capítulo; la primera escena aparecerá automáticamente.</p><button className="primary-action" onClick={newChapter}>+ Crear primer capítulo</button></div></article>}
        </div>
      )}
    </section>
  );
}

function SceneBoard({ chapters, viewport, compact, onSaveView, onChange, onOpen, onNewChapter, onDeleteChapter, onDeleteScene }: { chapters: WritingChapter[]; viewport: BoardViewport; compact: boolean; onSaveView: (viewport: BoardViewport, compact: boolean) => void; onChange: (chapters: WritingChapter[]) => void; onOpen: (chapterId: string, sceneId: string) => void; onNewChapter: () => void; onDeleteChapter: (id: string) => boolean; onDeleteScene: (chapterId: string, sceneId: string) => boolean }) {
  const [layerFilter, setLayerFilter] = useState("Todas");
  const [povFilter, setPovFilter] = useState("Todos");
  const [draggedSceneId, setDraggedSceneId] = useState("");
  const [draggedChapterId, setDraggedChapterId] = useState("");
  const [trashActive, setTrashActive] = useState(false);
  const [camera, setCamera] = useState<BoardViewport>(() => ({ scale: Math.max(.35, Math.min(1.8, viewport.scale || 1)), x: viewport.x || 18, y: viewport.y || 18, worldSpace: true }));
  const [compactMode, setCompactMode] = useState(compact);
  const boardRef = useRef<HTMLDivElement>(null);
  const saveRef = useRef<number | null>(null);
  const panRef = useRef<{ pointerId: number; startX: number; startY: number; camera: BoardViewport } | null>(null);
  const spaceHeldRef = useRef(false);
  const [spaceHeld, setSpaceHeld] = useState(false);
  const [boardSize, setBoardSize] = useState({ width: 1100, height: 640 });
  const layers = [...new Set(chapters.flatMap((chapter) => chapter.scenes.map((scene) => scene.narrativeLayer.trim()).filter(Boolean)))].sort((a, b) => a.localeCompare(b, "es"));
  const povs = [...new Set(chapters.flatMap((chapter) => chapter.scenes.map((scene) => scene.pov.trim()).filter(Boolean)))].sort((a, b) => a.localeCompare(b, "es"));
  const sorted = orderedChapters(chapters);
  const visibleSceneCounts = sorted.map((chapter) => orderedScenes(chapter).filter((scene) => (layerFilter === "Todas" || scene.narrativeLayer === layerFilter) && (povFilter === "Todos" || scene.pov === povFilter)).length);
  const columnWidth = compactMode ? 226 : 294;
  const worldWidth = Math.max(900, 40 + sorted.length * (columnWidth + 16));
  const worldHeight = Math.max(600, 150 + Math.max(1, ...visibleSceneCounts) * (compactMode ? 91 : 142));

  useEffect(() => {
    const board = boardRef.current;
    if (!board) return;
    const update = () => setBoardSize({ width: board.clientWidth, height: board.clientHeight });
    update();
    const observer = new ResizeObserver(update);
    observer.observe(board);
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    const keyDown = (event: KeyboardEvent) => {
      if (event.code !== "Space" || event.repeat || (event.target as HTMLElement | null)?.matches("input, textarea, select, [contenteditable=true]")) return;
      event.preventDefault();
      spaceHeldRef.current = true;
      setSpaceHeld(true);
    };
    const keyUp = (event: KeyboardEvent) => {
      if (event.code !== "Space") return;
      spaceHeldRef.current = false;
      setSpaceHeld(false);
    };
    window.addEventListener("keydown", keyDown);
    window.addEventListener("keyup", keyUp);
    return () => {
      window.removeEventListener("keydown", keyDown);
      window.removeEventListener("keyup", keyUp);
      if (saveRef.current) window.clearTimeout(saveRef.current);
    };
  }, []);

  const commitSoon = (next: BoardViewport, nextCompact = compactMode) => {
    if (saveRef.current) window.clearTimeout(saveRef.current);
    saveRef.current = window.setTimeout(() => onSaveView(next, nextCompact), 380);
  };

  const zoomAt = (nextScale: number, clientX?: number, clientY?: number) => {
    const rect = boardRef.current?.getBoundingClientRect();
    setCamera((current) => {
      const next = { ...zoomCameraAt(current, nextScale, {
        x: rect && clientX !== undefined ? clientX - rect.left : boardSize.width / 2,
        y: rect && clientY !== undefined ? clientY - rect.top : boardSize.height / 2,
      }, .35, 1.8), worldSpace: true as const };
      commitSoon(next);
      return next;
    });
  };

  const fitAll = (nextCompact = compactMode) => {
    const nextColumnWidth = nextCompact ? 226 : 294;
    const nextWorldWidth = Math.max(900, 40 + sorted.length * (nextColumnWidth + 16));
    const nextWorldHeight = Math.max(600, 150 + Math.max(1, ...visibleSceneCounts) * (nextCompact ? 91 : 142));
    const next = { ...fitCameraToBounds({ minX: 0, minY: 0, maxX: nextWorldWidth, maxY: nextWorldHeight }, boardSize.width, boardSize.height, .35, 1.35, 30), worldSpace: true as const };
    setCamera(next);
    onSaveView(next, nextCompact);
  };

  const resetCamera = () => {
    const next = { scale: 1, x: 18, y: 18, worldSpace: true as const };
    setCamera(next);
    onSaveView(next, compactMode);
  };

  const toggleCompact = () => {
    const nextCompact = !compactMode;
    setCompactMode(nextCompact);
    onSaveView(camera, nextCompact);
  };

  const panoramic = () => {
    setCompactMode(true);
    window.setTimeout(() => fitAll(true), 0);
  };

  const beginPan = (event: React.PointerEvent<HTMLDivElement>) => {
    const interactive = Boolean((event.target as HTMLElement).closest(".scene-card, .scene-column > header, select, button, .scene-board-minimap, .scene-board-trash"));
    const forced = event.button === 1 || spaceHeldRef.current;
    if ((interactive && !forced) || (event.button !== 0 && event.button !== 1)) return;
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    panRef.current = { pointerId: event.pointerId, startX: event.clientX, startY: event.clientY, camera };
    event.currentTarget.classList.add("is-panning");
  };

  const movePan = (event: React.PointerEvent<HTMLDivElement>) => {
    const pan = panRef.current;
    if (!pan || pan.pointerId !== event.pointerId) return;
    setCamera({ ...pan.camera, x: pan.camera.x + event.clientX - pan.startX, y: pan.camera.y + event.clientY - pan.startY, worldSpace: true });
  };

  const endPan = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!panRef.current || panRef.current.pointerId !== event.pointerId) return;
    panRef.current = null;
    event.currentTarget.classList.remove("is-panning");
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    setCamera((current) => { onSaveView(current, compactMode); return current; });
  };

  const moveScene = (sceneId: string, targetChapterId: string, targetIndex: number) => {
    let moving: WritingScene | undefined;
    let sourceChapterId = "";
    let sourceIndex = -1;
    const next = chapters.map((chapter) => {
      const ordered = orderedScenes(chapter);
      const index = ordered.findIndex((scene) => scene.id === sceneId);
      if (index >= 0) {
        moving = ordered[index];
        sourceChapterId = chapter.id;
        sourceIndex = index;
      }
      return { ...chapter, scenes: ordered.filter((scene) => scene.id !== sceneId) };
    });
    if (!moving) return;
    const target = next.find((chapter) => chapter.id === targetChapterId);
    if (!target) return;
    let insertion = targetIndex;
    if (sourceChapterId === targetChapterId && sourceIndex < targetIndex) insertion -= 1;
    insertion = Math.max(0, Math.min(insertion, target.scenes.length));
    target.scenes.splice(insertion, 0, { ...moving, chapterId: targetChapterId });
    onChange(next.map((chapter, chapterOrder) => ({ ...chapter, order: chapterOrder, scenes: chapter.scenes.map((scene, order) => ({ ...scene, chapterId: chapter.id, order })) })));
    setDraggedSceneId("");
  };

  const moveChapter = (targetChapterId: string) => {
    if (!draggedChapterId || draggedChapterId === targetChapterId) return;
    const ordered = orderedChapters(chapters);
    const sourceIndex = ordered.findIndex((chapter) => chapter.id === draggedChapterId);
    const targetIndex = ordered.findIndex((chapter) => chapter.id === targetChapterId);
    if (sourceIndex < 0 || targetIndex < 0) return;
    const [moving] = ordered.splice(sourceIndex, 1);
    ordered.splice(targetIndex, 0, moving);
    onChange(ordered.map((chapter, order) => ({ ...chapter, order })));
    setDraggedChapterId("");
  };

  const dropIntoTrash = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    event.stopPropagation();
    setTrashActive(false);
    if (draggedSceneId) {
      const owner = chapters.find((chapter) => chapter.scenes.some((scene) => scene.id === draggedSceneId));
      if (owner) onDeleteScene(owner.id, draggedSceneId);
    } else if (draggedChapterId) {
      onDeleteChapter(draggedChapterId);
    }
    setDraggedSceneId("");
    setDraggedChapterId("");
  };

  return (
    <section className="scene-board-wrap">
      <div className="scene-board-filters"><label><span>Capa narrativa</span><select value={layerFilter} onChange={(event) => setLayerFilter(event.target.value)}><option>Todas</option>{layers.map((layer) => <option key={layer}>{layer}</option>)}</select></label><label><span>POV</span><select value={povFilter} onChange={(event) => setPovFilter(event.target.value)}><option>Todos</option>{povs.map((pov) => <option key={pov}>{pov}</option>)}</select></label><div className="scene-board-zoom"><button onClick={() => zoomAt(camera.scale - .1)} aria-label="Alejar tablero de escenas">−</button><span>{Math.round(camera.scale * 100)}%</span><button onClick={() => zoomAt(camera.scale + .1)} aria-label="Acercar tablero de escenas">+</button><button onClick={resetCamera}>100%</button><button onClick={() => fitAll()}>Ajustar todo</button><button className={compactMode ? "active" : ""} onClick={toggleCompact}>{compactMode ? "Detallado" : "Compacto"}</button><button onClick={panoramic}>Panorámica</button></div><p>Arrastra encabezados para ordenar capítulos y tarjetas para mover escenas. Doble clic abre el editor. Rueda para zoom; arrastra el fondo, usa el botón central o mantén espacio para desplazarte.</p></div>
      {chapters.length === 0 ? <div className="scene-board-empty"><h3>No hay capítulos todavía</h3><p>Crea el primero para comenzar a organizar escenas.</p><button className="primary-action" onClick={onNewChapter}>+ Crear capítulo</button></div> : <div className={`scene-board scene-board-viewport ${compactMode ? "compact" : "detailed"} ${spaceHeld ? "space-ready" : ""}`} ref={boardRef} onWheel={(event) => { event.preventDefault(); zoomAt(camera.scale * Math.exp(-event.deltaY * .00125), event.clientX, event.clientY); }} onPointerDown={beginPan} onPointerMove={movePan} onPointerUp={endPan} onPointerCancel={endPan}><div className="scene-board-world" style={{ width: worldWidth, minHeight: worldHeight, transform: `translate(${camera.x}px, ${camera.y}px) scale(${camera.scale})` }}>{sorted.map((chapter) => {
        const allScenes = orderedScenes(chapter);
        const visibleScenes = allScenes.filter((scene) => (layerFilter === "Todas" || scene.narrativeLayer === layerFilter) && (povFilter === "Todos" || scene.pov === povFilter));
        return <section className="scene-column" key={chapter.id} onDragOver={(event) => event.preventDefault()} onDrop={(event) => { event.preventDefault(); if (draggedSceneId) moveScene(draggedSceneId, chapter.id, allScenes.length); else moveChapter(chapter.id); }}><header draggable onDragStart={(event) => { setDraggedSceneId(""); setDraggedChapterId(chapter.id); event.dataTransfer.effectAllowed = "move"; }} onDragEnd={() => { setDraggedChapterId(""); setTrashActive(false); }}><span>{chapter.label}</span><h3>{chapter.title || "Sin título"}</h3><small>{allScenes.length} escenas · {chapterWordCount(chapter).toLocaleString("es-CL")} palabras</small></header><div className="scene-column-cards">{visibleScenes.map((scene) => {
          const actualIndex = allScenes.findIndex((item) => item.id === scene.id);
          const layerHue = [...scene.narrativeLayer].reduce((total, character) => total + character.charCodeAt(0), 32) % 360;
          return <article className={`scene-card status-${scene.status.toLocaleLowerCase("es")}`} style={{ "--layer-hue": layerHue } as React.CSSProperties} key={scene.id} draggable onDragStart={(event) => { event.stopPropagation(); setDraggedChapterId(""); setDraggedSceneId(scene.id); event.dataTransfer.effectAllowed = "move"; }} onDragEnd={() => { setDraggedSceneId(""); setTrashActive(false); }} onDragOver={(event) => event.preventDefault()} onDrop={(event) => { event.preventDefault(); event.stopPropagation(); moveScene(draggedSceneId, chapter.id, actualIndex); }} onDoubleClick={() => onOpen(chapter.id, scene.id)} tabIndex={0} onKeyDown={(event) => { if (event.key === "Enter") onOpen(chapter.id, scene.id); }}><div><span>{scene.narrativeLayer || "Sin capa"}</span><i>{scene.status}</i></div><h4>{scene.title || `Escena ${scene.order + 1}`}</h4><p>{scene.pov ? `POV · ${scene.pov}` : "POV sin definir"}</p><small>{scene.location || "Lugar sin definir"} · {wordCount(scene.content).toLocaleString("es-CL")} palabras</small></article>;
        })}{visibleScenes.length === 0 && <p className="scene-column-empty">Ninguna escena coincide con los filtros.</p>}</div></section>;
      })}</div><div className={`scene-board-trash ${trashActive ? "active" : ""}`} role="button" aria-label="Papelera: arrastra aquí una escena o un capítulo para eliminarlo" onPointerDown={(event) => event.stopPropagation()} onDragEnter={(event) => { event.preventDefault(); event.stopPropagation(); setTrashActive(true); }} onDragOver={(event) => { event.preventDefault(); event.stopPropagation(); event.dataTransfer.dropEffect = "move"; setTrashActive(true); }} onDragLeave={(event) => { if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setTrashActive(false); }} onDrop={dropIntoTrash}><span>⌫</span><strong>PAPELERA</strong><small>Arrastra aquí una escena o capítulo</small></div><button className="scene-board-minimap" type="button" aria-label="Minimapa del manuscrito" onPointerDown={(event) => event.stopPropagation()} onClick={(event) => { const rect = event.currentTarget.getBoundingClientRect(); const worldX = (event.clientX - rect.left) / rect.width * worldWidth; const worldY = (event.clientY - rect.top) / rect.height * worldHeight; const next = { ...camera, x: boardSize.width / 2 - worldX * camera.scale, y: boardSize.height / 2 - worldY * camera.scale, worldSpace: true as const }; setCamera(next); onSaveView(next, compactMode); }}><span>{sorted.map((chapter, index) => <i key={chapter.id} style={{ left: `${(20 + index * (columnWidth + 16)) / worldWidth * 100}%`, width: `${columnWidth / worldWidth * 100}%` }} />)}<b style={{ left: `${Math.max(0, -camera.x / camera.scale) / worldWidth * 100}%`, top: `${Math.max(0, -camera.y / camera.scale) / worldHeight * 100}%`, width: `${Math.min(100, boardSize.width / camera.scale / worldWidth * 100)}%`, height: `${Math.min(100, boardSize.height / camera.scale / worldHeight * 100)}%` }} /></span></button></div>}
    </section>
  );
}

function ManuscriptEditor({ chapter, scene, references, analysisSettings, onOpenReference, onSave, onSaveAnalysisSettings, onRegisterSave, onNewScene, onDeleteScene, onDeleteChapter, onDirtyChange }: {
  chapter: WritingChapter;
  scene: WritingScene;
  references: ManuscriptReference[];
  analysisSettings: WritingAnalysisSettings;
  onOpenReference: (reference: ManuscriptReference) => void;
  onSave: (chapter: WritingChapter) => Promise<boolean>;
  onSaveAnalysisSettings: (settings: WritingAnalysisSettings) => void;
  onRegisterSave: (handler: (() => Promise<boolean>) | null) => void;
  onNewScene: () => void;
  onDeleteScene: () => boolean;
  onDeleteChapter: () => boolean;
  onDirtyChange: (dirty: boolean) => void;
}) {
  const [chapterDraft, setChapterDraft] = useState(() => ({ label: chapter.label, title: chapter.title }));
  const [draft, setDraft] = useState<WritingScene>(() => ({ ...scene }));
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [focusMode, setFocusMode] = useState(false);
  const [proofLoading, setProofLoading] = useState(false);
  const [proofError, setProofError] = useState("");
  const [proofMatches, setProofMatches] = useState<ProofreadMatch[]>([]);
  const [proofBaseOffset, setProofBaseOffset] = useState(0);
  const [analysisOpen, setAnalysisOpen] = useState(false);
  const [analysisWindow, setAnalysisWindow] = useState(analysisSettings.repetitionWindow);
  const [fillerText, setFillerText] = useState(analysisSettings.fillerPhrases.join("\n"));
  const [referenceExtension] = useState(() => createReferenceExtension({ references, onOpen: onOpenReference }));
  const [styleAnalysisExtension] = useState(() => createStyleAnalysisExtension({ enabled: false, repeatedKeys: [], fillerPhrases: [] }));
  const effectiveAnalysisSettings = useMemo<WritingAnalysisSettings>(() => ({ repetitionWindow: analysisWindow, fillerPhrases: fillerText.split("\n").map((item) => item.trim()).filter(Boolean) }), [analysisWindow, fillerText]);
  const styleAnalysis = useMemo(() => analyzeWritingStyle(draft.content, effectiveAnalysisSettings), [draft.content, effectiveAnalysisSettings]);
  const editor = useEditor({
    immediatelyRender: false,
    extensions: [StarterKit, referenceExtension, styleAnalysisExtension],
    content: plainTextContent(scene.content),
    editorProps: { attributes: { class: "manuscript-prosemirror", lang: "es", spellcheck: "true", "aria-label": `Texto de ${scene.title}` } },
    onUpdate: ({ editor: activeEditor }) => {
      const content = activeEditor.getText({ blockSeparator: "\n" });
      setDraft((current) => ({ ...current, content }));
      setDirty(true);
      onDirtyChange(true);
      setProofMatches([]);
    },
  });

  useEffect(() => {
    if (!editor) return;
    editor.view.dispatch(editor.state.tr.setMeta("semanticReferences", { references, onOpen: onOpenReference } satisfies ReferenceRuntime));
  }, [editor, references, onOpenReference]);

  useEffect(() => {
    if (!editor) return;
    editor.view.dispatch(editor.state.tr.setMeta("writingStyleAnalysis", { enabled: analysisOpen, repeatedKeys: styleAnalysis.repeatedKeys, fillerPhrases: effectiveAnalysisSettings.fillerPhrases } satisfies StyleAnalysisRuntime));
  }, [editor, analysisOpen, styleAnalysis.repeatedKeys, effectiveAnalysisSettings.fillerPhrases]);

  useEffect(() => {
    const handleFullscreen = () => {
      if (!document.fullscreenElement) setFocusMode(false);
    };
    document.addEventListener("fullscreenchange", handleFullscreen);
    return () => document.removeEventListener("fullscreenchange", handleFullscreen);
  }, []);

  const updateDraft = <K extends keyof WritingScene>(key: K, value: WritingScene[K]) => {
    setDraft((current) => ({ ...current, [key]: value }));
    setDirty(true);
    onDirtyChange(true);
    setProofMatches([]);
  };

  const updateChapterDraft = (key: "label" | "title", value: string) => {
    setChapterDraft((current) => ({ ...current, [key]: value }));
    setDirty(true);
    onDirtyChange(true);
  };

  const saveScene = useCallback(async () => {
    if (saving) return false;
    const next = { ...draft, updatedAt: new Date().toISOString() };
    const nextChapter: WritingChapter = { ...chapter, ...chapterDraft, scenes: chapter.scenes.map((item) => item.id === next.id ? next : item) };
    setSaving(true);
    const saved = await onSave(nextChapter);
    setSaving(false);
    if (saved) {
      setDraft(next);
      setDirty(false);
      onDirtyChange(false);
    }
    return saved;
  }, [chapter, chapterDraft, draft, onDirtyChange, onSave, saving]);

  useEffect(() => {
    onRegisterSave(saveScene);
    return () => onRegisterSave(null);
  }, [onRegisterSave, saveScene]);

  const deleteScene = () => {
    if (onDeleteScene()) onDirtyChange(false);
  };

  const enterFocusMode = () => {
    setFocusMode(true);
    void document.documentElement.requestFullscreen?.().catch(() => undefined);
    window.setTimeout(() => editor?.commands.focus(), 0);
  };

  const leaveFocusMode = () => {
    setFocusMode(false);
    if (document.fullscreenElement) void document.exitFullscreen().catch(() => undefined);
  };

  const runProofread = async () => {
    if (!draft.content.trim() || proofLoading) return;
    const selection = editor?.state.selection;
    const hasSelection = Boolean(selection && !selection.empty);
    const text = selection && !selection.empty ? editor.state.doc.textBetween(selection.from, selection.to, "\n", "\n") : draft.content;
    const selectionOffset = selection && !selection.empty ? editor.state.doc.textBetween(0, selection.from, "\n", "\n").length : 0;
    if (new TextEncoder().encode(text).byteLength > 18_000) {
      setProofMatches([]);
      setProofError("La escena es demasiado larga para una sola revisión. Selecciona un fragmento y vuelve a pulsar “Revisar texto”.");
      return;
    }
    setProofLoading(true);
    setProofError("");
    setProofMatches([]);
    setProofBaseOffset(hasSelection ? selectionOffset : 0);
    try {
      const response = await fetch("/api/proofread", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ text }),
      });
      const result = (await response.json()) as { matches?: ProofreadMatch[]; error?: string };
      if (!response.ok) throw new Error(result.error ?? "No fue posible revisar el texto.");
      setProofMatches(result.matches ?? []);
      if ((result.matches ?? []).length === 0) setProofError("LanguageTool no encontró sugerencias en este fragmento.");
    } catch (error) {
      setProofError(error instanceof Error ? error.message : "El corrector online no está disponible. El subrayado del navegador sigue activo.");
    } finally {
      setProofLoading(false);
    }
  };

  const applyReplacement = (match: ProofreadMatch, replacement: string) => {
    const start = proofBaseOffset + match.offset;
    const end = start + match.length;
    const currentContent = editor?.getText({ blockSeparator: "\n" }) ?? draft.content;
    const content = `${currentContent.slice(0, start)}${replacement}${currentContent.slice(end)}`;
    setDraft((current) => ({ ...current, content }));
    setDirty(true);
    onDirtyChange(true);
    editor?.commands.setContent(plainTextContent(content), { emitUpdate: false });
    setProofMatches([]);
    setProofError("Cambio aplicado. Vuelve a revisar para recalcular las sugerencias del texto actualizado.");
    window.setTimeout(() => editor?.commands.focus(), 0);
  };

  const saveAnalysisPreferences = () => {
    const settings = { repetitionWindow: Math.max(10, Math.min(100, Math.round(analysisWindow))), fillerPhrases: effectiveAnalysisSettings.fillerPhrases };
    setAnalysisWindow(settings.repetitionWindow);
    onSaveAnalysisSettings(settings);
  };

  const jumpToFlag = (flag: StyleFlag, occurrenceIndex: number) => {
    if (!editor) return;
    const positions = styleFlagOccurrences(editor, flag);
    const target = positions[occurrenceIndex];
    if (!target) return;
    editor.commands.focus();
    editor.commands.setTextSelection(target);
    editor.commands.scrollIntoView();
  };

  return (
    <article className={`manuscript-editor ${focusMode ? "focus-mode" : ""}`}>
      <>
              <header className="manuscript-toolbar">
                <div><span>{focusMode ? chapterDraft.label : "EDITOR DE ESCENA"}</span><strong>{focusMode ? draft.title : dirty ? "Cambios sin guardar" : `${chapterDraft.title} · ${draft.title}`}</strong></div>
                <div className="manuscript-toolbar-actions">
                  <button type="button" className="proofread-action" onClick={() => void runProofread()} disabled={proofLoading}>{proofLoading ? "Revisando…" : "Revisar texto"}</button>
                  <button type="button" className={analysisOpen ? "analysis-action active" : "analysis-action"} onClick={() => setAnalysisOpen((current) => !current)}>Análisis local</button>
                  {!focusMode && <button type="button" onClick={onNewScene}>+ Escena</button>}
                  {!focusMode && <button type="button" onClick={enterFocusMode}>Sin distracciones</button>}
                  {focusMode && <button type="button" onClick={leaveFocusMode}>Salir</button>}
                  <button type="button" className="save-writing" onClick={() => void saveScene()} disabled={!dirty || saving}>{saving ? "Guardando…" : "Guardar escena"}</button>
                </div>
              </header>

              <div className="manuscript-fields">
                <label><span>Capítulo</span><input value={chapterDraft.label} onChange={(event) => updateChapterDraft("label", event.target.value)} placeholder="Capítulo 1, Prólogo…" /></label>
                <label><span>Título del capítulo</span><input value={chapterDraft.title} onChange={(event) => updateChapterDraft("title", event.target.value)} placeholder="Sin título" /></label>
                <label><span>Título de escena</span><input value={draft.title} onChange={(event) => updateDraft("title", event.target.value)} placeholder="Escena 1" /></label>
                <label><span>POV</span><input value={draft.pov} onChange={(event) => updateDraft("pov", event.target.value)} placeholder="Personaje focal" /></label>
                <label><span>Ubicación</span><input value={draft.location} onChange={(event) => updateDraft("location", event.target.value)} placeholder="Lugar de la escena" /></label>
                <label><span>Capa narrativa</span><input value={draft.narrativeLayer} onChange={(event) => updateDraft("narrativeLayer", event.target.value)} placeholder="Venesdun, ciencia ficción oculta…" /></label>
                <label><span>Estado</span><select value={draft.status} onChange={(event) => updateDraft("status", event.target.value as WritingScene["status"])}><option>Borrador</option><option>Revisión</option><option>Final</option></select></label>
              </div>

              <div className="manuscript-textarea manuscript-rich-editor"><EditorContent editor={editor} /></div>

              {(proofLoading || proofError || proofMatches.length > 0) && (
                <aside className="proofread-panel" aria-live="polite">
                  <div className="proofread-panel-head"><span>CORRECCIÓN ONLINE</span><button type="button" onClick={() => { setProofMatches([]); setProofError(""); }} aria-label="Cerrar sugerencias">×</button></div>
                  {proofError && <p>{proofError}</p>}
                  {proofMatches.map((match, index) => (
                    <article key={`${match.offset}-${index}`}><strong>{match.shortMessage || match.message}</strong>{match.shortMessage && <p>{match.message}</p>}<small>{match.context}</small>{match.replacements.length > 0 && <div>{match.replacements.slice(0, 5).map((replacement) => <button type="button" key={replacement} onClick={() => applyReplacement(match, replacement)}>{replacement}</button>)}</div>}</article>
                  ))}
                </aside>
              )}

              {analysisOpen && (
                <aside className="style-analysis-panel">
                  <div className="style-analysis-head"><div><span>ANÁLISIS LOCAL · SIN ENVÍO A INTERNET</span><h3>{styleAnalysis.readability.toFixed(1)} · {styleAnalysis.readabilityLabel}</h3></div><button type="button" onClick={() => setAnalysisOpen(false)} aria-label="Cerrar análisis">×</button></div>
                  <div className="style-stat-grid"><article><span>Legibilidad Fernández-Huerta</span><strong>{styleAnalysis.readability.toFixed(1)}</strong><small>{styleAnalysis.readabilityLabel}</small></article><article><span>Longitud media</span><strong>{styleAnalysis.averageSentenceLength.toFixed(1)}</strong><small>palabras por oración</small></article><article><span>Muletillas</span><strong>{styleAnalysis.fillerPercentage.toFixed(2)}%</strong><small>{styleAnalysis.fillers.reduce((total, item) => total + item.count, 0)} coincidencias</small></article><article><span>Repeticiones cercanas</span><strong>{styleAnalysis.repetitions.length}</strong><small>palabras señaladas</small></article></div>
                  <div className="style-settings"><label><span>Ventana de repetición</span><input type="number" min="10" max="100" value={analysisWindow} onChange={(event) => setAnalysisWindow(Math.max(10, Math.min(100, Number(event.target.value))))} /><small>palabras</small></label><label><span>Muletillas configurables, una por línea</span><textarea rows={5} value={fillerText} onChange={(event) => setFillerText(event.target.value)} /></label><button type="button" onClick={saveAnalysisPreferences}>Guardar preferencias</button></div>
                  <div className="style-findings"><section><h4>Repeticiones dentro de {analysisWindow} palabras</h4>{styleAnalysis.repetitions.length === 0 ? <p>No se encontraron repeticiones cercanas fuera de las palabras funcionales.</p> : styleAnalysis.repetitions.slice(0, 30).map((flag) => <article key={`rep-${flag.key}`}><div><strong>{flag.label}</strong><span>{flag.count} veces · {flag.perThousand.toFixed(1)} por 1000</span></div><div>{(editor ? styleFlagOccurrences(editor, flag) : []).slice(0, 12).map((_, index) => <button type="button" key={index} onClick={() => jumpToFlag(flag, index)}>{index + 1}</button>)}</div></article>)}</section><section><h4>Muletillas y patrones</h4>{styleAnalysis.fillers.length === 0 ? <p>No se encontraron las expresiones configuradas.</p> : styleAnalysis.fillers.map((flag) => <article key={`filler-${flag.key}`}><div><strong>{flag.label}</strong><span>{flag.count} veces · {flag.perThousand.toFixed(1)} por 1000</span></div><div>{(editor ? styleFlagOccurrences(editor, flag) : []).slice(0, 12).map((_, index) => <button type="button" key={index} onClick={() => jumpToFlag(flag, index)}>{index + 1}</button>)}</div></article>)}</section></div>
                </aside>
              )}

              <footer className="manuscript-footer">
                <span>{wordCount(draft.content).toLocaleString("es-CL")} palabras · {draft.status}</span>
                <div className="manuscript-footer-notes"><p>{references.length} fichas y textos enlazables. Sus nombres y alias se resaltan; pulsa uno para abrirlo.</p><p>El análisis de estilo funciona localmente. “Revisar texto” envía el fragmento seleccionado —o la escena— a <a href="https://languagetool.org" target="_blank" rel="noreferrer">LanguageTool</a>. <a href="https://languagetool.org/legal/privacy" target="_blank" rel="noreferrer">Privacidad</a>.</p></div>
                {!focusMode && <div className="delete-writing-group"><button type="button" className="delete-writing" onClick={deleteScene}>Eliminar escena</button><button type="button" className="delete-writing" onClick={() => { if (onDeleteChapter()) onDirtyChange(false); }}>Eliminar capítulo</button></div>}
              </footer>
      </>
    </article>
  );
}

function ImportedTextEditor({ scope, text, onCancel, onSave }: { scope: TextScope; text: ImportedTextRecord; onCancel: () => void; onSave: (text: ImportedTextRecord) => void }) {
  const [draft, setDraft] = useState(() => ({ ...text }));
  const [importing, setImporting] = useState(false);
  const [error, setError] = useState("");
  useEscape(onCancel);

  const importFile = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    setImporting(true);
    setError("");
    try {
      const imported = await extractTextFromFile(file);
      setDraft({ ...imported, id: draft.id });
    } catch (importError) {
      setError(importError instanceof Error ? importError.message : "No se pudo extraer el texto.");
    } finally {
      setImporting(false);
    }
  };

  const submit = () => {
    const content = draft.content.trim();
    if (!content) {
      setError("Carga un documento o escribe el contenido antes de guardarlo.");
      return;
    }
    onSave({ ...draft, title: draft.title.trim() || "Texto sin título", content });
  };

  return (
    <EditorShell title={`Texto de ${scope === "world" ? "Mundo" : "Magia"}`} onCancel={onCancel} onSubmit={submit} compact>
      <div className="imported-text-editor">
        <div className="text-import-zone">
          <div><span>IMPORTACIÓN INDEPENDIENTE</span><p>El documento se lee en tu equipo. Solo se guarda el texto extraído; el archivo original no se sube ni queda como adjunto.</p></div>
          <label className="import-text-button"><input type="file" accept=".docx,.txt,.md,.csv,application/vnd.openxmlformats-officedocument.wordprocessingml.document,text/plain,text/markdown,text/csv" onChange={(event) => void importFile(event)} disabled={importing} /><span>{importing ? "Extrayendo texto…" : "Elegir Word o archivo de texto"}</span></label>
        </div>
        {error && <p className="attachment-error" role="alert">{error}</p>}
        <label><span>Título dentro del apartado</span><input value={draft.title} onChange={(event) => setDraft({ ...draft, title: event.target.value })} /></label>
        <label><span>Contenido editable</span><textarea rows={22} value={draft.content} onChange={(event) => setDraft({ ...draft, content: event.target.value, sourceName: draft.sourceName || "Texto escrito en la aplicación" })} placeholder="También puedes pegar o escribir aquí el contenido directamente." /></label>
        <small>{wordCount(draft.content).toLocaleString("es-CL")} palabras · {draft.sourceName}</small>
      </div>
    </EditorShell>
  );
}

function AttachmentStrip({ projectId, scope, recordId, attachments }: { projectId: string; scope: "world" | "magic"; recordId: string; attachments: ArchiveAttachment[] }) {
  const images = attachments.filter((attachment) => attachment.kind === "image");
  if (images.length === 0) return null;
  return (
    <div className="attachment-strip" aria-label="Galería de imágenes">
      {images.map((attachment) => {
        const url = attachmentUrl(projectId, scope, recordId, attachment);
        return <a className="attachment-thumb" href={url} target="_blank" rel="noreferrer" key={attachment.id} title={`${attachment.name} · ${formatFileSize(attachment.size)}`}><img src={url} alt={attachment.name} /><span>{attachment.name}</span></a>;
      })}
    </div>
  );
}

function AttachmentManager({ attachments, onChange }: { attachments: ArchiveAttachment[]; onChange: (attachments: ArchiveAttachment[]) => void }) {
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState("");

  const uploadFiles = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(event.target.files ?? []);
    event.target.value = "";
    if (files.length === 0) return;
    setUploading(true);
    setError("");
    const uploaded: ArchiveAttachment[] = [];
    for (const file of files) {
      try {
        if (file.size > 12 * 1024 * 1024) throw new Error(`${file.name} supera el límite de 12 MB.`);
        uploaded.push({
          id: nextId("imagen"),
          name: file.name,
          mimeType: file.type || "image/jpeg",
          size: file.size,
          kind: "image",
          dataUrl: await imageFileToDataUrl(file, 1_800, 1_800, file.name),
        });
      } catch (uploadError) {
        setError(uploadError instanceof Error ? uploadError.message : `No se pudo cargar ${file.name}.`);
      }
    }
    if (uploaded.length > 0) onChange([...attachments, ...uploaded]);
    setUploading(false);
  };

  return (
    <fieldset className="wide attachment-manager">
      <legend>Galería de imágenes</legend>
      <div className="attachment-upload-row">
        <label className="attachment-upload-button"><input type="file" multiple accept=".jpg,.jpeg,.png,.webp,.gif,image/*" onChange={(event) => void uploadFiles(event)} disabled={uploading} /><span>{uploading ? "Cargando…" : "+ Añadir imágenes"}</span></label>
        <small>JPG, PNG, WEBP o GIF · hasta 12 MB por imagen</small>
      </div>
      {error && <p className="attachment-error">{error}</p>}
      {attachments.some((attachment) => attachment.kind === "image") && (
        <div className="attachment-manager-list">
          {attachments.filter((attachment) => attachment.kind === "image").map((attachment) => {
            const url = attachmentUrl(projectId, scope, recordId, attachment);
            return (
              <article key={attachment.id}>
                <img src={url} alt="" />
                <span>{attachment.name}<small>{formatFileSize(attachment.size)}</small></span>
                <a href={url} target="_blank" rel="noreferrer">Abrir</a>
                <button type="button" onClick={() => onChange(attachments.filter((item) => item.id !== attachment.id))}>Quitar</button>
              </article>
            );
          })}
        </div>
      )}
    </fieldset>
  );
}

function AttachmentLibraryModal({ target, attachments, onChange, onClose }: { target: AttachmentTarget; attachments: ArchiveAttachment[]; onChange: (attachments: ArchiveAttachment[]) => void; onClose: () => void }) {
  useEscape(onClose);
  return (
    <div className="editor-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <section className="editor-modal compact attachment-library-modal" role="dialog" aria-modal="true" aria-label={`Imágenes de ${target.name}`}>
        <div className="editor-head"><div><span>GALERÍA DEL WORLDBUILDING</span><h2>Imágenes · {target.name}</h2></div><button type="button" onClick={onClose} aria-label="Cerrar imágenes">×</button></div>
        <div className="editor-body">
          <p className="attachment-library-intro">Aquí solo se guardan imágenes. Los documentos de texto se importan desde el botón independiente del apartado y se convierten en contenido editable.</p>
          <AttachmentManager attachments={attachments} onChange={onChange} />
        </div>
        <div className="editor-actions"><span className="action-spacer" /><button type="button" onClick={onClose}>Cerrar</button></div>
      </section>
    </div>
  );
}

function AmbienceEditor({ spotifyUrl, youtubeUrl, onCancel, onSave }: { spotifyUrl: string; youtubeUrl: string; onCancel: () => void; onSave: (spotifyUrl: string, youtubeUrl: string) => void }) {
  const [spotify, setSpotify] = useState(spotifyUrl);
  const [youtube, setYoutube] = useState(youtubeUrl);
  const [error, setError] = useState("");
  const spotifyPreview = spotifyEmbedUrl(spotify);
  const youtubePreview = youtubeEmbedUrl(youtube);
  useEscape(onCancel);

  const submit = () => {
    const cleanSpotify = spotify.trim();
    const cleanYoutube = youtube.trim();
    if (cleanSpotify && !spotifyEmbedUrl(cleanSpotify)) {
      setError("El enlace de Spotify no corresponde a una playlist válida.");
      return;
    }
    if (cleanYoutube && !youtubeEmbedUrl(cleanYoutube)) {
      setError("El enlace de YouTube no corresponde a un video o una playlist válida.");
      return;
    }
    onSave(cleanSpotify, cleanYoutube);
  };

  return (
    <EditorShell title="Reproductor de ambientación" onCancel={onCancel} onSubmit={submit} compact>
      <div className="ambience-editor">
        <label><span>Playlist de Spotify</span><div><input value={spotify} onChange={(event) => { setSpotify(event.target.value); setError(""); }} placeholder="https://open.spotify.com/playlist/…" />{spotify && <button type="button" onClick={() => setSpotify("")}>Quitar</button>}</div></label>
        <label><span>Video o playlist de YouTube</span><div><input value={youtube} onChange={(event) => { setYoutube(event.target.value); setError(""); }} placeholder="https://www.youtube.com/watch?v=…" />{youtube && <button type="button" onClick={() => setYoutube("")}>Quitar</button>}</div></label>
        <p>Spotify y YouTube necesitan internet. YouTube se abre con el dominio de privacidad reforzada; la aplicación no añade anuncios, aunque no puede impedir la publicidad que YouTube inserte en su reproductor oficial.</p>
        <p>Los archivos MP3, MP4, OGG y VOB se cargan desde el propio reproductor y no se suben a internet.</p>
        {error && <p className="ambience-error">{error}</p>}
        {(spotifyPreview || youtubePreview) && <div className="ambience-previews">{spotifyPreview && <iframe src={spotifyPreview} title="Vista previa de Spotify" loading="lazy" allow="autoplay; clipboard-write; encrypted-media; fullscreen; picture-in-picture" />}{youtubePreview && <iframe src={youtubePreview} title="Vista previa de YouTube" loading="lazy" allow="autoplay; encrypted-media; picture-in-picture; fullscreen" allowFullScreen />}</div>}
      </div>
    </EditorShell>
  );
}

type AmbienceSource = "spotify" | "youtube" | "local";
type AmbienceDockMode = "expanded" | "compact" | "hidden";

function AmbienceDock({ projectId, spotifyUrl, youtubeUrl, onConfigure }: { projectId: string; spotifyUrl: string; youtubeUrl: string; onConfigure: () => void }) {
  const spotifyEmbed = spotifyEmbedUrl(spotifyUrl);
  const youtubeEmbed = youtubeEmbedUrl(youtubeUrl);
  const modeStorageKey = `worldbuilder-writer-ambience-${projectId}`;
  const [mode, setMode] = useState<AmbienceDockMode>("compact");
  const [source, setSource] = useState<AmbienceSource>(spotifyEmbed ? "spotify" : youtubeEmbed ? "youtube" : "local");
  const [localTracks, setLocalTracks] = useState<LocalMediaTrack[]>([]);
  const [activeTrackId, setActiveTrackId] = useState("");
  const [mediaError, setMediaError] = useState("");
  const fileInputRef = useRef<HTMLInputElement>(null);
  const localUrlsRef = useRef<string[]>([]);

  const activeTrack = localTracks.find((track) => track.id === activeTrackId) ?? localTracks[0];
  const activeSource: AmbienceSource = source === "spotify" && spotifyEmbed
    ? "spotify"
    : source === "youtube" && youtubeEmbed
      ? "youtube"
      : source === "local" && localTracks.length
        ? "local"
        : spotifyEmbed ? "spotify" : youtubeEmbed ? "youtube" : "local";

  useEffect(() => {
    const stored = window.localStorage.getItem(modeStorageKey);
    const restore = window.setTimeout(() => {
      if (stored === "expanded" || stored === "compact" || stored === "hidden") setMode(stored);
    }, 0);
    return () => window.clearTimeout(restore);
  }, [modeStorageKey]);

  useEffect(() => {
    window.localStorage.setItem(modeStorageKey, mode);
  }, [mode, modeStorageKey]);

  useEffect(() => () => {
    localUrlsRef.current.forEach((url) => URL.revokeObjectURL(url));
  }, []);

  const changeMode = (nextMode: AmbienceDockMode) => {
    setMode(nextMode);
    setMediaError("");
  };

  const selectSource = (nextSource: AmbienceSource) => {
    if (nextSource === "spotify" && !spotifyEmbed) { onConfigure(); return; }
    if (nextSource === "youtube" && !youtubeEmbed) { onConfigure(); return; }
    if (nextSource === "local" && localTracks.length === 0) { fileInputRef.current?.click(); return; }
    setSource(nextSource);
    setMediaError("");
  };

  const addLocalFiles = (files: FileList | null) => {
    if (!files?.length) return;
    const accepted: LocalMediaTrack[] = [];
    const rejected: string[] = [];
    Array.from(files).forEach((file) => {
      const extension = localMediaExtension(file);
      if (!extension) { rejected.push(file.name); return; }
      const url = URL.createObjectURL(file);
      localUrlsRef.current.push(url);
      accepted.push({
        id: nextId("media"),
        name: file.name,
        url,
        kind: extension === "mp3" || (extension === "ogg" && !file.type.startsWith("video/")) ? "audio" : "video",
        extension,
        size: file.size,
      });
    });
    if (accepted.length) {
      setLocalTracks((current) => [...current, ...accepted]);
      setActiveTrackId((current) => current || accepted[0].id);
      setSource("local");
      setMode("expanded");
    }
    setMediaError(rejected.length ? `No se cargaron: ${rejected.join(", ")}. Usa MP3, MP4, OGG o VOB.` : "");
  };

  const removeLocalTrack = (track: LocalMediaTrack) => {
    URL.revokeObjectURL(track.url);
    localUrlsRef.current = localUrlsRef.current.filter((url) => url !== track.url);
    const remaining = localTracks.filter((item) => item.id !== track.id);
    setLocalTracks(remaining);
    if (activeTrack?.id === track.id) setActiveTrackId(remaining[0]?.id ?? "");
  };

  const playNextLocal = () => {
    if (!activeTrack || localTracks.length < 2) return;
    const index = localTracks.findIndex((track) => track.id === activeTrack.id);
    setActiveTrackId(localTracks[(index + 1) % localTracks.length].id);
  };

  const sourceLabel = activeSource === "spotify" ? "Spotify" : activeSource === "youtube" ? "YouTube" : activeTrack?.name ?? "Archivos locales";

  return (
    <aside className={`ambience-dock mode-${mode}`} aria-label="Reproductor de ambientación">
      <button type="button" className="ambience-restore" onClick={() => changeMode("compact")} aria-label="Mostrar reproductor" title="Mostrar reproductor">♫</button>
      <div className="ambience-dock-head">
        <button type="button" className="ambience-toggle" onClick={() => changeMode(mode === "expanded" ? "compact" : "expanded")}><span>♫</span><b>Ambientación</b><small>{mode === "expanded" ? sourceLabel : `Abrir · ${sourceLabel}`}</small></button>
        <button type="button" className="ambience-window-button" onClick={() => changeMode("compact")} aria-label="Minimizar reproductor" title="Minimizar">−</button>
        <button type="button" className="ambience-window-button" onClick={() => changeMode("hidden")} aria-label="Ocultar reproductor" title="Ocultar">×</button>
      </div>
      <div className="ambience-dock-body">
        <nav className="ambience-source-tabs" aria-label="Fuente de audio">
          <button type="button" className={activeSource === "spotify" ? "active" : ""} onClick={() => selectSource("spotify")}><span>Spotify</span><small>{spotifyEmbed ? "listo" : "configurar"}</small></button>
          <button type="button" className={activeSource === "youtube" ? "active" : ""} onClick={() => selectSource("youtube")}><span>YouTube</span><small>{youtubeEmbed ? "privacidad" : "configurar"}</small></button>
          <button type="button" className={activeSource === "local" ? "active" : ""} onClick={() => selectSource("local")}><span>Local</span><small>{localTracks.length ? `${localTracks.length} archivo${localTracks.length === 1 ? "" : "s"}` : "cargar"}</small></button>
        </nav>

        <div className="ambience-player-stage">
          {activeSource === "spotify" && spotifyEmbed && <iframe src={spotifyEmbed} title="Playlist de ambientación en Spotify" loading="lazy" allow="autoplay; clipboard-write; encrypted-media; fullscreen; picture-in-picture" />}
          {activeSource === "youtube" && youtubeEmbed && <iframe className="youtube-player" src={youtubeEmbed} title="Ambientación en YouTube" loading="lazy" allow="autoplay; encrypted-media; picture-in-picture; fullscreen" allowFullScreen />}
          {activeSource === "local" && activeTrack && (activeTrack.kind === "audio"
            ? <audio key={activeTrack.id} src={activeTrack.url} controls preload="metadata" onEnded={playNextLocal} onError={() => setMediaError(`No fue posible reproducir ${activeTrack.name} con el motor del navegador.`)} />
            : <video key={activeTrack.id} src={activeTrack.url} controls playsInline preload="metadata" onEnded={playNextLocal} onError={() => setMediaError(activeTrack.extension === "vob" ? "VOB fue cargado, pero Chromium no incluye siempre el códec MPEG-2. La edición C# usará un motor multimedia nativo para reproducirlo." : `No fue posible reproducir ${activeTrack.name} con el motor del navegador.`)} />)}
          {activeSource === "local" && !activeTrack && <button type="button" className="ambience-empty-local" onClick={() => fileInputRef.current?.click()}><span>＋</span>Cargar MP3, MP4, OGG o VOB<small>Los archivos permanecen en tu equipo.</small></button>}
        </div>

        {activeSource === "local" && localTracks.length > 0 && <div className="ambience-local-list">{localTracks.map((track) => <div className={track.id === activeTrack?.id ? "active" : ""} key={track.id}><button type="button" onClick={() => { setActiveTrackId(track.id); setMediaError(""); }}><span>{track.kind === "audio" ? "♪" : "▶"}</span><b>{track.name}</b><small>{track.extension.toLocaleUpperCase("es")} · {formatFileSize(track.size)}</small></button><button type="button" onClick={() => removeLocalTrack(track)} aria-label={`Quitar ${track.name}`}>×</button></div>)}</div>}
        {mediaError && <p className="ambience-media-error" role="alert">{mediaError}</p>}
        <footer className="ambience-dock-actions"><label><input ref={fileInputRef} type="file" multiple accept=".mp3,.mp4,.ogg,.vob,audio/mpeg,audio/mp3,audio/ogg,video/mp4,video/ogg,video/mpeg" onChange={(event) => { addLocalFiles(event.target.files); event.target.value = ""; }} /><span>＋ Archivos locales</span></label></footer>
      </div>
    </aside>
  );
}

function DossierDrawer({ character, relationships, characters, onClose, onEdit, onPhoto, onOpen }: { character: CharacterRecord; relationships: RelationshipRecord[]; characters: CharacterRecord[]; onClose: () => void; onEdit: (character: CharacterRecord) => void; onPhoto: (character: CharacterRecord) => void; onOpen: (id: string) => void }) {
  useEscape(onClose);
  return (
    <div className="drawer-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <aside className="dossier-drawer" role="dialog" aria-modal="true" aria-labelledby="dossier-title">
        <div className="drawer-head"><span>FICHA DE PERSONAJE</span><button onClick={onClose} aria-label="Cerrar ficha">×</button></div>
        <div className="dossier-identity"><div className={`dossier-portrait tone-${character.color}`}>{character.imageUrl ? <img src={character.imageUrl} alt={`Retrato de ${character.name}`} /> : initials(character.name)}</div><div><small>{character.category} · {character.status}</small><h2 id="dossier-title">{character.name}</h2><p>{character.aliases.join(" · ") || "Sin alias registrados"}</p></div></div>
        <div className="dossier-actions"><div><button onClick={() => onEdit(character)}>Editar ficha</button><button className="secondary" onClick={() => onPhoto(character)}>{character.imageUrl ? "Cambiar foto" : "Añadir foto"}</button></div><span>{character.presence.reduce((sum, value) => sum + value, 0)} menciones en el manuscrito</span></div>
        <section className="dossier-lead"><p>{character.summary}</p></section>
        <dl className="dossier-facts"><div><dt>Rol</dt><dd>{character.role}</dd></div><div><dt>Ocupación</dt><dd>{character.occupation}</dd></div><div><dt>Origen</dt><dd>{character.origin}</dd></div><div><dt>Afiliación</dt><dd>{character.affiliation}</dd></div></dl>
        <section className="dossier-section"><h3>Antecedentes</h3><p>{character.background}</p></section>
        <section className="dossier-section"><h3>Apariencia</h3><p>{character.physical}</p></section>
        <section className="dossier-section"><h3>Rasgos</h3><div className="tag-list">{character.traits.map((trait) => <span key={trait}>{trait}</span>)}</div></section>
        <section className="dossier-section"><h3>Referencias del manuscrito</h3><div className="evidence-list">{character.evidence.map((item, index) => <article key={index}><b>{item.chapter}</b><p>{item.text}</p></article>)}</div></section>
        <section className="dossier-section"><h3>Conexiones</h3><div className="dossier-relations">{relationships.map((rel) => { const otherId = rel.source === character.id ? rel.target : rel.source; const other = characters.find((item) => item.id === otherId); return <button key={rel.id} onClick={() => other && onOpen(other.id)}><i className={rel.certainty === "Hipótesis" ? "theory" : "fact"} /><span><b>{other?.name}</b><small>{rel.type} · {rel.label}</small></span><em>→</em></button>; })}</div></section>
      </aside>
    </div>
  );
}

function PhotoEditor({ character, onCancel, onSave }: { character: CharacterRecord; onCancel: () => void; onSave: (character: CharacterRecord) => void }) {
  const [file, setFile] = useState<File | null>(null);
  const [filePreviewUrl, setFilePreviewUrl] = useState("");
  const previewObjectRef = useRef("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  useEscape(() => { if (!busy) onCancel(); });

  useEffect(() => {
    return () => {
      if (previewObjectRef.current) URL.revokeObjectURL(previewObjectRef.current);
    };
  }, []);

  const chooseFile = (nextFile: File | null) => {
    if (previewObjectRef.current) URL.revokeObjectURL(previewObjectRef.current);
    const nextPreview = nextFile ? URL.createObjectURL(nextFile) : "";
    previewObjectRef.current = nextPreview;
    setFilePreviewUrl(nextPreview);
    setFile(nextFile);
    setError("");
  };

  const previewUrl = filePreviewUrl || character.imageUrl || "";

  const upload = async () => {
    if (!file) {
      setError("Selecciona una fotografía antes de guardarla.");
      return;
    }
    if (file.size > 5 * 1024 * 1024) {
      setError("La fotografía supera el límite de 5 MB.");
      return;
    }

    setBusy(true);
    setError("");
    try {
      onSave({ ...character, imageUrl: await imageFileToDataUrl(file, 1_200, 1_200, `fotografía de ${character.name}`) });
    } catch (uploadError) {
      setError(uploadError instanceof Error ? uploadError.message : "No se pudo guardar la fotografía.");
      setBusy(false);
    }
  };

  const remove = () => {
    setBusy(true);
    setError("");
    onSave({ ...character, imageUrl: undefined });
  };

  return (
    <div className="editor-backdrop" role="presentation" onMouseDown={(event) => { if (!busy && event.target === event.currentTarget) onCancel(); }}>
      <form className="editor-modal compact photo-modal" role="dialog" aria-modal="true" aria-labelledby="photo-editor-title" onSubmit={(event) => { event.preventDefault(); void upload(); }}>
        <div className="editor-head"><div><span>RETRATO DE LA FICHA</span><h2 id="photo-editor-title">Foto de {character.name}</h2></div><button type="button" disabled={busy} onClick={onCancel} aria-label="Cerrar editor">×</button></div>
        <div className="editor-body photo-editor">
          <div className={`photo-preview tone-${character.color}`}>{previewUrl ? <img src={previewUrl} alt={`Vista previa de ${character.name}`} /> : <span>{initials(character.name)}</span>}</div>
          <label className="photo-drop" htmlFor={`photo-${character.id}`}>
            <b>{file ? file.name : character.imageUrl ? "Reemplazar fotografía" : "Seleccionar fotografía"}</b>
            <span>JPG, PNG, WEBP o GIF · máximo 5 MB</span>
          </label>
          <input id={`photo-${character.id}`} className="photo-input" type="file" accept="image/jpeg,image/png,image/webp,image/gif" onChange={(event) => chooseFile(event.target.files?.[0] ?? null)} />
          <p>La imagen se usará en el tablero, la biblioteca de personajes y la ficha completa.</p>
          {error && <p className="editor-error" role="alert">{error}</p>}
        </div>
        <div className="editor-actions photo-actions">
          {character.imageUrl && <button type="button" className="danger-action" disabled={busy} onClick={remove}>Quitar foto</button>}
          <span />
          <button type="button" disabled={busy} onClick={onCancel}>Cancelar</button>
          <button type="submit" disabled={busy || !file}>{busy ? "Guardando…" : "Guardar fotografía"}</button>
        </div>
      </form>
    </div>
  );
}

function CharacterEditor({ character, chapterNames, onCancel, onSave }: { character: CharacterRecord; chapterNames: string[]; onCancel: () => void; onSave: (character: CharacterRecord) => void }) {
  const [draft, setDraft] = useState(character);
  const [evidenceText, setEvidenceText] = useState(() => evidenceToText(character.evidence));
  useEscape(onCancel);
  const patch = <K extends keyof CharacterRecord>(key: K, value: CharacterRecord[K]) => setDraft((current) => ({ ...current, [key]: value }));
  return (
    <EditorShell title="Editar ficha de personaje" onCancel={onCancel} onSubmit={() => onSave({ ...draft, evidence: textToEvidence(evidenceText) })}>
      <div className="editor-grid">
        <label className="wide"><span>Nombre</span><input value={draft.name} onChange={(e) => patch("name", e.target.value)} required /></label>
        <label><span>Categoría</span><select value={draft.category} onChange={(e) => patch("category", e.target.value as CharacterRecord["category"])}>{categories.map((item) => <option key={item}>{item}</option>)}</select></label>
        <label><span>Estado</span><select value={draft.status} onChange={(e) => patch("status", e.target.value as CharacterRecord["status"])}>{statuses.map((item) => <option key={item}>{item}</option>)}</select></label>
        <label className="wide"><span>Alias, separados por comas</span><input value={draft.aliases.join(", ")} onChange={(e) => patch("aliases", e.target.value.split(",").map((item) => item.trim()).filter(Boolean))} /></label>
        <label className="wide"><span>Rol narrativo</span><input value={draft.role} onChange={(e) => patch("role", e.target.value)} /></label>
        <label><span>Ocupación</span><input value={draft.occupation} onChange={(e) => patch("occupation", e.target.value)} /></label>
        <label><span>Origen</span><input value={draft.origin} onChange={(e) => patch("origin", e.target.value)} /></label>
        <label className="wide"><span>Afiliación</span><input value={draft.affiliation} onChange={(e) => patch("affiliation", e.target.value)} /></label>
        <label className="wide"><span>Resumen de ficha</span><textarea rows={3} value={draft.summary} onChange={(e) => patch("summary", e.target.value)} /></label>
        <label className="wide"><span>Antecedentes completos</span><textarea rows={6} value={draft.background} onChange={(e) => patch("background", e.target.value)} /></label>
        <label className="wide"><span>Apariencia</span><textarea rows={3} value={draft.physical} onChange={(e) => patch("physical", e.target.value)} /></label>
        <label className="wide"><span>Rasgos, separados por comas</span><input value={draft.traits.join(", ")} onChange={(e) => patch("traits", e.target.value.split(",").map((item) => item.trim()).filter(Boolean))} /></label>
        <label className="wide"><span>Referencias narrativas: una por línea como “Capítulo | detalle”</span><textarea rows={5} value={evidenceText} onChange={(e) => setEvidenceText(e.target.value)} /></label>
        <fieldset className="wide chapter-inputs" style={{ "--chapter-count": Math.min(Math.max(chapterNames.length, 1), 13) } as React.CSSProperties}><legend>Presencia por capítulo</legend>{chapterNames.map((label, index) => <label key={`${label}-${index}`}><span>{label}</span><input type="number" min="0" value={draft.presence[index] ?? 0} onChange={(e) => { const next = [...draft.presence]; next[index] = Math.max(0, Number(e.target.value)); patch("presence", next); }} /></label>)}</fieldset>
      </div>
    </EditorShell>
  );
}

function RelationshipEditor({ relationship, characters, onCancel, onSave, onDelete }: { relationship: RelationshipRecord; characters: CharacterRecord[]; onCancel: () => void; onSave: (relationship: RelationshipRecord) => void; onDelete?: () => void }) {
  const [draft, setDraft] = useState(relationship);
  const [error, setError] = useState("");
  useEscape(onCancel);
  const submit = () => {
    if (draft.source === draft.target) {
      setError("Elige dos fichas diferentes para crear la conexión.");
      return;
    }
    if (!draft.label.trim()) {
      setError("Añade una etiqueta breve que explique la conexión.");
      return;
    }
    onSave({ ...draft, label: draft.label.trim(), details: draft.details.trim() });
  };
  return (
    <EditorShell title="Definir conexión" onCancel={onCancel} onSubmit={submit} onDelete={onDelete} compact>
      <div className="editor-grid">
        <label><span>Origen</span><select value={draft.source} onChange={(e) => setDraft({ ...draft, source: e.target.value })}>{characters.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}</select></label>
        <label><span>Destino</span><select value={draft.target} onChange={(e) => setDraft({ ...draft, target: e.target.value })}>{characters.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}</select></label>
        <fieldset className="wide relationship-kind-picker">
          <legend>¿Qué tipo de vínculo tienen?</legend>
          <div>{relationshipTypes.map((item) => <button type="button" className={`relationship-type-option ${relationshipVisuals[item].dash ? "segmented" : ""} ${draft.type === item ? "active" : ""}`} style={{ "--relationship-color": relationshipVisuals[item].color } as React.CSSProperties} key={item} onClick={() => setDraft({ ...draft, type: item })} aria-pressed={draft.type === item}>{item}</button>)}</div>
        </fieldset>
        <label><span>Certeza</span><select value={draft.certainty} onChange={(e) => setDraft({ ...draft, certainty: e.target.value as RelationshipRecord["certainty"] })}><option>Hecho</option><option>Hipótesis</option></select></label>
        <label className="wide"><span>Etiqueta breve</span><input required value={draft.label} onChange={(e) => { setError(""); setDraft({ ...draft, label: e.target.value }); }} /></label>
        <label className="wide"><span>Detalle</span><textarea rows={5} value={draft.details} onChange={(e) => setDraft({ ...draft, details: e.target.value })} /></label>
        {error && <p className="editor-error wide" role="alert">{error}</p>}
      </div>
    </EditorShell>
  );
}

function TheoryEditor({ theory, characters, onCancel, onSave }: { theory: TheoryRecord; characters: CharacterRecord[]; onCancel: () => void; onSave: (theory: TheoryRecord) => void }) {
  const [draft, setDraft] = useState(theory);
  const [evidenceText, setEvidenceText] = useState(() => evidenceToText(theory.evidence));
  useEscape(onCancel);
  const toggleCharacter = (id: string) => setDraft((current) => ({ ...current, characterIds: current.characterIds.includes(id) ? current.characterIds.filter((item) => item !== id) : [...current.characterIds, id] }));
  return (
    <EditorShell title="Editar hilo" onCancel={onCancel} onSubmit={() => onSave({ ...draft, evidence: textToEvidence(evidenceText) })}>
      <div className="editor-grid">
        <label className="wide"><span>Título</span><input value={draft.title} onChange={(e) => setDraft({ ...draft, title: e.target.value })} /></label>
        <label><span>Estado</span><select value={draft.status} onChange={(e) => setDraft({ ...draft, status: e.target.value as TheoryRecord["status"] })}><option>Confirmada</option><option>Muy probable</option><option>Abierta</option><option>Descartada</option></select></label>
        <label><span>Confianza: {draft.confidence}%</span><input type="range" min="0" max="100" value={draft.confidence} onChange={(e) => setDraft({ ...draft, confidence: Number(e.target.value) })} /></label>
        <label className="wide"><span>Tesis</span><textarea rows={5} value={draft.thesis} onChange={(e) => setDraft({ ...draft, thesis: e.target.value })} /></label>
        <label className="wide"><span>Referencias: una por línea como “Capítulo | detalle”</span><textarea rows={6} value={evidenceText} onChange={(e) => setEvidenceText(e.target.value)} /></label>
        <label className="wide"><span>Objeción o punto débil</span><textarea rows={4} value={draft.counterpoint} onChange={(e) => setDraft({ ...draft, counterpoint: e.target.value })} /></label>
        <label className="wide"><span>Etiquetas, separadas por comas</span><input value={draft.tags.join(", ")} onChange={(e) => setDraft({ ...draft, tags: e.target.value.split(",").map((item) => item.trim()).filter(Boolean) })} /></label>
        <fieldset className="wide character-picker"><legend>Personajes vinculados</legend>{characters.map((character) => <label key={character.id}><input type="checkbox" checked={draft.characterIds.includes(character.id)} onChange={() => toggleCharacter(character.id)} /><span>{character.name}</span></label>)}</fieldset>
      </div>
    </EditorShell>
  );
}

function TimelineEditor({ timelineEvent, characters, onCancel, onSave, onDelete }: { timelineEvent: TimelineEvent; characters: CharacterRecord[]; onCancel: () => void; onSave: (event: TimelineEvent) => void; onDelete?: () => void }) {
  const [draft, setDraft] = useState(timelineEvent);
  useEscape(onCancel);
  const toggleCharacter = (id: string) => setDraft((current) => ({ ...current, characterIds: current.characterIds.includes(id) ? current.characterIds.filter((item) => item !== id) : [...current.characterIds, id] }));
  return (
    <EditorShell title="Editar suceso de la cronología" onCancel={onCancel} onSubmit={() => onSave(draft)} onDelete={onDelete}>
      <div className="editor-grid">
        <label><span>Capítulo o sección</span><input value={draft.chapter} onChange={(event) => setDraft({ ...draft, chapter: event.target.value })} required /></label>
        <label><span>Momento</span><input value={draft.when} onChange={(event) => setDraft({ ...draft, when: event.target.value })} placeholder="Ej.: antes del amanecer" /></label>
        <label className="wide"><span>Título del suceso</span><input value={draft.title} onChange={(event) => setDraft({ ...draft, title: event.target.value })} required /></label>
        <label className="wide"><span>Resumen</span><textarea rows={5} value={draft.summary} onChange={(event) => setDraft({ ...draft, summary: event.target.value })} /></label>
        <label className="wide"><span>Intensidad narrativa: {draft.intensity}/5</span><input type="range" min="1" max="5" step="1" value={draft.intensity} onChange={(event) => setDraft({ ...draft, intensity: Number(event.target.value) })} /></label>
        <fieldset className="wide character-picker"><legend>Personajes vinculados</legend>{characters.map((character) => <label key={character.id}><input type="checkbox" checked={draft.characterIds.includes(character.id)} onChange={() => toggleCharacter(character.id)} /><span>{character.name}</span></label>)}</fieldset>
      </div>
    </EditorShell>
  );
}

function WorldEditor({ projectId, record, onCancel, onSave, onDelete }: { projectId: string; record: WorldRecord; onCancel: () => void; onSave: (record: WorldRecord) => void; onDelete?: () => void }) {
  const [draft, setDraft] = useState(record);
  useEscape(onCancel);
  const patch = <K extends keyof WorldRecord>(key: K, value: WorldRecord[K]) => setDraft((current) => ({ ...current, [key]: value }));
  return (
    <EditorShell title="Editar entrada del mundo" onCancel={onCancel} onSubmit={() => onSave(draft)} onDelete={onDelete}>
      <div className="editor-grid world-editor-grid">
        <label className="wide"><span>Nombre</span><input value={draft.name} onChange={(event) => patch("name", event.target.value)} required /></label>
        <label><span>Tipo de entrada</span><select value={draft.kind} onChange={(event) => patch("kind", event.target.value as WorldRecord["kind"])}>{worldKinds.map((item) => <option key={item}>{item}</option>)}</select></label>
        <label><span>Alias, separados por comas</span><input value={draft.aliases.join(", ")} onChange={(event) => patch("aliases", event.target.value.split(",").map((item) => item.trim()).filter(Boolean))} /></label>
        <label className="wide"><span>Resumen</span><textarea rows={3} value={draft.summary} onChange={(event) => patch("summary", event.target.value)} /></label>
        <label className="wide"><span>Geografía y clima</span><textarea rows={4} value={draft.geography} onChange={(event) => patch("geography", event.target.value)} /></label>
        <label className="wide"><span>Gobierno y leyes</span><textarea rows={4} value={draft.government} onChange={(event) => patch("government", event.target.value)} /></label>
        <label><span>Pueblos y grupos sociales</span><textarea rows={4} value={draft.peoples} onChange={(event) => patch("peoples", event.target.value)} /></label>
        <label><span>Cultura y costumbres</span><textarea rows={4} value={draft.culture} onChange={(event) => patch("culture", event.target.value)} /></label>
        <label><span>Economía y comercio</span><textarea rows={4} value={draft.economy} onChange={(event) => patch("economy", event.target.value)} /></label>
        <label><span>Moneda</span><textarea rows={4} value={draft.currency} onChange={(event) => patch("currency", event.target.value)} /></label>
        <label><span>Lenguas</span><textarea rows={3} value={draft.languages} onChange={(event) => patch("languages", event.target.value)} /></label>
        <label><span>Religiones y creencias</span><textarea rows={3} value={draft.religions} onChange={(event) => patch("religions", event.target.value)} /></label>
        <label><span>Fuerzas armadas</span><textarea rows={3} value={draft.military} onChange={(event) => patch("military", event.target.value)} /></label>
        <label><span>Historia</span><textarea rows={3} value={draft.history} onChange={(event) => patch("history", event.target.value)} /></label>
        <label><span>Relaciones internacionales</span><textarea rows={4} value={draft.relations} onChange={(event) => patch("relations", event.target.value)} /></label>
        <label><span>Lugares importantes</span><textarea rows={4} value={draft.locations} onChange={(event) => patch("locations", event.target.value)} /></label>
        <label className="wide"><span>Conflictos y tensiones</span><textarea rows={4} value={draft.conflicts} onChange={(event) => patch("conflicts", event.target.value)} /></label>
        <label className="wide"><span>Notas de continuidad</span><textarea rows={4} value={draft.notes} onChange={(event) => patch("notes", event.target.value)} /></label>
        <AttachmentManager projectId={projectId} scope="world" recordId={draft.id} attachments={draft.attachments ?? []} onChange={(attachments) => patch("attachments", attachments)} />
        <label className="wide"><span>Etiquetas, separadas por comas</span><input value={draft.tags.join(", ")} onChange={(event) => patch("tags", event.target.value.split(",").map((item) => item.trim()).filter(Boolean))} /></label>
      </div>
    </EditorShell>
  );
}

function MagicEditor({ projectId, system, onCancel, onSave, onDelete }: { projectId: string; system: MagicSystemRecord; onCancel: () => void; onSave: (system: MagicSystemRecord) => void; onDelete?: () => void }) {
  const [draft, setDraft] = useState(system);
  const [evidenceText, setEvidenceText] = useState(() => evidenceToText(system.evidence));
  useEscape(onCancel);
  const patch = <K extends keyof MagicSystemRecord>(key: K, value: MagicSystemRecord[K]) => setDraft((current) => ({ ...current, [key]: value }));
  return (
    <EditorShell title="Editar sistema de magia" onCancel={onCancel} onSubmit={() => onSave({ ...draft, evidence: textToEvidence(evidenceText) })} onDelete={onDelete}>
      <div className="editor-grid magic-editor-grid">
        <label className="wide"><span>Nombre del sistema</span><input value={draft.name} onChange={(event) => patch("name", event.target.value)} required /></label>
        <label><span>Categoría o dominio</span><input value={draft.category} onChange={(event) => patch("category", event.target.value)} /></label>
        <label><span>Estado</span><select value={draft.status} onChange={(event) => patch("status", event.target.value as MagicSystemRecord["status"])}>{magicStatuses.map((item) => <option key={item}>{item}</option>)}</select></label>
        <label className="wide"><span>Fuente o documento</span><input value={draft.source} onChange={(event) => patch("source", event.target.value)} /></label>
        <label className="wide"><span>Principio fundamental</span><textarea rows={4} value={draft.principle} onChange={(event) => patch("principle", event.target.value)} /></label>
        <label><span>Quién accede y cómo</span><textarea rows={4} value={draft.access} onChange={(event) => patch("access", event.target.value)} /></label>
        <label><span>Costo</span><textarea rows={4} value={draft.cost} onChange={(event) => patch("cost", event.target.value)} /></label>
        <label><span>Límites y reglas</span><textarea rows={4} value={draft.limits} onChange={(event) => patch("limits", event.target.value)} /></label>
        <label><span>Manifestaciones</span><textarea rows={4} value={draft.manifestations} onChange={(event) => patch("manifestations", event.target.value)} /></label>
        <label><span>Materiales, focos o símbolos</span><textarea rows={4} value={draft.materials} onChange={(event) => patch("materials", event.target.value)} /></label>
        <label><span>Instituciones</span><textarea rows={4} value={draft.institutions} onChange={(event) => patch("institutions", event.target.value)} /></label>
        <label><span>Usuarios conocidos</span><textarea rows={4} value={draft.users} onChange={(event) => patch("users", event.target.value)} /></label>
        <label><span>Riesgos y consecuencias</span><textarea rows={4} value={draft.risks} onChange={(event) => patch("risks", event.target.value)} /></label>
        <label><span>Historia del sistema</span><textarea rows={4} value={draft.history} onChange={(event) => patch("history", event.target.value)} /></label>
        <label className="wide"><span>Notas de continuidad</span><textarea rows={4} value={draft.notes} onChange={(event) => patch("notes", event.target.value)} /></label>
        <label className="wide"><span>Fuentes narrativas: una por línea como “Capítulo o fuente | detalle”</span><textarea rows={5} value={evidenceText} onChange={(event) => setEvidenceText(event.target.value)} /></label>
        <AttachmentManager projectId={projectId} scope="magic" recordId={draft.id} attachments={draft.attachments ?? []} onChange={(attachments) => patch("attachments", attachments)} />
        <label className="wide"><span>Etiquetas, separadas por comas</span><input value={draft.tags.join(", ")} onChange={(event) => patch("tags", event.target.value.split(",").map((item) => item.trim()).filter(Boolean))} /></label>
      </div>
    </EditorShell>
  );
}


function MapEditor({ map, onCancel, onSave, onDelete }: { map: WorldMapRecord; onCancel: () => void; onSave: (map: WorldMapRecord) => void; onDelete?: () => void }) {
  const [draft, setDraft] = useState<WorldMapRecord>(() => ({ ...map, markers: map.markers.map((marker) => ({ ...marker })) }));
  const [imageBusy, setImageBusy] = useState(false);
  const [imageError, setImageError] = useState("");
  useEscape(onCancel);
  const addMarker = (x: number, y: number) => setDraft((current) => ({ ...current, markers: [...current.markers, { id: nextId("lugar"), label: `Lugar ${current.markers.length + 1}`, x, y, kind: "Lugar" }] }));
  const patchMarker = <K extends keyof WorldMapMarker>(id: string, key: K, value: WorldMapMarker[K]) => setDraft((current) => ({ ...current, markers: current.markers.map((marker) => marker.id === id ? { ...marker, [key]: value } : marker) }));
  const loadBackground = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    setImageBusy(true);
    setImageError("");
    try {
      const dataUrl = await imageFileToDataUrl(file, 1_800, 1_200, "imagen del mapa");
      setDraft((current) => ({ ...current, backgroundImageDataUrl: dataUrl }));
    } catch (error) {
      setImageError(error instanceof Error ? error.message : "No se pudo cargar el mapa.");
    } finally {
      setImageBusy(false);
    }
  };
  return (
    <EditorShell title="Editor visual de mapa" onCancel={onCancel} onSubmit={() => onSave(draft)} onDelete={onDelete}>
      <div className="map-editor-layout">
        <div className="map-editor-controls">
          <label><span>Nombre del mapa</span><input value={draft.name} onChange={(event) => setDraft({ ...draft, name: event.target.value })} required /></label>
          <label><span>Descripción</span><textarea rows={3} value={draft.description} onChange={(event) => setDraft({ ...draft, description: event.target.value })} /></label>
          <div className="map-generator-controls"><label><span>Semilla</span><input type="number" value={draft.seed} onChange={(event) => setDraft({ ...draft, seed: Math.round(Number(event.target.value)) || 1 })} /></label><button type="button" className="secondary-action" onClick={() => setDraft({ ...draft, seed: Math.floor(Math.random() * 9_000_000) + 1_000_000, backgroundImageDataUrl: "" })}>Regenerar</button></div>
          <label><span>Estilo</span><select value={draft.style} onChange={(event) => setDraft({ ...draft, style: event.target.value as WorldMapRecord["style"] })}><option>Pergamino</option><option>Atlas</option><option>Nocturno</option></select></label>
          <label><span>Continentes: {draft.continents}</span><input type="range" min="1" max="6" value={draft.continents} onChange={(event) => setDraft({ ...draft, continents: Number(event.target.value), backgroundImageDataUrl: "" })} /></label>
          <label><span>Islas: {draft.islands}</span><input type="range" min="0" max="8" value={draft.islands} onChange={(event) => setDraft({ ...draft, islands: Number(event.target.value), backgroundImageDataUrl: "" })} /></label>
          <label><span>Costa irregular: {draft.roughness}%</span><input type="range" min="0" max="100" value={draft.roughness} onChange={(event) => setDraft({ ...draft, roughness: Number(event.target.value), backgroundImageDataUrl: "" })} /></label>
          <div className="map-upload-control"><span>MAPA PROPIO</span><p>Carga una imagen como base. Los lugares seguirán siendo editables encima.</p>{imageError && <small>{imageError}</small>}<div><label className="secondary-action import-text-action"><input type="file" accept="image/jpeg,image/png,image/webp,image/gif" onChange={(event) => void loadBackground(event)} disabled={imageBusy} /><span>{imageBusy ? "Preparando…" : draft.backgroundImageDataUrl ? "Cambiar imagen" : "Cargar imagen"}</span></label>{draft.backgroundImageDataUrl && <button type="button" className="danger-action" onClick={() => setDraft({ ...draft, backgroundImageDataUrl: "" })}>Volver al generador</button>}</div></div>
        </div>
        <div className="map-editor-visual"><div className="map-editor-preview"><WorldMapCanvas map={draft} onAddMarker={addMarker} /></div><p>Haz clic en el mapa para añadir un lugar. Después cambia su nombre y tipo.</p><div className="map-marker-list">{draft.markers.map((marker) => <article key={marker.id}><input value={marker.label} onChange={(event) => patchMarker(marker.id, "label", event.target.value)} aria-label="Nombre del lugar" /><select value={marker.kind} onChange={(event) => patchMarker(marker.id, "kind", event.target.value as WorldMapMarker["kind"])}>{["Capital", "Ciudad", "Ruina", "Puerto", "Fortaleza", "Lugar"].map((kind) => <option key={kind}>{kind}</option>)}</select><small>{marker.x.toFixed(1)}, {marker.y.toFixed(1)}</small><button type="button" onClick={() => setDraft((current) => ({ ...current, markers: current.markers.filter((item) => item.id !== marker.id) }))} aria-label={`Quitar ${marker.label}`}>×</button></article>)}{draft.markers.length === 0 && <span>No hay lugares señalados todavía.</span>}</div></div>
      </div>
    </EditorShell>
  );
}

function ProjectEditor({ archive, onCancel, onSave }: { archive: ArchiveState; onCancel: () => void; onSave: (profile: ProjectProfile, manuscript: ArchiveState["manuscript"]) => void }) {
  const [profile, setProfile] = useState<ProjectProfile>({ ...archive.profile });
  const [manuscript, setManuscript] = useState({ ...archive.manuscript });
  const [coverBusy, setCoverBusy] = useState(false);
  const [bannerBusy, setBannerBusy] = useState(false);
  const [iconBusy, setIconBusy] = useState(false);
  const [coverError, setCoverError] = useState("");
  const [assetError, setAssetError] = useState("");
  useEscape(onCancel);
  const patchProfile = <K extends keyof ProjectProfile>(key: K, value: ProjectProfile[K]) => setProfile((current) => ({ ...current, [key]: value }));
  const loadCover = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    setCoverBusy(true);
    setCoverError("");
    try {
      patchProfile("coverImageDataUrl", await coverFileToDataUrl(file));
    } catch (error) {
      setCoverError(error instanceof Error ? error.message : "No se pudo cargar la portada.");
    } finally {
      setCoverBusy(false);
    }
  };
  const loadProjectImage = async (event: React.ChangeEvent<HTMLInputElement>, kind: "banner" | "icon") => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    const busy = kind === "banner" ? setBannerBusy : setIconBusy;
    busy(true);
    setAssetError("");
    try {
      const dataUrl = await imageFileToDataUrl(file, kind === "banner" ? 1_800 : 320, kind === "banner" ? 620 : 320, kind === "banner" ? "banner" : "icono");
      patchProfile(kind === "banner" ? "bannerImageDataUrl" : "boardIconDataUrl", dataUrl);
    } catch (error) {
      setAssetError(error instanceof Error ? error.message : "No se pudo cargar la imagen.");
    } finally {
      busy(false);
    }
  };
  return (
    <EditorShell title="Configuración de obra y espacio" onCancel={onCancel} onSubmit={() => onSave({ ...profile, theme: "desk", activeThemeId: "desk", customThemes: [], chapterLabels: profile.chapterLabels.length > 0 ? profile.chapterLabels : ["1"] }, manuscript)}>
      <div className="editor-grid project-editor-grid">
        <div className="editor-section-heading wide"><span>IDENTIDAD REUTILIZABLE</span><p>Estos textos identifican la obra y pueden cambiarse para reutilizar el proyecto con cualquier historia.</p></div>
        <label><span>Título del archivo</span><input value={profile.archiveTitle} onChange={(event) => patchProfile("archiveTitle", event.target.value)} required /></label>
        <label><span>Título de la historia</span><input value={profile.storyTitle} onChange={(event) => patchProfile("storyTitle", event.target.value)} /></label>
        <label className="wide"><span>Subtítulo o propósito</span><input value={profile.subtitle} onChange={(event) => patchProfile("subtitle", event.target.value)} /></label>
        <label className="wide"><span>Línea institucional superior</span><input value={profile.projectLabel} onChange={(event) => patchProfile("projectLabel", event.target.value)} /></label>
        <label><span>Título del tablero</span><input value={profile.homeHeading} onChange={(event) => patchProfile("homeHeading", event.target.value)} /></label>
        <label><span>Lugar principal</span><input value={profile.location} onChange={(event) => patchProfile("location", event.target.value)} /></label>
        <label><span>Autoría</span><input value={profile.author} onChange={(event) => patchProfile("author", event.target.value)} /></label>
        <label><span>Género</span><input value={profile.genre} onChange={(event) => patchProfile("genre", event.target.value)} /></label>
        <label className="wide"><span>Estado del proyecto</span><input value={profile.status} onChange={(event) => patchProfile("status", event.target.value)} /></label>
        <label className="wide"><span>Sinopsis</span><textarea rows={5} value={profile.synopsis} onChange={(event) => patchProfile("synopsis", event.target.value)} /></label>
        <div className="project-brand-assets wide"><div className="editor-section-heading"><span>APARIENCIA DEL PROYECTO</span><p>El banner y el icono identifican esta obra dentro de la interfaz de escritorio y viajan con su copia de seguridad.</p></div>{assetError && <small className="project-asset-error">{assetError}</small>}<div className="project-brand-grid"><article><span>BANNER SUPERIOR</span><div className="project-banner-preview">{profile.bannerImageDataUrl ? <img src={profile.bannerImageDataUrl} alt="Vista previa del banner" /> : <em>Sin banner</em>}</div><footer><label className="secondary-action import-text-action"><input type="file" accept="image/jpeg,image/png,image/webp,image/gif" onChange={(event) => void loadProjectImage(event, "banner")} disabled={bannerBusy} /><span>{bannerBusy ? "Preparando…" : profile.bannerImageDataUrl ? "Cambiar banner" : "Elegir banner"}</span></label>{profile.bannerImageDataUrl && <button type="button" className="danger-action" onClick={() => patchProfile("bannerImageDataUrl", "")}>Quitar</button>}</footer></article><article><span>ICONO DEL PROYECTO</span><div className="project-icon-preview">{profile.boardIconDataUrl ? <img src={profile.boardIconDataUrl} alt="Vista previa del icono" /> : <em>WW</em>}</div><footer><label className="secondary-action import-text-action"><input type="file" accept="image/jpeg,image/png,image/webp,image/gif" onChange={(event) => void loadProjectImage(event, "icon")} disabled={iconBusy} /><span>{iconBusy ? "Preparando…" : profile.boardIconDataUrl ? "Cambiar icono" : "Elegir icono"}</span></label>{profile.boardIconDataUrl && <button type="button" className="danger-action" onClick={() => patchProfile("boardIconDataUrl", "")}>Usar icono estándar</button>}</footer></article></div></div>
        <div className="project-cover-editor wide"><div><span>PORTADA PARA DOCX Y EPUB</span><p>Se guarda una copia optimizada dentro del proyecto y se incorpora a las exportaciones.</p>{coverError && <small>{coverError}</small>}<div><label className="secondary-action import-text-action"><input type="file" accept="image/jpeg,image/png,image/webp,image/gif" onChange={(event) => void loadCover(event)} disabled={coverBusy} /><span>{coverBusy ? "Preparando portada…" : profile.coverImageDataUrl ? "Cambiar portada" : "Elegir portada"}</span></label>{profile.coverImageDataUrl && <button type="button" className="danger-action" onClick={() => patchProfile("coverImageDataUrl", "")}>Quitar portada</button>}</div></div>{profile.coverImageDataUrl ? <img src={profile.coverImageDataUrl} alt="Vista previa de la portada" /> : <div className="project-cover-placeholder">Sin portada</div>}</div>
        <label className="wide"><span>Capítulos o secciones del manuscrito, separados por comas</span><input value={profile.chapterLabels.join(", ")} onChange={(event) => patchProfile("chapterLabels", event.target.value.split(",").map((item) => item.trim()).filter(Boolean))} placeholder="P, 1, 2, 3…" /></label>
        <div className="editor-section-heading wide"><span>MANUSCRITO</span><p>Metadatos visibles en la cabecera; no cambian el contenido de las fichas.</p></div>
        <label className="wide"><span>Archivo o fuente principal</span><input value={manuscript.fileName} onChange={(event) => setManuscript({ ...manuscript, fileName: event.target.value })} /></label>
        <label><span>Número de palabras</span><input type="number" min="0" value={manuscript.words} onChange={(event) => setManuscript({ ...manuscript, words: Math.max(0, Number(event.target.value)) })} /></label>
        <label><span>Capítulos</span><input type="number" min="0" value={manuscript.chapters} onChange={(event) => setManuscript({ ...manuscript, chapters: Math.max(0, Number(event.target.value)) })} /></label>
        <label className="wide"><span>Fecha o nota de actualización</span><input value={manuscript.updatedLabel} onChange={(event) => setManuscript({ ...manuscript, updatedLabel: event.target.value })} /></label>
      </div>
    </EditorShell>
  );
}

function EditorShell({ title, onCancel, onSubmit, onDelete, compact = false, children }: { title: string; onCancel: () => void; onSubmit: () => void; onDelete?: () => void; compact?: boolean; children: React.ReactNode }) {
  return (
    <div className="editor-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onCancel(); }}>
      <form className={`editor-modal ${compact ? "compact" : ""}`} role="dialog" aria-modal="true" onSubmit={(event) => { event.preventDefault(); onSubmit(); }}>
        <div className="editor-head"><div><span>ARCHIVO EDITABLE</span><h2>{title}</h2></div><button type="button" onClick={onCancel} aria-label="Cerrar editor">×</button></div>
        <div className="editor-body">{children}</div>
        <div className="editor-actions">{onDelete && <button type="button" className="danger-action" onClick={onDelete}>Eliminar</button>}<span className="action-spacer" /><button type="button" onClick={onCancel}>Cancelar</button><button type="submit">Guardar cambios</button></div>
      </form>
    </div>
  );
}

function useEscape(onClose: () => void) {
  useEffect(() => {
    const handler = (event: KeyboardEvent) => { if (event.key === "Escape") onClose(); };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [onClose]);
}

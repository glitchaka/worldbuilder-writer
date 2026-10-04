import assert from "node:assert/strict";
import test from "node:test";
import { MANUSCRIPT_HEADING_MARKER, chaptersFromImportedManuscript } from "../app/manuscript-import.ts";

test("imports every chapter when Word headings use mixed structures", () => {
  const manuscript = [
    "PRÓLOGO Guerra. Catorce años antes.",
    "Texto del prólogo.",
    "",
    "CAPÍTULO 1 Se levanta el telón.",
    "Texto del primer capítulo.",
    "",
    "CAPÍTULO 2: Carnaval.",
    "Texto del segundo capítulo.",
    "",
    "CAPÍTULO 3 — Alvargio.",
    "Texto del tercer capítulo.",
    "",
    `${MANUSCRIPT_HEADING_MARKER}Sacro Hierro.`,
    "Texto exclusivo del cuarto capítulo.",
    "",
    "CAPÍTULO 5. El Pósito.",
    "Texto del quinto capítulo.",
    "",
    "CAPÍTULO",
    "6 — Doña Clota.",
    "Texto exclusivo del sexto capítulo.",
    "",
    "INTERLUDIO I El buwano.",
    "Texto exclusivo del interludio.",
    "",
    "CAPÍTULO 7 Umbrales.",
    "Texto del séptimo capítulo.",
    "",
    "CAPÍTULO 8 Por un trago y un jamón.",
    "Texto del octavo capítulo.",
    "",
    "CAPÍTULO 9 Felisa.",
    "Texto del noveno capítulo.",
  ].join("\n");

  const result = chaptersFromImportedManuscript(manuscript, "Manuscrito de prueba", 0);

  assert.equal(result.detectedChapterHeadings, 11);
  assert.equal(result.chapters.length, 11);
  assert.deepEqual(
    result.chapters.map((chapter) => chapter.label.toLocaleLowerCase("es")),
    ["prólogo", "capítulo 1", "capítulo 2", "capítulo 3", "capítulo 4", "capítulo 5", "capítulo 6", "interludio i", "capítulo 7", "capítulo 8", "capítulo 9"],
  );
  assert.equal(result.chapters[4].title, "Sacro Hierro.");
  assert.equal(result.chapters[6].title, "Doña Clota.");
  assert.equal(result.chapters[7].title, "El buwano.");
  assert.match(result.chapters[4].scenes[0].content, /exclusivo del cuarto/u);
  assert.match(result.chapters[6].scenes[0].content, /exclusivo del sexto/u);
  assert.match(result.chapters[7].scenes[0].content, /exclusivo del interludio/u);
  assert.doesNotMatch(result.chapters[3].scenes[0].content, /cuarto/u);
});

test("keeps a styled title with the explicit chapter that immediately precedes it", () => {
  const manuscript = [
    "Capítulo 4",
    `${MANUSCRIPT_HEADING_MARKER}Sacro Hierro`,
    "El capítulo comienza aquí.",
  ].join("\n");

  const result = chaptersFromImportedManuscript(manuscript, "Prueba", 0);

  assert.equal(result.chapters.length, 1);
  assert.equal(result.chapters[0].label, "Capítulo 4");
  assert.equal(result.chapters[0].title, "Sacro Hierro");
  assert.equal(result.chapters[0].scenes[0].content, "El capítulo comienza aquí.");
});

test("treats an empty line as a scene break without splitting ordinary paragraphs", () => {
  const manuscript = [
    "Capítulo 1: La prueba",
    "Primer párrafo de la escena inicial.",
    "Segundo párrafo de la misma escena.",
    "",
    "Primer párrafo de la escena siguiente.",
    "Segundo párrafo de la escena siguiente.",
  ].join("\n");

  const result = chaptersFromImportedManuscript(manuscript, "Prueba", 0);
  const scenes = result.chapters[0].scenes;

  assert.equal(scenes.length, 2);
  assert.equal(scenes[0].content, "Primer párrafo de la escena inicial.\nSegundo párrafo de la misma escena.");
  assert.equal(scenes[1].content, "Primer párrafo de la escena siguiente.\nSegundo párrafo de la escena siguiente.");
});

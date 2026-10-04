import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const developmentPreviewMeta =
  /<meta(?=[^>]*\bname=["']codex-preview["'])(?=[^>]*\bcontent=["']development["'])[^>]*>/i;

test("renders development preview metadata", async () => {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);

  const response = await worker.fetch(
    new Request("http://localhost/", {
      headers: {
        accept: "text/html",
      },
    }),
    {
      ASSETS: {
        fetch: async () => new Response("Not found", { status: 404 }),
      },
    },
    {
      waitUntil() {},
      passThroughOnException() {},
    },
  );

  assert.equal(response.status, 200);
  assert.match(
    response.headers.get("content-type") ?? "",
    /^text\/html\b/i,
  );
  assert.match(await response.text(), developmentPreviewMeta);
});

test("keeps the desktop controls and removes the visible theme system", async () => {
  const archiveClient = await readFile(new URL("../app/archive-client.tsx", import.meta.url), "utf8");
  const projectHub = await readFile(new URL("../app/project-hub.tsx", import.meta.url), "utf8");
  const home = await readFile(new URL("../app/page.tsx", import.meta.url), "utf8");

  assert.match(archiveClient, /className="desktop-ribbon"/u);
  assert.match(archiveClient, /> Guardar<\/button>/u);
  assert.match(archiveClient, /> Configuración<\/button>/u);
  assert.match(archiveClient, /Google Drive/u);
  assert.doesNotMatch(archiveClient, /ThemeStudio|\.thm|Temas de la aplicación/u);
  assert.doesNotMatch(projectHub, /themeOptions|new-theme-picker/u);
  assert.match(home, /getChatGPTUser/u);
  assert.doesNotMatch(home, /requireChatGPTUser/u);
});

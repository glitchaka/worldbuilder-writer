import { createBlankArchive, migrateArchive } from "../lib/archive-data.ts";

const DATABASE_NAME = "archivo-neo-desktop";
const STORE_NAME = "workspace";
const HANDLE_KEY = "projects-folder";
const PROJECT_FILE = "project.json";
const nativeFetch = window.fetch.bind(window);

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8" },
  });
}

function openHandleDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DATABASE_NAME, 1);
    request.onupgradeneeded = () => {
      if (!request.result.objectStoreNames.contains(STORE_NAME)) request.result.createObjectStore(STORE_NAME);
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

export async function loadWorkspaceHandle() {
  const database = await openHandleDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(STORE_NAME, "readonly");
    const request = transaction.objectStore(STORE_NAME).get(HANDLE_KEY);
    request.onsuccess = () => resolve(request.result ?? null);
    request.onerror = () => reject(request.error);
    transaction.oncomplete = () => database.close();
  });
}

export async function storeWorkspaceHandle(handle) {
  const database = await openHandleDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(STORE_NAME, "readwrite");
    transaction.objectStore(STORE_NAME).put(handle, HANDLE_KEY);
    transaction.oncomplete = () => { database.close(); resolve(); };
    transaction.onerror = () => { database.close(); reject(transaction.error); };
  });
}

export async function workspacePermission(handle, request = false) {
  if (!handle) return false;
  const options = { mode: "readwrite" };
  if (await handle.queryPermission(options) === "granted") return true;
  return request && await handle.requestPermission(options) === "granted";
}

export async function chooseWorkspaceFolder() {
  if (!("showDirectoryPicker" in window)) {
    throw new Error("Esta edición necesita Microsoft Edge o un navegador Chromium compatible con carpetas locales.");
  }
  const handle = await window.showDirectoryPicker({ id: "archivo-neo-projects", mode: "readwrite" });
  await storeWorkspaceHandle(handle);
  return handle;
}

async function readTextFile(directory, name) {
  const handle = await directory.getFileHandle(name);
  const file = await handle.getFile();
  return file.text();
}

async function writeTextFile(directory, name, contents) {
  const handle = await directory.getFileHandle(name, { create: true });
  const writable = await handle.createWritable();
  await writable.write(contents);
  await writable.close();
}

async function removeObsoleteProjectFiles(directory) {
  try {
    await directory.removeEntry("project.backup.json");
  } catch (error) {
    if (error?.name !== "NotFoundError") throw error;
  }
}

async function readProjectDirectory(workspace, projectId) {
  const directory = await workspace.getDirectoryHandle(projectId);
  await removeObsoleteProjectFiles(directory);
  const parsed = JSON.parse(await readTextFile(directory, PROJECT_FILE));
  const state = migrateArchive(parsed.state ?? parsed);
  const now = new Date().toISOString();
  return {
    directory,
    record: {
      formatVersion: 1,
      id: parsed.id ?? projectId,
      createdAt: parsed.createdAt ?? now,
      updatedAt: parsed.updatedAt ?? now,
      state,
    },
  };
}

async function saveProjectRecord(directory, record) {
  await removeObsoleteProjectFiles(directory);
  await writeTextFile(directory, PROJECT_FILE, `${JSON.stringify(record, null, 2)}\n`);
}

function projectSummary(record) {
  const state = record.state;
  return {
    id: record.id,
    title: state.profile.archiveTitle,
    storyTitle: state.profile.storyTitle,
    genre: state.profile.genre,
    status: state.profile.status,
    theme: state.profile.theme,
    characters: state.characters.length,
    relationships: state.relationships.length,
    worldEntries: state.world.length,
    magicSystems: state.magicSystems.length,
    chapters: state.manuscript.chapters,
    words: state.manuscript.words,
    createdAt: record.createdAt,
    updatedAt: record.updatedAt,
  };
}

async function listProjects(workspace) {
  const projects = [];
  for await (const [name, handle] of workspace.entries()) {
    if (handle.kind !== "directory" || !/^project-[a-z0-9-]{8,72}$/i.test(name)) continue;
    try {
      const { record } = await readProjectDirectory(workspace, name);
      projects.push(projectSummary(record));
    } catch {
      // Ignore unrelated or incomplete folders.
    }
  }
  return projects.sort((left, right) => right.updatedAt.localeCompare(left.updatedAt));
}

function fileToDataUrl(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result));
    reader.onerror = () => reject(reader.error ?? new Error("No se pudo leer la imagen."));
    reader.readAsDataURL(file);
  });
}

async function handleProofread(init) {
  const body = JSON.parse(String(init?.body ?? "{}"));
  const text = typeof body.text === "string" ? body.text : "";
  if (!text.trim()) return json({ error: "Selecciona o escribe un fragmento para revisar." }, 400);
  const form = new URLSearchParams({ text, language: "es", enabledOnly: "false" });
  const token = window.__ARCHIVO_NEO_TOKEN__ ?? "";
  const response = await nativeFetch("/__online/proofread", {
    method: "POST",
    headers: {
      "content-type": "application/x-www-form-urlencoded",
      "x-archivo-neo-token": token,
    },
    body: form.toString(),
  });
  if (!response.ok) return json({ error: "LanguageTool no está disponible en este momento. El subrayado del navegador sigue activo." }, 502);
  const result = await response.json();
  const matches = Array.isArray(result.matches) ? result.matches.flatMap((match) => {
    if (typeof match.offset !== "number" || typeof match.length !== "number") return [];
    return [{
      offset: Math.max(0, match.offset),
      length: Math.max(0, match.length),
      message: typeof match.message === "string" ? match.message.slice(0, 500) : "",
      shortMessage: typeof match.shortMessage === "string" ? match.shortMessage.slice(0, 160) : "",
      replacements: Array.isArray(match.replacements)
        ? match.replacements.map((item) => typeof item?.value === "string" ? item.value.slice(0, 120) : "").filter(Boolean).slice(0, 8)
        : [],
      context: typeof match.context?.text === "string" ? match.context.text.slice(0, 320) : "",
    }];
  }) : [];
  return json({ matches });
}

function requestPath(input) {
  const raw = typeof input === "string" ? input : input instanceof URL ? input.href : input.url;
  return new URL(raw, window.location.href);
}

async function handleProjects(workspace, url, init) {
  const method = String(init?.method ?? "GET").toUpperCase();
  if (method === "GET") return json({ projects: await listProjects(workspace) });
  if (method === "POST") {
    const body = JSON.parse(String(init?.body ?? "{}"));
    const state = createBlankArchive(body);
    const id = `project-${crypto.randomUUID().toLowerCase()}`;
    const now = new Date().toISOString();
    const record = { formatVersion: 1, id, createdAt: now, updatedAt: now, state };
    const directory = await workspace.getDirectoryHandle(id, { create: true });
    await saveProjectRecord(directory, record);
    return json({ project: projectSummary(record) }, 201);
  }
  if (method === "DELETE") {
    const id = url.searchParams.get("projectId");
    if (!id || !/^project-[a-z0-9-]{8,72}$/i.test(id)) return json({ error: "La obra solicitada no es válida." }, 400);
    await workspace.removeEntry(id, { recursive: true });
    return json({ ok: true });
  }
  return json({ error: "Operación no disponible." }, 405);
}

async function handleArchive(workspace, url, init) {
  const method = String(init?.method ?? "GET").toUpperCase();
  if (method === "GET") {
    const id = url.searchParams.get("projectId");
    if (!id) return json({ error: "Falta el identificador de la obra." }, 400);
    const { record } = await readProjectDirectory(workspace, id);
    return json({ state: record.state, updatedAt: record.updatedAt, projectId: id });
  }
  if (method === "PUT") {
    const body = JSON.parse(String(init?.body ?? "{}"));
    const id = String(body.projectId ?? "");
    const { directory, record } = await readProjectDirectory(workspace, id);
    const updated = { ...record, updatedAt: new Date().toISOString(), state: migrateArchive(body.state) };
    await saveProjectRecord(directory, updated);
    return json({ ok: true, updatedAt: updated.updatedAt, project: projectSummary(updated) });
  }
  return json({ error: "Operación no disponible." }, 405);
}

async function handlePhoto(init) {
  const method = String(init?.method ?? "GET").toUpperCase();
  if (method === "POST" && init?.body instanceof FormData) {
    const file = init.body.get("file");
    if (!(file instanceof File)) return json({ error: "Selecciona una fotografía." }, 400);
    return json({ url: await fileToDataUrl(file) });
  }
  if (method === "DELETE") return json({ ok: true });
  return json({ error: "Fotografía no encontrada." }, 404);
}

async function handleAttachment(init) {
  const method = String(init?.method ?? "GET").toUpperCase();
  if (method === "POST" && init?.body instanceof FormData) {
    const file = init.body.get("file");
    if (!(file instanceof File)) return json({ error: "Selecciona una imagen." }, 400);
    const attachment = {
      id: `file-${crypto.randomUUID().toLowerCase()}`,
      name: file.name.replace(/[\r\n"\\]/g, "_").slice(0, 180) || "imagen",
      mimeType: file.type || "application/octet-stream",
      size: file.size,
      kind: "image",
      dataUrl: await fileToDataUrl(file),
    };
    return json({ attachment });
  }
  if (method === "DELETE") return json({ ok: true });
  return json({ error: "Imagen no encontrada." }, 404);
}

export function installDesktopStorage(workspace) {
  window.__ARCHIVO_NEO_DESKTOP__ = true;
  window.fetch = async (input, init) => {
    const url = requestPath(input);
    if (url.origin !== window.location.origin || !url.pathname.startsWith("/api/")) return nativeFetch(input, init);
    try {
      if (url.pathname === "/api/projects") return await handleProjects(workspace, url, init);
      if (url.pathname === "/api/archive") return await handleArchive(workspace, url, init);
      if (url.pathname === "/api/photos") return await handlePhoto(init);
      if (url.pathname === "/api/attachments") return await handleAttachment(init);
      if (url.pathname === "/api/proofread") return await handleProofread(init);
      return json({ error: "Servicio local no disponible." }, 404);
    } catch (error) {
      return json({ error: error instanceof Error ? error.message : "No se pudo acceder a la carpeta de proyectos." }, 500);
    }
  };
}

export { nativeFetch };

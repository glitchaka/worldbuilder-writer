"use client";

import {
  migrateArchive,
  type ArchiveState,
} from "../lib/archive-data";
import type { ArchiveProjectSummary } from "../lib/archive-projects";

const DATABASE_NAME = "worldbuilder-writer-local";
const DATABASE_VERSION = 1;
const PROJECT_STORE = "projects";

export type LocalProjectRecord = {
  id: string;
  state: ArchiveState;
  createdAt: string;
  updatedAt: string;
};

function requestResult<T>(request: IDBRequest<T>) {
  return new Promise<T>((resolve, reject) => {
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error("No se pudo acceder al almacenamiento local."));
  });
}

function transactionDone(transaction: IDBTransaction) {
  return new Promise<void>((resolve, reject) => {
    transaction.oncomplete = () => resolve();
    transaction.onerror = () => reject(transaction.error ?? new Error("No se pudo completar el guardado local."));
    transaction.onabort = () => reject(transaction.error ?? new Error("El guardado local fue cancelado."));
  });
}

async function openDatabase() {
  if (typeof indexedDB === "undefined") throw new Error("Este navegador no permite guardar proyectos localmente.");
  const request = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);
  request.onupgradeneeded = () => {
    const database = request.result;
    if (!database.objectStoreNames.contains(PROJECT_STORE)) {
      const store = database.createObjectStore(PROJECT_STORE, { keyPath: "id" });
      store.createIndex("updatedAt", "updatedAt");
    }
  };
  return requestResult(request);
}

function chapterWords(state: ArchiveState) {
  if (!state.writingChapters.length) return state.manuscript.words;
  return state.writingChapters.reduce((projectTotal, chapter) => projectTotal + chapter.scenes.reduce((chapterTotal, scene) => {
    const words = scene.content.trim().match(/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu);
    return chapterTotal + (words?.length ?? 0);
  }, 0), 0);
}

export function localProjectSummary(record: LocalProjectRecord): ArchiveProjectSummary {
  const state = migrateArchive(record.state);
  return {
    id: record.id,
    title: state.profile.archiveTitle || state.title,
    storyTitle: state.profile.storyTitle,
    genre: state.profile.genre,
    status: state.profile.status,
    theme: "desk",
    characters: state.characters.length,
    relationships: state.relationships.length,
    worldEntries: state.world.length,
    magicSystems: state.magicSystems.length,
    chapters: state.writingChapters.length || state.manuscript.chapters,
    words: chapterWords(state),
    createdAt: record.createdAt,
    updatedAt: record.updatedAt,
  };
}

export async function listLocalProjects() {
  const database = await openDatabase();
  try {
    const transaction = database.transaction(PROJECT_STORE, "readonly");
    const completed = transactionDone(transaction);
    const records = await requestResult(transaction.objectStore(PROJECT_STORE).getAll() as IDBRequest<LocalProjectRecord[]>);
    await completed;
    return records
      .map((record) => ({ ...record, state: migrateArchive(record.state) }))
      .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
  } finally {
    database.close();
  }
}

export async function getLocalProject(projectId: string) {
  const database = await openDatabase();
  try {
    const transaction = database.transaction(PROJECT_STORE, "readonly");
    const completed = transactionDone(transaction);
    const record = await requestResult(transaction.objectStore(PROJECT_STORE).get(projectId) as IDBRequest<LocalProjectRecord | undefined>);
    await completed;
    return record ? { ...record, state: migrateArchive(record.state) } : null;
  } finally {
    database.close();
  }
}

export async function saveLocalProject(projectId: string, state: ArchiveState, createdAt?: string) {
  const existing = await getLocalProject(projectId);
  const database = await openDatabase();
  try {
    const transaction = database.transaction(PROJECT_STORE, "readwrite");
    const completed = transactionDone(transaction);
    const store = transaction.objectStore(PROJECT_STORE);
    const now = new Date().toISOString();
    const record: LocalProjectRecord = {
      id: projectId,
      state: migrateArchive(state),
      createdAt: existing?.createdAt ?? createdAt ?? now,
      updatedAt: now,
    };
    store.put(record);
    await completed;
    return record;
  } finally {
    database.close();
  }
}

export async function deleteLocalProject(projectId: string) {
  const database = await openDatabase();
  try {
    const transaction = database.transaction(PROJECT_STORE, "readwrite");
    const completed = transactionDone(transaction);
    transaction.objectStore(PROJECT_STORE).delete(projectId);
    await completed;
  } finally {
    database.close();
  }
}

export function createLocalProjectId() {
  return `project-${crypto.randomUUID().toLocaleLowerCase("en-US")}`;
}

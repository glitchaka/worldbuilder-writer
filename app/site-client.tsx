"use client";

import { useCallback, useEffect, useState } from "react";
import { createBlankArchive, migrateArchive, type ArchiveState } from "../lib/archive-data";
import type { ArchiveProjectSummary } from "../lib/archive-projects";
import ArchiveClient from "./archive-client";
import GoogleDriveBackup from "./google-drive-backup";
import {
  createLocalProjectId,
  deleteLocalProject,
  getLocalProject,
  listLocalProjects,
  localProjectSummary,
  saveLocalProject,
} from "./local-project-store";
import ProjectHub from "./project-hub";
import { embedWbwAssets, readWbwPackage } from "./wbw-project";

export type AuthenticatedUser = {
  displayName: string;
  email: string;
};

function desktopState(value: ArchiveState) {
  const state = migrateArchive(value);
  return {
    ...state,
    profile: { ...state.profile, theme: "desk" as const, activeThemeId: "desk", customThemes: [] },
  };
}

function blobDataUrl(blob: Blob) {
  return new Promise<string>((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result ?? ""));
    reader.onerror = () => reject(reader.error ?? new Error("No se pudo preparar una imagen."));
    reader.readAsDataURL(blob);
  });
}

async function localizeLegacyAssets(projectId: string, value: ArchiveState) {
  let state = desktopState(value);
  const characters = [] as ArchiveState["characters"];
  for (const character of state.characters) {
    if (!character.imageUrl || character.imageUrl.startsWith("data:")) {
      characters.push(character);
      continue;
    }
    try {
      const response = await fetch(character.imageUrl);
      characters.push(response.ok ? { ...character, imageUrl: await blobDataUrl(await response.blob()) } : character);
    } catch {
      characters.push(character);
    }
  }
  state = { ...state, characters };

  const localizeAttachments = async (scope: "world" | "magic", recordId: string, attachments: ArchiveState["world"][number]["attachments"]) => {
    const localized = [] as NonNullable<typeof attachments>;
    for (const attachment of attachments ?? []) {
      if (attachment.dataUrl) {
        localized.push(attachment);
        continue;
      }
      try {
        const params = new URLSearchParams({ projectId, scope, recordId, attachmentId: attachment.id });
        const response = await fetch(`/api/attachments?${params.toString()}`);
        localized.push(response.ok ? { ...attachment, dataUrl: await blobDataUrl(await response.blob()) } : attachment);
      } catch {
        localized.push(attachment);
      }
    }
    return localized;
  };

  const world = [] as ArchiveState["world"];
  for (const record of state.world) world.push({ ...record, attachments: await localizeAttachments("world", record.id, record.attachments) });
  const magicSystems = [] as ArchiveState["magicSystems"];
  for (const record of state.magicSystems) magicSystems.push({ ...record, attachments: await localizeAttachments("magic", record.id, record.attachments) });
  return { ...state, world, magicSystems };
}

function DesktopTitlebar() {
  const [desktop, setDesktop] = useState(false);
  const [nativeDesktop, setNativeDesktop] = useState(false);
  const [maximized, setMaximized] = useState(false);

  useEffect(() => {
    const runtime = window as typeof window & { __TAURI_INTERNALS__?: unknown; __ARCHIVO_NEO_DESKTOP__?: boolean };
    const tauriRuntime = Boolean(runtime.__TAURI_INTERNALS__);
    const portableRuntime = Boolean(runtime.__ARCHIVO_NEO_DESKTOP__);
    if (!tauriRuntime && !portableRuntime) return;
    const revealTitlebar = window.setTimeout(() => { setDesktop(true); setNativeDesktop(portableRuntime); }, 0);
    document.documentElement.classList.add("tauri-desktop", "portable-desktop");
    if (tauriRuntime) {
      void import("@tauri-apps/api/window").then(async ({ getCurrentWindow }) => {
        const appWindow = getCurrentWindow();
        await appWindow.setDecorations(false);
        setMaximized(await appWindow.isMaximized());
      }).catch(() => undefined);
    }
    return () => {
      window.clearTimeout(revealTitlebar);
      document.documentElement.classList.remove("tauri-desktop", "portable-desktop");
    };
  }, []);

  const windowAction = async (action: "minimize" | "maximize" | "close") => {
    if (nativeDesktop) {
      const token = (window as typeof window & { __ARCHIVO_NEO_TOKEN__?: string }).__ARCHIVO_NEO_TOKEN__ ?? "";
      const response = await fetch(`/__window/${action}`, { method: "POST", headers: { "x-archivo-neo-token": token } }).catch(() => null);
      if (action === "maximize" && response?.ok) {
        const state = await response.json().catch(() => ({ maximized: !maximized })) as { maximized?: boolean };
        setMaximized(Boolean(state.maximized));
      }
      return;
    }
    const { getCurrentWindow } = await import("@tauri-apps/api/window");
    const appWindow = getCurrentWindow();
    if (action === "minimize") await appWindow.minimize();
    if (action === "close") await appWindow.close();
    if (action === "maximize") {
      await appWindow.toggleMaximize();
      setMaximized(await appWindow.isMaximized());
    }
  };

  if (!desktop) return null;
  return (
    <header className="desktop-titlebar" data-tauri-drag-region onDoubleClick={() => void windowAction("maximize")}>
      <div className="desktop-titlebar-brand" data-tauri-drag-region><span>WW</span><p data-tauri-drag-region>Worldbuilder Writer</p></div>
      <div className="desktop-window-actions">
        <button type="button" aria-label="Minimizar" title="Minimizar" onClick={() => void windowAction("minimize")}>−</button>
        <button type="button" aria-label={maximized ? "Restaurar" : "Maximizar"} title={maximized ? "Restaurar" : "Maximizar"} onClick={() => void windowAction("maximize")}>{maximized ? "❐" : "□"}</button>
        <button type="button" className="desktop-close" aria-label="Cerrar" title="Cerrar" onClick={() => void windowAction("close")}>×</button>
      </div>
    </header>
  );
}

export default function SiteClient({ legacyUser }: { legacyUser: AuthenticatedUser | null }) {
  const [projects, setProjects] = useState<ArchiveProjectSummary[]>([]);
  const [currentProjectId, setCurrentProjectId] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [creating, setCreating] = useState(false);
  const [importing, setImporting] = useState(false);
  const [deletingProjectId, setDeletingProjectId] = useState<string | null>(null);
  const [settingsOpen, setSettingsOpen] = useState(false);

  const importLegacyProjectsOnce = useCallback(async () => {
    if (!legacyUser || window.localStorage.getItem("worldbuilder-writer-local-migration-v1") === "done") return;
    try {
      const response = await fetch("/api/projects", { cache: "no-store" });
      const result = (await response.json()) as { projects?: ArchiveProjectSummary[] };
      if (!response.ok || !result.projects) return;
      for (const project of result.projects) {
        if (await getLocalProject(project.id)) continue;
        const archiveResponse = await fetch(`/api/archive?projectId=${encodeURIComponent(project.id)}`, { cache: "no-store" });
        const archiveResult = (await archiveResponse.json()) as { state?: ArchiveState };
        if (!archiveResponse.ok || !archiveResult.state) continue;
        await saveLocalProject(project.id, await localizeLegacyAssets(project.id, archiveResult.state), project.createdAt);
      }
      window.localStorage.setItem("worldbuilder-writer-local-migration-v1", "done");
    } catch {
      // The old account-backed storage is only a migration source; local use must remain available.
    }
  }, [legacyUser]);

  const loadProjects = useCallback(async () => {
    setLoading(true);
    setError("");
    try {
      await importLegacyProjectsOnce();
      setProjects((await listLocalProjects()).map(localProjectSummary));
    } catch (loadError) {
      setError(loadError instanceof Error ? loadError.message : "No se pudieron abrir tus obras locales.");
    } finally {
      setLoading(false);
    }
  }, [importLegacyProjectsOnce]);

  useEffect(() => {
    const initialLoad = window.setTimeout(() => void loadProjects(), 0);
    return () => window.clearTimeout(initialLoad);
  }, [loadProjects]);

  const createProject = async ({ archiveTitle, storyTitle }: { archiveTitle: string; storyTitle: string }) => {
    setCreating(true);
    setError("");
    try {
      const id = createLocalProjectId();
      const record = await saveLocalProject(id, desktopState(createBlankArchive({ archiveTitle, storyTitle, theme: "desk" })));
      setProjects((current) => [localProjectSummary(record), ...current]);
      setCurrentProjectId(id);
    } catch (createError) {
      setError(createError instanceof Error ? createError.message : "No se pudo crear la obra.");
      throw createError;
    } finally {
      setCreating(false);
    }
  };

  const deleteProject = async (project: ArchiveProjectSummary) => {
    if (!window.confirm(`¿Borrar definitivamente “${project.title}”?\n\nEsta acción elimina la copia guardada en este navegador y no se puede deshacer.`)) return;
    setDeletingProjectId(project.id);
    setError("");
    try {
      await deleteLocalProject(project.id);
      setProjects((current) => current.filter((item) => item.id !== project.id));
    } catch (deleteError) {
      setError(deleteError instanceof Error ? deleteError.message : "No se pudo borrar la obra.");
    } finally {
      setDeletingProjectId(null);
    }
  };

  const importProject = async (file: File) => {
    if (importing) return;
    setImporting(true);
    setError("");
    try {
      const state = desktopState(await embedWbwAssets(await readWbwPackage(file)));
      const id = createLocalProjectId();
      const record = await saveLocalProject(id, state);
      setProjects((current) => [localProjectSummary(record), ...current]);
      setCurrentProjectId(id);
    } catch (importError) {
      setError(importError instanceof Error ? importError.message : "No se pudo importar el proyecto .wbw.");
    } finally {
      setImporting(false);
    }
  };

  if (currentProjectId) {
    return <><DesktopTitlebar /><ArchiveClient key={currentProjectId} projectId={currentProjectId} onBack={() => { setCurrentProjectId(null); void loadProjects(); }} /></>;
  }

  return (
    <>
      <DesktopTitlebar />
      <ProjectHub projects={projects} loading={loading} creating={creating} importing={importing} deletingProjectId={deletingProjectId} error={error} onRetry={() => void loadProjects()} onOpen={setCurrentProjectId} onSettings={() => setSettingsOpen(true)} onCreate={createProject} onImport={importProject} onDelete={deleteProject} />
      {settingsOpen && <GoogleDriveBackup onClose={() => setSettingsOpen(false)} onRestored={() => void loadProjects()} />}
    </>
  );
}

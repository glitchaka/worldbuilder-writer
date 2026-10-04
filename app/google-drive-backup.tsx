"use client";

import { useEffect, useRef, useState } from "react";
import type { ArchiveState } from "../lib/archive-data";
import { createLocalProjectId, listLocalProjects, saveLocalProject } from "./local-project-store";
import { createWbwPackage, embedWbwAssets, readWbwPackage, WBW_MIME } from "./wbw-project";

type TokenResponse = { access_token?: string; error?: string; error_description?: string };
type GoogleTokenClient = { requestAccessToken: (options?: { prompt?: string }) => void };
type GoogleIdentityRuntime = {
  accounts?: { oauth2?: { initTokenClient: (options: { client_id: string; scope: string; callback: (response: TokenResponse) => void; error_callback?: (error: { type?: string }) => void }) => GoogleTokenClient } };
};
type DriveFile = {
  id: string;
  name: string;
  modifiedTime?: string;
  size?: string;
  appProperties?: { worldbuilderWriterProject?: string };
};

const CLIENT_ID_KEY = "worldbuilder-writer-google-client-id";
const GOOGLE_SCRIPT_ID = "worldbuilder-writer-google-identity";
const DRIVE_SCOPE = "openid email https://www.googleapis.com/auth/drive.file";

function safeName(value: string) {
  return value.normalize("NFKD").replace(/[\u0300-\u036f]/g, "").replace(/[^a-z0-9._-]+/gi, "-").replace(/^-+|-+$/g, "").slice(0, 100) || "proyecto";
}

function loadGoogleIdentity() {
  return new Promise<void>((resolve, reject) => {
    if ((window as typeof window & { google?: GoogleIdentityRuntime }).google?.accounts?.oauth2) { resolve(); return; }
    const existing = document.getElementById(GOOGLE_SCRIPT_ID) as HTMLScriptElement | null;
    if (existing) {
      existing.addEventListener("load", () => resolve(), { once: true });
      existing.addEventListener("error", () => reject(new Error("No se pudo abrir la conexión de Google.")), { once: true });
      return;
    }
    const script = document.createElement("script");
    script.id = GOOGLE_SCRIPT_ID;
    script.src = "https://accounts.google.com/gsi/client";
    script.async = true;
    script.defer = true;
    script.onload = () => resolve();
    script.onerror = () => reject(new Error("No se pudo abrir la conexión de Google."));
    document.head.appendChild(script);
  });
}

async function requestToken(clientId: string) {
  await loadGoogleIdentity();
  const runtime = (window as typeof window & { google?: GoogleIdentityRuntime }).google;
  if (!runtime?.accounts?.oauth2) throw new Error("La conexión de Google Drive no está disponible.");
  return new Promise<string>((resolve, reject) => {
    const client = runtime.accounts!.oauth2!.initTokenClient({
      client_id: clientId,
      scope: DRIVE_SCOPE,
      callback: (response) => response.access_token ? resolve(response.access_token) : reject(new Error(response.error_description || response.error || "Google no autorizó el acceso.")),
      error_callback: () => reject(new Error("La ventana de acceso a Google se cerró o fue bloqueada.")),
    });
    client.requestAccessToken({ prompt: "consent" });
  });
}

async function driveJson<T>(url: string, token: string, init?: RequestInit) {
  const response = await fetch(url, { ...init, headers: { authorization: `Bearer ${token}`, ...(init?.headers ?? {}) } });
  const result = await response.json().catch(() => ({})) as T & { error?: { message?: string } };
  if (!response.ok) throw new Error(result.error?.message ?? "Google Drive no pudo completar la operación.");
  return result;
}

async function uploadBackup(token: string, projectId: string, archive: ArchiveState) {
  const escapedProjectId = projectId.replace(/'/g, "\\'");
  const query = encodeURIComponent(`trashed = false and appProperties has { key='worldbuilderWriterProject' and value='${escapedProjectId}' }`);
  const existing = await driveJson<{ files: DriveFile[] }>(`https://www.googleapis.com/drive/v3/files?q=${query}&spaces=drive&fields=files(id,name,modifiedTime)&pageSize=10`, token);
  const blob = await createWbwPackage(archive, projectId);
  const fileName = `${safeName(archive.profile.archiveTitle)}.wbw`;
  const metadata = {
    name: fileName,
    mimeType: WBW_MIME,
    appProperties: { worldbuilderWriterBackup: "true", worldbuilderWriterProject: projectId },
  };
  const boundary = `worldbuilder_writer_${crypto.randomUUID().replace(/-/g, "")}`;
  const body = new Blob([
    `--${boundary}\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n${JSON.stringify(metadata)}\r\n--${boundary}\r\nContent-Type: ${WBW_MIME}\r\n\r\n`,
    blob,
    `\r\n--${boundary}--`,
  ], { type: `multipart/related; boundary=${boundary}` });
  const previous = existing.files[0];
  const endpoint = previous
    ? `https://www.googleapis.com/upload/drive/v3/files/${encodeURIComponent(previous.id)}?uploadType=multipart&fields=id,name,modifiedTime,size`
    : "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,name,modifiedTime,size";
  return driveJson<DriveFile>(endpoint, token, { method: previous ? "PATCH" : "POST", headers: { "content-type": `multipart/related; boundary=${boundary}` }, body });
}

async function listBackups(token: string) {
  const query = encodeURIComponent("trashed = false and appProperties has { key='worldbuilderWriterBackup' and value='true' }");
  return (await driveJson<{ files: DriveFile[] }>(`https://www.googleapis.com/drive/v3/files?q=${query}&spaces=drive&orderBy=modifiedTime%20desc&fields=files(id,name,modifiedTime,size,appProperties)&pageSize=100`, token)).files;
}

export default function GoogleDriveBackup({ onRestored, onClose }: {
  onRestored?: (projectId: string, state: ArchiveState) => void | Promise<void>;
  onClose: () => void;
}) {
  const [clientId, setClientId] = useState(() => typeof window === "undefined" ? "" : window.localStorage.getItem(CLIENT_ID_KEY) ?? "");
  const [token, setToken] = useState("");
  const [account, setAccount] = useState("");
  const [backups, setBackups] = useState<DriveFile[]>([]);
  const [busy, setBusy] = useState<"connect" | "backup" | "restore" | "">("");
  const [message, setMessage] = useState("");
  const tokenRef = useRef("");

  useEffect(() => {
    const keyDown = (event: KeyboardEvent) => { if (event.key === "Escape" && !busy) onClose(); };
    window.addEventListener("keydown", keyDown);
    return () => window.removeEventListener("keydown", keyDown);
  }, [busy, onClose]);

  const connect = async () => {
    const cleanClientId = clientId.trim();
    if (!/^[0-9a-z-]+\.apps\.googleusercontent\.com$/i.test(cleanClientId)) {
      setMessage("Introduce un identificador OAuth de aplicación web válido.");
      return "";
    }
    setBusy("connect");
    setMessage("");
    try {
      window.localStorage.setItem(CLIENT_ID_KEY, cleanClientId);
      const accessToken = await requestToken(cleanClientId);
      tokenRef.current = accessToken;
      setToken(accessToken);
      const profile = await driveJson<{ email?: string }>("https://www.googleapis.com/oauth2/v3/userinfo", accessToken);
      setAccount(profile.email ?? "Cuenta de Google conectada");
      setBackups(await listBackups(accessToken));
      setMessage("Google Drive quedó conectado para respaldos.");
      return accessToken;
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "No se pudo conectar Google Drive.");
      return "";
    } finally {
      setBusy("");
    }
  };

  const backup = async () => {
    const accessToken = tokenRef.current || token || await connect();
    if (!accessToken) return;
    setBusy("backup");
    setMessage("Preparando la biblioteca completa…");
    try {
      const projects = await listLocalProjects();
      if (!projects.length) throw new Error("No hay obras locales para respaldar todavía.");
      for (const project of projects) await uploadBackup(accessToken, project.id, project.state);
      setBackups(await listBackups(accessToken));
      setMessage(`${projects.length} ${projects.length === 1 ? "obra respaldada" : "obras respaldadas"} en Google Drive.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "No se pudo crear el respaldo.");
    } finally {
      setBusy("");
    }
  };

  const restore = async (file: DriveFile) => {
    const accessToken = tokenRef.current || token || await connect();
    if (!accessToken || !window.confirm(`¿Restaurar “${file.name}” sobre el proyecto abierto? Los cambios actuales serán reemplazados.`)) return;
    setBusy("restore");
    setMessage("Descargando y comprobando el respaldo…");
    try {
      const response = await fetch(`https://www.googleapis.com/drive/v3/files/${encodeURIComponent(file.id)}?alt=media`, { headers: { authorization: `Bearer ${accessToken}` } });
      if (!response.ok) throw new Error("Google Drive no pudo descargar el respaldo.");
      const projectFile = new File([await response.blob()], file.name, { type: WBW_MIME });
      const imported = await embedWbwAssets(await readWbwPackage(projectFile));
      const restored = { ...imported, profile: { ...imported.profile, theme: "desk" as const, activeThemeId: "desk", customThemes: [] } };
      const restoredProjectId = file.appProperties?.worldbuilderWriterProject || createLocalProjectId();
      await saveLocalProject(restoredProjectId, restored);
      await onRestored?.(restoredProjectId, restored);
      setMessage(`“${restored.profile.archiveTitle}” quedó restaurado en la biblioteca local.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "No se pudo restaurar el respaldo.");
    } finally {
      setBusy("");
    }
  };

  return (
    <div className="editor-backdrop" role="presentation" onMouseDown={(event) => { if (!busy && event.target === event.currentTarget) onClose(); }}>
      <section className="editor-modal compact drive-backup-modal" role="dialog" aria-modal="true" aria-labelledby="drive-backup-title">
        <div className="editor-head"><div><span>RESPALDO OPCIONAL</span><h2 id="drive-backup-title">Google Drive</h2></div><button type="button" disabled={Boolean(busy)} onClick={onClose} aria-label="Cerrar Google Drive">×</button></div>
        <div className="editor-body drive-backup-body">
          <p>La aplicación funciona y guarda sin cuenta. Google solo se solicita para respaldar o restaurar todas tus obras como paquetes completos <strong>.wbw</strong>.</p>
          <label><span>Identificador OAuth de Google</span><input value={clientId} onChange={(event) => setClientId(event.target.value)} placeholder="000000000000-abc.apps.googleusercontent.com" spellCheck={false} /><small>Debe ser un cliente OAuth de tipo «Aplicación web» con este sitio añadido como origen autorizado. Es un identificador público, no una contraseña.</small></label>
          <div className="drive-connect-row"><button type="button" className="secondary-action" disabled={Boolean(busy)} onClick={() => void connect()}>{busy === "connect" ? "Conectando…" : token ? "Cambiar cuenta" : "Conectar con Google Drive"}</button>{account && <span><i>●</i>{account}</span>}</div>
          {token && <div className="drive-backup-actions"><button type="button" className="primary-action" disabled={Boolean(busy)} onClick={() => void backup()}>{busy === "backup" ? "Respaldando biblioteca…" : "Respaldar todas las obras"}</button><small>Se crea o actualiza un archivo <b>.wbw</b> completo por cada obra local.</small></div>}
          {message && <p className="drive-message" role="status">{message}</p>}
          {token && <section className="drive-backup-list"><header><span>COPIAS DISPONIBLES</span><button type="button" disabled={Boolean(busy)} onClick={() => void listBackups(tokenRef.current || token).then(setBackups).catch((error) => setMessage(error instanceof Error ? error.message : "No se pudieron actualizar las copias."))}>Actualizar</button></header>{backups.length ? backups.map((file) => <article key={file.id}><div><strong>{file.name}</strong><small>{file.modifiedTime ? new Date(file.modifiedTime).toLocaleString("es-CL") : "Fecha desconocida"}{file.size ? ` · ${(Number(file.size) / 1024 / 1024).toFixed(1)} MB` : ""}</small></div><button type="button" disabled={Boolean(busy)} onClick={() => void restore(file)}>Restaurar en biblioteca</button></article>) : <p>No hay respaldos creados por Worldbuilder Writer en esta cuenta.</p>}</section>}
        </div>
        <div className="editor-actions"><span className="action-spacer" /><button type="button" disabled={Boolean(busy)} onClick={onClose}>Cerrar</button></div>
      </section>
    </div>
  );
}

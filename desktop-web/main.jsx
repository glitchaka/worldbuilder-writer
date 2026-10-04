import React, { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import "../app/globals.css";
import SiteClient from "../app/site-client.tsx";
import {
  chooseWorkspaceFolder,
  installDesktopStorage,
  loadWorkspaceHandle,
  workspacePermission,
} from "./desktop-storage.js";

window.__ARCHIVO_NEO_DESKTOP__ = true;

function FolderSetup({ reconnect = false }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  const choose = async () => {
    setBusy(true);
    setError("");
    try {
      await chooseWorkspaceFolder();
      window.location.reload();
    } catch (chooseError) {
      if (chooseError?.name !== "AbortError") setError(chooseError instanceof Error ? chooseError.message : "No se pudo abrir la carpeta.");
      setBusy(false);
    }
  };

  return (
    <main className="desktop-folder-setup">
      <section>
        <span className="desktop-folder-seal">AN</span>
        <p>EDICIÓN PORTÁTIL · WINDOWS</p>
        <h1>{reconnect ? "Volver a conectar tus proyectos" : "Elige dónde guardar tus historias"}</h1>
        <div className="desktop-folder-copy">
          <p>El programa comienza vacío y nunca guarda las obras dentro del ejecutable.</p>
          <p>Cada historia quedará como una carpeta legible y respaldable dentro de la ubicación que elijas.</p>
        </div>
        <button type="button" disabled={busy} onClick={() => void choose()}>{busy ? "Abriendo carpetas…" : reconnect ? "Conectar carpeta" : "Elegir carpeta de proyectos"}</button>
        <small>Puedes usar una carpeta en Documentos, un disco externo o una unidad sincronizada.</small>
        {error && <div className="desktop-folder-error" role="alert">{error}</div>}
      </section>
    </main>
  );
}

function DesktopApp() {
  const [status, setStatus] = useState("loading");
  const [handle, setHandle] = useState(null);

  useEffect(() => {
    let active = true;
    void loadWorkspaceHandle().then(async (stored) => {
      if (!active) return;
      if (!stored) { setStatus("setup"); return; }
      if (!await workspacePermission(stored, false)) { setStatus("reconnect"); return; }
      installDesktopStorage(stored);
      setHandle(stored);
      setStatus("ready");
    }).catch(() => { if (active) setStatus("setup"); });
    return () => { active = false; };
  }, []);

  if (status === "loading") return <main className="desktop-folder-loading"><span>AN</span><p>Preparando el archivo local…</p></main>;
  if (status === "setup") return <FolderSetup />;
  if (status === "reconnect") return <FolderSetup reconnect />;
  return (
    <>
      <SiteClient user={{ displayName: "Escritor/a", email: "Proyectos guardados fuera de la aplicación" }} />
      <button
        type="button"
        className="desktop-workspace-chip"
        title="Cambiar la carpeta externa de proyectos"
        onClick={async () => {
          try { await chooseWorkspaceFolder(); window.location.reload(); } catch { /* The user cancelled the picker. */ }
        }}
      >
        <span>CARPETA EXTERNA</span>{handle?.name ?? "Proyectos"}
      </button>
    </>
  );
}

createRoot(document.getElementById("root")).render(<DesktopApp />);

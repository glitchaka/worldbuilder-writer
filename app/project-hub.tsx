"use client";

import { useState } from "react";
import type { ArchiveProjectSummary } from "../lib/archive-projects";

function formatDate(value: string) {
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return "Fecha desconocida";
  return date.toLocaleDateString("es-CL", { day: "2-digit", month: "short", year: "numeric" });
}

export default function ProjectHub({
  projects,
  loading,
  creating,
  importing,
  deletingProjectId,
  error,
  onRetry,
  onOpen,
  onSettings,
  onCreate,
  onImport,
  onDelete,
}: {
  projects: ArchiveProjectSummary[];
  loading: boolean;
  creating: boolean;
  importing: boolean;
  deletingProjectId: string | null;
  error: string;
  onRetry: () => void;
  onOpen: (id: string) => void;
  onSettings: () => void;
  onCreate: (input: { archiveTitle: string; storyTitle: string }) => Promise<void>;
  onImport: (file: File) => Promise<void>;
  onDelete: (project: ArchiveProjectSummary) => Promise<void>;
}) {
  const [editorOpen, setEditorOpen] = useState(false);

  return (
    <main className="project-hub desktop-project-hub">
      <header className="hub-desktop-bar">
        <div className="hub-app-identity"><span>WW</span><div><strong>Worldbuilder Writer</strong><small>Biblioteca local de proyectos</small></div></div>
        <div className="hub-desktop-actions">
          <label><input type="file" accept=".wbw,application/vnd.worldbuilder-writer.project+zip" disabled={importing} onChange={(event) => { const file = event.target.files?.[0]; event.target.value = ""; if (file) void onImport(file); }} /><span>{importing ? "Importando…" : "Abrir proyecto .wbw"}</span></label>
          <button type="button" className="hub-new-action" onClick={() => setEditorOpen(true)} disabled={importing}>+ Nueva obra</button>
          <button type="button" className="hub-settings-button" onClick={onSettings}><span>⚙</span> Configuración</button>
        </div>
      </header>

      <div className="hub-shell">
        <section className="hub-intro desktop-hub-intro">
          <div><p>ARCHIVO</p><h1>Tus historias</h1><span>Los cambios se guardan en este navegador. Google Drive se conecta únicamente para crear o restaurar respaldos.</span></div>
          <div className="hub-storage-note"><span>●</span><div><strong>Guardado local disponible</strong><small>No necesitas iniciar sesión para escribir.</small></div></div>
        </section>

        {error && <div className="hub-error" role="alert"><span>{error}</span><button onClick={onRetry}>Reintentar</button></div>}

        {loading ? (
          <section className="hub-loading" aria-live="polite"><i /><span>Abriendo la biblioteca…</span></section>
        ) : (
          <section className="project-grid" aria-label="Obras guardadas">
            <button className="project-card project-card-new" onClick={() => setEditorOpen(true)}>
              <span className="new-project-mark">+</span>
              <div><h3>Nueva obra</h3><p>Comienza con un archivo completamente vacío.</p></div>
              <small>CREAR DESDE CERO</small>
            </button>

            {projects.map((project) => (
              <article className="project-card desktop-project-card" key={project.id}>
                <div className="project-card-top"><span>WW</span><small>{project.status || "Planificación"}</small></div>
                <div className="project-card-copy"><p>{project.genre || "Proyecto narrativo"}</p><h3>{project.title}</h3><span>{project.storyTitle || "Historia aún sin título"}</span></div>
                <dl>
                  <div><dt>Fichas</dt><dd>{project.characters}</dd></div>
                  <div><dt>Enlaces</dt><dd>{project.relationships}</dd></div>
                  <div><dt>Capítulos</dt><dd>{project.chapters}</dd></div>
                  <div><dt>Palabras</dt><dd>{project.words.toLocaleString("es-CL")}</dd></div>
                </dl>
                <footer>
                  <small>Guardado {formatDate(project.updatedAt)}</small>
                  <div className="project-card-actions">
                    <button className="project-delete" disabled={deletingProjectId === project.id} onClick={() => void onDelete(project)}>{deletingProjectId === project.id ? "Borrando…" : "Eliminar"}</button>
                    <button disabled={deletingProjectId === project.id} onClick={() => onOpen(project.id)}>Abrir <span>→</span></button>
                  </div>
                </footer>
              </article>
            ))}
          </section>
        )}
      </div>

      {editorOpen && <NewProjectEditor busy={creating} onCancel={() => setEditorOpen(false)} onCreate={async (input) => { await onCreate(input); setEditorOpen(false); }} />}
    </main>
  );
}

function NewProjectEditor({ busy, onCancel, onCreate }: { busy: boolean; onCancel: () => void; onCreate: (input: { archiveTitle: string; storyTitle: string }) => Promise<void> }) {
  const [archiveTitle, setArchiveTitle] = useState("");
  const [storyTitle, setStoryTitle] = useState("");
  const [error, setError] = useState("");

  return (
    <div className="editor-backdrop" role="presentation" onMouseDown={(event) => { if (!busy && event.target === event.currentTarget) onCancel(); }}>
      <form className="editor-modal compact new-project-modal" role="dialog" aria-modal="true" aria-labelledby="new-project-title" onSubmit={(event) => {
        event.preventDefault();
        if (!archiveTitle.trim()) { setError("Ponle un nombre al archivo para continuar."); return; }
        setError("");
        void onCreate({ archiveTitle: archiveTitle.trim(), storyTitle: storyTitle.trim() }).catch((createError) => setError(createError instanceof Error ? createError.message : "No se pudo crear la obra."));
      }}>
        <div className="editor-head"><div><span>NUEVA OBRA</span><h2 id="new-project-title">Crear una obra desde cero</h2></div><button type="button" disabled={busy} onClick={onCancel} aria-label="Cerrar editor">×</button></div>
        <div className="editor-body new-project-editor">
          <p className="new-project-explainer">El archivo comenzará vacío y usará la interfaz de escritorio única de Worldbuilder Writer.</p>
          <label><span>Nombre del archivo</span><input autoFocus maxLength={120} required value={archiveTitle} onChange={(event) => setArchiveTitle(event.target.value)} placeholder="Ej. Archivo de la Torre Hundida" /></label>
          <label><span>Título de la historia <small>(opcional)</small></span><input maxLength={180} value={storyTitle} onChange={(event) => setStoryTitle(event.target.value)} placeholder="Puede cambiarse después" /></label>
          {error && <p className="editor-error" role="alert">{error}</p>}
        </div>
        <div className="editor-actions"><span className="action-spacer" /><button type="button" disabled={busy} onClick={onCancel}>Cancelar</button><button type="submit" disabled={busy}>{busy ? "Creando archivo…" : "Crear obra vacía"}</button></div>
      </form>
    </div>
  );
}

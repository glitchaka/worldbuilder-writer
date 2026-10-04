import { and, eq, sql } from "drizzle-orm";
import { getDb } from "../../../db";
import { archiveProjects } from "../../../db/schema";
import { getChatGPTUser } from "../../chatgpt-auth";
import { migrateArchive, type ArchiveState } from "../../../lib/archive-data";
import {
  adoptLegacyArchive,
  getOwnedProject,
  isProjectId,
  normalizeOwnerEmail,
  projectSummaryFromRow,
} from "../../../lib/archive-projects";
import { ensureArchiveStorage } from "../../../lib/archive-store";

function isArchiveState(value: unknown): value is ArchiveState {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<ArchiveState>;
  return (
    typeof candidate.title === "string" &&
    Array.isArray(candidate.characters) &&
    Array.isArray(candidate.relationships) &&
    Array.isArray(candidate.theories) &&
    Array.isArray(candidate.timeline)
  );
}

async function currentOwnerEmail() {
  const user = await getChatGPTUser();
  return user ? normalizeOwnerEmail(user.email) : null;
}

export async function GET(request: Request) {
  try {
    const ownerEmail = await currentOwnerEmail();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para abrir esta obra." }, { status: 401 });

    const projectId = new URL(request.url).searchParams.get("projectId");
    if (!isProjectId(projectId)) return Response.json({ error: "La obra solicitada no es válida." }, { status: 400 });

    await adoptLegacyArchive(ownerEmail);
    const row = await getOwnedProject(ownerEmail, projectId);
    if (!row) return Response.json({ error: "No se encontró esta obra en tu cuenta." }, { status: 404 });

    const storedState = JSON.parse(row.payload) as ArchiveState;
    const state = migrateArchive(storedState);
    let updatedAt = row.updatedAt;

    if (state !== storedState) {
      await getDb()
        .update(archiveProjects)
        .set({
          title: state.profile.archiveTitle,
          storyTitle: state.profile.storyTitle,
          payload: JSON.stringify(state),
          updatedAt: sql`CURRENT_TIMESTAMP`,
        })
        .where(and(eq(archiveProjects.id, projectId), eq(archiveProjects.ownerEmail, ownerEmail)));

      const updatedRow = await getOwnedProject(ownerEmail, projectId);
      updatedAt = updatedRow?.updatedAt ?? updatedAt;
    }

    return Response.json({ state, updatedAt, projectId });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo abrir la obra." },
      { status: 500 },
    );
  }
}

export async function PUT(request: Request) {
  try {
    const ownerEmail = await currentOwnerEmail();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para guardar esta obra." }, { status: 401 });

    const body = (await request.json()) as { projectId?: unknown; state?: unknown };
    const projectId = typeof body.projectId === "string" ? body.projectId : null;
    if (!isProjectId(projectId)) return Response.json({ error: "La obra solicitada no es válida." }, { status: 400 });
    if (!isArchiveState(body.state)) return Response.json({ error: "El formato del archivo no es válido." }, { status: 400 });

    const existing = await getOwnedProject(ownerEmail, projectId);
    if (!existing) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });

    const state = migrateArchive(body.state);
    const payload = JSON.stringify(state);
    if (payload.length > 4_000_000) {
      return Response.json({ error: "El archivo supera el tamaño permitido." }, { status: 413 });
    }

    await ensureArchiveStorage();
    await getDb()
      .update(archiveProjects)
      .set({
        title: state.profile.archiveTitle,
        storyTitle: state.profile.storyTitle,
        payload,
        updatedAt: sql`CURRENT_TIMESTAMP`,
      })
      .where(and(eq(archiveProjects.id, projectId), eq(archiveProjects.ownerEmail, ownerEmail)));

    const row = await getOwnedProject(ownerEmail, projectId);
    return Response.json({
      ok: true,
      updatedAt: row?.updatedAt ?? null,
      project: row ? projectSummaryFromRow(row) : null,
    });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudieron guardar los cambios." },
      { status: 500 },
    );
  }
}

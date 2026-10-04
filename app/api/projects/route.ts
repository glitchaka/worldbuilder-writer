import { and, eq } from "drizzle-orm";
import { getDb } from "../../../db";
import { archiveProjects, archiveStates } from "../../../db/schema";
import { getChatGPTUser } from "../../chatgpt-auth";
import { type ArchiveState, type ArchiveTheme } from "../../../lib/archive-data";
import { getArchiveBucket } from "../../../lib/archive-media";
import {
  createProjectId,
  createProjectState,
  getOwnedProject,
  isProjectId,
  listOwnedProjects,
  normalizeOwnerEmail,
  projectSummaryFromRow,
} from "../../../lib/archive-projects";
import { ensureArchiveStorage } from "../../../lib/archive-store";

const THEMES = new Set<ArchiveTheme>(["grim", "classic", "kawaii", "chronicle", "desk"]);
const LEGACY_PROJECT_ID = "legacy-main";

async function authenticatedUser() {
  const user = await getChatGPTUser();
  if (!user) return null;
  return { ...user, email: normalizeOwnerEmail(user.email) };
}

async function mediaOwnerKey(email: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(email));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("").slice(0, 20);
}

async function removeProjectMedia(ownerEmail: string, projectId: string, state: ArchiveState) {
  try {
    const bucket = getArchiveBucket();
    const prefix = `projects/${await mediaOwnerKey(ownerEmail)}/${projectId}/`;
    let cursor: string | undefined;
    do {
      const listed = await bucket.list({ prefix, cursor });
      if (listed.objects.length) await bucket.delete(listed.objects.map((object: { key: string }) => object.key));
      cursor = listed.truncated ? listed.cursor : undefined;
    } while (cursor);

    if (projectId === LEGACY_PROJECT_ID) {
      const legacyKeys = state.characters.filter((character) => character.imageUrl).map((character) => `characters/${character.id}`);
      if (legacyKeys.length) await bucket.delete(legacyKeys);
    }
  } catch {
    // Media cleanup is best effort; inaccessible orphaned objects must not keep a project undeletable.
  }
}

export async function GET() {
  try {
    const user = await authenticatedUser();
    if (!user) return Response.json({ error: "Inicia sesión para abrir tus archivos." }, { status: 401 });

    return Response.json({
      projects: await listOwnedProjects(user.email),
      user: { displayName: user.displayName, email: user.email },
    });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudieron abrir tus obras." },
      { status: 500 },
    );
  }
}

export async function POST(request: Request) {
  try {
    const user = await authenticatedUser();
    if (!user) return Response.json({ error: "Inicia sesión para crear una obra." }, { status: 401 });

    const body = (await request.json()) as { archiveTitle?: unknown; storyTitle?: unknown; theme?: unknown };
    const archiveTitle = typeof body.archiveTitle === "string" ? body.archiveTitle.trim() : "";
    const storyTitle = typeof body.storyTitle === "string" ? body.storyTitle.trim() : "";
    const theme = typeof body.theme === "string" && THEMES.has(body.theme as ArchiveTheme)
      ? body.theme as ArchiveTheme
      : "grim";

    if (!archiveTitle || archiveTitle.length > 120) {
      return Response.json({ error: "El nombre del archivo debe tener entre 1 y 120 caracteres." }, { status: 400 });
    }
    if (storyTitle.length > 180) {
      return Response.json({ error: "El título de la historia no puede superar 180 caracteres." }, { status: 400 });
    }

    const existing = await listOwnedProjects(user.email);
    if (existing.length >= 30) {
      return Response.json({ error: "Alcanzaste el máximo de 30 obras por cuenta." }, { status: 409 });
    }

    const state = createProjectState({ archiveTitle, storyTitle, theme });
    const id = createProjectId();
    await ensureArchiveStorage();
    await getDb().insert(archiveProjects).values({
      id,
      ownerEmail: user.email,
      title: state.profile.archiveTitle,
      storyTitle: state.profile.storyTitle,
      payload: JSON.stringify(state),
    });

    const [row] = await getDb()
      .select()
      .from(archiveProjects)
      .where(eq(archiveProjects.id, id))
      .limit(1);

    if (!row) throw new Error("No se pudo recuperar la obra recién creada.");
    return Response.json({ project: projectSummaryFromRow(row) }, { status: 201 });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo crear la obra." },
      { status: 500 },
    );
  }
}

export async function DELETE(request: Request) {
  try {
    const user = await authenticatedUser();
    if (!user) return Response.json({ error: "Inicia sesión para borrar una obra." }, { status: 401 });

    const projectId = new URL(request.url).searchParams.get("projectId");
    if (!isProjectId(projectId)) return Response.json({ error: "La obra solicitada no es válida." }, { status: 400 });

    const project = await getOwnedProject(user.email, projectId);
    if (!project) return Response.json({ error: "No se encontró esta obra en tu cuenta." }, { status: 404 });

    const state = JSON.parse(project.payload) as ArchiveState;
    await removeProjectMedia(user.email, projectId, state);
    await ensureArchiveStorage();

    const db = getDb();
    if (projectId === LEGACY_PROJECT_ID) {
      await db.delete(archiveStates).where(eq(archiveStates.id, "main"));
    }
    await db.delete(archiveProjects).where(and(
      eq(archiveProjects.id, projectId),
      eq(archiveProjects.ownerEmail, user.email),
    ));

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo borrar la obra." },
      { status: 500 },
    );
  }
}

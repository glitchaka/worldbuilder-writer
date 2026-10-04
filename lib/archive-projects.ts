import { and, desc, eq } from "drizzle-orm";
import { getDb } from "../db";
import { archiveProjects, archiveStates } from "../db/schema";
import {
  createBlankArchive,
  migrateArchive,
  type ArchiveState,
  type ArchiveTheme,
} from "./archive-data";
import { ensureArchiveStorage } from "./archive-store";

const LEGACY_PROJECT_ID = "legacy-main";

export type ArchiveProjectSummary = {
  id: string;
  title: string;
  storyTitle: string;
  genre: string;
  status: string;
  theme: ArchiveTheme;
  characters: number;
  relationships: number;
  worldEntries: number;
  magicSystems: number;
  chapters: number;
  words: number;
  createdAt: string;
  updatedAt: string;
};

export function normalizeOwnerEmail(email: string) {
  return email.trim().toLocaleLowerCase("en-US");
}

export function isProjectId(value: string | null): value is string {
  return Boolean(value && /^[a-z0-9-]{8,80}$/.test(value));
}

export async function adoptLegacyArchive(ownerEmail: string) {
  await ensureArchiveStorage();
  const db = getDb();
  const [existingLegacy] = await db
    .select({ id: archiveProjects.id })
    .from(archiveProjects)
    .where(eq(archiveProjects.id, LEGACY_PROJECT_ID))
    .limit(1);

  if (existingLegacy) return;

  const [legacyRow] = await db
    .select()
    .from(archiveStates)
    .where(eq(archiveStates.id, "main"))
    .limit(1);

  if (!legacyRow) return;

  const state = migrateArchive(JSON.parse(legacyRow.payload) as ArchiveState);
  await db
    .insert(archiveProjects)
    .values({
      id: LEGACY_PROJECT_ID,
      ownerEmail: normalizeOwnerEmail(ownerEmail),
      title: state.profile.archiveTitle,
      storyTitle: state.profile.storyTitle,
      payload: JSON.stringify(state),
      updatedAt: legacyRow.updatedAt,
    })
    .onConflictDoNothing();
}

export async function listOwnedProjects(ownerEmail: string) {
  await adoptLegacyArchive(ownerEmail);
  const rows = await getDb()
    .select()
    .from(archiveProjects)
    .where(eq(archiveProjects.ownerEmail, normalizeOwnerEmail(ownerEmail)))
    .orderBy(desc(archiveProjects.updatedAt));

  return rows.map(projectSummaryFromRow);
}

export async function getOwnedProject(ownerEmail: string, projectId: string) {
  await ensureArchiveStorage();
  const [row] = await getDb()
    .select()
    .from(archiveProjects)
    .where(and(
      eq(archiveProjects.id, projectId),
      eq(archiveProjects.ownerEmail, normalizeOwnerEmail(ownerEmail)),
    ))
    .limit(1);
  return row ?? null;
}

export function createProjectState({
  archiveTitle,
  storyTitle,
  theme,
}: {
  archiveTitle: string;
  storyTitle?: string;
  theme?: ArchiveTheme;
}) {
  return createBlankArchive({ archiveTitle, storyTitle, theme });
}

export function createProjectId() {
  return `project-${crypto.randomUUID().toLocaleLowerCase("en-US")}`;
}

export function projectSummaryFromRow(row: typeof archiveProjects.$inferSelect): ArchiveProjectSummary {
  const state = migrateArchive(JSON.parse(row.payload) as ArchiveState);
  return {
    id: row.id,
    title: state.profile.archiveTitle || row.title,
    storyTitle: state.profile.storyTitle || row.storyTitle,
    genre: state.profile.genre,
    status: state.profile.status,
    theme: state.profile.theme,
    characters: state.characters.length,
    relationships: state.relationships.length,
    worldEntries: state.world.length,
    magicSystems: state.magicSystems.length,
    chapters: state.manuscript.chapters,
    words: state.manuscript.words,
    createdAt: row.createdAt,
    updatedAt: row.updatedAt,
  };
}

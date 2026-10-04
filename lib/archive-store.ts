type ArchiveRuntime = typeof globalThis & {
  __ARCHIVE_DB__?: D1Database;
};

export async function ensureArchiveStorage() {
  const binding = (globalThis as ArchiveRuntime).__ARCHIVE_DB__;

  if (!binding) {
    throw new Error("El archivo persistente no está disponible.");
  }

  await binding.batch([
    binding.prepare(`
      CREATE TABLE IF NOT EXISTS archive_states (
        id TEXT PRIMARY KEY NOT NULL,
        payload TEXT NOT NULL,
        updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
      )
    `),
    binding.prepare(`
      CREATE TABLE IF NOT EXISTS archive_projects (
        id TEXT PRIMARY KEY NOT NULL,
        owner_email TEXT NOT NULL,
        title TEXT NOT NULL,
        story_title TEXT NOT NULL DEFAULT '',
        payload TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
      )
    `),
    binding.prepare(`
      CREATE INDEX IF NOT EXISTS archive_projects_owner_updated_idx
      ON archive_projects (owner_email, updated_at)
    `),
  ]);
}

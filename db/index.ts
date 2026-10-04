import { drizzle } from "drizzle-orm/d1";
import * as schema from "./schema";

type ArchiveRuntime = typeof globalThis & {
  __ARCHIVE_DB__?: D1Database;
};

export function getDb() {
  const binding = (globalThis as ArchiveRuntime).__ARCHIVE_DB__;

  if (!binding) {
    throw new Error(
      "Cloudflare D1 binding `DB` is unavailable. Set the `d1` field in .openai/hosting.json to `DB` or let your control plane inject the real binding values before using the database."
    );
  }

  return drizzle(binding, { schema });
}

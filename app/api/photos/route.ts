import { getArchiveBucket } from "../../../lib/archive-media";
import { getOwnedProject, isProjectId, normalizeOwnerEmail } from "../../../lib/archive-projects";
import { getChatGPTUser } from "../../chatgpt-auth";

const MAX_PHOTO_BYTES = 5 * 1024 * 1024;
const ALLOWED_TYPES = new Set(["image/jpeg", "image/png", "image/webp", "image/gif"]);
const LEGACY_PROJECT_ID = "legacy-main";

function validCharacterId(value: string | null): value is string {
  return Boolean(value && /^[a-z0-9-]{1,100}$/.test(value));
}

async function authenticatedOwner() {
  const user = await getChatGPTUser();
  return user ? normalizeOwnerEmail(user.email) : null;
}

async function ownerKey(email: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(email));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("").slice(0, 20);
}

async function projectCharacterKey(ownerEmail: string, projectId: string, characterId: string) {
  return `projects/${await ownerKey(ownerEmail)}/${projectId}/characters/${characterId}`;
}

function legacyCharacterKey(characterId: string) {
  return `characters/${characterId}`;
}

async function validateOwnedProject(ownerEmail: string, projectId: string | null) {
  if (!isProjectId(projectId)) return null;
  const project = await getOwnedProject(ownerEmail, projectId);
  return project ? projectId : null;
}

export async function GET(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para abrir esta fotografía." }, { status: 401 });

    const url = new URL(request.url);
    const characterId = url.searchParams.get("characterId");
    const requestedProjectId = url.searchParams.get("projectId");
    if (!validCharacterId(characterId)) return Response.json({ error: "Personaje no válido." }, { status: 400 });

    let key: string;
    if (requestedProjectId) {
      const projectId = await validateOwnedProject(ownerEmail, requestedProjectId);
      if (!projectId) return Response.json({ error: "No tienes acceso a esta fotografía." }, { status: 404 });
      key = await projectCharacterKey(ownerEmail, projectId, characterId);
    } else {
      const legacyProject = await getOwnedProject(ownerEmail, LEGACY_PROJECT_ID);
      if (!legacyProject) return Response.json({ error: "Fotografía no encontrada." }, { status: 404 });
      key = legacyCharacterKey(characterId);
    }

    const object = await getArchiveBucket().get(key);
    if (!object) return Response.json({ error: "Fotografía no encontrada." }, { status: 404 });

    return new Response(object.body, {
      headers: {
        "content-type": object.httpMetadata?.contentType ?? "application/octet-stream",
        "cache-control": "private, max-age=3600",
        etag: object.httpEtag,
      },
    });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo abrir la fotografía." },
      { status: 500 },
    );
  }
}

export async function POST(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para guardar una fotografía." }, { status: 401 });

    const form = await request.formData();
    const file = form.get("file");
    const projectValue = form.get("projectId");
    const characterValue = form.get("characterId");
    const projectId = await validateOwnedProject(ownerEmail, typeof projectValue === "string" ? projectValue : null);
    const characterId = typeof characterValue === "string" ? characterValue : null;

    if (!projectId) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });
    if (!validCharacterId(characterId)) return Response.json({ error: "Personaje no válido." }, { status: 400 });
    if (!(file instanceof File)) return Response.json({ error: "Selecciona una fotografía." }, { status: 400 });
    if (!ALLOWED_TYPES.has(file.type)) return Response.json({ error: "Usa una imagen JPG, PNG, WEBP o GIF." }, { status: 415 });
    if (file.size > MAX_PHOTO_BYTES) return Response.json({ error: "La fotografía supera el límite de 5 MB." }, { status: 413 });

    const key = await projectCharacterKey(ownerEmail, projectId, characterId);
    await getArchiveBucket().put(key, await file.arrayBuffer(), {
      httpMetadata: { contentType: file.type },
      customMetadata: { originalName: file.name.slice(0, 180), projectId },
    });

    const params = new URLSearchParams({ projectId, characterId, v: String(Date.now()) });
    return Response.json({ url: `/api/photos?${params.toString()}` });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo guardar la fotografía." },
      { status: 500 },
    );
  }
}

export async function DELETE(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para quitar esta fotografía." }, { status: 401 });

    const url = new URL(request.url);
    const projectId = await validateOwnedProject(ownerEmail, url.searchParams.get("projectId"));
    const characterId = url.searchParams.get("characterId");
    if (!projectId) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });
    if (!validCharacterId(characterId)) return Response.json({ error: "Personaje no válido." }, { status: 400 });

    const bucket = getArchiveBucket();
    await bucket.delete(await projectCharacterKey(ownerEmail, projectId, characterId));
    if (projectId === LEGACY_PROJECT_ID) await bucket.delete(legacyCharacterKey(characterId));
    return Response.json({ ok: true });
  } catch (error) {
    return Response.json(
      { error: error instanceof Error ? error.message : "No se pudo quitar la fotografía." },
      { status: 500 },
    );
  }
}

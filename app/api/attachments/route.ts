import { getArchiveBucket } from "../../../lib/archive-media";
import { getOwnedProject, isProjectId, normalizeOwnerEmail } from "../../../lib/archive-projects";
import type { ArchiveAttachment } from "../../../lib/archive-data";
import { getChatGPTUser } from "../../chatgpt-auth";

const MAX_ATTACHMENT_BYTES = 12 * 1024 * 1024;
const IMAGE_TYPES = new Set(["image/jpeg", "image/png", "image/webp", "image/gif"]);
const TEXT_TYPES = new Set(["text/plain", "text/markdown", "text/csv"]);
const DOCUMENT_TYPES = new Set([
  "application/pdf",
  "application/rtf",
  "text/rtf",
  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
]);
const EXTENSION_TYPES: Record<string, string> = {
  txt: "text/plain",
  md: "text/markdown",
  csv: "text/csv",
  rtf: "application/rtf",
  docx: "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
  pdf: "application/pdf",
  jpg: "image/jpeg",
  jpeg: "image/jpeg",
  png: "image/png",
  webp: "image/webp",
  gif: "image/gif",
};

type AttachmentScope = "world" | "magic";

function isScope(value: string | null): value is AttachmentScope {
  return value === "world" || value === "magic";
}

function validRecordId(value: string | null): value is string {
  return Boolean(value && /^[a-z0-9-]{1,120}$/.test(value));
}

function validAttachmentId(value: string | null): value is string {
  return Boolean(value && /^[a-z0-9-]{8,120}$/.test(value));
}

async function authenticatedOwner() {
  const user = await getChatGPTUser();
  return user ? normalizeOwnerEmail(user.email) : null;
}

async function ownerKey(email: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(email));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("").slice(0, 20);
}

async function attachmentKey(ownerEmail: string, projectId: string, scope: AttachmentScope, recordId: string, attachmentId: string) {
  return `projects/${await ownerKey(ownerEmail)}/${projectId}/attachments/${scope}/${recordId}/${attachmentId}`;
}

async function validateOwnedProject(ownerEmail: string, projectId: string | null) {
  if (!isProjectId(projectId)) return null;
  return await getOwnedProject(ownerEmail, projectId) ? projectId : null;
}

function normalizedMimeType(file: File) {
  if (IMAGE_TYPES.has(file.type) || TEXT_TYPES.has(file.type) || DOCUMENT_TYPES.has(file.type)) return file.type;
  const extension = file.name.split(".").pop()?.toLocaleLowerCase("en-US") ?? "";
  return EXTENSION_TYPES[extension] ?? null;
}

function attachmentKind(mimeType: string): ArchiveAttachment["kind"] {
  if (mimeType.startsWith("image/")) return "image";
  if (mimeType.startsWith("text/") || mimeType === "application/rtf") return "text";
  return "document";
}

function safeFileName(value: string) {
  return value.replace(/[\r\n"\\]/g, "_").slice(0, 180) || "archivo";
}

export async function GET(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para abrir este archivo." }, { status: 401 });

    const url = new URL(request.url);
    const projectId = await validateOwnedProject(ownerEmail, url.searchParams.get("projectId"));
    const scope = url.searchParams.get("scope");
    const recordId = url.searchParams.get("recordId");
    const attachmentId = url.searchParams.get("attachmentId");
    if (!projectId) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });
    if (!isScope(scope) || !validRecordId(recordId) || !validAttachmentId(attachmentId)) {
      return Response.json({ error: "Archivo no válido." }, { status: 400 });
    }

    const object = await getArchiveBucket().get(await attachmentKey(ownerEmail, projectId, scope, recordId, attachmentId));
    if (!object) return Response.json({ error: "Archivo no encontrado." }, { status: 404 });

    const contentType = object.httpMetadata?.contentType ?? "application/octet-stream";
    const originalName = safeFileName(object.customMetadata?.originalName ?? "archivo");
    const asciiName = originalName.replace(/[^\x20-\x7e]/g, "_");
    const disposition = contentType.startsWith("image/") || contentType.startsWith("text/") ? "inline" : "attachment";
    return new Response(object.body, {
      headers: {
        "content-type": contentType,
        "content-disposition": `${disposition}; filename="${asciiName}"; filename*=UTF-8''${encodeURIComponent(originalName)}`,
        "cache-control": "private, max-age=3600",
        etag: object.httpEtag,
      },
    });
  } catch (error) {
    return Response.json({ error: error instanceof Error ? error.message : "No se pudo abrir el archivo." }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para guardar archivos." }, { status: 401 });

    const form = await request.formData();
    const file = form.get("file");
    const projectValue = form.get("projectId");
    const scopeValue = form.get("scope");
    const recordValue = form.get("recordId");
    const projectId = await validateOwnedProject(ownerEmail, typeof projectValue === "string" ? projectValue : null);
    const scope = typeof scopeValue === "string" ? scopeValue : null;
    const recordId = typeof recordValue === "string" ? recordValue : null;

    if (!projectId) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });
    if (!isScope(scope) || !validRecordId(recordId)) return Response.json({ error: "Ficha no válida." }, { status: 400 });
    if (!(file instanceof File)) return Response.json({ error: "Selecciona un archivo." }, { status: 400 });
    if (file.size > MAX_ATTACHMENT_BYTES) return Response.json({ error: "El archivo supera el límite de 12 MB." }, { status: 413 });

    const mimeType = normalizedMimeType(file);
    if (!mimeType || !IMAGE_TYPES.has(mimeType)) {
      return Response.json({ error: "Este espacio solo guarda imágenes JPG, PNG, WEBP o GIF. Importa los documentos como texto desde el apartado correspondiente." }, { status: 415 });
    }

    const attachmentId = `file-${crypto.randomUUID().toLocaleLowerCase("en-US")}`;
    const originalName = safeFileName(file.name);
    await getArchiveBucket().put(await attachmentKey(ownerEmail, projectId, scope, recordId, attachmentId), await file.arrayBuffer(), {
      httpMetadata: { contentType: mimeType },
      customMetadata: { originalName, projectId, scope, recordId },
    });

    const attachment: ArchiveAttachment = {
      id: attachmentId,
      name: originalName,
      mimeType,
      size: file.size,
      kind: attachmentKind(mimeType),
    };
    return Response.json({ attachment });
  } catch (error) {
    return Response.json({ error: error instanceof Error ? error.message : "No se pudo guardar el archivo." }, { status: 500 });
  }
}

export async function DELETE(request: Request) {
  try {
    const ownerEmail = await authenticatedOwner();
    if (!ownerEmail) return Response.json({ error: "Inicia sesión para quitar archivos." }, { status: 401 });

    const url = new URL(request.url);
    const projectId = await validateOwnedProject(ownerEmail, url.searchParams.get("projectId"));
    const scope = url.searchParams.get("scope");
    const recordId = url.searchParams.get("recordId");
    const attachmentId = url.searchParams.get("attachmentId");
    if (!projectId) return Response.json({ error: "No tienes acceso a esta obra." }, { status: 404 });
    if (!isScope(scope) || !validRecordId(recordId) || !validAttachmentId(attachmentId)) {
      return Response.json({ error: "Archivo no válido." }, { status: 400 });
    }

    await getArchiveBucket().delete(await attachmentKey(ownerEmail, projectId, scope, recordId, attachmentId));
    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error instanceof Error ? error.message : "No se pudo quitar el archivo." }, { status: 500 });
  }
}

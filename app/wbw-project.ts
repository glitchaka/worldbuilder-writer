import { migrateArchive, type ArchiveAttachment, type ArchiveState } from "../lib/archive-data";

export const WBW_FORMAT = "worldbuilder-writer-project";
export const WBW_VERSION = 1;
export const WBW_MIME = "application/vnd.worldbuilder-writer.project+zip";

type CharacterAsset = {
  kind: "character-photo";
  path: string;
  characterId: string;
  name: string;
  mimeType: string;
};

type AttachmentAsset = {
  kind: "attachment";
  path: string;
  scope: "world" | "magic";
  recordId: string;
  attachmentId: string;
  name: string;
  mimeType: string;
};

export type WbwAsset = CharacterAsset | AttachmentAsset;

type WbwManifest = {
  format: typeof WBW_FORMAT;
  version: typeof WBW_VERSION;
  application: "Worldbuilder Writer";
  createdAt: string;
  projectFile: "project.json";
  assets: WbwAsset[];
};

export type ImportedWbwProject = {
  state: ArchiveState;
  assets: Array<{ descriptor: WbwAsset; file: File }>;
};

function blobDataUrl(blob: Blob) {
  return new Promise<string>((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result ?? ""));
    reader.onerror = () => reject(reader.error ?? new Error("No se pudo preparar un recurso del proyecto."));
    reader.readAsDataURL(blob);
  });
}

export async function embedWbwAssets(project: ImportedWbwProject) {
  let state = migrateArchive(project.state);
  for (const asset of project.assets) {
    const dataUrl = await blobDataUrl(asset.file);
    const descriptor = asset.descriptor;
    if (descriptor.kind === "character-photo") {
      state = {
        ...state,
        characters: state.characters.map((character) => character.id === descriptor.characterId ? { ...character, imageUrl: dataUrl } : character),
      };
      continue;
    }

    const replaceAttachment = (attachments: ArchiveAttachment[] | undefined) => {
      const current = attachments ?? [];
      const next: ArchiveAttachment = {
        id: descriptor.attachmentId,
        name: descriptor.name,
        mimeType: descriptor.mimeType,
        size: asset.file.size,
        kind: descriptor.mimeType.startsWith("image/")
          ? "image"
          : descriptor.mimeType.startsWith("text/") || /\.(txt|md|csv)$/i.test(descriptor.name)
            ? "text"
            : "document",
        dataUrl,
      };
      return current.some((item) => item.id === descriptor.attachmentId)
        ? current.map((item) => item.id === descriptor.attachmentId ? { ...item, ...next } : item)
        : [...current, next];
    };

    state = descriptor.scope === "world"
      ? { ...state, world: state.world.map((record) => record.id === descriptor.recordId ? { ...record, attachments: replaceAttachment(record.attachments) } : record) }
      : { ...state, magicSystems: state.magicSystems.map((record) => record.id === descriptor.recordId ? { ...record, attachments: replaceAttachment(record.attachments) } : record) };
  }
  return state;
}

function cleanSegment(value: string) {
  return value.normalize("NFKD").replace(/[\u0300-\u036f]/g, "").replace(/[^a-z0-9._-]+/gi, "-").replace(/^-+|-+$/g, "").slice(0, 100) || "asset";
}

function extensionFor(mimeType: string, name = "") {
  const provided = name.match(/\.([a-z0-9]{1,8})$/i)?.[1];
  if (provided) return provided.toLocaleLowerCase("en-US");
  const known: Record<string, string> = {
    "image/jpeg": "jpg",
    "image/png": "png",
    "image/webp": "webp",
    "image/gif": "gif",
  };
  return known[mimeType] ?? "bin";
}

function attachmentUrl(projectId: string, scope: "world" | "magic", recordId: string, attachmentId: string) {
  const params = new URLSearchParams({ projectId, scope, recordId, attachmentId });
  return `/api/attachments?${params.toString()}`;
}

async function fetchAsset(url: string, label: string) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`No se pudo incorporar “${label}” al paquete .wbw.`);
  return response.blob();
}

function cloneForPackage(state: ArchiveState) {
  const clone = JSON.parse(JSON.stringify(state)) as ArchiveState;
  clone.characters = clone.characters.map((character) => ({ ...character, imageUrl: character.imageUrl ? "" : undefined }));
  clone.world = clone.world.map((record) => ({ ...record, attachments: (record.attachments ?? []).map((attachment) => ({ ...attachment, dataUrl: undefined })) }));
  clone.magicSystems = clone.magicSystems.map((record) => ({ ...record, attachments: (record.attachments ?? []).map((attachment) => ({ ...attachment, dataUrl: undefined })) }));
  return clone;
}

export async function createWbwPackage(state: ArchiveState, projectId: string) {
  const { default: JSZip } = await import("jszip");
  const zip = new JSZip();
  const assets: WbwAsset[] = [];

  for (const character of state.characters) {
    if (!character.imageUrl) continue;
    const blob = await fetchAsset(character.imageUrl, `fotografía de ${character.name}`);
    const mimeType = blob.type || "image/jpeg";
    const path = `media/personajes/${cleanSegment(character.id)}.${extensionFor(mimeType)}`;
    zip.file(path, blob);
    assets.push({ kind: "character-photo", path, characterId: character.id, name: `${character.name}.${extensionFor(mimeType)}`, mimeType });
  }

  const attachmentGroups: Array<{ scope: "world" | "magic"; records: Array<{ id: string; attachments?: ArchiveAttachment[] }> }> = [
    { scope: "world", records: state.world },
    { scope: "magic", records: state.magicSystems },
  ];
  for (const group of attachmentGroups) {
    for (const record of group.records) {
      for (const attachment of record.attachments ?? []) {
        const source = attachment.dataUrl ?? attachmentUrl(projectId, group.scope, record.id, attachment.id);
        const blob = await fetchAsset(source, attachment.name);
        const mimeType = blob.type || attachment.mimeType || "application/octet-stream";
        const path = `media/${group.scope}/${cleanSegment(record.id)}/${cleanSegment(attachment.id)}-${cleanSegment(attachment.name)}.${extensionFor(mimeType, attachment.name)}`;
        zip.file(path, blob);
        assets.push({ kind: "attachment", path, scope: group.scope, recordId: record.id, attachmentId: attachment.id, name: attachment.name, mimeType });
      }
    }
  }

  const manifest: WbwManifest = {
    format: WBW_FORMAT,
    version: WBW_VERSION,
    application: "Worldbuilder Writer",
    createdAt: new Date().toISOString(),
    projectFile: "project.json",
    assets,
  };
  zip.file("manifest.json", JSON.stringify(manifest, null, 2));
  zip.file("project.json", JSON.stringify(cloneForPackage(state), null, 2));
  zip.file("LEEME.txt", "Paquete de proyecto de Worldbuilder Writer. Contiene el proyecto editable y todos sus recursos. Impórtalo desde la biblioteca de la aplicación; no es necesario descomprimirlo.\n");
  return zip.generateAsync({ type: "blob", compression: "DEFLATE", compressionOptions: { level: 6 }, mimeType: WBW_MIME });
}

function isManifest(value: unknown): value is WbwManifest {
  if (!value || typeof value !== "object") return false;
  const manifest = value as Partial<WbwManifest>;
  return manifest.format === WBW_FORMAT && manifest.version === WBW_VERSION && manifest.projectFile === "project.json" && Array.isArray(manifest.assets);
}

function isAsset(value: unknown): value is WbwAsset {
  if (!value || typeof value !== "object") return false;
  const asset = value as Partial<WbwAsset>;
  if (typeof asset.path !== "string" || !asset.path.startsWith("media/") || asset.path.includes("..") || typeof asset.name !== "string" || typeof asset.mimeType !== "string") return false;
  if (asset.kind === "character-photo") return typeof asset.characterId === "string";
  return asset.kind === "attachment" && (asset.scope === "world" || asset.scope === "magic") && typeof asset.recordId === "string" && typeof asset.attachmentId === "string";
}

export async function readWbwPackage(file: File): Promise<ImportedWbwProject> {
  if (!file.name.toLocaleLowerCase("es").endsWith(".wbw")) throw new Error("Selecciona un proyecto con extensión .wbw.");
  if (file.size > 300 * 1024 * 1024) throw new Error("El paquete supera el límite de 300 MB.");
  const { default: JSZip } = await import("jszip");
  const zip = await JSZip.loadAsync(file, { checkCRC32: true });
  const manifestEntry = zip.file("manifest.json");
  const projectEntry = zip.file("project.json");
  if (!manifestEntry || !projectEntry) throw new Error("El paquete .wbw está incompleto.");

  const manifest = JSON.parse(await manifestEntry.async("string")) as unknown;
  if (!isManifest(manifest)) throw new Error("El archivo no es un proyecto compatible de Worldbuilder Writer.");
  if (manifest.assets.length > 1500 || manifest.assets.some((asset) => !isAsset(asset))) throw new Error("La lista de recursos del paquete no es válida.");

  const projectRaw = await projectEntry.async("string");
  if (new TextEncoder().encode(projectRaw).byteLength > 4_000_000) throw new Error("Los datos editables del proyecto superan el límite admitido.");
  const parsed = JSON.parse(projectRaw) as ArchiveState;
  if (!parsed || typeof parsed !== "object" || !Array.isArray(parsed.characters) || !Array.isArray(parsed.relationships)) throw new Error("El proyecto incluido no contiene datos editables válidos.");
  const state = migrateArchive(parsed);

  const assets: ImportedWbwProject["assets"] = [];
  for (const descriptor of manifest.assets) {
    const entry = zip.file(descriptor.path);
    if (!entry) throw new Error(`Falta el recurso “${descriptor.name}” dentro del paquete.`);
    const blob = await entry.async("blob");
    assets.push({ descriptor, file: new File([blob], descriptor.name, { type: descriptor.mimeType }) });
  }
  return { state, assets };
}

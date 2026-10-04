type ArchiveRuntime = typeof globalThis & {
  __ARCHIVE_BUCKET__?: R2Bucket;
};

export function getArchiveBucket() {
  const bucket = (globalThis as ArchiveRuntime).__ARCHIVE_BUCKET__;
  if (!bucket) throw new Error("El almacén de archivos no está disponible.");
  return bucket;
}

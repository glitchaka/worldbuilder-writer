import { readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const dist = resolve(here, "dist");
const input = resolve(dist, "index.html");
let html = await readFile(input, "utf8");

const scriptMatch = html.match(/<script type="module"[^>]*src="([^"]+)"[^>]*><\/script>/);
if (!scriptMatch) throw new Error("No se encontró el bundle JavaScript del escritorio.");
const styleMatch = html.match(/<link rel="stylesheet"[^>]*href="([^"]+)"[^>]*>/);
const scriptPath = resolve(dist, scriptMatch[1].replace(/^\.\//, ""));
const script = (await readFile(scriptPath, "utf8")).replaceAll("</script", "<\\/script");
html = html.replace(scriptMatch[0], `<script type="module">${script}</script>`);

if (styleMatch) {
  const stylePath = resolve(dist, styleMatch[1].replace(/^\.\//, ""));
  let style = await readFile(stylePath, "utf8");
  const publicAssetPattern = /url\((\/assets\/[^)]+)\)/g;
  for (const match of [...style.matchAll(publicAssetPattern)]) {
    const assetUrl = match[1];
    const asset = await readFile(resolve(here, "../public", assetUrl.replace(/^\//, "")));
    const extension = assetUrl.split(".").pop()?.toLowerCase();
    const mimeType = extension === "webp" ? "image/webp" : extension === "png" ? "image/png" : "image/jpeg";
    style = style.replaceAll(match[0], `url(data:${mimeType};base64,${asset.toString("base64")})`);
  }
  html = html.replace(styleMatch[0], `<style>${style}</style>`);
}

await writeFile(resolve(dist, "archivo-neo-desktop.html"), html);

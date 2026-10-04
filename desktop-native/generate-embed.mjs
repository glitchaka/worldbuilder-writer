import { readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

const input = resolve(process.argv[2]);
const output = resolve(process.argv[3]);
const bytes = await readFile(input);
const lines = [];
for (let index = 0; index < bytes.length; index += 24) {
  lines.push(`  ${[...bytes.subarray(index, index + 24)].map((byte) => `0x${byte.toString(16).padStart(2, "0")}`).join(", ")},`);
}
await writeFile(output, [
  "static const unsigned char embedded_app_html[] = {",
  ...lines,
  "  0x00",
  "};",
  `static const size_t embedded_app_html_len = ${bytes.length}u;`,
  "",
].join("\n"));

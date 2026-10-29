/** Open local installed ZIPs or PC disc sets with bounded temporary storage. */
import {readISO, detectISO} from './iso.mjs';

const decoder = new TextDecoder();
const table = Uint32Array.from({length: 256}, (_, value) => {
  for (let bit = 0; bit < 8; bit++) value = (value >>> 1) ^ (value & 1 ? 0xedb88320 : 0);
  return value >>> 0;
});
function safe(path) {
  path = path.replaceAll('\\', '/');
  if (!path || path.startsWith('/') || path.split('/').some(part => !part || part === '.' || part === '..' || part.includes(':'))) throw new Error('The archive contains an unsafe path.');
  return path;
}
async function bytes(file, start, count) {
  if (!Number.isSafeInteger(start) || !Number.isSafeInteger(count) || start < 0 || count < 0 || start + count > file.size) throw new Error('An archive points outside its input file.');
  return new Uint8Array(await file.slice(start, start + count).arrayBuffer());
}

export async function readZIP(file, {storage, onProgress = () => {}, prefix = crypto.randomUUID()} = {}) {
  const tail = await bytes(file, Math.max(0, file.size - 65557), Math.min(file.size, 65557));
  const footer = new DataView(tail.buffer);
  let end = -1;
  for (let at = tail.length - 22; at >= 0; --at) if (footer.getUint32(at, true) === 0x06054b50 && at + 22 + footer.getUint16(at + 20, true) === tail.length) { end = at; break; }
  if (end < 0) throw new Error('The ZIP directory is missing or incomplete.');
  const count = footer.getUint16(end + 10, true), length = footer.getUint32(end + 12, true), offset = footer.getUint32(end + 16, true);
  if (footer.getUint16(end + 4, true) || footer.getUint16(end + 6, true) || count !== footer.getUint16(end + 8, true)) throw new Error('Split ZIP archives are unsupported. Select all PC disc images instead.');
  if (count === 65535 || offset === 0xffffffff || length === 0xffffffff) throw new Error('This ZIP64 archive is unsupported. Select its extracted game folder or ISO.');
  if (length > 32 * 1024 ** 2 || count > 20000) throw new Error('The ZIP directory is too large.');
  const directory = await bytes(file, offset, length), view = new DataView(directory.buffer);
  const result = new Map(), names = new Set(); let cursor = 0, total = 0, sequence = 0;
  for (let index = 0; index < count; index++) {
    if (cursor + 46 > directory.length || view.getUint32(cursor, true) !== 0x02014b50) throw new Error('A ZIP directory record is damaged.');
    const flags = view.getUint16(cursor + 8, true), method = view.getUint16(cursor + 10, true);
    const expected = view.getUint32(cursor + 16, true), compressed = view.getUint32(cursor + 20, true), size = view.getUint32(cursor + 24, true);
    const nameBytes = view.getUint16(cursor + 28, true), extra = view.getUint16(cursor + 30, true), comment = view.getUint16(cursor + 32, true), local = view.getUint32(cursor + 42, true);
    if (cursor + 46 + nameBytes + extra + comment > directory.length || flags & 1 || ![0, 8].includes(method)) throw new Error('The ZIP is encrypted, damaged or uses unsupported compression.');
    const raw = decoder.decode(directory.subarray(cursor + 46, cursor + 46 + nameBytes));
    cursor += 46 + nameBytes + extra + comment;
    if (raw.endsWith('/')) continue;
    const path = safe(raw), key = path.toLowerCase();
    if (names.has(key)) throw new Error('The ZIP contains duplicate game paths.');
    names.add(key); total += size;
    if (size > 4 * 1024 ** 3 - 1 || total > 8 * 1024 ** 3) throw new Error('The ZIP expands beyond the supported PC game size.');
    const header = await bytes(file, local, 30), h = new DataView(header.buffer);
    if (h.getUint32(0, true) !== 0x04034b50 || h.getUint16(8, true) !== method) throw new Error('A ZIP file header is damaged.');
    const start = local + 30 + h.getUint16(26, true) + h.getUint16(28, true);
    if (start + compressed > offset) throw new Error('A ZIP payload overlaps its directory.');
    const stored = file.slice(start, start + compressed);
    let crc = 0xffffffff, produced = 0;
    const checked = new TransformStream({transform(chunk, controller) {
      produced += chunk.length;
      if (produced > size) throw new Error('A ZIP payload exceeds its declared size.');
      for (let i = 0; i < chunk.length; ++i) crc = table[(crc ^ chunk[i]) & 255] ^ (crc >>> 8);
      controller.enqueue(chunk);
    }, flush() {
      if (produced !== size || ((crc ^ 0xffffffff) >>> 0) !== expected) throw new Error(`The ZIP contains damaged data: ${path}.`);
    }});
    const stream = (method === 8 ? stored.stream().pipeThrough(new DecompressionStream('deflate-raw')) : stored.stream()).pipeThrough(checked);
    let output;
    if (method === 8) {
      if (!storage) throw new Error('Temporary storage is required to open a compressed ZIP.');
      output = await storage.write(`zip-${prefix}/expanded-${sequence++}`, {stream: () => stream});
    } else {
      const reader = stream.getReader();
      for (;;) { if ((await reader.read()).done) break; }
      output = stored;
    }
    result.set(path, output);
    onProgress({stage: 'inspect', detail: 'Checking archive contents', completed: index + 1, total: count});
  }
  return result;
}

async function equal(left, right) {
  if (left.size !== right.size) return false;
  for (let at = 0; at < left.size; at += 1024 ** 2) {
    const [a, b] = await Promise.all([bytes(left, at, Math.min(1024 ** 2, left.size - at)), bytes(right, at, Math.min(1024 ** 2, right.size - at))]);
    if (a.some((value, index) => value !== b[index])) return false;
  }
  return true;
}

export async function readGameImages(inputs, {storage, onProgress = () => {}} = {}) {
  const expanded = new Map(); let index = 0;
  for (const input of inputs) {
    if (/\.zip$/i.test(input.name)) {
      const archive = await readZIP(input, {storage, onProgress});
      const installed = await detectISO(archive);
      if (installed.kind === 'installed') {
        if (inputs.length !== 1) throw new Error('Select one installed-copy ZIP, or one complete set of disc images.');
        return installed;
      }
      for (const [name, file] of archive) if (/\.iso$/i.test(name)) expanded.set(`${index++}/${name}`, file);
    } else expanded.set(`${index++}/${input.name}`, input);
  }
  if (!expanded.size) throw new Error('The selected archive contains neither a PC ISO nor an installed game folder.');
  const files = new Map(), known = new Map();
  for (const [name, image] of expanded) {
    onProgress({stage: 'inspect', detail: `Reading ${name.split('/').at(-1)}`});
    for (const [path, file] of await readISO(image)) {
      const prior = known.get(path.toLowerCase());
      if (prior) {
        if (!await equal(files.get(prior), file)) throw new Error(`The disc images conflict at ${path}. Select discs from the same PC edition.`);
      } else { known.set(path.toLowerCase(), path); files.set(path, file); }
    }
  }
  return detectISO(files);
}

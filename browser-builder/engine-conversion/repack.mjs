/** Static FreeArc/Inno data reader. Installer programs are never executed. */
import {mountDiskOutput} from '../opfs.mjs';

const decoder = new TextDecoder('utf-8', {fatal: true});
const encoder = new TextEncoder();
const crcTable = Uint32Array.from({length: 256}, (_, n) => {
  let value = n;
  for (let k = 0; k < 8; k++) value = value & 1 ? 0xedb88320 ^ value >>> 1 : value >>> 1;
  return value >>> 0;
});
function crcUpdate(state, data) {
  for (const byte of data) state = crcTable[(state ^ byte) & 255] ^ state >>> 8;
  return state >>> 0;
}
function crc32(data) { return (crcUpdate(0xffffffff, data) ^ 0xffffffff) >>> 0; }
function check(signal) { if (signal?.aborted) throw new DOMException('Build cancelled', 'AbortError'); }
function fail(detail) { throw new Error(`This installer archive cannot be read: ${detail}`); }
function bytesHex(bytes) { return [...bytes].map(n => n.toString(16).padStart(2, '0')).join(''); }
function unhex(text) {
  if (!/^(?:[a-f0-9]{2})+$/i.test(text)) fail('invalid encryption parameters');
  return Uint8Array.from(text.match(/../g), value => Number.parseInt(value, 16));
}
class Reader {
  constructor(data) { this.data = data; this.position = 0; }
  read(length) {
    if (!Number.isSafeInteger(length) || length < 0 || this.position + length > this.data.length) fail('truncated metadata');
    const result = this.data.subarray(this.position, this.position + length); this.position += length; return result;
  }
  number() {
    const first = this.read(1)[0]; let bits = 0;
    while (bits < 8 && (first & 1 << bits)) bits++;
    let result;
    if (bits === 8) result = this.read(8).reduceRight((v, b) => v * 256n + BigInt(b), 0n);
    else {
      const tail = this.read(bits); let value = BigInt(first);
      for (let i = 0; i < tail.length; i++) value |= BigInt(tail[i]) << BigInt(8 * (i + 1));
      result = value >> BigInt(bits + 1);
    }
    if (result > BigInt(Number.MAX_SAFE_INTEGER)) fail('metadata size is too large');
    return Number(result);
  }
  uint32() { return new DataView(this.read(4).buffer, this.data.byteOffset + this.position - 4, 4).getUint32(0, true); }
  string() {
    const end = this.data.indexOf(0, this.position);
    if (end < 0 || end - this.position > 4096) fail('invalid metadata string');
    const value = decoder.decode(this.data.subarray(this.position, end)); this.position = end + 1; return value;
  }
}
function boundedCount(reader) { const count = reader.number(); if (count > 100000) fail('too many entries'); return count; }
function methodParts(method) {
  const parts = method.split('+');
  if (parts.length > 8 || parts.some(part => !/^(?:storing|grzip|rep|exe|delta|lzma|4x4|aes-256\/ctr)(?::[a-zA-Z0-9./-]+)*$/.test(part))) fail('unsupported compression method');
  return parts;
}
function encryption(method) { return Object.fromEntries(method.split(':').slice(1).map(value => [value[0], value.slice(1)])); }
async function derived(password, method) {
  const p = encryption(method), iterations = Number(p.n);
  if (!Number.isInteger(iterations) || iterations < 1 || iterations > 100000 || !p.s || !p.c || !p.i) fail('unsupported encryption parameters');
  const key = await crypto.subtle.importKey('raw', encoder.encode(password), 'PBKDF2', false, ['deriveBits']);
  const bytes = new Uint8Array(await crypto.subtle.deriveBits({name: 'PBKDF2', hash: 'SHA-512', salt: unhex(p.s), iterations}, key, 272));
  if (bytesHex(bytes.subarray(32)) !== p.c) return null;
  return `${method}:k${bytesHex(bytes.subarray(0, 32))}`;
}
async function remove(storage, name) {
  const parts = name.split('/'); let dir = storage.directory;
  for (const part of parts.slice(0, -1)) dir = await dir.getDirectoryHandle(part);
  await dir.removeEntry(parts.at(-1));
}
async function makeCodec(storage, onProgress, factory) {
  const create = factory || (await import('../vendor/freearc-codec.mjs')).default;
  const logs = [], module = await create({noInitialRun: true, print: () => {}, printErr: text => logs.push(text)});
  module.FS.mkdir('/input'); let id = 0;
  return async (method, input, small = false) => {
    const name = `extract/stage-${id++}.bin`, path = '/output/data'; let destination;
    module.FS.mount(module.WORKERFS, {blobs: [{name: 'data', data: input}]}, '/input');
    try {
      module.FS.mkdirTree('/output');
      if (!small) {
        destination = await storage.openSync(name);
        mountDiskOutput(module, path, destination.handle, bytes => onProgress({stage: 'extract', bytes, detail: 'Unpacking your PC game files'}));
      }
      logs.length = 0;
      const result = module.callMain([method, '/input/data', path]);
      if (result) fail(logs.at(-1) || 'compressed data is damaged or unsupported');
      if (small) {
        const data = module.FS.readFile(path);
        if (data.length > 64 * 1024 * 1024) fail('control metadata is too large');
        return {blob: new Blob([data]), name: null};
      }
      destination.handle.flush(); destination.handle.close(); destination.handle = null;
      return {blob: await destination.file.getFile(), name};
    } finally {
      if (destination?.handle) destination.handle.close();
      module.FS.unmount('/input');
      try { module.FS.unlink(path); } catch {}
    }
  };
}
async function decodeChain(method, blob, password, codec, storage, signal, small) {
  let previous = null;
  for (let part of methodParts(method).reverse()) {
    check(signal);
    if (part.startsWith('aes-')) { part = await derived(password, part); if (!part) fail('archive password does not match'); }
    const output = await codec(part, blob, small);
    if (previous) await remove(storage, previous);
    previous = output.name; blob = output.blob;
  }
  return {blob, name: previous};
}
async function innoConstants(file, codec) {
  if (file.size > 32 * 1024 * 1024) fail('unsupported installer header size');
  const bytes = new Uint8Array(await file.arrayBuffer());
  if (!decoder.decode(bytes.subarray(0, 33)).startsWith('Inno Setup Setup Data (5.5.0)')) fail('unsupported Inno Setup version');
  const r = new Reader(bytes); r.read(64);
  const expected = r.uint32(), header = r.read(5), size = new DataView(header.buffer, header.byteOffset, 4).getUint32(0, true);
  if (crc32(header) !== expected) fail('installer header checksum failed');
  const stored = r.read(size), chunks = [];
  for (let p = 0; p < stored.length; p += 4100) {
    const block = stored.subarray(p, p + 4100);
    if (block.length < 5 || crc32(block.subarray(4)) !== new DataView(block.buffer, block.byteOffset, 4).getUint32(0, true)) fail('installer block checksum failed');
    chunks.push(block.subarray(4));
  }
  let blob = new Blob(chunks);
  if (header[4]) {
    const props = new Uint8Array(await blob.slice(0, 5).arrayBuffer());
    if (props[0] !== 93) fail('unsupported installer LZMA properties');
    const dictionary = new DataView(props.buffer, props.byteOffset + 1, 4).getUint32(0, true);
    blob = (await codec(`lzma:${dictionary}b`, blob.slice(5), true)).blob;
  }
  const data = new Uint8Array(await blob.arrayBuffer()); let text = '';
  for (let p = 0; p < data.length; p += 32768) text += String.fromCharCode(...data.subarray(p, p + 32768));
  const strings = new Set(text.match(/[ -~]{3,}/g) || []);
  for (const value of text.match(/(?:[ -~]\x00){3,}/g) || []) strings.add(value.replaceAll('\x00', ''));
  return [...strings].filter(value => value.length <= 128).sort((a, b) => (b.length === 32) - (a.length === 32));
}
function footerDescriptor(file, tail) {
  let offset = -1;
  for (let i = tail.length - 4; i >= 0; i--) if (tail[i] === 65 && tail[i + 1] === 114 && tail[i + 2] === 67 && tail[i + 3] === 1) { offset = i; break; }
  if (offset < 0) fail('FreeArc footer is missing');
  const bytes = tail.subarray(offset), r = new Reader(bytes);
  if (crc32(bytes.subarray(0, -4)) !== new DataView(bytes.buffer, bytes.byteOffset + bytes.length - 4, 4).getUint32(0, true)) fail('FreeArc footer checksum failed');
  r.read(4); const type = r.number(), method = r.string(), original = r.number(), compressed = r.number(), crc = r.uint32();
  if (type !== 4 || r.position !== bytes.length - 4) fail('invalid FreeArc footer');
  return {type, method, original, compressed, crc, position: file.size - tail.length + offset - compressed};
}
async function control(file, block, password, codec, storage, signal) {
  if (block.original > 64 * 1024 * 1024 || block.position < 0 || block.position + block.compressed > file.size) fail('invalid control block bounds');
  const {blob} = await decodeChain(block.method, file.slice(block.position, block.position + block.compressed), password, codec, storage, signal, true);
  const data = new Uint8Array(await blob.arrayBuffer());
  if (data.length !== block.original || crc32(data) !== block.crc) fail('control block checksum failed');
  return data;
}
function directory(data, base) {
  const r = new Reader(data), count = boundedCount(r), array = fn => Array.from({length: count}, fn);
  const counts = array(() => boundedCount(r)), methods = array(() => r.string()), offsets = array(() => r.number()), compressed = array(() => r.number());
  const folders = Array.from({length: boundedCount(r)}, () => r.string()), total = counts.reduce((a, b) => a + b, 0);
  if (total > 100000) fail('too many files');
  const all = fn => Array.from({length: total}, fn), names = all(() => r.string()), parents = all(() => r.number()), sizes = all(() => r.number());
  all(() => r.uint32()); const flags = r.read(total), checksums = all(() => r.uint32());
  if (r.number() !== 0 || r.position !== data.length) fail('unsupported directory fields');
  let index = 0;
  return methods.map((method, i) => ({method, position: base - offsets[i], compressed: compressed[i], members: Array.from({length: counts[i]}, () => {
    const n = index++; if (parents[n] >= folders.length) fail('invalid directory index');
    const path = `${folders[parents[n]]}/${names[n]}`.replaceAll('\\', '/').replace(/^\//, '');
    if (path.split('/').some(part => !part || part === '.' || part === '..') || path.includes(':')) fail('unsafe archive path');
    return {path, size: sizes[n], crc: checksums[n], folder: !!flags[n]};
  })}));
}
async function readArchive(file, candidates, codec, storage, signal) {
  const footer = footerDescriptor(file, new Uint8Array(await file.slice(-4096).arrayBuffer()));
  let password = '', decoded;
  if (footer.method.includes('aes-')) {
    const aes = methodParts(footer.method).find(part => part.startsWith('aes-'));
    for (const value of candidates) {
      check(signal); if (!await derived(value, aes)) continue;
      try { decoded = await control(file, footer, value, codec, storage, signal); password = value; break; } catch {}
    }
    if (!decoded) fail('the archive key was not found in this installer');
  } else decoded = await control(file, footer, password, codec, storage, signal);
  const r = new Reader(decoded), count = boundedCount(r), blocks = [];
  for (let i = 0; i < count; i++) {
    const type = r.number(), method = r.string(), relative = r.number(), original = r.number(), compressed = r.number(), crc = r.uint32();
    const block = {type, method, original, compressed, crc, position: footer.position - relative};
    if (type === 3) blocks.push(...directory(await control(file, block, password, codec, storage, signal), block.position));
  }
  return {password, blocks};
}
async function verify(blob, expected, signal) {
  let crc = 0xffffffff;
  for (let p = 0; p < blob.size; p += 1024 * 1024) { check(signal); crc = crcUpdate(crc, new Uint8Array(await blob.slice(p, p + 1024 * 1024).arrayBuffer())); }
  if (((crc ^ 0xffffffff) >>> 0) !== expected) fail('a game file failed its checksum');
}

export async function extractRepack(layout, {storage, onProgress = () => {}, signal, codecFactory} = {}) {
  if (!storage) throw new Error('Temporary browser storage is required to unpack this disc.');
  const entries = [...layout.files], metadata = entries.find(([name]) => /(?:^|\/)setup-0\.bin$/i.test(name));
  const primary = entries.find(([name]) => /(?:^|\/)elamigos-1\.bin$/i.test(name));
  const english = entries.find(([name]) => /(?:^|\/)elamigos-english-polish\.bin$/i.test(name));
  if (!metadata || !primary || !english) throw new Error('This disc layout is unsupported. This installer needs its main, English and setup data archives.');
  onProgress({stage: 'extract', detail: 'Reading the disc archive information'});
  const codec = await makeCodec(storage, onProgress, codecFactory), candidates = await innoConstants(metadata[1], codec), files = new Map();
  for (const [, archive] of [primary, english]) {
    const {password, blocks} = await readArchive(archive, candidates, codec, storage, signal);
    for (const block of blocks) {
      check(signal); const size = block.members.reduce((n, file) => n + file.size, 0);
      if (!size) continue;
      if (block.position < 0 || block.compressed < 0 || block.position + block.compressed > archive.size) fail('invalid game block bounds');
      const result = await decodeChain(block.method, archive.slice(block.position, block.position + block.compressed), password, codec, storage, signal, false);
      if (result.blob.size !== size) fail('a game block has an incorrect size');
      let position = 0;
      for (const file of block.members) {
        if (file.folder) continue;
        const blob = result.blob.slice(position, position + file.size); position += file.size;
        await verify(blob, file.crc, signal); files.set(file.path, blob);
      }
      onProgress({stage: 'extract', detail: 'Verified the unpacked game files', completed: files.size});
    }
  }
  return files;
}

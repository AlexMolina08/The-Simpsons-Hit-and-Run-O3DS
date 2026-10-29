/** Bounded ISO9660/Joliet inventory. File contents remain slices of the ISO. */
// Record layouts: ECMA-119 sections 8/9 and Annex C (Joliet).
// https://ecma-international.org/publications-and-standards/standards/ecma-119/
import {LANGUAGES, availableLanguages} from './languages.mjs';

const SECTOR_BYTES = 2048;
const SIGNATURE = [67, 68, 48, 48, 49]; // CD001
const DEFAULT_LIMITS = Object.freeze({
  maxDescriptors: 128,
  maxEntries: 100000,
  maxDepth: 32,
  maxDirectoryBytes: 8 * 1024 * 1024,
  maxMetadataBytes: 64 * 1024 * 1024,
});

function fail(message) { throw new Error(`Cannot read this ISO: ${message}`); }
function integer(value, label, maximum) {
  if (!Number.isSafeInteger(value) || value < 1 || value > maximum) fail(`invalid ${label} limit.`);
  return value;
}
function both16(view, offset) {
  const value = view.getUint16(offset, true);
  if (value !== view.getUint16(offset + 2, false)) fail('the directory contains inconsistent numbers.');
  return value;
}
function both32(view, offset) {
  const value = view.getUint32(offset, true);
  if (value !== view.getUint32(offset + 4, false)) fail('the directory contains inconsistent numbers.');
  return value;
}
function signature(bytes) { return SIGNATURE.every((value, index) => bytes[index + 1] === value); }

function identifier(bytes, joliet) {
  if (bytes.length === 1 && bytes[0] < 2) return null; // Current/parent directory.
  let name = '';
  if (joliet) {
    if (bytes.length % 2) fail('a Joliet name has an incomplete Unicode character.');
    for (let offset = 0; offset < bytes.length; offset += 2) {
      const code = bytes[offset] * 256 + bytes[offset + 1];
      if (code >= 0xd800 && code <= 0xdfff) fail('a Joliet name contains unsupported Unicode.');
      name += String.fromCharCode(code);
    }
  } else {
    if ([...bytes].some(value => value > 127)) fail('a primary directory name needs a supported Joliet index.');
    name = String.fromCharCode(...bytes);
  }
  // ISO file versions are separate from the filename used by the game.
  name = name.replace(/;[0-9]+$/, '').replace(/\.$/, '').normalize('NFC');
  if (!name || name === '.' || name === '..' || /[\/\\:\x00-\x1f\x7f]/.test(name)) {
    fail('the directory contains an unsafe file name.');
  }
  return name;
}

/**
 * Return game/disc paths mapped to Blob slices. This reads only descriptors and
 * directory blocks, never an entire ISO or the contents of its regular files.
 */
export async function readISO(file, options = {}) {
  if (!file || typeof file.slice !== 'function' || !Number.isSafeInteger(file.size) || file.size < 18 * SECTOR_BYTES) {
    fail('select a complete ISO9660 or Joliet disc image.');
  }
  const limits = {...DEFAULT_LIMITS, ...options};
  integer(limits.maxDescriptors, 'descriptor', 1024);
  integer(limits.maxEntries, 'entry', 1000000);
  integer(limits.maxDepth, 'directory depth', 128);
  integer(limits.maxDirectoryBytes, 'directory size', 64 * 1024 * 1024);
  integer(limits.maxMetadataBytes, 'metadata size', 256 * 1024 * 1024);
  let metadataBytes = 0;
  const read = async (start, length) => {
    if (!Number.isSafeInteger(start) || !Number.isSafeInteger(length) || start < 0 || length < 0 || start + length > file.size) {
      fail('a directory points outside the disc image.');
    }
    metadataBytes += length;
    if (metadataBytes > limits.maxMetadataBytes) fail('the directory index exceeds the supported size.');
    const bytes = new Uint8Array(await file.slice(start, start + length).arrayBuffer());
    if (bytes.length !== length) fail('the disc image is truncated.');
    return bytes;
  };

  let primary, supplementary, jolietLevel = 0, terminated = false;
  for (let index = 0; index < limits.maxDescriptors; index++) {
    const bytes = await read((16 + index) * SECTOR_BYTES, SECTOR_BYTES);
    if (!signature(bytes) || bytes[6] !== 1) fail('the ISO9660 volume signature is missing or unsupported.');
    if (bytes[0] === 255) { terminated = true; break; }
    if (bytes[0] === 1) {
      if (primary) fail('the image has duplicate primary volume descriptors.');
      primary = bytes;
    }
    if (bytes[0] === 2 && bytes[88] === 37 && bytes[89] === 47) {
      const level = ({64: 1, 67: 2, 69: 3})[bytes[90]] || 0;
      if (level > jolietLevel) { supplementary = bytes; jolietLevel = level; }
    }
  }
  if (!terminated || !primary) fail('the volume descriptor index is incomplete.');
  const descriptor = supplementary || primary, joliet = Boolean(supplementary);
  const volume = new DataView(descriptor.buffer, descriptor.byteOffset, descriptor.byteLength);
  const blockSize = both16(volume, 128), blockCount = both32(volume, 80);
  if (blockSize < 512 || blockSize > 32768 || (blockSize & (blockSize - 1)) || !blockCount) {
    fail('the logical block layout is unsupported.');
  }
  const volumeBytes = blockSize * blockCount;
  if (volumeBytes > file.size) fail('the declared volume is larger than the disc image.');
  const sequence = both16(volume, 124);
  if (both16(volume, 120) !== 1 || sequence !== 1) fail('multi-volume disc sets are not supported.');
  if (descriptor[881] !== 1) fail('the ISO9660 file structure version is unsupported.');

  const record = (bytes, offset, available) => {
    const length = bytes[offset];
    if (length < 34 || length > available || offset + length > bytes.length) fail('a directory record is truncated.');
    const nameLength = bytes[offset + 32];
    if (!nameLength || 33 + nameLength + (nameLength % 2 === 0 ? 1 : 0) > length) {
      fail('a directory record has an invalid file identifier.');
    }
    const view = new DataView(bytes.buffer, bytes.byteOffset + offset, length);
    const flags = bytes[offset + 25], directory = Boolean(flags & 2);
    if (flags & 128) fail('files stored across multiple extents are not supported.');
    if (bytes[offset + 26] || bytes[offset + 27]) fail('interleaved files are not supported.');
    if (both16(view, 28) !== sequence) fail('a file belongs to another disc volume.');
    const lba = both32(view, 2), size = both32(view, 10);
    const start = (lba + bytes[offset + 1]) * blockSize;
    if (start > volumeBytes || start + size > volumeBytes || start + size > file.size) {
      fail('a file extent points outside the disc image.');
    }
    if (directory && (!size || size > limits.maxDirectoryBytes)) fail('a directory exceeds the supported size.');
    return {length, start, size, directory,
      name: identifier(bytes.subarray(offset + 33, offset + 33 + nameLength), joliet)};
  };

  const root = record(descriptor, 156, descriptor.length - 156);
  if (!root.directory || root.name !== null) fail('the root directory is invalid.');
  const files = new Map(), names = new Set(), visited = new Set();
  const queue = [{...root, path: '', depth: 0}];
  let entries = 0;
  for (let index = 0; index < queue.length; index++) {
    const directory = queue[index], identity = `${directory.start}:${directory.size}`;
    if (visited.has(identity)) fail('the directory hierarchy contains a cycle or repeated directory.');
    visited.add(identity);
    for (let position = 0; position < directory.size; position += blockSize) {
      const bytes = await read(directory.start + position, Math.min(blockSize, directory.size - position));
      for (let offset = 0; offset < bytes.length && bytes[offset];) {
        const child = record(bytes, offset, bytes.length - offset);
        offset += child.length;
        if (child.name === null) continue;
        if (++entries > limits.maxEntries) fail('the disc contains too many files or directories.');
        const childPath = directory.path ? `${directory.path}/${child.name}` : child.name;
        if (childPath.length > 4096) fail('a file path exceeds the supported length.');
        const key = childPath.toLowerCase();
        if (names.has(key)) fail(`duplicate file name: ${childPath}.`);
        names.add(key);
        if (child.directory) {
          if (directory.depth + 1 > limits.maxDepth) fail('the directory hierarchy is too deep.');
          queue.push({...child, path: childPath, depth: directory.depth + 1});
        } else {
          files.set(childPath, file.slice(child.start, child.start + child.size));
        }
      }
    }
  }
  return files;
}

/** Identify the layouts understood by the builder without running installers. */
export async function detectISO(files) {
  if (!(files instanceof Map)) throw new Error('ISO detection requires a map of disc files.');
  const lower = new Map([...files].map(([name, file]) => [name.toLowerCase(), {path: name, file}]));
  const installed = [];
  for (const [key, item] of lower) {
    if (key !== 'scripts.rcf' && !key.endsWith('/scripts.rcf')) continue;
    const prefix = key.slice(0, -'scripts.rcf'.length);
    if (LANGUAGES.some(item => lower.has(`${prefix}${item.archive}`)) && lower.has(`${prefix}art/frontend/scrooby/bootup.p3d`)) {
      installed.push({lower: prefix, original: item.path.slice(0, -'scripts.rcf'.length)});
    }
  }
  if (installed.length > 1) throw new Error('The ISO contains multiple game copies. Select one installed game folder.');
  if (installed.length) {
    const prefix = installed[0];
    const installedFiles = new Map([...files]
      .filter(([name]) => name.toLowerCase().startsWith(prefix.lower))
      .map(([name, file]) => [name.slice(prefix.original.length), file]));
    return {kind: 'installed', root: prefix.original.slice(0, -1), files: installedFiles,
      languages: availableLanguages(installedFiles).map(item => item.code)};
  }
  if (lower.has('data1.cab') && lower.has('data1.hdr')) {
    return {kind: 'installshield', files, languages: availableLanguages(files).map(item => item.code)};
  }
  const setups = [...lower.values()].filter(({path}) => /(^|\/)setup\.exe$/i.test(path));
  const archives = [], slices = [];
  for (const item of lower.values()) {
    if (!/\.(bin|arc)$/i.test(item.path)) continue;
    if (/(^|\/)setup-\d+\.bin$/i.test(item.path)) slices.push(item);
    const head = new Uint8Array(await item.file.slice(0, 8).arrayBuffer());
    if (head[0] === 65 && head[1] === 114 && head[2] === 67 && head[3] === 1) archives.push(item);
  }
  if (setups.length === 1 && archives.length && slices.length) {
    const languages = LANGUAGES.filter(item => [...lower.keys()].some(path =>
      path.endsWith(`elamigos-${({en: 'english-polish', fr: 'french', es: 'spanish'})[item.code]}.bin`))).map(item => item.code);
    return {kind: 'inno-freearc', files, setup: setups[0], archives, slices, languages};
  }
  return {kind: 'unknown', files};
}

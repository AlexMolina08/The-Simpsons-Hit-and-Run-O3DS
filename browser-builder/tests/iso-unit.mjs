import assert from 'node:assert/strict';
import {readISO, detectISO} from '../engine-conversion/iso.mjs';

const BLOCK = 2048, BLOCKS = 64;
const ascii = value => new TextEncoder().encode(value);
const both16 = (view, at, value) => { view.setUint16(at, value, true); view.setUint16(at + 2, value, false); };
const both32 = (view, at, value) => { view.setUint32(at, value, true); view.setUint32(at + 4, value, false); };
const ucs2 = value => Uint8Array.from([...value].flatMap(char => [char.charCodeAt(0) >> 8, char.charCodeAt(0) & 255]));

function record(name, lba, size, flags = 0, joliet = false, attributes = 0) {
  const identifier = name instanceof Uint8Array ? name : joliet ? ucs2(name) : ascii(name);
  const bytes = new Uint8Array(33 + identifier.length + (identifier.length % 2 === 0 ? 1 : 0));
  const view = new DataView(bytes.buffer);
  bytes[0] = bytes.length; bytes[1] = attributes;
  both32(view, 2, lba); both32(view, 10, size);
  bytes[25] = flags; both16(view, 28, 1); bytes[32] = identifier.length;
  bytes.set(identifier, 33);
  return bytes;
}

function fixture({joliet = false, children} = {}) {
  const bytes = new Uint8Array(BLOCKS * BLOCK);
  const volume = (sector, type, root, unicode = false) => {
    const descriptor = bytes.subarray(sector * BLOCK, (sector + 1) * BLOCK), view = new DataView(descriptor.buffer, descriptor.byteOffset, descriptor.length);
    descriptor[0] = type; descriptor.set(ascii('CD001'), 1); descriptor[6] = 1;
    if (type === 255) return;
    both32(view, 80, BLOCKS); both16(view, 120, 1); both16(view, 124, 1); both16(view, 128, BLOCK);
    descriptor[881] = 1;
    if (unicode) descriptor.set(ascii('%/E'), 88);
    descriptor.set(record(new Uint8Array([0]), root, BLOCK, 2), 156);
  };
  const directory = (lba, parent, entries) => {
    const records = [record(new Uint8Array([0]), lba, BLOCK, 2), record(new Uint8Array([1]), parent, BLOCK, 2), ...entries];
    let at = lba * BLOCK;
    for (const entry of records) { bytes.set(entry, at); at += entry.length; }
    return records.map((entry, index) => lba * BLOCK + records.slice(0, index).reduce((sum, value) => sum + value.length, 0));
  };
  volume(16, 1, 20);
  const root = joliet ? 21 : 20;
  if (joliet) {
    volume(17, 2, root, true); volume(18, 255);
    directory(20, 20, [record('PRIMARY.TXT;1', 30, 7)]);
  } else volume(17, 255);
  bytes.set(ascii('payload'), 30 * BLOCK);
  const offsets = directory(root, root, children || [
    record(joliet ? 'Guía.txt;1' : 'README.TXT;1', 30, 7, 0, joliet),
    record(joliet ? 'Game Data' : 'DIR', 22, BLOCK, 2, joliet),
  ]);
  directory(22, root, [record(joliet ? 'Child.bin;1' : 'CHILD.BIN;1', 30, 7, 0, joliet)]);
  return {bytes, offsets, root, directory};
}

// Enforce range-only access independently from the reader implementation.
function image(bytes) {
  const blob = new Blob([bytes]), stats = {bytes: 0, reads: 0};
  class Range {
    constructor(start = 0, end = blob.size) { this.start = start; this.end = end; this.size = end - start; }
    slice(start = 0, end = this.size) { return new Range(this.start + Math.min(start, this.size), this.start + Math.min(end, this.size)); }
    async arrayBuffer() {
      assert.ok(this.size <= BLOCK, 'The ISO reader must never buffer the whole image or a regular file.');
      stats.bytes += this.size; stats.reads++;
      return blob.slice(this.start, this.end).arrayBuffer();
    }
  }
  return {file: new Range(), stats};
}
const read = bytes => readISO(image(bytes).file);
const base = fixture(), tracked = image(base.bytes), files = await readISO(tracked.file);
assert.deepEqual([...files.keys()], ['README.TXT', 'DIR/CHILD.BIN']);
assert.equal(tracked.stats.bytes, BLOCK * 4);
assert.equal(new TextDecoder().decode(await files.get('DIR/CHILD.BIN').arrayBuffer()), 'payload');
const unicode = await read(fixture({joliet: true}).bytes);
assert.deepEqual([...unicode.keys()], ['Guía.txt', 'Game Data/Child.bin']);
assert.equal(unicode.has('PRIMARY.TXT'), false, 'The Joliet directory must take precedence.');

const brokenSignature = base.bytes.slice(); brokenSignature[16 * BLOCK + 1] = 0;
await assert.rejects(() => read(brokenSignature), /signature/);
const outside = base.bytes.slice(); both32(new DataView(outside.buffer), base.offsets[2] + 2, BLOCKS);
await assert.rejects(() => read(outside), /outside the disc/);
const endian = base.bytes.slice(); new DataView(endian.buffer).setUint32(base.offsets[2] + 2, 31, true);
await assert.rejects(() => read(endian), /inconsistent numbers/);
const cyclic = fixture({children: [record('CYCLE', 20, BLOCK, 2)]});
await assert.rejects(() => read(cyclic.bytes), /cycle or repeated/);
await assert.rejects(() => read(fixture({children: [record('../OUTSIDE;1', 30, 7)]}).bytes), /unsafe file name/);
await assert.rejects(() => read(fixture({children: [record('README.TXT;1', 30, 7), record('readme.txt;2', 30, 7)]}).bytes), /duplicate file name/);
await assert.rejects(() => read(fixture({children: [record('PART.BIN;1', 30, 7, 128)]}).bytes), /multiple extents/);
await assert.rejects(() => read(fixture({joliet: true, children: [record(new Uint8Array([0, 65, 0]), 30, 7)]}).bytes), /incomplete Unicode/);
const truncated = base.bytes.slice(); both32(new DataView(truncated.buffer), 16 * BLOCK + 156 + 10, 50);
await assert.rejects(() => read(truncated), /record is truncated/);
const tooLarge = base.bytes.slice(); both32(new DataView(tooLarge.buffer), 16 * BLOCK + 80, BLOCKS + 1);
await assert.rejects(() => read(tooLarge), /larger than the disc/);
const multiVolume = base.bytes.slice(); both16(new DataView(multiVolume.buffer), 16 * BLOCK + 120, 2);
await assert.rejects(() => read(multiVolume), /multi-volume/);
await assert.rejects(() => readISO(image(base.bytes).file, {maxEntries: 1}), /too many files/);
await assert.rejects(() => readISO(image(base.bytes).file, {maxDirectoryBytes: 1}), /directory exceeds/);
await assert.rejects(() => readISO(image(base.bytes).file, {maxMetadataBytes: 1}), /index exceeds/);
const deep = fixture({children: [record('DIR', 22, BLOCK, 2)]});
deep.directory(22, 20, [record('SUB', 23, BLOCK, 2)]);
deep.directory(23, 22, []);
await assert.rejects(() => readISO(image(deep.bytes).file, {maxDepth: 1}), /too deep/);

const attributes = fixture({children: [record('EXTENDED.BIN;1', 29, 7, 0, false, 1), record('EMPTY.;1', 0, 0)]});
const attributed = await read(attributes.bytes);
assert.equal(new TextDecoder().decode(await attributed.get('EXTENDED.BIN').arrayBuffer()), 'payload');
assert.equal(attributed.get('EMPTY').size, 0);

const installed = new Map(['Disc/Game/SCRIPTS.RCF', 'Disc/Game/DIALOG.RCF', 'Disc/Game/ART/FRONTEND/SCROOBY/BOOTUP.P3D'].map(name => [name, new Blob()]));
const detected = await detectISO(installed);
assert.equal(detected.kind, 'installed');
assert.equal(detected.root, 'Disc/Game');
assert.deepEqual([...detected.files.keys()], ['SCRIPTS.RCF', 'DIALOG.RCF', 'ART/FRONTEND/SCROOBY/BOOTUP.P3D']);
const repack = await detectISO(new Map([
  ['Setup.exe', new Blob(['MZ'])], ['setup-0.bin', new Blob(['data'])],
  ['elamigos-1.bin', new Blob([new Uint8Array([65, 114, 67, 1])])],
]));
assert.equal(repack.kind, 'inno-freearc');
assert.equal(repack.archives[0].path, 'elamigos-1.bin');
assert.equal((await detectISO(new Map([['unknown.bin', new Blob()]]))).kind, 'unknown');
console.log('ISO checks passed: bounded range reads, primary/Joliet paths, nested files, layout detection, malformed extents and directory limits.');

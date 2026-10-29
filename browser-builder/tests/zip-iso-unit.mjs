import assert from 'node:assert/strict';
import {deflateRawSync} from 'node:zlib';
import {readGameImages} from '../engine-conversion/source-input.mjs';

// Construct a small ISO9660 installer, then wrap it as a typical downloaded ZIP.
const text = value => new TextEncoder().encode(value);
const both16 = (view, at, value) => { view.setUint16(at, value, true); view.setUint16(at + 2, value, false); };
const both32 = (view, at, value) => { view.setUint32(at, value, true); view.setUint32(at + 4, value, false); };
function record(name, lba, size, flags = 0) {
  const id = typeof name === 'string' ? text(name) : name;
  const result = new Uint8Array(33 + id.length + (id.length % 2 === 0 ? 1 : 0));
  const view = new DataView(result.buffer);
  result[0] = result.length;
  both32(view, 2, lba); both32(view, 10, size);
  result[25] = flags; both16(view, 28, 1); result[32] = id.length; result.set(id, 33);
  return result;
}
function iso() {
  const sector = 2048, blocks = 24, image = new Uint8Array(sector * blocks);
  const primary = image.subarray(16 * sector, 17 * sector), view = new DataView(primary.buffer, primary.byteOffset, primary.length);
  primary[0] = 1; primary.set(text('CD001'), 1); primary[6] = 1;
  both32(view, 80, blocks); both16(view, 120, 1); both16(view, 124, 1); both16(view, 128, sector);
  primary[881] = 1; primary.set(record(new Uint8Array([0]), 20, sector, 2), 156);
  const terminator = image.subarray(17 * sector, 18 * sector);
  terminator[0] = 255; terminator.set(text('CD001'), 1); terminator[6] = 1;
  let offset = 20 * sector;
  for (const entry of [record(new Uint8Array([0]), 20, sector, 2), record(new Uint8Array([1]), 20, sector, 2),
    record('DATA1.CAB;1', 21, 3), record('DATA1.HDR;1', 22, 3)]) {
    image.set(entry, offset); offset += entry.length;
  }
  image.set(text('cab'), 21 * sector); image.set(text('hdr'), 22 * sector);
  return image;
}
function crc(bytes) {
  let value = 0xffffffff;
  for (const byte of bytes) {
    value ^= byte;
    for (let i = 0; i < 8; i++) value = value >>> 1 ^ (value & 1 ? 0xedb88320 : 0);
  }
  return (value ^ 0xffffffff) >>> 0;
}
function zip(name, bytes, method) {
  const filename = text(name), packed = method === 8 ? deflateRawSync(bytes) : bytes;
  const head = new Uint8Array(30 + filename.length), local = new DataView(head.buffer);
  local.setUint32(0, 0x04034b50, true); local.setUint16(8, method, true); local.setUint32(14, crc(bytes), true);
  local.setUint32(18, packed.length, true); local.setUint32(22, bytes.length, true); local.setUint16(26, filename.length, true); head.set(filename, 30);
  const entry = new Uint8Array(46 + filename.length), dir = new DataView(entry.buffer);
  dir.setUint32(0, 0x02014b50, true); dir.setUint16(10, method, true); dir.setUint32(16, crc(bytes), true);
  dir.setUint32(20, packed.length, true); dir.setUint32(24, bytes.length, true); dir.setUint16(28, filename.length, true); entry.set(filename, 46);
  const end = new Uint8Array(22), footer = new DataView(end.buffer);
  footer.setUint32(0, 0x06054b50, true); footer.setUint16(8, 1, true); footer.setUint16(10, 1, true);
  footer.setUint32(12, entry.length, true); footer.setUint32(16, head.length + packed.length, true);
  return new File([head, packed, entry, end], 'game.zip');
}
const storage = {async write(path, blob) { return await new Response(blob.stream()).blob(); }};
for (const method of [0, 8]) {
  const result = await readGameImages([zip('download/Game.iso', iso(), method)], {storage});
  assert.equal(result.kind, 'installshield', `ZIP method ${method} must reach the disc-layout detector`);
  assert.equal(await result.files.get('DATA1.CAB').text(), 'cab');
  assert.equal(await result.files.get('DATA1.HDR').text(), 'hdr');
}
console.log('PASS: stored and deflated ZIPs containing an ISO are read and detected.');

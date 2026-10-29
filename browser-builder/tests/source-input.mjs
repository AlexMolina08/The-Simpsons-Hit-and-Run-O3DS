import assert from 'node:assert/strict';
import {readZIP, readGameImages} from '../engine-conversion/source-input.mjs';

const crc = bytes => {
  let value = 0xffffffff;
  for (const byte of bytes) {
    value ^= byte;
    for (let i = 0; i < 8; i++) value = value >>> 1 ^ (value & 1 ? 0xedb88320 : 0);
  }
  return (value ^ 0xffffffff) >>> 0;
};
function zip(entries, corrupt = false) {
  const local = [], directory = []; let offset = 0;
  for (const [path, text] of entries) {
    const name = new TextEncoder().encode(path), body = new TextEncoder().encode(text);
    const header = new Uint8Array(30 + name.length), h = new DataView(header.buffer);
    h.setUint32(0, 0x04034b50, true); h.setUint32(14, crc(body), true);
    h.setUint32(18, body.length, true); h.setUint32(22, body.length, true); h.setUint16(26, name.length, true); header.set(name, 30);
    const record = new Uint8Array(46 + name.length), r = new DataView(record.buffer);
    r.setUint32(0, 0x02014b50, true); r.setUint32(16, crc(body) ^ (corrupt ? 1 : 0), true);
    r.setUint32(20, body.length, true); r.setUint32(24, body.length, true); r.setUint16(28, name.length, true);
    r.setUint32(42, offset, true); record.set(name, 46);
    local.push(header, body); directory.push(record); offset += header.length + body.length;
  }
  const footer = new Uint8Array(22), f = new DataView(footer.buffer);
  f.setUint32(0, 0x06054b50, true); f.setUint16(8, entries.length, true); f.setUint16(10, entries.length, true);
  f.setUint32(12, directory.reduce((sum, item) => sum + item.length, 0), true); f.setUint32(16, offset, true);
  return new File([...local, ...directory, footer], 'copy.zip');
}
const game = zip([['PC/scripts.rcf', 'scripts'], ['PC/dialogf.rcf', 'voices'], ['PC/art/frontend/scrooby/bootup.p3d', 'art']]);
const extracted = await readZIP(game);
assert.equal(await extracted.get('PC/dialogf.rcf').text(), 'voices');
const installed = await readGameImages([game]);
assert.equal(installed.kind, 'installed'); assert.deepEqual(installed.languages, ['fr']);
assert.equal(await installed.files.get('scripts.rcf').text(), 'scripts');
await assert.rejects(() => readZIP(zip([['../escape', 'no']])), /unsafe path/);
await assert.rejects(() => readZIP(zip([['DIALOG.RCF', 'one'], ['dialog.rcf', 'two']])), /duplicate/);
await assert.rejects(() => readZIP(zip([['dialogf.rcf', 'voices']], true)), /damaged data/);
await assert.rejects(() => readGameImages([game, game]), /one installed-copy ZIP/);
await assert.rejects(() => readZIP(new Blob(['truncated'])), /directory/);
console.log('PASS: installed-copy ZIP detection, byte integrity, safe paths, duplicate and incompatible input rejection.');

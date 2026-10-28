/** Synthetic RCF/layout proof; optional full native-cache/WASM parity. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {buildAudioCache} from '../engine-conversion/audio-cache.mjs';

const hash = bytes => createHash('sha256').update(bytes).digest('hex');
function pcm(samples, seed) {
  const data = new Uint8Array(samples * 2), view = new DataView(data.buffer);
  for (let i = 0; i < samples; i++) view.setInt16(i * 2, ((i * 997 + seed) & 65535) - 32768, true);
  return data;
}
function rsd(data, channels = 1) {
  const bytes = new Uint8Array(2048 + data.length), view = new DataView(bytes.buffer);
  bytes.set(new TextEncoder().encode('RSD4PCM '));
  view.setUint32(8, channels, true);view.setUint32(12, 16, true);view.setUint32(16, 22050, true);
  bytes.set(data, 2048);
  return bytes;
}
function archive(records) {
  const start = 64 + records.length * 12;
  const bytes = new Uint8Array(start + records.reduce((sum, record) => sum + record.data.length, 0));
  const view = new DataView(bytes.buffer);
  bytes.set(new TextEncoder().encode('RADCORE CEMENT LIBRARY'));
  view.setUint32(44, 48, true);view.setUint32(48, records.length, true);
  let offset = start;
  records.forEach((record, i) => {
    view.setUint32(64 + i * 12, record.uid, true);view.setUint32(68 + i * 12, offset, true);
    view.setUint32(72 + i * 12, record.data.length, true);bytes.set(record.data, offset);offset += record.data.length;
  });
  return new Blob([bytes]);
}
function checksum(bytes) {let value = 2166136261;for (const byte of bytes) value = Math.imul(value ^ byte, 16777619) >>> 0;return value;}
const first = pcm(7492, 17), second = pcm(7681, 29);
const fixture = new Map([
  ['SOUNDFX.RCF', archive([{uid: 8, data: rsd(first)}, {uid: 7, data: rsd(second)},
    {uid: 9, data: rsd(pcm(4096, 5))}, {uid: 10, data: rsd(first, 2)}])],
  ['carsound.rcf', archive([{uid: 3, data: rsd(first)}])],
]);
let calls = 0;
const report = await buildAudioCache(fixture, (input, samples) => {
  calls++;
  input.fill(0xff); // An in-place tool must not overwrite the original PCM key.
  return new Uint8Array(Math.ceil(samples / 14) * 8);
});
assert.equal(calls, 2);assert.equal(report.clips, 2);
assert.equal(report.metadata.entries[0].archive, 'carsound.rcf');
const bytes = new Uint8Array(await report.blob.arrayBuffer()), view = new DataView(bytes.buffer);
assert.equal(new TextDecoder().decode(bytes.slice(0, 8)), 'SRRDSP1\0');
assert.deepEqual([8, 12, 16, 24, 28].map(at => view.getUint32(at, true)), [1, 2, bytes.length, 3, 0]);
assert.equal(view.getUint32(20, true), checksum(bytes.subarray(32)));
for (let i = 0; i < 2; i++) {
  const entry = 32 + i * 16, samples = view.getUint32(entry, true);
  const pcmAt = view.getUint32(entry + 4, true), dspAt = view.getUint32(entry + 8, true);
  assert.equal(pcmAt % 32, 0);assert.equal(dspAt % 32, 0);
  assert.equal(view.getUint32(entry + 12, true), Math.ceil(samples / 14) * 8);
  assert.deepEqual(bytes.slice(pcmAt, pcmAt + samples * 2), i ? second : first);
}
assert.equal(report.sha256, hash(bytes));
await assert.rejects(buildAudioCache(new Map([['soundfx.rcf', fixture.get('SOUNDFX.RCF')]]), () => {}), /missing carsound/);
await assert.rejects(buildAudioCache(fixture, () => new Uint8Array(8)), /invalid audio length/);
await assert.rejects(buildAudioCache(fixture, (_input, samples) => new Uint8Array(Math.ceil(samples / 14) * 8).fill(0xf0)), /invalid predictor/);
const malformed = new Uint8Array(await fixture.get('carsound.rcf').arrayBuffer());
new DataView(malformed.buffer).setUint32(68, malformed.length, true);
await assert.rejects(buildAudioCache(new Map([...fixture, ['carsound.rcf', new Blob([malformed])]]), () => {}), /Invalid audio member/);
console.log('PASS: source selection, raw PCM deduplication/order, 32-byte alignment, checksum, in-place ownership and malformed tool/archive rejection.');

const options = new Map();
for (let i = 2; i < process.argv.length; i += 2) {
  if (!process.argv[i + 1]) throw new Error(`Missing ${process.argv[i]} value`);
  options.set(process.argv[i], process.argv[i + 1]);
}
if (options.has('--wasm-dir')) {
  const directory = path.resolve(options.get('--wasm-dir'));
  const {default: createEncoder} = await import(pathToFileURL(path.join(directory, 'v12-audio.mjs')).href);
  const module = await createEncoder({wasmBinary: fs.readFileSync(path.join(directory, 'v12-audio.wasm'))});
  const encode = (source, samples) => {
    const pointer = module._malloc(source.length);
    try {
      module.HEAPU8.set(source, pointer);
      const length = module._encode_audio_cache(pointer, samples);
      return module.HEAPU8.slice(pointer, pointer + length);
    } finally { module._free(pointer); }
  };
  assert.equal(module._encode_audio_cache(0, 7492), 0);
  assert.equal(encode(new Uint8Array(4096 * 2), 4096).length, Math.ceil(4096 / 14) * 8);
  if (options.has('--accepted-cache')) {
    const accepted = fs.readFileSync(options.get('--accepted-cache'));
    const header = new DataView(accepted.buffer, accepted.byteOffset, accepted.byteLength);
    const started = performance.now();
    let totalSamples = 0;
    for (let i = 0; i < header.getUint32(12, true); i++) {
      const at = 32 + i * 16, samples = header.getUint32(at, true), pcmAt = header.getUint32(at + 4, true);
      const dspAt = header.getUint32(at + 8, true), dspBytes = header.getUint32(at + 12, true);
      assert.deepEqual(encode(accepted.subarray(pcmAt, pcmAt + samples * 2), samples), new Uint8Array(accepted.subarray(dspAt, dspAt + dspBytes)));
      totalSamples += samples;
    }
    console.log(JSON.stringify({proof: 'production accepted DSP bytes equal WASM for every clip',
      clips: header.getUint32(12, true), totalSamples, elapsedMs: performance.now() - started}));
    if (options.has('--game')) {
      function localFile(filename) {
        return {size: fs.statSync(filename).size, slice(start, end) {return {async arrayBuffer() {
          const file = await fsp.open(filename), bytes = new Uint8Array(end - start);
          try {let offset = 0;while (offset < bytes.length) {
            const {bytesRead} = await file.read(bytes, offset, bytes.length - offset, start + offset);
            if (!bytesRead) throw new Error('Truncated PCM fixture');offset += bytesRead;
          }return bytes.buffer;} finally {await file.close();}
        }}}};
      }
      const files = new Map(['soundfx.rcf', 'carsound.rcf'].map(name => [name, localFile(path.join(options.get('--game'), name))]));
      const start = performance.now(), output = await buildAudioCache(files, encode);
      assert.deepEqual(new Uint8Array(await output.blob.arrayBuffer()), new Uint8Array(accepted));
      console.log(JSON.stringify({proof: 'full source archive selection and WASM cache equal accepted v1.2 bytes',
        sha256: output.sha256, clips: output.clips, bytes: output.blob.size, elapsedMs: performance.now() - start}));
    }
  }
}

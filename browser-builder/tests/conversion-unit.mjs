import assert from 'node:assert/strict';
import {File} from 'node:buffer';
import {archiveMember, convertMovie, crc32, normalizeInputFiles, OPTIONAL_MOVIES, REQUIRED_MOVIES, validateInput} from '../engine-conversion/conversion.mjs';

// A fixed legacy RCF key, obtained independently with the existing native reader.
const archive = new Uint8Array(82), directory = new DataView(archive.buffer);
archive.set(new TextEncoder().encode('RADCORE CEMENT LIBRARY'));
directory.setUint32(44, 48, true);
directory.setUint32(48, 1, true);
directory.setUint32(64, 2858759933, true);
directory.setUint32(68, 76, true);
directory.setUint32(72, 6, true);
archive.set([12, 34, 56, 78, 90, 123], 76);
assert.deepEqual(await archiveMember(new Blob([archive]), 'sound\\music\\simpsons_theme.rsd'), archive.slice(76));
const invalidArchive = archive.slice();
new DataView(invalidArchive.buffer).setUint32(72, 1000, true);
await assert.rejects(() => archiveMember(new Blob([invalidArchive]), 'sound\\music\\simpsons_theme.rsd'), /invalid/);
assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926);

const names = [
  'art/frontend/scrooby/bootup.p3d', 'art/frontend/scrooby/frontend.p3d',
  'art/frontend/scrooby/resource/txtbible/srr2.p3d',
  'art/frontend/scrooby/resource/pure3d/logo_anim.p3d', 'scripts/example.mfk', 'sound/example.rsd',
  'scripts.rcf', 'dialog.rcf', 'soundfx.rcf', 'carsound.rcf', 'music00.rcf', 'music01.rcf', 'music02.rcf', 'music03.rcf',
  ...REQUIRED_MOVIES.map(name => `movies/${name}.rmv`),
];
const input = new Map(names.map(name => [name, new File([new Uint8Array([1])], name.split('/').at(-1))]));
assert.equal(validateInput(input).movieCount, 16);
assert.deepEqual(OPTIONAL_MOVIES, ['gvuglogo']);
const regional = new Map([...input, ['movies/gvuglogo.rmv', new File([new Uint8Array([1, 2, 3])], 'gvuglogo.rmv')]]);
assert.equal(validateInput(regional).movieCount, 17);
assert.equal(validateInput(regional).fileCount, validateInput(input).fileCount + 1);
assert.equal(validateInput(regional).inputBytes, validateInput(input).inputBytes + 3);
assert.equal(validateInput(new Map([...input, ['MOVIES/GVUGLOGO.RMV', new Blob(['regional'])]])).movieCount, 17);
const incomplete = new Map(input);
incomplete.delete('movies/fmv8.rmv');
assert.throws(() => validateInput(incomplete), /fmv8.rmv/);
assert.throws(() => normalizeInputFiles(new Map([['../outside', new Blob()]])), /invalid file path/);
assert.throws(() => validateInput(new Map([...input, ['Simpsons-Debug.exe', new Blob()]])), /development data/);
assert.throws(() => validateInput(new Map([...input, ['Scripts.rcf', new Blob()]])), /duplicate names/);
const upperSound = new Map(input);
upperSound.delete('soundfx.rcf'); upperSound.set('SOUNDFX.RCF', new Blob(['sound']));
assert.equal(validateInput(upperSound).language, 'en');
assert.ok(normalizeInputFiles(upperSound).has('soundfx.rcf'));
const noSound = new Map(input); noSound.delete('carsound.rcf');
assert.throws(() => validateInput(noSound), /carsound\.rcf/);
const mixedDirectories = normalizeInputFiles(new Map([
  ['SCRIPTS/cars/pursuit/L1cop.con', new Blob(['one'])],
  ['SCRIPTS/CARS/Pursuit/Other.con', new Blob(['two'])],
]));
assert.deepEqual([...mixedDirectories.keys()], ['scripts/cars/pursuit/L1cop.con', 'scripts/cars/pursuit/Other.con']);
const mergedAssets = normalizeInputFiles(new Map([
  ['ART/Frontend/menu.p3d', new Blob()], ['art/frontend/generated.p3d', new Blob()],
  ['SOUND/Effects/one.rsd', new Blob()], ['sound/3ds-dsp-clips.bin', new Blob()],
]));
assert.deepEqual([...mergedAssets.keys()], ['art/Frontend/menu.p3d', 'art/Frontend/generated.p3d',
  'sound/Effects/one.rsd', 'sound/3ds-dsp-clips.bin']);
assert.throws(() => normalizeInputFiles(new Map([
  ['scripts/cars/pursuit/L1cop.con', new Blob(['one'])],
  ['SCRIPTS/CARS/Pursuit/l1COP.CON', new Blob(['two'])],
])), /duplicate names/);

// A longer audio tail must extend the final image rather than truncate audio.
const pcm = new Uint8Array(12800), video = new Uint8Array([0, 0, 1, 0xb3]);
const calls = [];
const result = await convertMovie(new File([video], 'test.rmv'), 'test', async request => {
  calls.push(request);
  if (request.allowFailure) return {exitCode: 1, logs: ['Video: binkvideo', 'Audio: binkaudio_dct'], files: new Map()};
  if (!request.outputs.length) return {exitCode: 0, logs: ['frame=3'], files: new Map()};
  const output = request.outputs[0];
  return {exitCode: 0, logs: ['frame=2'], files: new Map([[output, output.endsWith('.pcm') ? pcm : video]])};
});
assert.equal(calls.length, 5);
assert.ok(calls[3].args.includes('-frames:v'));
assert.ok(calls[3].args.includes('3'));
const encoded = new Uint8Array(await result.blob.arrayBuffer()), header = new DataView(encoded.buffer);
assert.equal(new TextDecoder().decode(encoded.slice(0, 8)), 'SRR3DSMV');
assert.equal(header.getUint32(8, true), 2);
assert.equal(header.getUint16(12, true), 320);
assert.equal(header.getUint16(14, true), 240);
assert.equal(header.getUint32(24, true), 3);
assert.equal(header.getUint32(40, true), pcm.length);
assert.equal(header.getUint32(44, true), 52 + pcm.length);
assert.deepEqual(encoded.slice(52, 52 + pcm.length), pcm);
await assert.rejects(() => convertMovie(new File([video], 'broken.rmv'), 'broken', async () => ({exitCode: 1, logs: ['unknown format'], files: new Map()})), /no supported video stream/);
// Successful encoding is insufficient when the complete stream cannot decode.
await assert.rejects(() => convertMovie(new File([video], 'truncated.rmv'), 'truncated', async request => {
  if (request.allowFailure) return {exitCode: 1, logs: ['Video: binkvideo'], files: new Map()};
  if (!request.outputs.length) return {exitCode: 0, logs: ['frame=1'], files: new Map()};
  return {exitCode: 0, logs: ['frame=2'], files: new Map([[request.outputs[0], video]])};
}), /complete movie decoding/);
// FFmpeg keeps its previous log level between calls, including audio preparation.
let retainedLogLevel = 'error';
const reusedCore = async request => {
  const level = request.args.indexOf('-loglevel');
  if (level >= 0) retainedLogLevel = request.args[level + 1];
  if (request.allowFailure) return {exitCode: 1, files: new Map(), logs: retainedLogLevel === 'info' ? ['Video: binkvideo'] : []};
  return {exitCode: 0, logs: ['frame=2'], files: new Map(request.outputs.map(name => [name, video]))};
};
await convertMovie(new File([video], 'first.rmv'), 'first', reusedCore);
await convertMovie(new File([video], 'second.rmv'), 'second', reusedCore);
console.log('Conversion checks passed: retail preflight, RCF bounds, CRC, complete movie header and audio-tail preservation.');

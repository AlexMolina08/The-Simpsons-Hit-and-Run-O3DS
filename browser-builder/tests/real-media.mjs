/** Optional codec proof using private local fixtures; never copies game data. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import {File} from 'node:buffer';
import {pathToFileURL} from 'node:url';
import {archiveMember, convertMovie, OPTIONAL_MOVIES, REQUIRED_MOVIES, validateBannerCue} from '../engine-conversion/conversion.mjs';

const options = new Map();
for (let index = 2; index < process.argv.length; index += 2) {
  if (!process.argv[index + 1]) throw new Error(`Missing value for ${process.argv[index]}`);
  options.set(process.argv[index], process.argv[index + 1]);
}
if (!options.has('--core-dir') || options.has('--movie') === options.has('--movie-dir')) {
  throw new Error('Usage: node browser-builder/tests/real-media.mjs --core-dir <built FFmpeg directory> (--movie <local Bink .rmv> | --movie-dir <private PC movies directory>) [--game <local PC data>] [--frames <native reference count>] [--audio-bytes <native reference count>]');
}
const coreDirectory = path.resolve(options.get('--core-dir'));
const modulePath = ['ffmpeg-core.mjs', 'ffmpeg-core.js'].map(name => path.join(coreDirectory, name)).find(fs.existsSync);
assert.ok(modulePath, 'The built FFmpeg module is missing.');
// The upstream ffmpeg.wasm wrapper reads its worker location during setup.
globalThis.self = {location: {href: pathToFileURL(modulePath).href}};
globalThis.importScripts = () => {};
const {default: createCore} = await import(pathToFileURL(modulePath).href);
const core = await createCore({wasmBinary: fs.readFileSync(path.join(coreDirectory, 'ffmpeg-core.wasm'))});

async function runFFmpeg({inputs, args, outputs, allowFailure = false}) {
  const logs = [], files = new Map();
  core.reset();
  core.setLogger(({message}) => logs.push(message));
  try {
    for (const {name, data} of inputs) {
      core.FS.writeFile(`/${name}`, data instanceof Uint8Array ? data : new Uint8Array(await data.arrayBuffer()));
    }
    const exitCode = core.exec('-y', ...args);
    if (!allowFailure) assert.equal(exitCode, 0, logs.slice(-8).join('\n'));
    for (const name of outputs) files.set(name, core.FS.readFile(`/${name}`).slice());
    return {files, logs, exitCode};
  } finally {
    core.reset();
    for (const name of [...inputs.map(input => input.name), ...outputs]) {
      try { core.FS.unlink(`/${name}`); } catch {}
    }
  }
}

// RCF files can exceed a gigabyte; only read their requested ranges.
function localFile(filename) {
  const size = fs.statSync(filename).size;
  return {size, slice(start = 0, end = size) {
    return {async arrayBuffer() {
      const handle = await fsp.open(filename);
      try {
        const bytes = Buffer.alloc(end - start);
        let read = 0;
        while (read < bytes.length) {
          const result = await handle.read(bytes, read, bytes.length - read, start + read);
          if (!result.bytesRead) throw new Error(`Truncated private fixture: ${filename}`);
          read += result.bytesRead;
        }
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.length);
      } finally { await handle.close(); }
    }};
  }};
}

const movieDirectory = options.get('--movie-dir');
const moviePaths = movieDirectory ? [...REQUIRED_MOVIES, ...OPTIONAL_MOVIES
  .filter(name => fs.existsSync(path.join(movieDirectory, `${name}.rmv`)))]
  .map(name => path.join(movieDirectory, `${name}.rmv`)) : [options.get('--movie')];
if (moviePaths.length > 1 && (options.has('--frames') || options.has('--audio-bytes'))) {
  throw new Error('Native frame/audio reference flags apply to a single movie.');
}
const movies = [];
for (const moviePath of moviePaths) {
  const movie = await convertMovie(new File([fs.readFileSync(moviePath)], path.basename(moviePath)), 'proof', runFFmpeg);
  if (options.has('--frames')) assert.equal(movie.frames, Number(options.get('--frames')));
  if (options.has('--audio-bytes')) assert.equal(movie.audioBytes, Number(options.get('--audio-bytes')));
  const header = new DataView(await movie.blob.slice(0, 52).arrayBuffer());
  assert.equal(header.getUint32(8, true), 2);
  assert.equal(header.getUint32(44, true) + header.getUint32(48, true), movie.blob.size);
  movies.push({file: path.basename(moviePath), frames: movie.frames, audioBytes: movie.audioBytes, videoBytes: movie.videoBytes});
  process.stderr.write(`Verified ${path.basename(moviePath)}: ${movie.frames} decoded frames, ${movie.audioBytes} PCM bytes\n`);
}
const report = moviePaths.length === 1 ? {movie: movies[0]} : {movieCount: movies.length, movies};

if (options.has('--game')) {
  const game = path.resolve(options.get('--game'));
  const theme = await archiveMember(localFile(path.join(game, 'music02.rcf')), 'sound\\music\\simpsons_theme.rsd');
  const banner = await runFFmpeg({inputs: [{name: 'theme.rsd', data: theme}], args: [
    '-hide_banner', '-loglevel', 'error', '-i', 'theme.rsd', '-t', '2.8', '-af',
    'loudnorm=I=-20:TP=-2:LRA=7,afade=t=in:d=0.015,afade=t=out:st=2.6:d=0.2',
    '-ar', '32000', '-ac', '2', '-c:a', 'pcm_s16le', 'banner.wav',
  ], outputs: ['banner.wav']});
  report.banner = validateBannerCue(banner.files.get('banner.wav'));
  const members = [
    'homer\\w_greeting_hom_hanging.rsd', 'homer\\w_greeting_hom_happen.rsd',
    'bart\\w_greeting_brt_buddy.rsd', 'bart\\w_greeting_brt_dude.rsd',
    'lisa\\w_greeting_lis_hi.rsd', 'lisa\\w_greeting_lis_how.rsd',
    'marge\\w_greeting_mrg_hello.rsd', 'marge\\w_greeting_mrg_how.rsd',
    'apu\\w_greeting_apu_fine.rsd', 'apu\\w_greeting_apu_loyal.rsd',
  ];
  report.greetings = [];
  for (const member of members) {
    const source = await archiveMember(localFile(path.join(game, 'dialog.rcf')), member);
    const converted = await runFFmpeg({inputs: [{name: 'source.rsd', data: source}], args: [
      '-hide_banner', '-loglevel', 'error', '-i', 'source.rsd', '-af',
      'loudnorm=I=-18:TP=-3:LRA=7,afade=t=in:d=0.004,areverse,afade=t=in:d=0.012,areverse',
      '-ar', '24000', '-ac', '1', '-c:a', 'pcm_s16le', '-f', 's16le', 'greeting.pcm',
    ], outputs: ['greeting.pcm']});
    const pcm = converted.files.get('greeting.pcm');
    assert.ok(pcm.length > 0 && pcm.length <= 144000 && pcm.length % 2 === 0);
    const samples = new DataView(pcm.buffer, pcm.byteOffset, pcm.byteLength);
    let peak = 0;
    for (let offset = 0; offset < pcm.length; offset += 2) peak = Math.max(peak, Math.abs(samples.getInt16(offset, true)));
    assert.ok(peak > 32 && peak < 27000);
    report.greetings.push({member, samples: pcm.length / 2, peak});
  }
}
console.log(JSON.stringify({passed: true, ...report}, null, 2));

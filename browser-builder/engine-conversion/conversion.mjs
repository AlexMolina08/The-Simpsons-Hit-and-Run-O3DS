/** Prepare a complete CIA asset set from user-selected PC files, locally. */

export const REQUIRED_MOVIES = Object.freeze([
  'credits', 'fmv1A', 'fmv2', 'fmv3', 'fmv4', 'fmv5', 'fmv6', 'fmv7',
  'fmv8', 'foxlogo', 'gracie', 'intro', 'loot', 'radlogo', 'tele', 'vuglogo',
]);

// Some regional copies include this additional publisher logo. The game uses
// vuglogo; preserve gvuglogo when present without rejecting ordinary English data.
export const OPTIONAL_MOVIES = Object.freeze(['gvuglogo']);

export const PYTHON_MODULES = Object.freeze([
  'skin_inventory.py', 'prepare_native_textures.py',
  'prepare_home_artwork.py',
]);

const GREETINGS = Object.freeze([
  'homer\\w_greeting_hom_hanging.rsd', 'homer\\w_greeting_hom_happen.rsd',
  'bart\\w_greeting_brt_buddy.rsd', 'bart\\w_greeting_brt_dude.rsd',
  'lisa\\w_greeting_lis_hi.rsd', 'lisa\\w_greeting_lis_how.rsd',
  'marge\\w_greeting_mrg_hello.rsd', 'marge\\w_greeting_mrg_how.rsd',
  'apu\\w_greeting_apu_fine.rsd', 'apu\\w_greeting_apu_loyal.rsd',
]);

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();
const MOVIE_HEADER_SIZE = 52;
const VIDEO_FILTER = 'fps=30000/1001,scale=320:240:force_original_aspect_ratio=decrease:force_divisible_by=2:flags=lanczos,pad=320:240:(ow-iw)/2:(oh-ih)/2,setsar=1';
const VIDEO_ENCODER = ['-c:v', 'mpeg1video', '-pix_fmt', 'yuv420p', '-q:v', '3', '-g', '15', '-bf', '0', '-threads', '1'];

function safePath(path) {
  const value = path.replaceAll('\\', '/');
  if (!value || value.startsWith('/') || value.split('/').some(part => !part || part === '.' || part === '..')) {
    throw new Error('The selected folder contains an invalid file path.');
  }
  return value;
}

/** Accept either a relative-path map, or the result of an HTML folder picker. */
export function normalizeInputFiles(input) {
  if (input instanceof Map) {
    return new Map([...input].map(([path, file]) => [safePath(path), file]));
  }
  const entries = [...input].map(file => [file.webkitRelativePath || file.name, file]);
  const firstComponents = new Set(entries.map(([path]) => path.replaceAll('\\', '/').split('/')[0]));
  const stripRoot = firstComponents.size === 1 && entries.every(([path]) => path.includes('/'));
  return new Map(entries.map(([path, file]) => [safePath(stripRoot ? path.slice(path.indexOf('/') + 1) : path), file]));
}

function lookupFiles(input) {
  const files = normalizeInputFiles(input);
  const lower = new Map();
  for (const [path, file] of files) {
    const key = path.toLowerCase();
    if (lower.has(key)) throw new Error(`The selected folder has duplicate names: ${path}`);
    lower.set(key, {path, file});
  }
  return {files, lower, get: path => lower.get(path.toLowerCase())?.file};
}

function movieNames(selected) {
  return [...REQUIRED_MOVIES, ...OPTIONAL_MOVIES.filter(name => selected.get(`movies/${name}.rmv`))];
}

/** Validate the full supported PC layout before downloading conversion tools. */
export function validateInput(input) {
  const selected = lookupFiles(input);
  const required = [
    'art/frontend/scrooby/bootup.p3d', 'art/frontend/scrooby/frontend.p3d',
    'art/frontend/scrooby/resource/pure3d/logo_anim.p3d',
    'scripts.rcf', 'dialog.rcf', 'music00.rcf', 'music01.rcf', 'music02.rcf', 'music03.rcf',
    ...REQUIRED_MOVIES.map(name => `movies/${name}.rmv`),
  ];
  const missing = required.filter(path => !selected.get(path));
  if (![...selected.lower.keys()].some(path => path.startsWith('scripts/'))) missing.push('scripts/');
  if (![...selected.lower.keys()].some(path => path.startsWith('sound/'))) missing.push('sound/');
  const development = ['Simpsons-Debug.exe', 'SRR2wd.exe', 'music.rcf'].filter(path => selected.get(path));
  if (development.length) {
    throw new Error('This looks like development data. Select an installed retail PC copy of the game.');
  }
  if (missing.length) {
    throw new Error(`Select the complete installed PC game folder. Missing: ${missing.slice(0, 6).join(', ')}${missing.length > 6 ? ` and ${missing.length - 6} more` : ''}.`);
  }
  const originalFiles = new Map([...selected.files].filter(([path]) => {
    const key = path.toLowerCase();
    return path.split('/').at(-1).toLowerCase() !== 'temp.err' &&
      (/^(art|scripts|sound)\//.test(key) || (!key.includes('/') && key.endsWith('.rcf')));
  }));
  const movies = movieNames(selected);
  const movieBytes = movies.reduce((sum, name) => sum + selected.get(`movies/${name}.rmv`).size, 0);
  return {fileCount: originalFiles.size + movies.length,
    inputBytes: [...originalFiles.values()].reduce((sum, file) => sum + file.size, 0) + movieBytes,
    movieCount: movies.length};
}

async function bytesOf(value, start = 0, end) {
  if (value instanceof Uint8Array) return value.slice(start, end);
  if (value instanceof ArrayBuffer) return new Uint8Array(value.slice(start, end));
  return new Uint8Array(await value.slice(start, end).arrayBuffer());
}

function fileSize(value) {
  return value.size ?? value.byteLength;
}

function concatBytes(...parts) {
  const output = new Uint8Array(parts.reduce((size, part) => size + part.byteLength, 0));
  let cursor = 0;
  for (const part of parts) { output.set(part, cursor); cursor += part.byteLength; }
  return output;
}

async function sha256(value) {
  const bytes = value instanceof Uint8Array ? value : await bytesOf(value);
  return new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
}

function hex(bytes) {
  return [...bytes].map(value => value.toString(16).padStart(2, '0')).join('');
}

const CRC_TABLE = Uint32Array.from({length: 256}, (_, initial) => {
  let crc = initial;
  for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
  return crc >>> 0;
});

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = CRC_TABLE[(crc ^ byte) & 255] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

/** Match prepare_home_menu.archive_member, including its legacy case hash. */
export async function archiveMember(archive, name) {
  const size = fileSize(archive);
  const header = await bytesOf(archive, 0, 48);
  if (header.length !== 48 || !textDecoder.decode(header).startsWith('RADCORE CEMENT LIBRARY') || header[34] !== 0) {
    throw new Error('The selected game has an unsupported RCF archive.');
  }
  let key = 0;
  for (const char of name) {
    let value = char.charCodeAt(0);
    if (value < 97) value += 32;
    key = (Math.imul(31, key) + value) >>> 0;
  }
  const offset = new DataView(header.buffer, header.byteOffset, header.byteLength).getUint32(44, true);
  if (offset + 16 > size) throw new Error('An RCF archive has an invalid directory offset.');
  const directoryHeader = await bytesOf(archive, offset, offset + 16);
  const count = new DataView(directoryHeader.buffer, directoryHeader.byteOffset, 16).getUint32(0, true);
  if (count > 100000 || offset + 16 + count * 12 > size) throw new Error('An RCF archive has an invalid directory.');
  const directory = await bytesOf(archive, offset + 16, offset + 16 + count * 12);
  const view = new DataView(directory.buffer, directory.byteOffset, directory.byteLength);
  for (let index = 0; index < count; index++) {
    if (view.getUint32(index * 12, true) !== key) continue;
    const position = view.getUint32(index * 12 + 4, true);
    const length = view.getUint32(index * 12 + 8, true);
    if (!length || position + length > size) throw new Error(`The game archive contains an invalid ${name}.`);
    return bytesOf(archive, position, position + length);
  }
  throw new Error(`The PC game is missing ${name}.`);
}

function toolOutput(result, name) {
  const value = result.files instanceof Map ? result.files.get(name) : result.files?.[name];
  if (!value) throw new Error(`The media converter did not produce ${name}.`);
  return value;
}

function logLines(result) {
  return (result.logs || []).map(line => typeof line === 'string' ? line : line.message || '').flatMap(line => line.split('\n'));
}

async function mediaCommand(runFFmpeg, request) {
  const result = await runFFmpeg(request);
  if (typeof result.exitCode === 'number' && result.exitCode !== 0 && !request.allowFailure) {
    throw new Error(`Media conversion failed: ${logLines(result).filter(line => line && line !== 'Aborted()').slice(-3).join(' ')}`);
  }
  return result;
}

function frameCount(result) {
  const frames = logLines(result).filter(line => /^frame=\s*\d+$/.test(line)).map(line => Number(line.split('=')[1]));
  if (!frames.length || frames.at(-1) < 1) throw new Error('A movie conversion produced no video frames.');
  return frames.at(-1);
}

/** Keep the native converter's frame rate, letterboxing, sound and tail handling. */
export async function convertMovie(source, stem, runFFmpeg) {
  const inputName = `${stem}.rmv`;
  const input = [{name: inputName, data: source}];
  // The reusable FFmpeg core retains its log level after earlier conversions.
  const probe = await mediaCommand(runFFmpeg, {inputs: input, args: ['-hide_banner', '-loglevel', 'info', '-i', inputName], outputs: [], allowFailure: true});
  const probeLogs = logLines(probe);
  if (!probeLogs.some(line => line.includes('Video:'))) throw new Error(`${inputName} has no supported video stream.`);
  const hasAudio = probeLogs.some(line => line.includes('Audio:'));
  const videoName = `${stem}.m1v`, audioName = `${stem}.pcm`;
  const video = await mediaCommand(runFFmpeg, {
    inputs: input, args: ['-hide_banner', '-loglevel', 'error', '-i', inputName, '-an', '-vf', VIDEO_FILTER,
      ...VIDEO_ENCODER, '-progress', 'pipe:1', '-nostats', '-f', 'mpeg1video', videoName], outputs: [videoName],
  });
  let frames = frameCount(video), videoFile = toolOutput(video, videoName);
  let audioFile = new Uint8Array(0);
  if (hasAudio) {
    const audio = await mediaCommand(runFFmpeg, {
      inputs: input, args: ['-hide_banner', '-loglevel', 'error', '-i', inputName, '-vn', '-map', '0:a:0',
        '-ac', '2', '-ar', '32000', '-c:a', 'pcm_s16le', '-f', 's16le', audioName], outputs: [audioName],
    });
    audioFile = toolOutput(audio, audioName);
  }
  const audioBytes = fileSize(audioFile);
  const audioFrames = Math.floor((audioBytes * 30000 + 128000 * 1001 - 1) / (128000 * 1001));
  if (audioFrames > frames) {
    const paddedName = `${stem}-padded.m1v`;
    const padded = await mediaCommand(runFFmpeg, {
      inputs: input, args: ['-hide_banner', '-loglevel', 'error', '-i', inputName, '-an', '-vf',
        `${VIDEO_FILTER},tpad=stop_mode=clone:stop=-1`, '-frames:v', String(audioFrames), ...VIDEO_ENCODER,
        '-f', 'mpeg1video', paddedName], outputs: [paddedName],
    });
    videoFile = toolOutput(padded, paddedName);
    frames = audioFrames;
  }
  const videoBytes = fileSize(videoFile);
  if (!videoBytes || audioBytes % 4) throw new Error(`${inputName} produced invalid movie data.`);
  const verificationName = `${stem}-verify.m1v`;
  const verification = await mediaCommand(runFFmpeg, {
    inputs: [{name: verificationName, data: videoFile}],
    args: ['-hide_banner', '-loglevel', 'error', '-i', verificationName, '-progress', 'pipe:1', '-nostats', '-f', 'null', '-'],
    outputs: [],
  });
  if (frameCount(verification) !== frames) throw new Error(`${inputName} did not pass complete movie decoding.`);
  const header = new Uint8Array(MOVIE_HEADER_SIZE);
  header.set(textEncoder.encode('SRR3DSMV'));
  const view = new DataView(header.buffer);
  view.setUint32(8, 2, true);
  view.setUint16(12, 320, true); view.setUint16(14, 240, true);
  view.setUint32(16, 30000, true); view.setUint32(20, 1001, true);
  view.setUint32(24, frames, true); view.setUint32(28, audioBytes ? 32000 : 0, true);
  view.setUint16(32, audioBytes ? 2 : 0, true);
  view.setUint32(36, MOVIE_HEADER_SIZE, true); view.setUint32(40, audioBytes, true);
  view.setUint32(44, MOVIE_HEADER_SIZE + audioBytes, true); view.setUint32(48, videoBytes, true);
  return {blob: new Blob([header, audioFile, videoFile]), frames, audioBytes, videoBytes};
}

/** Check the same HOME Menu cue format and peak bounds as the native builder. */
export function validateBannerCue(bytes) {
  if (bytes.length < 44 || textDecoder.decode(bytes.slice(0, 4)) !== 'RIFF' || textDecoder.decode(bytes.slice(8, 12)) !== 'WAVE') {
    throw new Error('The HOME Menu audio converter returned an invalid WAV file.');
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let format, audio;
  for (let offset = 12; offset + 8 <= bytes.length;) {
    const kind = textDecoder.decode(bytes.slice(offset, offset + 4));
    const size = view.getUint32(offset + 4, true), start = offset + 8;
    if (start + size > bytes.length) throw new Error('The HOME Menu WAV file is truncated.');
    if (kind === 'fmt ') {
      if (size < 16) throw new Error('The HOME Menu WAV format is incomplete.');
      format = {kind: view.getUint16(start, true), channels: view.getUint16(start + 2, true),
        rate: view.getUint32(start + 4, true), bits: view.getUint16(start + 14, true)};
      if (format.kind === 0xfffe && size >= 40) format.kind = view.getUint16(start + 24, true);
    }
    if (kind === 'data') audio = bytes.subarray(start, start + size);
    offset = start + size + (size & 1);
  }
  if (!format || !audio || format.kind !== 1 || format.channels !== 2 || format.rate !== 32000 || format.bits !== 16 || audio.length !== 358400) {
    throw new Error('The HOME Menu audio has the wrong format or duration.');
  }
  const samples = new DataView(audio.buffer, audio.byteOffset, audio.byteLength);
  let peak = 0;
  for (let offset = 0; offset < audio.length; offset += 2) peak = Math.max(peak, Math.abs(samples.getInt16(offset, true)));
  if (peak <= 32 || peak >= 26900) throw new Error('The HOME Menu audio is silent or exceeds its peak limit.');
  return {frames: audio.length / 4, peak};
}

function morton(x, y) {
  return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

async function opaqueEtc1(request, nativeBytes, encodeEtc1) {
  const chunks = [];
  let offset = 0;
  for (let level = 0; level <= request.levels; level++) {
    const width = request.dw >> level, height = request.dh >> level;
    const view = new DataView(nativeBytes.buffer, nativeBytes.byteOffset + offset, width * height * 2);
    const rgb = new Uint8Array(width * height * 3);
    for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
      const tiled = ((y >> 3) * (width >> 3) + (x >> 3)) * 64 + morton(x & 7, y & 7);
      const pixel = view.getUint16(tiled * 2, true), target = (y * width + x) * 3;
      if (request.format === 1) {
        rgb[target] = Math.floor(((pixel >> 11) * 255 + 15) / 31);
        rgb[target + 1] = Math.floor((((pixel >> 5) & 63) * 255 + 31) / 63);
        rgb[target + 2] = Math.floor(((pixel & 31) * 255 + 15) / 31);
      } else if (request.format === 2) {
        if (!(pixel & 1)) throw new Error('An expected opaque world texture contains transparency.');
        for (let channel = 0; channel < 3; channel++) rgb[target + channel] = Math.floor((((pixel >> [11, 6, 1][channel]) & 31) * 255 + 15) / 31);
      } else {
        if ((pixel & 15) !== 15) throw new Error('An expected opaque world texture contains transparency.');
        rgb[target] = (pixel >> 12) * 17; rgb[target + 1] = ((pixel >> 8) & 15) * 17; rgb[target + 2] = ((pixel >> 4) & 15) * 17;
      }
    }
    const encoded = await encodeEtc1({width, height, rgb});
    if (encoded.byteLength !== width * height / 2) throw new Error('The texture encoder returned an invalid ETC1 payload.');
    chunks.push(encoded);
    offset += width * height * 2;
  }
  if (offset !== nativeBytes.byteLength) throw new Error('The texture converter returned an invalid mip chain.');
  const payload = concatBytes(...chunks), prefix = new Uint8Array(request.prefix);
  const view = new DataView(prefix.buffer, prefix.byteOffset, prefix.byteLength);
  view.setUint32(4, 2, true); view.setUint32(28, 4, true); view.setUint32(40, payload.byteLength, true);
  return {key: hex(await sha256(prefix)), body: concatBytes(prefix, await sha256(payload), payload)};
}

export async function prepareTextureBundle({files, profile, pyodide, tools, progress = () => {}, checkCancelled = () => {}}) {
  const expected = profile.source_variant_keys;
  const accepted = pyodide.toPy(expected);
  const extract = pyodide.globals.get('texture_requests');
  const found = new Map();
  const sources = [...files].filter(([path]) => /^art\/l[1-7](?:_terra|[irz][0-9].*)\.p3d$/i.test(path)).sort(([a], [b]) => a.localeCompare(b, 'en'));
  try {
    for (let index = 0; index < sources.length; index++) {
      checkCancelled();
      const [path, file] = sources[index];
      const input = pyodide.toPy(await bytesOf(file));
      let requests;
      try {
        const result = extract(input, accepted);
        try { requests = result.toJs({dict_converter: Object.fromEntries}); } finally { result.destroy(); }
      } finally { input.destroy(); }
      const pending = requests.filter(request => !found.has(request.key));
      const concurrency = Math.max(1, Math.min(8, Math.floor(tools.textureConcurrency || 1)));
      let next = 0, failure;
      const work = async () => {
        while (next < pending.length && !failure) {
          const request = pending[next++];
          try {
            checkCancelled();
            const native = await tools.convertTexture(request);
            if (native.byteLength !== request.bytes) throw new Error(`The texture converter returned incomplete data for ${path}.`);
            found.set(request.key, await opaqueEtc1(request, native, tools.encodeEtc1));
            progress({stage: 'textures', completed: found.size, total: expected.length,
              detail: 'Converting textures'});
          } catch (error) { failure = error; }
        }
      };
      await Promise.all(Array.from({length: Math.min(concurrency, pending.length)}, work));
      if (failure) throw failure;
    }
  } finally { accepted.destroy(); extract.destroy(); }
  const missing = expected.filter(key => !found.has(key));
  if (missing.length) throw new Error(`This PC copy differs from the supported artwork set (${missing.length} texture variants missing).`);
  const header = new Uint8Array(8), view = new DataView(header.buffer);
  view.setUint32(0, 0x314e4233, true); view.setUint32(4, expected.length, true);
  const parts = [header];
  for (const key of expected) {
    const item = found.get(key), record = new Uint8Array(76), recordView = new DataView(record.buffer);
    record.set(textEncoder.encode(`${item.key}.ntx`));
    recordView.setUint32(68, item.body.byteLength, true); recordView.setUint32(72, crc32(item.body), true);
    parts.push(record, item.body);
  }
  const blob = new Blob(parts);
  const digest = hex(await sha256(blob));
  if (digest !== profile.bundle_sha256) throw new Error('The browser texture conversion differs from the accepted game build. No installer was produced.');
  return {blob, sha256: digest, entries: expected.length};
}

/** Register this module's Python parser after loading its existing dependencies. */
export async function loadTextureParser(pyodide, source) {
  await pyodide.runPythonAsync(source);
}

function checkAbort(signal) {
  if (signal?.aborted) throw new DOMException('Build cancelled.', 'AbortError');
}

/**
 * tools.runFFmpeg uses isolated native-WASM filesystem inputs and outputs.
 * writeOutput persists generated data to OPFS and returns a File; originals are
 * referenced unchanged, and no user game bytes are sent over the network.
 */
export async function prepareGameAssets({files: input, tools, pyodide, profile, writeOutput, onProgress = () => {}, signal,
  phase = 'all', previous = null, checkpoints = null}) {
  const summary = validateInput(input), selected = lookupFiles(input);
  const files = previous?.files ? new Map(previous.files) : new Map([...selected.files].filter(([path]) => {
    const key = path.toLowerCase();
    return path.split('/').at(-1).toLowerCase() !== 'temp.err' &&
      (/^(art|scripts|sound)\//.test(key) || (!key.includes('/') && key.endsWith('.rcf')));
  }));
  const report = {...summary, ...(previous?.report || {})};
  const homeMenu = {...(previous?.homeMenu || {})};
  const persist = async (path, value) => {
    checkAbort(signal);
    const file = await writeOutput(path, value instanceof Blob ? value : new Blob([value]));
    files.set(path, file);
    return file;
  };
  if (phase === 'all' || phase === 'textures') {
    const cached = await checkpoints?.read('native-textures');
    if (cached) {
      for (const entry of cached.files) files.set(...entry);
      Object.assign(report, cached.metadata);
      onProgress({stage: 'textures', detail: 'Reusing verified textures'});
    } else {
      const texture = await prepareTextureBundle({files: selected.files, profile, pyodide, tools, progress: onProgress, checkCancelled: () => checkAbort(signal)});
      const file = await persist('native146.bundle', texture.blob);
      const metadata = {textureBundleSHA256: texture.sha256, textures: texture.entries};
      await checkpoints?.commit('native-textures', new Map([['native146.bundle', file]]), metadata);
      Object.assign(report, metadata);
    }
  }

  if (phase === 'all' || phase === 'media') {

  const dialog = selected.get('dialog.rcf');
  for (let index = 0; index < GREETINGS.length; index++) {
    checkAbort(signal);
    const cached = await checkpoints?.read(`greeting-${index}`);
    if (cached) { for (const entry of cached.files) files.set(...entry); continue; }
    const source = await archiveMember(dialog, GREETINGS[index]);
    const name = `greeting-${index}.pcm`;
    const result = await mediaCommand(tools.runFFmpeg, {
      inputs: [{name: 'source.rsd', data: source}], args: ['-hide_banner', '-loglevel', 'error', '-i', 'source.rsd', '-af',
        'loudnorm=I=-18:TP=-3:LRA=7,afade=t=in:d=0.004,areverse,afade=t=in:d=0.012,areverse',
        '-ar', '24000', '-ac', '1', '-c:a', 'pcm_s16le', '-f', 's16le', name], outputs: [name],
    });
    const audio = await bytesOf(toolOutput(result, name));
    if (!audio.length || audio.length > 24000 * 3 * 2 || audio.length % 2) throw new Error('A character greeting has an invalid audio duration.');
    const samples = new DataView(audio.buffer, audio.byteOffset, audio.byteLength);
    let peak = 0;
    for (let offset = 0; offset < audio.length; offset += 2) peak = Math.max(peak, Math.abs(samples.getInt16(offset, true)));
    if (peak <= 32 || peak >= 27000) throw new Error('A character greeting is silent or clips.');
    const header = new Uint8Array(12), view = new DataView(header.buffer);
    header.set(textEncoder.encode('LID1')); view.setUint32(4, 24000, true); view.setUint32(8, audio.length / 2, true);
    const path = `sound/3ds-lid/${String(index).padStart(2, '0')}.bin`;
    const file = await persist(path, new Blob([header, audio]));
    await checkpoints?.commit(`greeting-${index}`, new Map([[path, file]]));
    onProgress({stage: 'audio', completed: index + 1, total: GREETINGS.length, detail: 'Converting character voices'});
  }

  const cueCheckpoint = await checkpoints?.read('home-cue');
  if (cueCheckpoint) homeMenu.cue = cueCheckpoint.files.get('home-menu/banner.wav');
  else {
  const theme = await archiveMember(selected.get('music02.rcf'), 'sound\\music\\simpsons_theme.rsd');
  const cue = await mediaCommand(tools.runFFmpeg, {
    inputs: [{name: 'theme.rsd', data: theme}], args: ['-hide_banner', '-loglevel', 'error', '-i', 'theme.rsd', '-t', '2.8', '-af',
      'loudnorm=I=-20:TP=-2:LRA=7,afade=t=in:d=0.015,afade=t=out:st=2.6:d=0.2',
      '-ar', '32000', '-ac', '2', '-c:a', 'pcm_s16le', 'banner.wav'], outputs: ['banner.wav'],
  });
  const cueBytes = await bytesOf(toolOutput(cue, 'banner.wav'));
  validateBannerCue(cueBytes);
  homeMenu.cue = await writeOutput('home-menu/banner.wav', new Blob([cueBytes]));
  await checkpoints?.commit('home-cue', new Map([['home-menu/banner.wav', homeMenu.cue]]));
  }
  }

  if (phase === 'all' || phase === 'textures') {
  const artworkCheckpoint = await checkpoints?.read('home-artwork');
  if (artworkCheckpoint) {
    homeMenu.banner = artworkCheckpoint.files.get('home-menu/banner.png');
    homeMenu.icon = artworkCheckpoint.files.get('home-menu/icon.png');
  } else {

  const render = pyodide.globals.get('render_artwork');
  const frontend = pyodide.toPy(await bytesOf(selected.get('art/frontend/scrooby/frontend.p3d')));
  const logo = pyodide.toPy(await bytesOf(selected.get('art/frontend/scrooby/resource/pure3d/logo_anim.p3d')));
  try {
    const result = render(frontend, logo);
    try {
      const artwork = result.toJs({dict_converter: Object.fromEntries});
      homeMenu.banner = await writeOutput('home-menu/banner.png', new Blob([artwork['banner.png']]));
      homeMenu.icon = await writeOutput('home-menu/icon.png', new Blob([artwork['icon.png']]));
    } finally { result.destroy(); }
  } finally { render.destroy(); frontend.destroy(); logo.destroy(); }
  await checkpoints?.commit('home-artwork', new Map([['home-menu/banner.png', homeMenu.banner], ['home-menu/icon.png', homeMenu.icon]]));
  }
  }

  if (phase === 'all' || phase === 'media') {

  const movieReport = [], movies = movieNames(selected);
  for (let index = 0; index < movies.length; index++) {
    checkAbort(signal);
    const stem = movies[index];
    onProgress({stage: 'movies', completed: index, total: movies.length, detail: `Converting ${stem}`});
    const cached = await checkpoints?.read(`movie-${stem}`);
    if (cached) {
      for (const entry of cached.files) files.set(...entry);
      movieReport.push(cached.metadata);
      onProgress({stage: 'movies', completed: index + 1, total: movies.length, detail: `Reused ${stem}`});
      continue;
    }
    const movie = await convertMovie(selected.get(`movies/${stem}.rmv`), stem, tools.runFFmpeg);
    const path = `movies/${stem}.3dm`, file = await persist(path, movie.blob);
    const metadata = {file: `${stem}.3dm`, frames: movie.frames, audioBytes: movie.audioBytes, videoBytes: movie.videoBytes};
    movieReport.push(metadata);
    await checkpoints?.commit(`movie-${stem}`, new Map([[path, file]]), metadata);
    onProgress({stage: 'movies', completed: index + 1, total: movies.length, detail: `Converted ${stem}`});
  }
  report.movies = movieReport;
  }
  return {files, homeMenu, report};
}

/** Exact-PCM-keyed v1.2 DSP cache, generated only from selected local archives. */
export const AUDIO_CACHE_SAMPLE_LENGTHS = Object.freeze([
  7492, 7681, 7697, 7888, 8778, 9536, 11200, 11521, 11872, 12544,
  13056, 14624, 15297, 15808, 17248, 17760, 18796, 19264, 19328,
  19584, 20416, 21696, 22337, 25665, 26305, 26488, 28553, 31616,
  32183, 35999, 36673, 38209, 57605, 61662, 66925, 77312, 81015,
  113714,
]);
const ARCHIVES = Object.freeze(['soundfx.rcf', 'carsound.rcf']);
const LIMIT = 3 * 1024 * 1024, HEADER_BYTES = 32, ENTRY_BYTES = 16;
const encoder = new TextEncoder(), decoder = new TextDecoder();
const hex = bytes => [...bytes].map(byte => byte.toString(16).padStart(2, '0')).join('');
const sha256 = async bytes => hex(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)));
const align32 = value => Math.ceil(value / 32) * 32;
function fnv1a(bytes, initial = 2166136261) {
  let value = initial;
  for (const byte of bytes) value = Math.imul(value ^ byte, 16777619) >>> 0;
  return value;
}
function byteLength(file) { return file.size ?? file.byteLength; }
async function read(file, start, length) {
  if (!Number.isSafeInteger(start) || !Number.isSafeInteger(length) || start < 0 || length < 0 || start + length > byteLength(file)) {
    throw new Error('An audio archive contains an invalid byte range.');
  }
  let bytes;
  if (file instanceof Uint8Array) bytes = file.slice(start, start + length);
  else if (file instanceof ArrayBuffer) bytes = new Uint8Array(file.slice(start, start + length));
  else bytes = new Uint8Array(await file.slice(start, start + length).arrayBuffer());
  if (bytes.byteLength !== length) throw new Error('A selected audio archive is truncated.');
  return bytes;
}
function sourcePart(file, start, length, bytes) {
  const part = file.slice(start, start + length);
  // Browser File/Blob slices remain disk-backed. Small Node range fixtures can
  // supply file-like slices instead; freeze their PCM before an in-place encode.
  return part instanceof Blob ? part : new Blob([bytes]);
}

async function members(file, archive) {
  const size = byteLength(file), header = await read(file, 0, 48);
  if (!decoder.decode(header).startsWith('RADCORE CEMENT LIBRARY') || header[34] !== 0) {
    throw new Error(`Unsupported audio archive: ${archive}.`);
  }
  const directory = new DataView(header.buffer, header.byteOffset, header.byteLength).getUint32(44, true);
  const tableHeader = await read(file, directory, 16);
  const count = new DataView(tableHeader.buffer, tableHeader.byteOffset, tableHeader.byteLength).getUint32(0, true);
  if (count > 100000 || directory + 16 + count * 12 > size) throw new Error(`Invalid audio directory: ${archive}.`);
  const table = await read(file, directory + 16, count * 12);
  const view = new DataView(table.buffer, table.byteOffset, table.byteLength);
  const lengths = new Set(AUDIO_CACHE_SAMPLE_LENGTHS.map(samples => 2048 + samples * 2));
  const selected = [];
  for (let index = 0; index < count; index++) {
    const uid = view.getUint32(index * 12, true), offset = view.getUint32(index * 12 + 4, true);
    const length = view.getUint32(index * 12 + 8, true);
    if (length < 20 || offset + length > size) throw new Error(`Invalid audio member: ${archive}.`);
    // Only these byte lengths can be eligible mono PCM. Skip unrelated members
    // before reading their headers; large RCF archives need only small ranges.
    if (!lengths.has(length)) continue;
    const sound = await read(file, offset, 20);
    if (decoder.decode(sound.subarray(0, 8)) !== 'RSD4PCM ') continue;
    const fields = new DataView(sound.buffer, sound.byteOffset, sound.byteLength);
    if (fields.getUint32(8, true) !== 1 || fields.getUint32(12, true) !== 16) continue;
    selected.push({archive, uid, offset, length, samples: (length - 2048) / 2,
      sampleRate: fields.getUint32(16, true), file});
  }
  return selected;
}

/**
 * encode(pcmBytes, samples) returns production kernel<3> DSP bytes. It may modify
 * its input. Only one clip is materialized at a time; PCM Blob parts reference
 * the original local files. The complete result is bounded to three MiB.
 */
export async function buildAudioCache(files, encode, progress = () => {}) {
  if (!(files instanceof Map) || typeof encode !== 'function') throw new TypeError('Audio preparation requires selected files and an encoder.');
  const originals = new Map([...files].map(([path, file]) => [path.replaceAll('\\', '/').toLowerCase(), file]));
  const candidates = [];
  for (const archive of ARCHIVES) {
    const file = originals.get(archive);
    if (!file) throw new Error(`The PC copy is missing ${archive}.`);
    candidates.push(...await members(file, archive));
  }
  candidates.sort((a, b) => a.samples - b.samples ||
    (a.archive < b.archive ? -1 : a.archive > b.archive ? 1 : 0) || a.uid - b.uid);
  const seen = new Set(), selected = [];
  for (const record of candidates) {
    const pcm = await read(record.file, record.offset + 2048, record.samples * 2);
    const key = await sha256(pcm);
    if (!seen.has(key)) { seen.add(key); selected.push({...record, pcmSHA256: key}); }
  }
  if (!selected.length || selected.length > 256) throw new Error('This PC copy has no supported bounded audio-cache selection.');

  const index = new Uint8Array(selected.length * ENTRY_BYTES);
  const entries = new DataView(index.buffer);
  let size = HEADER_BYTES + index.length;
  for (let i = 0; i < selected.length; i++) {
    const record = selected[i];
    record.pcmOffset = align32(size);
    record.dspOffset = align32(record.pcmOffset + record.samples * 2);
    record.dspBytes = Math.ceil(record.samples / 14) * 8;
    size = record.dspOffset + record.dspBytes;
    entries.setUint32(i * ENTRY_BYTES, record.samples, true);
    entries.setUint32(i * ENTRY_BYTES + 4, record.pcmOffset, true);
    entries.setUint32(i * ENTRY_BYTES + 8, record.dspOffset, true);
    entries.setUint32(i * ENTRY_BYTES + 12, record.dspBytes, true);
  }
  if (size > LIMIT) throw new Error('Selected audio exceeds the accepted three-MiB cache budget.');

  let checksum = fnv1a(index), position = HEADER_BYTES + index.length;
  const payload = [], metadata = [];
  for (let i = 0; i < selected.length; i++) {
    const record = selected[i], pcm = await read(record.file, record.offset + 2048, record.samples * 2);
    const pcmPadding = new Uint8Array(record.pcmOffset - position);
    const dspPadding = new Uint8Array(record.dspOffset - record.pcmOffset - pcm.length);
    checksum = fnv1a(pcmPadding, checksum);
    checksum = fnv1a(pcm, checksum);
    checksum = fnv1a(dspPadding, checksum);
    const pcmPart = sourcePart(record.file, record.offset + 2048, record.samples * 2, pcm);
    const dsp = await encode(pcm, record.samples);
    if (!(dsp instanceof Uint8Array) || dsp.byteLength !== record.dspBytes) throw new Error('The DSP encoder returned an invalid audio length.');
    for (let offset = 0; offset < dsp.length; offset += 8) {
      if ((dsp[offset] >>> 4) > 2 || (dsp[offset] & 15) > 12) throw new Error('The DSP encoder returned an invalid predictor or scale.');
    }
    checksum = fnv1a(dsp, checksum);
    payload.push(pcmPadding, pcmPart, dspPadding, new Blob([dsp]));
    position = record.dspOffset + record.dspBytes;
    metadata.push({archive: record.archive, uid: record.uid, samples: record.samples,
      sampleRate: record.sampleRate, pcmSHA256: record.pcmSHA256,
      dspSHA256: await sha256(dsp), dspBytes: record.dspBytes});
    progress({stage: 'audio-cache', completed: i + 1, total: selected.length, detail: 'Preparing game sounds'});
  }
  const header = new Uint8Array(HEADER_BYTES), view = new DataView(header.buffer);
  header.set(encoder.encode('SRRDSP1\0'));
  [1, selected.length, size, checksum, 3, 0].forEach((value, i) => view.setUint32(8 + i * 4, value, true));
  const blob = new Blob([header, index, ...payload]);
  if (blob.size !== size) throw new Error('The prepared audio cache is incomplete.');
  return {blob, sha256: await sha256(await blob.arrayBuffer()), clips: selected.length,
    metadata: {version: 1, predictors: 3, maximumBytes: LIMIT, bytes: size, checksum,
      pcmKey: 'exact complete PCM bytes', entries: metadata}};
}

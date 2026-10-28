/** Content-keyed local checkpoints. A changed source or recipe cannot reuse outputs. */
const encoder = new TextEncoder();
const hex = bytes => [...new Uint8Array(bytes)].map(value => value.toString(16).padStart(2, '0')).join('');
export async function contentFingerprint(blob, onProgress = () => {}) {
  // Hash bounded pieces, then their ordered hashes and byte length. This is a
  // cache identity, deliberately not labelled the file's ordinary SHA-256.
  const chunkBytes = 4 * 1024 * 1024, hashes = [];
  for (let start = 0; start < blob.size; start += chunkBytes) {
    const buffer = await blob.slice(start, start + chunkBytes).arrayBuffer();
    hashes.push(new Uint8Array(await crypto.subtle.digest('SHA-256', buffer)));
    onProgress(Math.min(start + chunkBytes, blob.size), blob.size);
  }
  const prefix = encoder.encode(`hit-run-input-v1:${blob.size}:`);
  const identity = new Uint8Array(prefix.length + hashes.length * 32);
  identity.set(prefix); hashes.forEach((hash, i) => identity.set(hash, prefix.length + i * 32));
  return hex(await crypto.subtle.digest('SHA-256', identity));
}

export async function inputFingerprint(input, onProgress = () => {}) {
  if (input instanceof Blob) return contentFingerprint(input, onProgress);
  const entries = [...input].sort(([a], [b]) => a.localeCompare(b, 'en'));
  const identities = [];
  const total = entries.reduce((sum, [, file]) => sum + file.size, 0);
  let completed = 0;
  for (const [name, file] of entries) {
    const fingerprint = await contentFingerprint(file, bytes => onProgress(completed + bytes, total));
    identities.push(`${name.toLowerCase()}\0${file.size}\0${fingerprint}`); completed += file.size;
  }
  return hex(await crypto.subtle.digest('SHA-256', encoder.encode(identities.join('\n'))));
}

export async function createCheckpoints(storage, identity, recipe) {
  const key = `${identity}:${recipe}`;
  const previous = await storage.readJSON('checkpoint.json');
  if (previous && previous.key !== key) await storage.clear();
  const state = previous?.key === key ? previous : {version: 2, key, stages: {}};
  const save = () => storage.writeJSON('checkpoint.json', state);
  const valid = async record => {
    if (!record?.files) return false;
    for (const [path, bytes, fingerprint] of record.files) {
      const file = await storage.read(path);
      if (!file || file.size !== bytes || !fingerprint || await contentFingerprint(file) !== fingerprint) return false;
    }
    return true;
  };
  return {
    async read(stage) {
      const record = state.stages[stage];
      if (!await valid(record)) {
        if (record) { delete state.stages[stage]; await save(); }
        return null;
      }
      const files = new Map();
      for (const [path] of record.files) files.set(path, await storage.read(path));
      return {...record, files};
    },
    async commit(stage, files, metadata = {}) {
      const records = [];
      for (const [path, file] of files) records.push([path, file.size, await contentFingerprint(file)]);
      const record = {files: records, metadata};
      state.stages[stage] = record; await save(); return record;
    },
    async discard(stage) { delete state.stages[stage]; await save(); },
    state,
  };
}

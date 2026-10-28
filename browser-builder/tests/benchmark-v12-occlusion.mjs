/** Matched WASM ABBA timing using one private, locally generated ray request. */
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const [referencePath, candidatePath, requestPath] = process.argv.slice(2);
if (!requestPath) throw new Error('Pass reference .mjs, candidate .mjs, and a private request .bin.');
const request = new Uint8Array(await readFile(requestPath));
const vertices = new DataView(request.buffer, request.byteOffset, request.byteLength).getUint32(0, true);
const modules = [];
for (const path of [referencePath, candidatePath]) {
  const {default: create} = await import(pathToFileURL(resolve(path)).href);
  modules.push(await create());
}
function run(mode) {
  const baker = modules[mode], start = performance.now();
  const input = baker._malloc(request.length), output = baker._malloc(vertices);
  if (!input || !output) throw new Error('Not enough benchmark memory.');
  let value;
  try {
    baker.HEAPU8.set(request, input);
    assert.equal(baker._v12_occlusion_bake(input, request.length, output, vertices, 0, 0, 1, 0), vertices);
    value = baker.HEAPU8.slice(output, output + vertices);
  } finally { baker._free(input); baker._free(output); }
  return {mode: mode ? 'memoized' : 'reference', milliseconds: performance.now() - start,
    heapBytes: baker.HEAPU8.length, sha256: createHash('sha256').update(value).digest('hex')};
}
const expected = run(0).sha256;
assert.equal(run(1).sha256, expected);
const runs = [];
for (const mode of [0, 1, 1, 0]) {
  const result = run(mode); assert.equal(result.sha256, expected); runs.push(result);
}
const mean = mode => runs.filter(row => row.mode === mode).reduce((sum, row) => sum + row.milliseconds, 0) / 2;
const reference = mean('reference'), candidate = mean('memoized');
console.log(JSON.stringify({vertices, requestSHA256: createHash('sha256').update(request).digest('hex'),
  scope: 'Node WASM on this host, same request; malloc/copy/bake/output/free included; module startup and output hashing excluded',
  runs, referenceMilliseconds: reference, candidateMilliseconds: candidate,
  timeSavedPercent: (1 - candidate / reference) * 100}, null, 2));

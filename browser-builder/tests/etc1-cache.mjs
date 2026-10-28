// Opt-in matched WASM comparison using private, sampled RGB texture fixtures.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {pathToFileURL} from 'node:url';
const args = process.argv.slice(2);
const argument = name => { const i = args.indexOf(name); assert.ok(i >= 0, `Missing ${name}`); return path.resolve(args[i + 1]); };
const baseline = argument('--baseline'), candidate = argument('--candidate'), fixturesPath = argument('--fixtures'), output = argument('--out');
const fixtures = JSON.parse(fs.readFileSync(fixturesPath));
const data = fs.readFileSync(fixturesPath.replace(/\.json$/, '.bin'));
assert.ok(data.length <= 64 * 1024 ** 2 && fixtures.length > 0);
for (const item of fixtures) {
  assert.equal(item.bytes, item.width * item.height * 3);
  assert.ok(item.offset >= 0 && item.offset + item.bytes <= data.length);
}
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const expected = [];
const runs = [];
for (const [index, filename] of [baseline, candidate, candidate, baseline].entries()) {
  const {default: create} = await import(pathToFileURL(filename));
  const module = await create({wasmBinary: fs.readFileSync(filename.replace(/\.mjs$/, '.wasm'))});
  const started = performance.now();
  for (const [i, item] of fixtures.entries()) {
    const pixels = data.subarray(item.offset, item.offset + item.bytes), size = item.width * item.height / 2;
    const source = module._malloc(pixels.length), target = module._malloc(size);
    assert.ok(source && target);
    try {
      module.HEAPU8.set(pixels, source);
      assert.equal(module._encodeEtc1(source, item.width, item.height, target), size);
      const digest = hash(module.HEAPU8.subarray(target, target + size));
      if (index === 0) expected.push(digest); else assert.equal(digest, expected[i], `Texture fixture ${i}`);
    } finally { module._free(source); module._free(target); }
  }
  runs.push({kind: filename === baseline ? 'baseline' : 'candidate', seconds: (performance.now() - started) / 1000, heapBytes: module.HEAPU8.byteLength});
  console.log(JSON.stringify(runs.at(-1)));
}
const average = kind => runs.filter(run => run.kind === kind).reduce((total, run) => total + run.seconds, 0) / 2;
const result = {passed: true, fixtures: fixtures.length, inputBytes: data.length, fixtureSHA256: hash(data), baselineWASMSHA256: hash(fs.readFileSync(baseline.replace(/\.mjs$/, '.wasm'))), candidateWASMSHA256: hash(fs.readFileSync(candidate.replace(/\.mjs$/, '.wasm'))), runs, baselineSeconds: average('baseline'), candidateSeconds: average('candidate'), exactOutput: true, scope: 'Cold-instance Node WASM ABBA on sampled real RGB inputs; not full-browser or phone timing'};
result.timeSavedPercent = (1 - result.candidateSeconds / result.baselineSeconds) * 100;
fs.mkdirSync(path.dirname(output), {recursive: true}); fs.writeFileSync(output, JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result, null, 2));

import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {worldResidencySettings, validateResidencyAdmission} from '../engine-conversion/world-residency.mjs';

globalThis.crypto ??= webcrypto;
const hash = async bytes => new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
const prefix = 'world-residency/', bundle = 'ab'.repeat(32);
const names = ['landmark-visibility.bin', ...Array.from({length: 7}, (_, i) => `resident-visibility${i + 1}.bin`),
  ...Array.from({length: 79}, (_, i) => `art/fixture-${String(i).padStart(2, '0')}.p3d`)].sort();
const files = new Map(names.map((name, i) => [prefix + name, new Blob([new Uint8Array([i, 3, 7])])]));
const bytes = new Uint8Array(80 + names.length * 84), view = new DataView(bytes.buffer);
bytes.set(new TextEncoder().encode('WRA1'));
view.setUint32(4, 1, true); view.setUint32(8, 87, true); view.setUint32(12, 84, true);
bytes.fill(0xab, 16, 48);
for (let i = 0; i < names.length; i++) {
  const offset = 80 + i * 84, file = files.get(prefix + names[i]);
  bytes.set(new TextEncoder().encode(names[i]), offset);
  view.setUint32(offset + 48, file.size, true);
  bytes.set(await hash(await file.arrayBuffer()), offset + 52);
}
bytes.set(await hash(bytes.subarray(80)), 48);
await validateResidencyAdmission(bytes, files, bundle);
for (const offset of [0, 4, 8, 12, 16, 48, 80, 128, 132]) {
  const bad = bytes.slice(); bad[offset] ^= 1;
  await assert.rejects(validateResidencyAdmission(bad, files, bundle));
}
const changed = new Map(files); changed.set(prefix + names[0], new Blob(['bad']));
await assert.rejects(validateResidencyAdmission(bytes, changed, bundle));
const missing = new Map(files); missing.delete(prefix + names[0]);
await assert.rejects(validateResidencyAdmission(bytes, missing, bundle));
const trailing = new Uint8Array(bytes.length + 1); trailing.set(bytes);
await assert.rejects(validateResidencyAdmission(trailing, files, bundle));
// Even a self-consistent marker cannot silently reorder or duplicate records.
const reordered = bytes.slice(); reordered.copyWithin(80 + 84, 80, 80 + 84);
reordered.set(await hash(reordered.subarray(80)), 48);
await assert.rejects(validateResidencyAdmission(reordered, files, bundle));

assert.equal(worldResidencySettings({expectedFiles: {}, runtime: {}}).enabled, false);
const recipe = {expectedFiles: Object.fromEntries([...names, 'asset-admission.bin'].map(name => [prefix + name, {}])),
  runtime: {worldResidency: true}, nativeTextures: {sha256: bundle},
  worldResidencyResources: {format: 1, levels: Object.fromEntries(Array.from({length: 7}, (_, i) => [i + 1, {}]))}};
assert.equal(worldResidencySettings(recipe).enabled, true);
for (const mutation of [r => {delete r.worldResidencyResources;}, r => {delete r.worldResidencyResources.levels['7'];},
  r => {r.nativeTextures.sha256 = 'invalid';}, r => {r.runtime.worldResidency = false;}]) {
  const bad = structuredClone(recipe); mutation(bad); assert.throws(() => worldResidencySettings(bad));
}
console.log('PASS: actual-output admission hashes, inventory, ordering, native identity and resource metadata fail closed.');

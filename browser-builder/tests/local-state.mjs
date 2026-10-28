import assert from 'node:assert/strict';
import {contentFingerprint, createCheckpoints} from '../checkpoints.mjs';
import {parseByteRange} from '../downloads.mjs';
import {executionProfile} from '../platform.mjs';
const data = new Map();
const storage = {
  read: async key => data.get(key) || null,
  readJSON: async key => data.has(key) ? JSON.parse(await data.get(key).text()) : null,
  writeJSON: async (key, value) => data.set(key, new Blob([JSON.stringify(value)])),
  clear: async () => data.clear(),
};
const first = new Blob(['abc']), changed = new Blob(['abd']);
assert.notEqual(await contentFingerprint(first), await contentFingerprint(changed));
let cache = await createCheckpoints(storage, 'one', 'recipe');
data.set('a.bin', first); await cache.commit('stage', new Map([['a.bin', first]]), {done: true});
cache = await createCheckpoints(storage, 'one', 'recipe');
assert.equal((await cache.read('stage')).metadata.done, true);
data.set('a.bin', changed); assert.equal(await cache.read('stage'), null);
assert.equal((await storage.readJSON('checkpoint.json')).stages.stage, undefined);
await createCheckpoints(storage, 'changed-source', 'recipe'); assert.equal(data.size, 0);
assert.deepEqual(parseByteRange(null, 100), {start: 0, end: 99, partial: false});
assert.deepEqual(parseByteRange('bytes=10-19', 100), {start: 10, end: 19, partial: true});
assert.deepEqual(parseByteRange('bytes=-5', 100), {start: 95, end: 99, partial: true});
assert.equal(parseByteRange('bytes=100-', 100), null);
assert.equal(parseByteRange('bytes=20-19', 100), null);
assert.equal(parseByteRange('bytes=0-9,20-30', 100), null);
assert.equal(parseByteRange('bytes=-0', 100), null);
assert.equal(executionProfile({cores: 8, memoryGiB: 8}).textureWorkers, 4);
assert.equal(executionProfile({cores: 8, mobile: true}).textureWorkers, 2);
assert.equal(executionProfile({cores: 8, memoryGiB: 2}).textureWorkers, 2);
assert.equal(executionProfile({cores: 1, mobile: true}).textureWorkers, 1);
console.log('PASS: content-bound resume rejects changed same-size bytes, invalidates changed recipes; local range export and device scheduling.');

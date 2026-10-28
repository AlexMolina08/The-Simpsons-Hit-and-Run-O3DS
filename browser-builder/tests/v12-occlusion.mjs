/** Exercise the real WASM baker; --bridge supplies binary requests to parity tests. */
import assert from 'node:assert/strict';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
const path = process.argv[2] || 'out/browser-tools/v12-occlusion/occlusion.mjs';
const {default: createOcclusion} = await import(pathToFileURL(resolve(path)).href);
const baker = await createOcclusion({print: message => process.stderr.write(`${message}\n`),
  printErr: message => process.stderr.write(`${message}\n`)});

function bake(request, sun) {
  const input = baker._malloc(request.length), count = new DataView(request.buffer, request.byteOffset, request.byteLength).getUint32(0, true);
  const output = baker._malloc(count);
  if (!input || !output) throw new Error('Not enough test memory.');
  try {
    baker.HEAPU8.set(request, input);
    const written = baker._v12_occlusion_bake(input, request.length, output, count, sun ? 1 : 0, ...(sun || [0, 1, 0]));
    if (written !== count) throw new Error('WASM baker rejected a parity request.');
    return baker.HEAPU8.slice(output, output + count);
  } finally { baker._free(input); baker._free(output); }
}

if (process.argv.includes('--bridge')) {
  const iterator = process.stdin[Symbol.asyncIterator]();
  let chunk = new Uint8Array(), position = 0;
  async function read(count) {
    const output = Buffer.allocUnsafe(count);
    let written = 0;
    while (written < count) {
      if (position === chunk.length) {
        const value = await iterator.next();
        if (value.done) { if (!written) return null; throw new Error('Truncated parity request.'); }
        chunk = value.value; position = 0;
      }
      const length = Math.min(count - written, chunk.length - position);
      output.set(chunk.subarray(position, position + length), written);
      written += length; position += length;
    }
    return output;
  }
  while (true) {
    const header = await read(20);
    if (!header) break;
    const request = await read(header.readUInt32LE(0));
    if (!request) throw new Error('Missing parity request.');
    const output = bake(request, header.readUInt32LE(4) ? [8, 12, 16].map(at => header.readFloatLE(at)) : null);
    const prefix = Buffer.alloc(4); prefix.writeUInt32LE(output.length);
    await new Promise((done, reject) => process.stdout.write(Buffer.concat([prefix, output]), error => error ? reject(error) : done()));
  }
} else {
  function request(height) {
    const vertices = [[0, 0, 0, 0, 1, 0], [-3, height, -3, 0, 1, 0], [3, height, -3, 0, 1, 0],
      [3, height, 3, 0, 1, 0], [-3, height, 3, 0, 1, 0]];
    const data = new Uint8Array(16 + vertices.length * 24 + 24), view = new DataView(data.buffer);
    [vertices.length, 2, 8].forEach((value, index) => view.setUint32(index * 4, value, true));
    view.setFloat32(12, 80, true);
    vertices.flat().forEach((value, index) => view.setFloat32(16 + index * 4, value, true));
    [1, 2, 3, 1, 3, 4].forEach((value, index) => view.setUint32(16 + vertices.length * 24 + index * 4, value, true));
    return data;
  }
  for (const [height, expected] of [[1, 0], [-1, 255], [100, 255]]) assert.equal(bake(request(height), [0, 1, 0])[0], expected);
  const data = request(1), input = baker._malloc(data.length), output = baker._malloc(5);
  baker.HEAPU8.set(data, input);
  const handle = baker._v12_occlusion_create(input, data.length, 1, 0, 1, 0);
  try {
    assert.ok(handle); assert.equal(baker._v12_occlusion_count(handle), 5);
    assert.equal(baker._v12_occlusion_sample(handle, 0, 1, output, 1), 1);
    assert.equal(baker._v12_occlusion_sample(handle, 1, 4, output + 1, 4), 4);
    assert.deepEqual(baker.HEAPU8.slice(output, output + 5), bake(data, [0, 1, 0]));
    assert.equal(baker._v12_occlusion_sample(handle, 5, 1, output, 1), -1);
    assert.equal(baker._v12_occlusion_sample(handle, 0, 5, output, 4), -1);
    assert.equal(baker._v12_occlusion_create(input, data.length - 1, 1, 0, 1, 0), 0);
    assert.equal(baker._v12_occlusion_create(input, data.length, 1, 0, 0, 0), 0);
  } finally { baker._v12_occlusion_destroy(handle); baker._free(input); baker._free(output); }
  // Query an exact duplicate before its earlier representative. Batched callers
  // need identical results even when they do not sample vertices in index order.
  const repeated = new Uint8Array(data.length + 24), repeatedView = new DataView(repeated.buffer);
  repeated.set(data.subarray(0, 16 + 5 * 24));
  repeated.set(data.subarray(16, 40), 16 + 5 * 24);
  repeated.set(data.subarray(16 + 5 * 24), 16 + 6 * 24);
  repeatedView.setUint32(0, 6, true);
  const repeatedInput = baker._malloc(repeated.length), repeatedOutput = baker._malloc(2);
  baker.HEAPU8.set(repeated, repeatedInput);
  const repeatedHandle = baker._v12_occlusion_create(repeatedInput, repeated.length, 0, 0, 1, 0);
  try {
    assert.ok(repeatedHandle);
    assert.equal(baker._v12_occlusion_sample(repeatedHandle, 5, 1, repeatedOutput, 1), 1);
    assert.equal(baker._v12_occlusion_sample(repeatedHandle, 0, 1, repeatedOutput + 1, 1), 1);
    assert.equal(baker.HEAPU8[repeatedOutput], baker.HEAPU8[repeatedOutput + 1]);
  } finally { baker._v12_occlusion_destroy(repeatedHandle); baker._free(repeatedInput); baker._free(repeatedOutput); }
  console.log('PASS: production WASM ray baking, sunlight range, batch parity, invalid bounds and helper lifecycle.');
}

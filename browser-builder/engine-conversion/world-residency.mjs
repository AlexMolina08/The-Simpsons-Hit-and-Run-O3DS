/** Keep the browser's prepared subtree and compiled runtime contract together. */
export function worldResidencySettings(recipe) {
  const files = Object.keys(recipe.expectedFiles).filter(path => path.startsWith('world-residency/'));
  const enabled = files.length !== 0;
  if (enabled !== (recipe.runtime?.worldResidency === true)) throw new Error('The game and map settings come from different builder versions. Reload the page and retry.');
  if (!enabled) return {enabled: false, nativeBundleSHA256: null};
  const required = ['asset-admission.bin', 'landmark-visibility.bin',
    ...Array.from({length: 7}, (_, i) => `resident-visibility${i + 1}.bin`)];
  if (files.length !== 88 || required.some(name => !files.includes(`world-residency/${name}`))) {
    throw new Error('The builder is missing data for one or more maps. Reload the page and retry.');
  }
  const nativeBundleSHA256 = recipe.nativeTextures?.sha256;
  if (!/^[0-9a-f]{64}$/.test(nativeBundleSHA256 || '')) throw new Error('The builder is missing texture validation data. Reload the page and retry.');
  const resources = recipe.worldResidencyResources;
  if (resources?.format !== 1 || !resources.levels || Object.keys(resources.levels).sort().join('') !== '1234567'
      || Object.values(resources.levels).some(level => !level || typeof level !== 'object' || Array.isArray(level))) {
    throw new Error('The builder is missing map resource validation data. Reload the page and retry.');
  }
  return {enabled: true, nativeBundleSHA256};
}

const hex = bytes => [...new Uint8Array(bytes)].map(value => value.toString(16).padStart(2, '0')).join('');
const sha256 = async bytes => hex(await crypto.subtle.digest('SHA-256', bytes));

/** Bind admission to the actual, individually accepted browser output bytes. */
export async function validateResidencyAdmission(bytes, files, nativeBundleSHA256) {
  const prefix = 'world-residency/', count = 87, stride = 84, header = 80;
  const fail = () => { throw new Error('The complete map inventory failed validation.'); };
  if (!(bytes instanceof Uint8Array) || bytes.length !== header + count * stride) fail();
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (String.fromCharCode(...bytes.subarray(0, 4)) !== 'WRA1' || view.getUint32(4, true) !== 1
      || view.getUint32(8, true) !== count || view.getUint32(12, true) !== stride
      || hex(bytes.subarray(16, 48)) !== nativeBundleSHA256
      || hex(bytes.subarray(48, 80)) !== await sha256(bytes.subarray(header))) fail();
  const accepted = new Map([...files].filter(([path]) => path.startsWith(prefix)
    && path !== prefix + 'asset-admission.bin').map(([path, file]) => [path.slice(prefix.length), file]));
  if (accepted.size !== count) fail();
  const names = [...accepted.keys()].sort();
  for (let index = 0; index < count; index++) {
    const offset = header + index * stride, field = bytes.subarray(offset, offset + 48), end = field.indexOf(0);
    if (end < 1 || field.subarray(end).some(value => value !== 0) || field.subarray(0, end).some(value => value > 127)) fail();
    const name = String.fromCharCode(...field.subarray(0, end)), file = accepted.get(name);
    if (name !== names[index] || !file || file.size !== view.getUint32(offset + 48, true)
        || hex(bytes.subarray(offset + 52, offset + stride)) !== await sha256(await file.arrayBuffer())) fail();
  }
}

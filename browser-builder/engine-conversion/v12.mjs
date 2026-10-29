/** Accepted v1.2 visual and audio preparation. Only user-selected inputs enter Python. */
import createOcclusion from '../vendor/occlusion.mjs';
import createAudioEncoder from '../vendor/v12-audio.mjs';
import {buildAudioCache} from './audio-cache.mjs';

export const VISUAL_PYTHON_MODULES = Object.freeze([
  'prepare_character_detail.py', 'strip_character_detail.py', 'prepare_character_occlusion.py',
  'prepare_vehicle_reflections.py', 'prepare_scene_occlusion.py', 'prepare_world_sun.py',
  'pack_world_sun.py', 'prepare_sprite_atlases.py', 'prepare_landmark_visibility.py',
  'prepare_streaming_order.py',
]);
const text = new TextDecoder();
const hex = value => [...new Uint8Array(value)].map(byte => byte.toString(16).padStart(2, '0')).join('');
function copyPython(value) { return value?.toJs ? value.toJs({create_pyproxies: false}) : value; }

export async function createRayBaker(progress = () => {}) {
  const module = await createOcclusion();
  return (request, sunlight) => {
    const bytes = copyPython(request), sun = copyPython(sunlight);
    const input = module._malloc(bytes.length);
    if (!input) throw new Error('Not enough memory to prepare this map. Close other tabs and retry.');
    let handle = 0, output = 0;
    try {
      module.HEAPU8.set(bytes, input);
      handle = module._v12_occlusion_create(input, bytes.length, sun ? 1 : 0, ...(sun || [0, 0, 0]));
      if (!handle) throw new Error('The map contains unsupported lighting geometry.');
      const count = module._v12_occlusion_count(handle), batch = Math.min(count, 8192);
      output = module._malloc(batch);
      if (!output) throw new Error('Not enough memory to prepare map lighting.');
      const result = new Uint8Array(count);
      for (let begin = 0; begin < count; begin += batch) {
        const length = Math.min(batch, count - begin);
        if (module._v12_occlusion_sample(handle, begin, length, output, batch) !== length) throw new Error('Map lighting returned incomplete output.');
        result.set(module.HEAPU8.subarray(output, output + length), begin);
        if (count >= 100000) progress({stage: 'visuals', detail: sunlight ? 'Tracing sunlight' : 'Tracing contact shading', completed: begin + length, total: count});
      }
      return result;
    } finally { if (output) module._free(output); if (handle) module._v12_occlusion_destroy(handle); module._free(input); }
  };
}

async function stageInputs(pyodide, inputs, canonicalNames) {
  await pyodide.runPythonAsync("import shutil\nshutil.rmtree('/input', ignore_errors=True)\nshutil.rmtree('/prepared', ignore_errors=True)");
  pyodide.FS.mkdirTree('/input/art');
  for (const [name, file] of inputs) {
    const canonical = canonicalNames.get(name.toLowerCase()) || name.split('/').map((part, i, parts) =>
      i < parts.length - 1 ? part.toLowerCase() : part.replace(/\.p3d$/i, '.p3d')).join('/');
    const path = `/input/${canonical}`;
    pyodide.FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
    pyodide.FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
  }
}

export async function prepareV12Assets({files, pyodide, recipe, storage, checkpoints, rounding, progress = () => {}}) {
  const source = new Map(files), all = new Map(), landmarks = [], stages = [];
  const canonicalNames = new Map(Object.keys(recipe.knownCopy.sourceFiles).map(path => [path.toLowerCase(), path]));
  const baker = await createRayBaker(progress);
  const code = await (await fetch('./engine-conversion/v12-assets.py')).text();
  await pyodide.runPythonAsync(code);
  // Kept in an independent module so helper globals never collide with Python
  // parser globals. Both native recipe generation and browser validation share it.
  const semantics = await (await fetch('./engine-conversion/asset-semantics.py')).text();
  await pyodide.runPythonAsync(semantics);
  const prepare = pyodide.globals.get('prepare_stage'), semantic = pyodide.globals.get('semantic_fingerprint');
  const expected = new Map(Object.entries(recipe.expectedFiles).map(([path, item]) => [path.toLowerCase(), item]));
  const plans = [
    ['cars', ([path]) => /^art\/cars\/[^/]+\.p3d$/i.test(path)],
    ['characters', ([path]) => /^art\/chars\/[^/]+_m\.p3d$/i.test(path)],
    ['hud', ([path]) => path.toLowerCase() === 'art/frontend/scrooby/ingame.p3d'],
    ...Array.from({length: 7}, (_, i) => [`world-${i + 1}`, ([path]) => new RegExp(`^art/l${i + 1}[^/]*\\.p3d$`, 'i').test(path)]),
    ['landmarks', () => false],
  ];
  try {
    for (let index = 0; index < plans.length; index++) {
      const [name, match] = plans[index];
      progress({stage: 'visuals', detail: name.startsWith('world') ? `Preparing map ${name.split('-')[1]} of 7` : `Preparing ${name}`, completed: index, total: plans.length});
      const cached = await checkpoints.read(`v12-${name}`);
      if (cached) {
        for (const entry of cached.files) all.set(...entry);
        if (cached.metadata.landmarks) landmarks.push(cached.metadata.landmarks);
        stages.push(cached.metadata); continue;
      }
      await stageInputs(pyodide, [...source].filter(match), canonicalNames);
      const level = name.startsWith('world-') ? name.split('-')[1] : null;
      const result = prepare.callKwargs(name.startsWith('world') ? 'world' : name, '/input', '/prepared', {
        level, rounding: rounding ? JSON.stringify(rounding) : null, occlusion: baker, landmark_levels: name === 'landmarks' ? JSON.stringify(landmarks) : null,
        progress: (stage, detail, completed, total) => progress({stage: 'visuals', detail, completed, total}),
      });
      let metadata;
      try { metadata = result.toJs({dict_converter: Object.fromEntries, create_pyproxies: false}); } finally { result.destroy(); }
      const produced = new Map();
      for (const item of metadata.files) {
        const bytes = pyodide.FS.readFile(`/prepared/${item.path}`), accepted = expected.get(item.path.toLowerCase());
        if (!accepted) {
          const original = [...source].find(([path]) => path.toLowerCase() === item.path.toLowerCase())?.[1];
          const originalHash = original && hex(await crypto.subtle.digest('SHA-256', await original.arrayBuffer()));
          if (originalHash !== item.sha256) throw new Error(`The visual preparation produced an unaccepted change: ${item.path}.`);
          continue;
        }
        if (item.sha256 !== accepted.sha256) {
          const argument = pyodide.toPy(bytes);
          let digest;
          try { digest = semantic(argument); } finally { argument.destroy(); }
          if (!accepted.semantic_sha256 || digest !== accepted.semantic_sha256) throw new Error(`The prepared v1.2 artwork differs from the accepted release: ${item.path}.`);
          item.losslessPNGContainerDifference = true;
        }
        produced.set(item.path, await storage.write(item.path, new Blob([bytes])));
      }
      await checkpoints.commit(`v12-${name}`, produced, metadata);
      for (const entry of produced) all.set(...entry);
      if (metadata.landmarks) landmarks.push(metadata.landmarks);
      stages.push(metadata);
      await pyodide.runPythonAsync("import shutil, gc\nshutil.rmtree('/input', ignore_errors=True)\nshutil.rmtree('/prepared', ignore_errors=True)\ngc.collect()");
    }
  } finally { prepare.destroy(); semantic.destroy(); }
  return {files: all, stages};
}

export async function prepareV12Audio({files, recipe, storage, checkpoints, progress = () => {}}) {
  const cached = await checkpoints.read('v12-audio');
  if (cached) return cached;
  const module = await createAudioEncoder();
  const encode = (pcm, samples) => {
    const pointer = module._malloc(pcm.length);
    if (!pointer) throw new Error('Not enough memory to prepare audio.');
    try {
      module.HEAPU8.set(pcm, pointer);
      const bytes = module._encode_audio_cache(pointer, samples);
      if (bytes < 0) throw new Error('The audio encoder rejected a clip.');
      return module.HEAPU8.slice(pointer, pointer + bytes);
    } finally { module._free(pointer); }
  };
  const result = await buildAudioCache(files, encode, progress);
  if (result.sha256 !== recipe.audioCache.sha256 || result.clips !== 38) throw new Error('The prepared audio differs from the accepted v1.2 cache.');
  const path = 'sound/3ds-dsp-clips.bin';
  const produced = new Map([[path, await storage.write(path, result.blob)]]);
  const metadata = {sha256: result.sha256, clips: result.clips};
  await checkpoints.commit('v12-audio', produced, metadata);
  return {files: produced, metadata};
}

import {prepareGameAssets, loadTextureParser, PYTHON_MODULES, validateInput, validateDialogue, validateTextBible, normalizeInputFiles} from './engine-conversion/conversion.mjs';
import {VISUAL_PYTHON_MODULES, prepareV12Assets, prepareV12Audio} from './engine-conversion/v12.mjs';
import {createTools, packageCIA} from './tools.mjs';
import {createBuildStorage} from './opfs.mjs';
import {createCheckpoints, inputFingerprint} from './checkpoints.mjs';
import {readISO, detectISO} from './engine-conversion/iso.mjs';
import {languageProfile} from './engine-conversion/languages.mjs';

const progress = event => postMessage({type: 'progress', ...event});
async function fetchChecked(path, expected) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`A build tool could not be loaded (${response.status}). Reload and try again.`);
  const value = new Uint8Array(await response.arrayBuffer());
  if (expected) {
    const hash = [...new Uint8Array(await crypto.subtle.digest('SHA-256', value))].map(v => v.toString(16).padStart(2, '0')).join('');
    if (hash !== expected) throw new Error('A build tool failed its integrity check. No installer was produced.');
  }
  return value;
}

export async function loadPython({visuals = false} = {}) {
  const {loadPyodide} = await import('./vendor/pyodide/pyodide.mjs');
  const pyodide = await loadPyodide({indexURL: new URL('./vendor/pyodide/', import.meta.url).href});
  await pyodide.loadPackage(visuals ? ['pillow', 'numpy'] : ['pillow']);
  pyodide.FS.mkdirTree('/app/tools/3ds');
  const modules = [...new Set([...PYTHON_MODULES, ...(visuals ? VISUAL_PYTHON_MODULES : [])])];
  for (const name of modules) pyodide.FS.writeFile(`/app/tools/3ds/${name}`, await fetchChecked(`./python/${name}`));
  await pyodide.runPythonAsync("import sys\nsys.path.insert(0, '/app/tools/3ds')\nfrom prepare_home_artwork import render_artwork");
  if (!visuals) await loadTextureParser(pyodide, new TextDecoder().decode(await fetchChecked('./engine-conversion/texture-extract.py')));
  return pyodide;
}

function mergeFiles(target, overrides) {
  const names = new Map([...target.keys()].map(path => [path.toLowerCase(), path]));
  for (const [path, file] of overrides) {
    const previous = names.get(path.toLowerCase());
    if (previous && previous !== path) target.delete(previous);
    target.set(path, file); names.set(path.toLowerCase(), path);
  }
}
function runtimeInputs(files, language) {
  const selected = languageProfile(language);
  return new Map([...files].filter(([path]) => {
    const key = path.toLowerCase();
    if (/^dialog[fgs]?\.rcf$/.test(key)) return key === selected.archive;
    return path.split('/').at(-1).toLowerCase() !== 'temp.err' &&
      (/^(art|scripts|sound)\//.test(key) || (!key.includes('/') && key.endsWith('.rcf')));
  }));
}
function completePhase(context, phase, started, extras = {}) {
  context.timings ||= {};
  context.timings[phase] = (performance.now() - started) / 1000;
  postMessage({type: 'phase', phase, context, ...extras});
}

onmessage = async ({data}) => {
  if (!['build', 'inspect', 'textures', 'visuals', 'media', 'package'].includes(data.type)) return;
  const started = performance.now(), phase = data.type === 'build' ? 'inspect' : data.type;
  let storage;
  try {
    storage = await createBuildStorage();
    if (!navigator.storage?.getDirectory) throw new Error('This browser cannot create local build storage. Update your browser and try again.');
    if (phase === 'inspect') {
      progress({stage: 'inspect', detail: 'Checking your game and available space'});
      const manifest = JSON.parse(new TextDecoder().decode(await fetchChecked('./build-manifest.json')));
      if (manifest.version !== '1.2' || manifest.packedVersion !== 1056) throw new Error('This website does not contain the accepted v1.2 builder.');
      let files = data.files ? normalizeInputFiles(data.files) : null;
      const estimate = await navigator.storage.estimate();
      const sourceBytes = data.source ? [...data.source.inputs.values()].reduce((sum, file) => sum + file.size, 0) : data.iso?.size;
      const needed = Math.max(8 * 1024 ** 3, sourceBytes ? sourceBytes * 3 : validateInput(files, data.language).inputBytes * 3);
      // Existing checkpoint data already uses this origin's quota and is reusable.
      if (Number.isFinite(estimate.quota) && estimate.quota < needed) throw new Error('The browser has too little temporary storage. Free space on this device and try again.');
      progress({stage: 'inspect', detail: 'Identifying your local copy for safe resume'});
      const layout = data.source?.layout || (data.iso ? await detectISO(await readISO(data.iso)) : null);
      const language = data.language || layout?.languages?.[0] || (files ? validateInput(files).language : 'en');
      languageProfile(language);
      const originalIdentity = await inputFingerprint(data.source?.inputs || data.iso || files, (bytes, total) => progress({stage: 'inspect', detail: 'Checking local game files', bytes, completed: bytes, total}));
      const identity = `${originalIdentity}:${language}`;
      const checkpoints = await createCheckpoints(storage, identity, manifest.buildID || manifest.v12RecipeSHA256);
      if (layout) {
        progress({stage: 'extract', detail: 'Reading disc image'});
        if (layout.kind === 'installed') files = layout.files;
        else if (layout.kind === 'inno-freearc' || layout.kind === 'installshield') {
          const cabinet = layout.kind === 'installshield' ? await import('./engine-conversion/installshield.mjs') : null;
          const cached = await checkpoints.read('extract');
          if (cached) {
            files = new Map(cached.metadata.members.map(([path, item]) => {
              const parent = cached.files.get(item.container);
              if (!parent || item.offset < 0 || item.offset + item.size > parent.size) throw new Error('An extraction checkpoint is incomplete.');
              return [path, parent.slice(item.offset, item.offset + item.size)];
            }));
            if (cabinet) files = new Map([...cabinet.looseGameFiles(layout), ...files]);
            progress({stage: 'extract', detail: 'Reusing verified extracted game files'});
          }
          else {
            const extract = cabinet ? cabinet.extractInstallShield : (await import('./engine-conversion/repack.mjs')).extractRepack;
            const members = new Map();
            files = await extract(layout, {storage, language, onProgress: progress, onFile: (path, item) => members.set(path, item)});
            const containers = new Map();
            for (const item of members.values()) {
              if (!item.container) throw new Error('The disc extractor did not persist its output.');
              if (!containers.has(item.container)) containers.set(item.container, await storage.read(item.container));
            }
            await checkpoints.commit('extract', containers, {members: [...members]});
          }
        } else throw new Error('This PC image uses an unsupported installer. Select its installed game folder instead.');
      }
      // Disc extraction returns its own path map. Normalize it before keeping
      // those paths for the preparation and packaging phases as well.
      files = normalizeInputFiles(files);
      const summary = validateInput(files, language);
      summary.text = await validateTextBible(files, language);
      const recipe = JSON.parse(new TextDecoder().decode(await fetchChecked('./v12-recipe.json', manifest.v12RecipeSHA256)));
      const dialogue = [...files].find(([path]) => path.toLowerCase() === languageProfile(language).archive)?.[1];
      summary.dialogue = await validateDialogue(dialogue, recipe.dialogueKeys?.[language]);
      const context = {files, identity, language, manifest, execution: data.execution,
        prepared: {files: runtimeInputs(files, language), homeMenu: {}, report: summary}};
      completePhase(context, phase, started); return;
    }
    const context = data.context || {prepared: data.prepared, manifest: data.manifest};
    context.language ||= context.prepared?.report?.language || 'en';
    const {manifest} = context;
    const checkpoints = context.identity ? await createCheckpoints(storage, context.identity, manifest.buildID || manifest.v12RecipeSHA256) : null;
    if (phase === 'textures') {
      progress({stage: 'textures', detail: 'Preparing textures and HOME Menu artwork'});
      const profile = JSON.parse(new TextDecoder().decode(await fetchChecked('./native_texture_profile.json', manifest.textureProfileSHA256)));
      const pyodide = await loadPython();
      const tools = await createTools(progress, context.execution);
      context.prepared = await prepareGameAssets({files: context.files, tools, pyodide, profile,
        writeOutput: (path, value) => storage.write(path, value), onProgress: progress,
        phase: 'textures', previous: context.prepared, checkpoints, language: context.language});
      context.prepared.report.textureHeapBytes = pyodide._module?.HEAPU8?.length || null;
      tools.releaseConverters();
    } else if (phase === 'visuals') {
      progress({stage: 'visuals', detail: 'Loading the v1.2 visual preparation tools'});
      const recipe = JSON.parse(new TextDecoder().decode(await fetchChecked('./v12-recipe.json', manifest.v12RecipeSHA256)));
      const rounding = JSON.parse(new TextDecoder().decode(await fetchChecked('./engine-conversion/v12-rounding.json', manifest.v12RoundingsSHA256)));
      const pyodide = await loadPython({visuals: true});
      const prepared = await prepareV12Assets({files: context.files, pyodide, recipe, rounding, storage, checkpoints, progress, language: context.language});
      mergeFiles(context.prepared.files, prepared.files);
      const audio = await prepareV12Audio({files: context.files, recipe, storage, checkpoints, progress});
      mergeFiles(context.prepared.files, audio.files);
      // Required enhancements must come from the verified preparation stages;
      // an original PC file with the same path cannot satisfy this check.
      const present = new Set([...prepared.files.keys(), ...audio.files.keys()].map(path => path.toLowerCase()));
      for (const [path, record] of Object.entries(recipe.requiredOverrides)) {
        if (record.required !== false && !present.has(path.toLowerCase())) throw new Error(`The v1.2 preparation is missing ${path}.`);
      }
      context.prepared.report.visuals = {features: 255, stages: prepared.stages, audio: audio.metadata,
        pythonHeapBytes: pyodide._module?.HEAPU8?.length || null};
    } else if (phase === 'media') {
      progress({stage: 'media', detail: 'Preparing voices and movies'});
      const tools = await createTools(progress, context.execution);
      context.prepared = await prepareGameAssets({files: context.files, tools,
        writeOutput: (path, value) => storage.write(path, value), onProgress: progress,
        phase: 'media', previous: context.prepared, checkpoints, language: context.language});
      tools.releaseConverters();
    } else if (phase === 'package') {
      const {prepared} = context;
      // Generated overrides can introduce a second spelling of an existing
      // directory. Unify it before writing the case-sensitive RomFS tree.
      prepared.files = normalizeInputFiles(prepared.files);
      const runtime = data.runtime || await fetchChecked('./runtime.elf', manifest.runtimeSHA256);
      const rsf = data.rsf || new TextDecoder().decode(await fetchChecked('./game.rsf', manifest.rsfSHA256));
      let packageHeapBytes;
      const cia = await packageCIA({assets: prepared.files, homeMenu: prepared.homeMenu, runtime, rsf, storage,
        packedVersion: manifest.packedVersion,
        onProgress: event => { if (event.heapBytes) packageHeapBytes = event.heapBytes; progress(event); }});
      const header = new DataView(await cia.slice(0, 0x2020).arrayBuffer());
      if (header.getUint32(0, true) !== 0x2020 || cia.size < manifest.minimumCIABytes) throw new Error('The generated installer failed its container check.');
      const report = {...prepared.report, version: manifest.version, titleID: manifest.titleID, ciaBytes: cia.size,
        runtimeSHA256: manifest.runtimeSHA256, sourceCommit: manifest.sourceCommit, packageHeapBytes,
        execution: context.execution, timings: {...context.timings, package: (performance.now() - started) / 1000},
        validation: 'Local asset preparation and container checks; console installation and gameplay are separate validation.'};
      const filename = `Simpsons-Hit-and-Run-3DS-v${manifest.version}${context.language === 'en' ? '' : `-${context.language}`}.cia`;
      const localToken = [...crypto.getRandomValues(new Uint8Array(16))].map(value => value.toString(16).padStart(2, '0')).join('');
      await storage.writeJSON('build-report.json', report);
      await storage.writeJSON('completed.json', {filename, bytes: cia.size, localToken, version: manifest.version, runtimeSHA256: manifest.runtimeSHA256, buildID: manifest.buildID, identity: context.identity});
      postMessage({type: 'complete', file: cia, filename, localToken, report}); return;
    }
    completePhase(context, phase, started);
  } catch (error) {
    postMessage({type: 'error', message: error.message || String(error), name: error.name});
  }
};

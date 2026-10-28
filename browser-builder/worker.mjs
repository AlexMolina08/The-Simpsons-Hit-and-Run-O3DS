import {prepareGameAssets, loadTextureParser, PYTHON_MODULES, validateInput} from './engine-conversion/conversion.mjs';
import {createTools, packageCIA} from './tools.mjs';
import {createBuildStorage} from './opfs.mjs';
import {readISO, detectISO} from './engine-conversion/iso.mjs';

const progress = event => postMessage({type: 'progress', ...event});
async function fetchChecked(path, expected) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`A build tool could not be downloaded (${response.status}). Reload and try again.`);
  const value = new Uint8Array(await response.arrayBuffer());
  if (expected) {
    const hash = [...new Uint8Array(await crypto.subtle.digest('SHA-256', value))].map(v => v.toString(16).padStart(2, '0')).join('');
    if (hash !== expected) throw new Error('A build tool failed its integrity check. No installer was produced.');
  }
  return value;
}

export async function loadPython() {
  const {loadPyodide} = await import('./vendor/pyodide/pyodide.mjs');
  const pyodide = await loadPyodide({indexURL: new URL('./vendor/pyodide/', import.meta.url).href});
  await pyodide.loadPackage('pillow');
  pyodide.FS.mkdirTree('/app/tools/3ds');
  for (const name of PYTHON_MODULES) pyodide.FS.writeFile(`/app/tools/3ds/${name}`, await fetchChecked(`./python/${name}`));
  await pyodide.runPythonAsync("import sys\nsys.path.insert(0, '/app/tools/3ds')\nfrom prepare_home_artwork import render_artwork");
  const parser = new TextDecoder().decode(await fetchChecked('./engine-conversion/texture-extract.py'));
  await loadTextureParser(pyodide, parser);
  return pyodide;
}

onmessage = async ({data}) => {
  if (!['build', 'package'].includes(data.type)) return;
  let storage;
  try {
    if (data.type === 'package') {
      const {prepared, manifest, runtime, rsf} = data;
      storage = await createBuildStorage();
      const cia = await packageCIA({assets: prepared.files, homeMenu: prepared.homeMenu, runtime, rsf, storage, onProgress: progress});
      const header = new DataView(await cia.slice(0, 0x2020).arrayBuffer());
      if (header.getUint32(0, true) !== 0x2020 || cia.size < manifest.minimumCIABytes) throw new Error('The generated installer failed its container check.');
      const report = {...prepared.report, version: manifest.version, titleID: manifest.titleID, ciaBytes: cia.size,
        runtimeSHA256: manifest.runtimeSHA256, validation: 'Browser generation and container checks; installation and gameplay must be verified on the console.'};
      await storage.write('build-report.json', new Blob([JSON.stringify(report, null, 2)]));
      postMessage({type: 'complete', file: cia, filename: `Simpsons-Hit-and-Run-3DS-v${manifest.version}.cia`, report});
      return;
    }
    const summary = data.iso ? null : validateInput(data.files);
    progress({stage: 'tools', detail: 'Checking space and preparing build tools'});
    if (!navigator.storage?.getDirectory) throw new Error('This browser cannot create a large installer locally. Open this page in desktop Chrome or Edge.');
    const estimate = await navigator.storage.estimate();
    const needed = Math.max(8 * 1024 ** 3, (summary?.inputBytes || 0) * 2);
    if (estimate.quota - estimate.usage < needed) throw new Error('Not enough temporary browser storage. Free at least 8 GB on your computer and try again.');
    storage = await createBuildStorage();
    await storage.clear();
    let files = data.files;
    if (data.iso) {
      progress({stage: 'extract', detail: 'Reading disc image'});
      const layout = await detectISO(await readISO(data.iso));
      if (layout.kind === 'installed') files = layout.files;
      else if (layout.kind === 'inno-freearc') {
        const {extractRepack} = await import('./engine-conversion/repack.mjs');
        files = await extractRepack(layout, {storage, onProgress: progress});
      } else throw new Error('This disc image uses an unsupported installer.');
      validateInput(files);
    }
    const manifestResponse = await fetch('./build-manifest.json');
    if (!manifestResponse.ok) throw new Error('Build information is unavailable. Reload and try again.');
    const manifest = await manifestResponse.json();
    const profile = JSON.parse(new TextDecoder().decode(await fetchChecked('./native_texture_profile.json', manifest.textureProfileSHA256)));
    const runtime = await fetchChecked('./runtime.elf', manifest.runtimeSHA256);
    const rsf = new TextDecoder().decode(await fetchChecked('./game.rsf', manifest.rsfSHA256));
    const pyodide = await loadPython();
    const tools = await createTools(progress);
    const prepared = await prepareGameAssets({files, tools, pyodide, profile,
      writeOutput: (path, value) => storage.write(path, value), onProgress: progress});
    tools.releaseConverters();
    postMessage({type: 'prepared', prepared, manifest, runtime, rsf});
  } catch (error) {
    postMessage({type: 'error', message: error.message || String(error), name: error.name});
  }
};

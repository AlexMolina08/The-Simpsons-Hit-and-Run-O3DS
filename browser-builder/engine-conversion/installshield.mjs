/** Read retail CAB volumes locally; never execute the PC installer. */
import {mountDiskOutput} from '../opfs.mjs';

function canonical(path) {
  path = path.replaceAll('\\', '/');
  if (!path || path.startsWith('/') || path.split('/').some(part => !part || part === '.' || part === '..' || part.includes(':'))) {
    throw new Error('The disc contains an invalid game path.');
  }
  const parts = path.split('/');
  if (parts.length > 1) for (let index = 0; index < parts.length - 1; index++) parts[index] = parts[index].toLowerCase();
  return parts.join('/');
}

export function looseGameFiles(layout) {
  return new Map([...layout.files].filter(([path]) => /^(?:movies\/.*\.rmv|(?:dialog[fgs]?|music0[0-3]|soundfx|carsound|ambience|nis|scripts)\.rcf)$/i.test(path))
    .map(([path, file]) => [canonical(path), file]));
}

export async function extractInstallShield(layout, {storage, onProgress = () => {}, onFile = () => {}, signal, moduleFactory} = {}) {
  if (!storage) throw new Error('Temporary browser storage is required to unpack this disc.');
  const create = moduleFactory || (await import('../vendor/installshield.mjs')).default;
  const logs = [];
  const module = await create({printErr: value => { logs.push(value); if (logs.length > 8) logs.shift(); }});
  const volumes = [...layout.files].filter(([path]) => /(^|\/)data\d+\.(?:cab|hdr)$/i.test(path));
  module.FS.mkdir('/input'); module.FS.mkdir('/output');
  module.FS.mount(module.WORKERFS, {blobs: volumes.map(([path, data]) => ({name: path.split('/').at(-1).toLowerCase(), data}))}, '/input');
  const files = looseGameFiles(layout);
  try {
    if (!module.ccall('cabinet_open', 'number', ['string'], ['/input/data1.cab'])) throw new Error('The InstallShield header is missing or damaged.');
    const count = module._cabinet_count();
    if (!Number.isInteger(count) || count < 1 || count > 100000) throw new Error('The InstallShield file index is invalid.');
    let total = 0;
    for (let index = 0; index < count; index++) {
      if (signal?.aborted) throw new DOMException('Build cancelled.', 'AbortError');
      const source = module.ccall('cabinet_name', 'string', ['number'], [index]);
      if (!source) continue;
      const path = canonical(source);
      if (!/^(?:art\/|scripts\/|sound\/|movies\/|[^/]+\.rcf$)/i.test(path)) throw new Error('An unsupported game path was found in the installer.');
      const size = module._cabinet_size(index);
      total += size;
      if (!Number.isSafeInteger(size) || size < 0 || size > 2 * 1024 ** 3 || total > 8 * 1024 ** 3) throw new Error('The installer declares unsupported game file sizes.');
      const container = `extract/installed/${path}`;
      const destination = await storage.openSync(container);
      try {
        destination.handle.truncate(0);
        mountDiskOutput(module, '/output/data', destination.handle);
        if (!module.ccall('cabinet_save', 'number', ['number', 'string'], [index, '/output/data'])) {
          throw new Error(`Could not verify ${path}. Select all original disc images, or check this copy for damage. ${logs.at(-1) || ''}`);
        }
        destination.handle.flush();
        if (destination.handle.getSize() !== size) throw new Error(`The extracted ${path} is incomplete.`);
      } finally {
        destination.handle.close();
        try { module.FS.unlink('/output/data'); } catch {}
      }
      const file = await destination.file.getFile();
      files.set(path, file); onFile(path, {container, offset: 0, size});
      if (index % 50 === 0) onProgress({stage: 'extract', detail: 'Extracting and verifying PC game files', completed: index, total: count});
    }
    onProgress({stage: 'extract', detail: 'PC files extracted and verified', completed: count, total: count});
    return files;
  } finally { module._cabinet_close(); module.FS.unmount('/input'); }
}

/** Browser-only adapters for the pinned conversion and packaging tools. */
import createNativeTexture from './vendor/native-texture.mjs';
import createBannertool from './vendor/bannertool.mjs';
import createMakerom from './vendor/makerom.mjs';
import {mountDiskOutput} from './opfs.mjs';

function bytes(value) { return value instanceof Uint8Array ? value : new Uint8Array(value); }
function exitStatus(module, args) {
  try { return module.callMain(args) || 0; }
  catch (error) { if (error?.name === 'ExitStatus') return error.status; throw error; }
}
function requireSuccess(code, logs, tool) {
  if (code !== 0) throw new Error(`${tool} failed: ${logs.slice(-4).join(' ')}`);
}

export async function createTools(onProgress = () => {}) {
  let nativePromise, ffmpeg, stopped = false;
  const queue = [], encoders = [];
  const rejectJobs = error => {
    stopped = true;
    for (const slot of encoders) { slot.worker.terminate(); slot.job?.reject(error); slot.job = null; }
    for (const job of queue.splice(0)) job.reject(error);
  };
  const dispatch = () => {
    for (const slot of encoders) {
      if (slot.job || !queue.length) continue;
      slot.job = queue.shift();
      const {width, height, rgb} = slot.job.request;
      slot.worker.postMessage({width, height, rgb}, [rgb.buffer]);
    }
  };
  const startEncoders = () => {
    if (encoders.length) return;
    for (let index = 0; index < 4; index++) {
      const worker = new Worker(new URL('./texture-worker.mjs', import.meta.url), {type: 'module'});
      const slot = {worker, job: null};
      encoders.push(slot);
      worker.onmessage = ({data}) => {
        const job = slot.job;
        slot.job = null;
        if (data.error) { const error = new Error(data.error); job.reject(error); rejectJobs(error); }
        else { job.resolve(data.result); dispatch(); }
      };
      worker.onerror = event => rejectJobs(new Error(event.message || 'Texture conversion worker failed.'));
    }
  };
  const runFFmpeg = async ({inputs, args, outputs, allowFailure = false}) => {
    if (!ffmpeg) {
      onProgress({stage: 'tools', detail: 'Preparing the movie and sound converter'});
      const {default: createFFmpeg} = await import('./vendor/ffmpeg-core.js');
      ffmpeg = await createFFmpeg({locateFile: name => new URL(`./vendor/${name}`, import.meta.url).href});
    }
    const logs = [];
    ffmpeg.setLogger(({message}) => logs.push(message));
    try { ffmpeg.FS.mkdir('/sources'); } catch {}
    ffmpeg.FS.mount(ffmpeg.WORKERFS, {blobs: inputs.map(input => ({name: input.name,
      data: input.data instanceof Blob ? input.data : new Blob([input.data])}))}, '/sources');
    const inputNames = new Set(inputs.map(input => input.name));
    const mountedArgs = args.map((arg, index) => index > 0 && args[index - 1] === '-i' && inputNames.has(arg) ? `/sources/${arg}` : arg);
    let code;
    try {
      code = ffmpeg.exec('-y', ...mountedArgs);
      const files = new Map();
      for (const output of outputs) {
        try { files.set(output, ffmpeg.FS.readFile(`/${output}`).slice()); }
        catch (error) { if (!allowFailure) throw error; }
      }
      if (!allowFailure) requireSuccess(code, logs, 'Movie conversion');
      return {files, logs, exitCode: code};
    } finally {
      ffmpeg.reset();
      ffmpeg.FS.unmount('/sources');
      for (const name of outputs) {
        try { ffmpeg.FS.unlink(`/${name}`); } catch {}
      }
    }
  };
  return {
    textureConcurrency: 4,
    runFFmpeg,
    async convertTexture(request) {
      nativePromise ||= createNativeTexture();
      const native = await nativePromise;
      const input = bytes(request.data), source = native._malloc(input.length), target = native._malloc(request.bytes);
      if (!source || !target) throw new Error('Not enough browser memory to prepare a texture.');
      try {
        native.HEAPU8.set(input, source);
        const count = native._convert_texture(request.w, request.h, request.kind, request.dw, request.dh,
          request.format, request.levels, source, input.length, target, request.bytes);
        if (count < 0) throw new Error(`Unsupported texture data (${count}).`);
        return native.HEAPU8.slice(target, target + count);
      } finally { native._free(source); native._free(target); }
    },
    encodeEtc1({width, height, rgb}) {
      if (stopped) return Promise.reject(new Error('Texture conversion stopped.'));
      startEncoders();
      return new Promise((resolve, reject) => {
        queue.push({request: {width, height, rgb: rgb.slice()}, resolve, reject});
        dispatch();
      });
    },
    releaseConverters() { rejectJobs(new Error('Conversion finished.')); nativePromise = null; ffmpeg = null; },
  };
}

export async function packageCIA({assets, homeMenu, runtime, rsf, storage, onProgress = () => {}}) {
  const bannerLogs = [];
  const banner = await createBannertool({print: line => bannerLogs.push(line), printErr: line => bannerLogs.push(line)});
  banner.FS.writeFile('/banner.png', new Uint8Array(await homeMenu.banner.arrayBuffer()));
  banner.FS.writeFile('/icon.png', new Uint8Array(await homeMenu.icon.arrayBuffer()));
  banner.FS.writeFile('/banner.wav', new Uint8Array(await homeMenu.cue.arrayBuffer()));
  requireSuccess(exitStatus(banner, ['makebanner', '-i', '/banner.png', '-a', '/banner.wav', '-o', '/banner.bnr']), bannerLogs, 'HOME Menu banner');
  requireSuccess(exitStatus(banner, ['makesmdh', '-s', 'The Simpsons: Hit & Run', '-l', 'The Simpsons: Hit & Run — Nintendo 3DS',
    '-p', 'Hit & Run 3DS contributors', '-i', '/icon.png', '-r', 'regionfree', '-f', 'visible,recordusage', '-o', '/icon.smdh']), bannerLogs, 'HOME Menu icon');
  const bannerData = banner.FS.readFile('/banner.bnr').slice(), iconData = banner.FS.readFile('/icon.smdh').slice();
  const logs = [];
  const makerom = await createMakerom({print: line => logs.push(line), printErr: line => logs.push(line)});
  makerom.FS.mkdir('/game');
  makerom.FS.mount(makerom.WORKERFS, {blobs: [...assets].map(([name, data]) => ({name, data}))}, '/game');
  makerom.FS.writeFile('/runtime.elf', runtime);
  makerom.FS.writeFile('/game.rsf', rsf);
  makerom.FS.writeFile('/banner.bnr', bannerData);
  makerom.FS.writeFile('/icon.smdh', iconData);
  const output = await storage.openSync('installer.cia');
  const disk = mountDiskOutput(makerom, '/output/game.cia', output.handle, size => onProgress({stage: 'package', bytes: size, detail: 'Writing your installer'}));
  try {
    onProgress({stage: 'package', detail: 'Assembling your complete installer'});
    const code = exitStatus(makerom, ['-f', 'cia', '-o', '/output/game.cia', '-target', 't', '-rsf', '/game.rsf',
      '-DROMFS_ROOT=/game', '-elf', '/runtime.elf', '-icon', '/icon.smdh', '-banner', '/banner.bnr', '-ver', '1041']);
    requireSuccess(code, logs, 'Installer packaging');
    disk.flush();
    if (disk.size < 1024 * 1024) throw new Error('The installer is incomplete.');
  } finally {
    output.handle.close();
    makerom.FS.unmount('/game');
  }
  return output.file.getFile();
}

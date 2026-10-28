import {validateInput} from './engine-conversion/conversion.mjs';
import {readISO, detectISO} from './engine-conversion/iso.mjs';
import {createBuildStorage} from './opfs.mjs';
import {offerFileDownload} from './downloads.mjs';
import {browserProfile, keepScreenAwake} from './platform.mjs';
const $ = id => document.getElementById(id);
const phases = ['inspect', 'textures', 'visuals', 'media', 'package'];
const phaseNames = {inspect: 'Check game', textures: 'Prepare textures', visuals: 'Prepare v1.2 visuals', media: 'Convert sound and movies', package: 'Build CIA'};
let files, iso, worker, result, resultName, report, wakeLock;
let saving = false, clearing = false, selecting = false, starting = false, currentPhase, started = 0, elapsedTimer;
let restoring = true;
const formatSize = value => `${(value / 1024 ** 3).toFixed(2)} GB`;
const formatTime = seconds => `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, '0')}`;
function error(message) { $('error').textContent = message; $('error').hidden = !message; }
function busy() {
  const locked = !!worker || saving || clearing || selecting || starting || restoring;
  $('choose').disabled = locked; $('choose-iso').disabled = locked;
  $('build').disabled = locked || (!files && !iso);
  $('cancel').hidden = !worker;
  $('clear').disabled = locked; $('save').disabled = locked;
}
function markPhase(phase, complete = false) {
  const index = phases.indexOf(phase);
  for (const [i, name] of phases.entries()) {
    const row = $(`phase-${name}`);
    row.dataset.state = i < index || (complete && i === index) ? 'complete' : i === index ? 'active' : 'waiting';
    if (i === index && !complete) row.setAttribute('aria-current', 'step'); else row.removeAttribute('aria-current');
  }
  $('phase-name').textContent = complete && phase === 'package' ? 'Build complete' : phaseNames[phase];
}
async function stopWorker() {
  worker?.terminate(); worker = null;
  clearInterval(elapsedTimer); elapsedTimer = null;
  try { await wakeLock?.release(); } catch {}
  wakeLock = null; busy();
}
function showResult(file, filename, details) {
  result = file; resultName = filename; report = details;
  $('result-size').textContent = `${filename} · ${formatSize(file.size)}`;
  $('result').hidden = false;
  $('status').textContent = 'Your CIA is ready to save.';
}
function launchWorker(phase, context, selection) {
  currentPhase = phase; markPhase(phase);
  const instance = new Worker('./worker.mjs', {type: 'module'}); worker = instance; busy();
  instance.onmessage = async ({data}) => {
    if (worker !== instance) return;
    if (data.type === 'progress') {
      const count = data.total ? data.bytes ? ` (${formatSize(data.completed)}/${formatSize(data.total)})` : ` (${data.completed.toLocaleString()}/${data.total.toLocaleString()})` : data.bytes ? ` · ${formatSize(data.bytes)}` : '';
      $('status').textContent = `${data.detail || phaseNames[phase]}${count}`;
      if (data.total) { $('progress').max = data.total; $('progress').value = data.completed; }
      else $('progress').removeAttribute('value');
    } else if (data.type === 'phase') {
      instance.terminate(); worker = null;
      const next = phases[phases.indexOf(phase) + 1];
      if (next) launchWorker(next, data.context);
    } else if (data.type === 'complete') {
      showResult(data.file, data.filename, data.report);
      markPhase('package', true); $('progress').max = 1; $('progress').value = 1;
      await stopWorker();
    } else if (data.type === 'error') {
      error(data.name === 'QuotaExceededError'
        ? 'Temporary storage is full. Free space on this device, then select the same game to continue.' : data.message);
      $('status').textContent = 'Build stopped. Completed steps are kept so you can retry.';
      await stopWorker();
    }
  };
  instance.onerror = async event => {
    if (worker !== instance) return;
    error(event.message || 'The browser ran out of working memory. Close other tabs, then retry with the same game.');
    $('status').textContent = 'Build stopped. Completed steps are kept on this device.';
    await stopWorker();
  };
  instance.postMessage({type: phase, context, ...selection});
}

$('choose').onclick = () => $('folder').click();
$('choose-iso').onclick = () => $('iso').click();
$('iso').onchange = async () => {
  if (worker || saving || clearing || selecting || !$('iso').files.length) return;
  error(''); files = null; iso = null; selecting = true; busy();
  $('selection').textContent = 'Reading disc image…';
  try {
    const selected = $('iso').files[0];
    const layout = await detectISO(await readISO(selected));
    if (layout.kind === 'unknown') throw new Error('This PC image uses an unsupported installer. You can select its installed game folder instead.');
    if (layout.kind === 'installed') validateInput(layout.files);
    iso = selected;
    $('selection').textContent = `${selected.name} · ${formatSize(selected.size)}`;
    $('selection').classList.add('valid'); $('status').textContent = 'Ready. The builder will reuse verified completed steps for this copy.';
  } catch (reason) { $('selection').textContent = 'The image could not be opened.'; $('selection').classList.remove('valid'); error(reason.message); }
  finally { selecting = false; busy(); }
};
$('folder').onchange = () => {
  if (worker || saving || clearing || selecting) return;
  error(''); files = null; iso = null;
  try {
    const chosen = [...$('folder').files], summary = validateInput(chosen);
    files = chosen;
    $('selection').textContent = `${chosen[0].webkitRelativePath.split('/')[0]} · ${summary.fileCount.toLocaleString()} files`;
    $('selection').classList.add('valid'); $('status').textContent = 'Your PC copy is ready. Completed steps can be reused.';
  } catch (reason) { $('selection').textContent = 'This folder is not ready to build.'; $('selection').classList.remove('valid'); error(reason.message); }
  busy();
};
$('build').onclick = async () => {
  if ((!files && !iso) || worker || saving || clearing || selecting || starting || restoring) return;
  error(''); $('result').hidden = true;
  if (!navigator.storage?.getDirectory || !('Worker' in window) || !('WebAssembly' in window)) {
    error('This browser is missing local build support. Try current Chrome, Edge or Safari.'); return;
  }
  $('work-status').hidden = false; $('progress').removeAttribute('value');
  started = performance.now(); $('elapsed').textContent = '0:00';
  elapsedTimer = setInterval(() => { $('elapsed').textContent = formatTime(Math.floor((performance.now() - started) / 1000)); }, 1000);
  starting = true; busy();
  wakeLock = await keepScreenAwake();
  wakeLock?.addEventListener('release', () => { wakeLock = null; });
  try { await navigator.storage.persist?.(); } catch {}
  starting = false;
  launchWorker('inspect', null, {files, iso, execution: browserProfile()});
};
$('cancel').onclick = async () => {
  await stopWorker(); $('status').textContent = 'Build cancelled. Select the same copy to reuse completed steps.';
};
$('save').onclick = async () => {
  if (!result || worker || saving || clearing) return;
  saving = true; busy(); error('');
  try {
    if (typeof window.showSaveFilePicker === 'function') {
      const handle = await showSaveFilePicker({suggestedName: resultName, types: [{description: 'Nintendo 3DS installer', accept: {'application/octet-stream': ['.cia']}}]});
      const writable = await handle.createWritable();
      await result.stream().pipeTo(writable);
      $('status').textContent = 'CIA saved. It is ready to copy to your 3DS SD card.';
    } else {
      // Safari's download manager bypasses service-worker responses. A File
      // object URL lets the browser save OPFS data without a whole-file JS copy.
      offerFileDownload(result, resultName);
      $('status').textContent = 'Download started. Keep this tab open until the CIA is saved.';
    }
  } catch (reason) { if (reason.name !== 'AbortError') error(reason.message); }
  finally { saving = false; busy(); }
};
$('save-report').onclick = () => {
  if (report) offerFileDownload(new Blob([JSON.stringify(report, null, 2)], {type: 'application/json'}), 'Hit-Run-3DS-build-report.json');
};
$('clear').onclick = async () => {
  if (worker || saving || clearing) return;
  clearing = true; busy();
  try {
    const directory = await navigator.storage.getDirectory();
    await directory.removeEntry('hit-run-3ds-build', {recursive: true});
    result = null; report = null; $('result').hidden = true;
    $('status').textContent = 'Temporary build files cleared. Your original game files are unchanged.';
  } catch (reason) { if (reason.name !== 'NotFoundError') error(reason.message); }
  finally { clearing = false; busy(); }
};
addEventListener('visibilitychange', async () => { if (document.visibilityState === 'visible' && worker && !wakeLock) wakeLock = await keepScreenAwake(); });
addEventListener('beforeunload', event => { if (worker || saving || clearing) { event.preventDefault(); event.returnValue = ''; } });

async function restore() {
  try {
    if (navigator.storage?.getDirectory) {
      const storage = await createBuildStorage(), record = await storage.readJSON('completed.json');
      const manifest = await (await fetch('./build-manifest.json')).json();
      if (record?.version === manifest.version && record.runtimeSHA256 === manifest.runtimeSHA256 && record.buildID === manifest.buildID) {
        const file = await storage.read('installer.cia');
        if (file && file.size === record.bytes) showResult(file, record.filename, await storage.readJSON('build-report.json'));
      }
    }
  } finally { restoring = false; busy(); }
}
restore().catch(() => { restoring = false; busy(); });

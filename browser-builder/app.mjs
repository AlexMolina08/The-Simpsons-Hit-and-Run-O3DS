import {validateInput} from './engine-conversion/conversion.mjs';
import {readISO, detectISO} from './engine-conversion/iso.mjs';
const $ = id => document.getElementById(id);
let files, iso, worker, result, resultName, report, saving = false, clearing = false, selecting = false;
const formatSize = value => `${(value / 1024 ** 3).toFixed(2)} GB`;
function error(message) { $('error').textContent = message; $('error').hidden = !message; }
function busy(value) {
  const locked = value || saving || clearing || selecting;
  $('choose').disabled = locked; $('choose-iso').disabled = locked;
  $('build').disabled = locked || (!files && !iso);
  $('cancel').hidden = !value; $('progress').hidden = !value;
  $('clear').disabled = locked; $('save').disabled = locked;
}
function download(blob, filename) {
  const link = document.createElement('a'), url = URL.createObjectURL(blob);
  link.href = url; link.download = filename; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
$('choose').onclick = () => { if (!worker && !saving && !clearing && !selecting) $('folder').click(); };
$('choose-iso').onclick = () => { if (!worker && !saving && !clearing && !selecting) $('iso').click(); };
$('iso').onchange = async () => {
  if (worker || saving || clearing || selecting || !$('iso').files.length) return;
  error(''); result = null; $('result').hidden = true; files = null; iso = null;
  selecting = true; busy(false);
  $('selection').textContent = 'Reading disc image…';
  try {
    const selected = $('iso').files[0];
    const layout = await detectISO(await readISO(selected));
    if (layout.kind === 'unknown') throw new Error('This image uses an installer the builder cannot open yet. You can select an installed game folder instead.');
    if (layout.kind === 'installed') validateInput(layout.files);
    iso = selected;
    $('selection').textContent = `${selected.name} · ${formatSize(selected.size)}`;
    $('selection').classList.add('valid');
    $('status').textContent = 'Disc image selected. Build CIA to continue.';
  } catch (reason) {
    $('selection').textContent = 'The disc image could not be opened.';
    $('selection').classList.remove('valid'); error(reason.message);
  } finally { selecting = false; busy(false); }
};
$('folder').onchange = () => {
  if (worker || saving || clearing || selecting) return;
  error(''); result = null; $('result').hidden = true; files = null; iso = null;
  try {
    const chosen = [...$('folder').files];
    const summary = validateInput(chosen);
    files = chosen;
    $('selection').textContent = `${chosen[0].webkitRelativePath.split('/')[0]} · ${summary.fileCount.toLocaleString()} files · ${summary.movieCount} movies`;
    $('selection').classList.add('valid');
    $('status').textContent = 'Your PC game is ready. Build its installer below.';
  } catch (reason) {
    $('selection').textContent = 'This folder is not ready to build.';
    $('selection').classList.remove('valid'); error(reason.message);
  }
  $('build').disabled = !files;
};
function launchWorker(message) {
  worker = new Worker('./worker.mjs', {type: 'module'});
  worker.onmessage = ({data}) => {
      if (data.type === 'progress') {
        const amount = data.total ? ` (${data.completed}/${data.total})` : data.bytes ? ` (${formatSize(data.bytes)})` : '';
        $('status').textContent = `${data.detail || 'Building'}${amount}`;
        if (data.total) { $('progress').max = data.total; $('progress').value = data.completed; }
        else $('progress').removeAttribute('value');
      } else if (data.type === 'prepared') {
        // Release Python and media heaps before makerom allocates its large heap.
        worker.terminate(); worker = null;
        try {
          launchWorker({type: 'package', prepared: data.prepared, manifest: data.manifest, runtime: data.runtime, rsf: data.rsf});
        } catch (reason) {
          error(reason.message); $('status').textContent = 'The packaging worker could not start. Your original files were not changed.';
          worker?.terminate(); worker = null; busy(false);
        }
      } else if (data.type === 'complete') {
        result = data.file; resultName = data.filename; report = data.report;
        $('status').textContent = 'Build complete. Save your installer below.';
        $('result-size').textContent = `${resultName} · ${formatSize(result.size)}`;
        $('result').hidden = false; worker.terminate(); worker = null; busy(false);
      } else if (data.type === 'error') {
        error(data.message); $('status').textContent = 'Build stopped. Your original game files were not changed.';
        worker.terminate(); worker = null; busy(false);
      }
    };
    worker.onerror = event => {
      error(event.message || 'The browser could not finish this build. Free memory, reload, and try again.');
      $('status').textContent = 'Build stopped. Your original game files were not changed.';
      worker?.terminate(); worker = null; busy(false);
    };
  worker.postMessage(message);
}
$('build').onclick = async () => {
  if ((!files && !iso) || worker || saving || clearing || selecting) return;
  error(''); $('result').hidden = true; $('progress').removeAttribute('value'); busy(true);
  try {
    if (!navigator.storage?.getDirectory) throw new Error('Open this page in desktop Chrome or Edge to build a large installer locally.');
    if (!window.showSaveFilePicker) throw new Error('Open this page in desktop Chrome or Edge. This browser cannot save the large installer safely.');
    launchWorker({type: 'build', files, iso});
  } catch (reason) { error(reason.message); worker?.terminate(); worker = null; busy(false); }
};
$('cancel').onclick = () => {
  worker?.terminate(); worker = null; busy(false);
  $('status').textContent = 'Build cancelled. Your original game files were not changed.';
};
$('save').onclick = async () => {
  if (!result || worker || saving || clearing) return;
  saving = true; busy(false); error('');
  try {
    if (window.showSaveFilePicker) {
      const handle = await showSaveFilePicker({suggestedName: resultName, types: [{description: 'Nintendo 3DS installer', accept: {'application/octet-stream': ['.cia']}}]});
      const writable = await handle.createWritable();
      await result.stream().pipeTo(writable);
      $('status').textContent = 'Installer saved. Follow the SD-card steps below.';
    } else throw new Error('Open this page in desktop Chrome or Edge to save the installer.');
  } catch (reason) { if (reason.name !== 'AbortError') error(reason.message); }
  finally { saving = false; busy(false); }
};
$('save-report').onclick = () => { if (report) download(new Blob([JSON.stringify(report, null, 2)], {type: 'application/json'}), 'Hit-Run-3DS-build-report.json'); };
$('clear').onclick = async () => {
  if (worker || saving || clearing) return;
  clearing = true; busy(false);
  try {
    const directory = await navigator.storage.getDirectory();
    await directory.removeEntry('hit-run-3ds-build', {recursive: true});
    result = null; $('result').hidden = true; $('status').textContent = 'This builder’s temporary files have been cleared.';
  } catch (reason) { if (reason.name !== 'NotFoundError') error(reason.message); }
  finally { clearing = false; busy(false); }
};
addEventListener('beforeunload', event => { if (worker || saving || clearing) { event.preventDefault(); event.returnValue = ''; } });

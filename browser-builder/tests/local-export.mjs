import {prepareLocalDownload, offerFileDownload} from '../downloads.mjs';
const $ = id => document.getElementById(id);
const rootURL = new URL('../', import.meta.url);
let file, record, worker, localWorkerReady = false, running = false;
const log = message => { $('details').textContent += message + '\n'; };
const check = (condition, message) => { if (!condition) throw new Error(message); };
const equal = (actual, expected) => actual.length === expected.length && actual.every((value, index) => value === expected[index]);
const bytesOf = async response => new Uint8Array(await response.arrayBuffer());
function controls() {
  for (const name of ['small', 'large', 'restore']) $(name).disabled = running;
  $('checks').disabled = running || !record || !localWorkerReady;
  $('stream').disabled = running || !file || !localWorkerReady;
  $('blob').disabled = running || !file;
  $('verify').disabled = running || !record;
}
// The production helpers derive their scope from the page URL. Invoke them at
// the builder root without navigating, then restore the test-page address.
async function atBuilderRoot(action) {
  const original = location.href, state = history.state;
  history.replaceState(state, '', rootURL);
  try { return await action(); }
  finally { history.replaceState(state, '', original); }
}
function offer(streamed) {
  const original = location.href, state = history.state;
  history.replaceState(state, '', rootURL);
  try { offerFileDownload(file, record.filename, {localToken: record.localToken, streamed}); }
  finally { history.replaceState(state, '', original); }
  $('status').textContent = `Download started through ${streamed ? 'the local worker' : 'an OPFS File URL'}. Check Downloads or Files; saving is not verified yet.`;
}
const downloadURL = () => new URL(`__local-download/${encodeURIComponent(record.localToken)}/${encodeURIComponent(record.filename)}`, rootURL);
async function range(label, value, expected, start) {
  const response = await fetch(downloadURL(), {headers: {Range: value}, cache: 'no-store'});
  check(response.status === 206, `${label}: expected 206, got ${response.status}`);
  check(response.headers.get('X-Local-Only') === '1', `${label}: missing local-only response header`);
  check(response.headers.get('Content-Length') === String(expected.length), `${label}: wrong content length`);
  check(response.headers.get('Content-Range') === `bytes ${start}-${start + expected.length - 1}/${record.bytes}`, `${label}: wrong content range`);
  check(equal(await bytesOf(response), expected), `${label}: incorrect payload`);
  log(`PASS ${label}: ${value}, ${expected.length} bytes match`);
}
async function runChecks() {
  if (!record || !localWorkerReady || running) return;
  running = true; controls();
  try {
    $('status').textContent = 'Checking local HEAD and bounded byte ranges…';
    const head = await fetch(downloadURL(), {method: 'HEAD', cache: 'no-store'});
    check(head.status === 200, `HEAD returned ${head.status}`);
    check(head.headers.get('X-Local-Only') === '1', 'HEAD did not come from the local worker');
    check(head.headers.get('Content-Length') === String(record.bytes), 'HEAD length differs from the fixture');
    check(head.headers.get('Accept-Ranges') === 'bytes', 'Range support is missing');
    check((head.headers.get('Content-Disposition') || '').includes(record.filename), 'Download filename is missing');
    check((await head.arrayBuffer()).byteLength === 0, 'HEAD unexpectedly contains a body');
    log(`PASS HEAD: X-Local-Only=1, Content-Length=${record.bytes}, attachment filename matches`);
    await range('First sentinel', `bytes=0-${record.first.length - 1}`, record.first, 0);
    const middle = Math.floor(record.bytes / 2);
    await range('Middle zero padding', `bytes=${middle}-${middle + 63}`, new Uint8Array(64), middle);
    await range('Tail sentinel', `bytes=${record.bytes - record.tail.length}-`, record.tail, record.bytes - record.tail.length);
    await range('Suffix sentinel', `bytes=-${record.tail.length}`, record.tail, record.bytes - record.tail.length);
    const invalid = await fetch(downloadURL(), {headers: {Range: `bytes=${record.bytes}-`}, cache: 'no-store'});
    check(invalid.status === 416 && invalid.headers.get('Content-Range') === `bytes */${record.bytes}`, 'Unsatisfiable range was not rejected');
    log('PASS Invalid range: 416 with the correct complete length');
    const wrong = new URL(downloadURL());wrong.pathname = wrong.pathname.replace(record.localToken, 'invalid-token');
    check((await fetch(wrong, {method: 'HEAD', cache: 'no-store'})).status === 404, 'Wrong token was accepted');
    log('PASS Wrong token: 404');
    $('status').textContent = 'PASS: local response headers and sentinel ranges. Download and verify the saved file to test export.';
  } catch (error) { log(`FAIL ${error.message}`); $('status').textContent = `FAIL: ${error.message}`; }
  finally { running = false; controls(); }
}
function fixture(action, bytes) {
  if (running) return;
  running = true; controls(); $('verified').textContent = '';
  worker = new Worker(new URL('./local-export-worker.mjs', import.meta.url), {type: 'module'});
  const finish = () => { worker?.terminate(); worker = null; running = false; controls(); };
  worker.onmessage = ({data}) => {
    if (data.type === 'progress') { $('status').textContent = data.detail; return; }
    if (data.type === 'ready') {
      file = data.file; record = data.record;
      $('fixture').replaceChildren();
      for (const [name, value] of [['Filename', record.filename], ['Bytes', record.bytes.toLocaleString()], ['Size', `${(record.bytes / 1024 ** 3).toFixed(3)} GiB`]]) {
        const key = document.createElement('dt'), item = document.createElement('dd');key.textContent = name; item.textContent = value;$('fixture').append(key, item);
      }
      log(`PASS Fixture ready: ${record.bytes} bytes; first and tail sentinels recorded; no game files used`);
      $('status').textContent = 'Fixture ready. Run response checks, then test each download route.';
      finish();if (localWorkerReady) runChecks();
    } else if (data.type === 'error') { log(`FAIL ${data.message}`);$('status').textContent = data.message;finish(); }
  };
  worker.onerror = error => { log(`FAIL ${error.message}`);$('status').textContent = error.message;finish(); };
  worker.postMessage({type: action, bytes});
}
$('small').onclick = () => fixture('create', 8 * 1024 ** 2);
$('large').onclick = () => fixture('create', Math.floor(1.9 * 1024 ** 3));
$('restore').onclick = () => fixture('restore');
$('checks').onclick = runChecks;
$('stream').onclick = () => offer(true);
$('blob').onclick = () => offer(false);
$('verify').onchange = async () => {
  const selected = $('verify').files[0];if (!selected || !record) return;
  try {
    check(selected.size === record.bytes, `Saved size differs: ${selected.size} versus ${record.bytes}`);
    check(equal(await bytesOf(selected.slice(0, record.first.length)), record.first), 'Saved first sentinel differs');
    check(equal(await bytesOf(selected.slice(selected.size - record.tail.length)), record.tail), 'Saved tail sentinel differs');
    const middle = Math.floor(selected.size / 2);
    check(equal(await bytesOf(selected.slice(middle, middle + 64)), new Uint8Array(64)), 'Saved middle zero padding differs');
    $('verified').textContent = `PASS: selected saved file has ${selected.size.toLocaleString()} bytes and matching first, middle and tail checks.`;
    log(`PASS Saved-file length and sampled sentinels: ${selected.name}. This is sampled verification, not a full-file hash.`);
  } catch (error) { $('verified').textContent = `FAIL: ${error.message}`;log(`FAIL ${error.message}`); }
};
(async () => {
  log(`Browser: ${navigator.userAgent}`);
  log(`Secure context: ${isSecureContext}; OPFS: ${!!navigator.storage?.getDirectory}; picker: ${typeof showSaveFilePicker === 'function'}`);
  localWorkerReady = await atBuilderRoot(prepareLocalDownload);
  log(`Root local download worker controlling this page: ${localWorkerReady}`);
  $('status').textContent = localWorkerReady ? 'Ready. Create the small fixture first.' : 'Local worker unavailable. OPFS File download can still be tested.';
  controls();
})();

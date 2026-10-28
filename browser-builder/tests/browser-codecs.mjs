const $ = id => document.getElementById(id);
let files;
$('choose').onclick = () => $('folder').click();
$('folder').onchange = () => {
  files = new Map([...$('folder').files].map(file => [file.name, file]));
  $('status').textContent = `${files.size} fixture files selected.`;
  $('run').disabled = !files.has('sample_bink.rmv');
};
$('run').onclick = () => {
  $('run').disabled = true;
  const worker = new Worker('./qa-codecs-worker.mjs', {type: 'module'});
  worker.onmessage = ({data}) => {
    $('status').textContent = data.detail || JSON.stringify(data, null, 2);
    if (data.passed || data.error) worker.terminate();
  };
  worker.onerror = error => { $('status').textContent = `FAIL: ${error.message}`; worker.terminate(); };
  worker.postMessage({files});
};

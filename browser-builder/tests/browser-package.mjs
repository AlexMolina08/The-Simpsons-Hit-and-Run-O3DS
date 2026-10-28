const $ = id => document.getElementById(id);
let selected;
$('choose').onclick = () => $('folder').click();
$('folder').onchange = () => {
  selected = new Map([...$('folder').files].map(file => [file.webkitRelativePath.split('/').slice(1).join('/'), file]));
  $('status').textContent = `${selected.size} fixture files selected.`;
  $('run').disabled = !selected.has('assets/native146.bundle');
};
$('run').onclick = async () => {
  $('run').disabled = true;
  try {
    const manifest = await (await fetch('./build-manifest.json')).json();
    const runtime = new Uint8Array(await (await fetch('./runtime.elf')).arrayBuffer());
    const rsf = await (await fetch('./game.rsf')).text();
    const prepared = {files: new Map([...selected].filter(([path]) => path.startsWith('assets/')).map(([path, file]) => [path.slice(7), file])),
      homeMenu: {banner: selected.get('home/banner.png'), icon: selected.get('home/icon.png'), cue: selected.get('home/banner.wav')},
      report: {test: 'Browser WORKERFS input, OPFS sync output, production makerom'}};
    const worker = new Worker('./worker.mjs', {type: 'module'});
    worker.onmessage = ({data}) => {
      if (data.type === 'progress') $('status').textContent = `${data.detail} ${data.bytes || ''}`;
      else if (data.type === 'complete') {
        $('status').textContent = `PASS: browser generated ${data.file.size} bytes.`;
        $('report').textContent = JSON.stringify(data.report, null, 2);
        $('save').href = URL.createObjectURL(data.file);
        $('save').hidden = false;
        worker.terminate();
      } else if (data.type === 'error') {
        $('status').textContent = `FAIL: ${data.message}`;
        worker.terminate();
      }
    };
    worker.onerror = error => { $('status').textContent = `FAIL: ${error.message}`; worker.terminate(); };
    worker.postMessage({type: 'package', prepared, manifest, runtime, rsf});
    $('status').textContent = 'Packaging in browser…';
  } catch (error) { $('status').textContent = `FAIL: ${error.message}`; }
};

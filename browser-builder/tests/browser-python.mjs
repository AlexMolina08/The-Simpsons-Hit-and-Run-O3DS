const button = document.getElementById('run'), status = document.getElementById('status');
document.getElementById('support').textContent = `OPFS: ${!!navigator.storage?.getDirectory}; save picker: ${typeof window.showSaveFilePicker === 'function'}`;
button.onclick = () => {
  button.disabled = true;
  status.textContent = 'Loading Python and Pillow from this local site…';
  const worker = new Worker('./qa-python-worker.mjs', {type: 'module'});
  worker.onmessage = ({data}) => { status.textContent = JSON.stringify(data, null, 2); worker.terminate(); };
  worker.onerror = error => { status.textContent = `FAIL: ${error.message}`; worker.terminate(); };
  worker.postMessage({});
};

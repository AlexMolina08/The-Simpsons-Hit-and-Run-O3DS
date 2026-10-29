/** Save an OPFS-backed installer without reading the whole CIA into JavaScript. */
export async function prepareLocalDownload() {
  if (!('serviceWorker' in navigator) || !window.isSecureContext) return false;
  try {
    await navigator.serviceWorker.register(new URL('./service-worker.mjs', import.meta.url), {type: 'module', scope: new URL('./', import.meta.url).pathname});
    await navigator.serviceWorker.ready;
    if (!navigator.serviceWorker.controller) {
      await new Promise((resolve, reject) => {
        const timer = setTimeout(() => { navigator.serviceWorker.removeEventListener('controllerchange', changed); reject(new Error('Local download worker did not start.')); }, 5000);
        const changed = () => { clearTimeout(timer); resolve(); };
        navigator.serviceWorker.addEventListener('controllerchange', changed, {once: true});
      });
    }
    return !!navigator.serviceWorker.controller;
  } catch { return false; }
}

export function offerFileDownload(file, filename, {localToken, streamed = false} = {}) {
  const anchor = document.createElement('a');
  let objectURL;
  anchor.download = filename;
  anchor.rel = 'noopener';
  // The worker serves this response directly from local browser storage. No
  // game bytes are fetched from or posted to the website's server.
  anchor.href = streamed && localToken && navigator.serviceWorker?.controller
    ? new URL(`./__local-download/${encodeURIComponent(localToken)}/${encodeURIComponent(filename)}`, import.meta.url).href
    : (objectURL = URL.createObjectURL(file));
  document.body.append(anchor); anchor.click(); anchor.remove();
  // Large downloads may retain the URL while Safari hands them to its download
  // manager. Revoke on page close; do not race that handoff with a short timer.
  if (objectURL) addEventListener('pagehide', () => URL.revokeObjectURL(objectURL), {once: true});
}

export function parseByteRange(value, size) {
  if (!value) return {start: 0, end: size - 1, partial: false};
  const match = /^bytes=(\d*)-(\d*)$/.exec(value);
  if (!match || (!match[1] && !match[2])) return null;
  const start = match[1] ? Number(match[1]) : Math.max(0, size - Number(match[2]));
  const end = match[1] && match[2] ? Math.min(size - 1, Number(match[2])) : size - 1;
  if (!Number.isSafeInteger(start) || !Number.isSafeInteger(end) || start < 0 || start >= size || end < start) return null;
  return {start, end, partial: true};
}

import {parseByteRange} from './downloads.mjs';
self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
const prefix = new URL('./__local-download/', self.registration.scope).pathname;
self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin || !url.pathname.startsWith(prefix)) return;
  event.respondWith(download(event.request, url));
});
async function download(request, url) {
  try {
    if (request.method !== 'GET' && request.method !== 'HEAD') return new Response(null, {status: 405});
    const parts = url.pathname.slice(prefix.length).split('/').map(decodeURIComponent);
    if (parts.length !== 2) return new Response(null, {status: 404});
    const root = await navigator.storage.getDirectory();
    const directory = await root.getDirectoryHandle('hit-run-3ds-build');
    const record = JSON.parse(await (await (await directory.getFileHandle('completed.json')).getFile()).text());
    if (!record.localToken || parts[0] !== record.localToken || parts[1] !== record.filename) return new Response(null, {status: 404});
    const file = await (await directory.getFileHandle('installer.cia')).getFile();
    if (file.size !== record.bytes) return new Response('The installer is incomplete.', {status: 409});
    const range = parseByteRange(request.headers.get('Range'), file.size);
    if (!range) return new Response(null, {status: 416, headers: {'Content-Range': `bytes */${file.size}`}});
    const headers = {
      'Content-Type': 'application/octet-stream',
      'Content-Disposition': `attachment; filename="${record.filename.replaceAll(/[^a-zA-Z0-9._-]/g, '_')}"`,
      'Content-Length': String(range.end - range.start + 1),
      'Accept-Ranges': 'bytes', 'Cache-Control': 'no-store',
      'X-Content-Type-Options': 'nosniff', 'X-Local-Only': '1',
    };
    if (range.partial) headers['Content-Range'] = `bytes ${range.start}-${range.end}/${file.size}`;
    return new Response(request.method === 'HEAD' ? null : file.slice(range.start, range.end + 1).stream(),
      {status: range.partial ? 206 : 200, headers});
  } catch { return new Response('This local installer is no longer available. Reopen the builder and build again.', {status: 404}); }
}

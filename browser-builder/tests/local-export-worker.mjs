// Private synthetic fixture only. Refuse to overwrite a real builder session.
const fixtureKind = 'local-export-qa-v1';
const filename = 'builder-export-test.bin';
const directoryName = 'hit-run-3ds-build';
async function readRecord(directory) {
  try { return JSON.parse(await (await (await directory.getFileHandle('completed.json')).getFile()).text()); }
  catch (error) { if (error.name === 'NotFoundError') return null; throw error; }
}
async function openFixture() {
  const root = await navigator.storage.getDirectory();
  const directory = await root.getDirectoryHandle(directoryName);
  const record = await readRecord(directory);
  if (record?.fixtureKind !== fixtureKind) throw new Error('No synthetic fixture exists on this origin. Use a separate test port.');
  const file = await (await directory.getFileHandle('installer.cia')).getFile();
  if (file.size !== record.bytes) throw new Error('The existing fixture is incomplete.');
  return {directory, file, record};
}
async function createFixture(bytes) {
  if (!Number.isSafeInteger(bytes) || bytes < 1024 || bytes > 2 * 1024 ** 3) throw new Error('Invalid fixture size.');
  const root = await navigator.storage.getDirectory();
  const directory = await root.getDirectoryHandle(directoryName, {create: true});
  const record = await readRecord(directory);
  if (record && record.fixtureKind !== fixtureKind) throw new Error('This origin contains a real builder result. Open the test on a separate port.');
  for await (const [name] of directory.entries()) {
    if (!record || record.fixtureKind !== fixtureKind) throw new Error('This origin contains unidentified data. Open the test on a separate port.');
    if (!['installer.cia', 'completed.json'].includes(name)) throw new Error('This origin contains builder data. Open the test on a separate port.');
  }
  const first = new TextEncoder().encode('Hit & Run local export QA fixture v1\nSynthetic bytes; no game data.\n');
  const tail = Uint8Array.from({length: 64}, (_, index) => (index * 37 + 91) & 255);
  const handle = await directory.getFileHandle('installer.cia', {create: true});
  const writer = await handle.createSyncAccessHandle();
  try {
    postMessage({type: 'progress', detail: `Creating ${bytes.toLocaleString()} synthetic bytes on browser storage…`});
    // Truncate to zero first, so reusing a fixture cannot retain stale bytes.
    writer.truncate(0); writer.truncate(bytes);
    if (writer.write(first, {at: 0}) !== first.length || writer.write(tail, {at: bytes - tail.length}) !== tail.length) throw new Error('Incomplete sentinel write.');
    writer.flush();
  } finally { writer.close(); }
  const localToken = [...crypto.getRandomValues(new Uint8Array(16))].map(value => value.toString(16).padStart(2, '0')).join('');
  const completed = {fixtureKind, filename, bytes, localToken, first: [...first], tail: [...tail]};
  const metadata = await directory.getFileHandle('completed.json', {create: true});
  const metadataWriter = await metadata.createSyncAccessHandle();
  try {
    const encoded = new TextEncoder().encode(JSON.stringify(completed));
    metadataWriter.truncate(0);
    if (metadataWriter.write(encoded, {at: 0}) !== encoded.length) throw new Error('Incomplete fixture record.');
    metadataWriter.flush();
  } finally { metadataWriter.close(); }
  return openFixture();
}
self.onmessage = async ({data}) => {
  try {
    const result = data.type === 'restore' ? await openFixture() : await createFixture(data.bytes);
    postMessage({type: 'ready', file: result.file, record: result.record});
  } catch (error) { postMessage({type: 'error', message: error.message || String(error)}); }
};

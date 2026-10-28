/** Disk-backed temporary files. Game originals are never opened for writing. */
export async function createBuildStorage(name = 'hit-run-3ds-build') {
  const root = await navigator.storage.getDirectory();
  const directory = await root.getDirectoryHandle(name, {create: true});
  const resolve = async path => {
    const components = path.split('/');
    if (components.some(part => !part || part === '.' || part === '..')) throw new Error('Invalid temporary file path.');
    let parent = directory;
    for (const component of components.slice(0, -1)) parent = await parent.getDirectoryHandle(component, {create: true});
    return parent.getFileHandle(components.at(-1), {create: true});
  };
  return {
    directory,
    async write(path, blob) {
      const file = await resolve(path);
      const stream = await file.createWritable();
      try { await blob.stream().pipeTo(stream); } catch (error) { await stream.abort().catch(() => {}); throw error; }
      return file.getFile();
    },
    async openSync(path) {
      const file = await resolve(path);
      return {file, handle: await file.createSyncAccessHandle()};
    },
    async clear() {
      for await (const [entry] of directory.entries()) await directory.removeEntry(entry, {recursive: true});
    },
  };
}

/** Replace one MEMFS file's byte storage with a synchronous OPFS handle. */
export function mountDiskOutput(module, path, handle, onWrite = () => {}) {
  const {FS} = module;
  FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
  FS.writeFile(path, new Uint8Array());
  const node = FS.lookupPath(path).node;
  const original = node.node_ops;
  let size = handle.getSize(), lastProgress = 0;
  node.usedBytes = size;
  node.node_ops = {
    ...original,
    getattr(target) { const value = original.getattr(target); value.size = size; value.blocks = Math.ceil(size / 4096); return value; },
    setattr(target, attributes) {
      if (attributes.size !== undefined) { handle.truncate(attributes.size); size = attributes.size; target.usedBytes = size; }
      if (attributes.mode !== undefined) target.mode = attributes.mode;
      if (attributes.timestamp !== undefined) target.timestamp = attributes.timestamp;
    },
  };
  node.stream_ops = {
    ...node.stream_ops,
    read(stream, buffer, offset, length, position) {
      return handle.read(buffer.subarray(offset, offset + length), {at: position});
    },
    write(stream, buffer, offset, length, position) {
      const written = handle.write(buffer.subarray(offset, offset + length), {at: position});
      size = Math.max(size, position + written); node.usedBytes = size;
      const now = performance.now();
      if (now - lastProgress > 200) { onWrite(size); lastProgress = now; }
      return written;
    },
    llseek(stream, offset, whence) {
      const position = whence === 1 ? stream.position + offset : whence === 2 ? size + offset : offset;
      if (position < 0) throw new FS.ErrnoError(28);
      return position;
    },
    allocate(stream, offset, length) {
      if (offset + length > size) { size = offset + length; handle.truncate(size); node.usedBytes = size; }
    },
    fsync() { handle.flush(); return 0; },
    close() { handle.flush(); },
  };
  return {get size() { return size; }, flush() { handle.flush(); }};
}

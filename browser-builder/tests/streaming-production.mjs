// Opt-in same-input old/streamed WASM packaging proof. No assets are fetched or published.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath, pathToFileURL} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const args = process.argv.slice(2);
const flag = (name, fallback) => { const i = args.indexOf(name); return i < 0 ? fallback : args[i + 1]; };
const output = path.resolve(flag('--out', path.join(root, 'out/browser-tools/streaming-production')));
assert.ok(output.startsWith(path.join(root, 'out/')));
const data = path.resolve(flag('--data', path.join(root, 'sdcard/3ds/simpsons')));
const runtime = path.resolve(flag('--runtime', path.join(root, 'out/browser-tools/streaming-native-test/runtime.elf')));
const artwork = path.resolve(flag('--artwork', path.join(root, 'out/local-dev/visual-upgrade-20261025/Simpsons-Hit-and-Run-3DS-v1.2.0-local-test.home-menu')));
const rsf = path.join(root, 'tools/3ds/cia/game.rsf');
const modules = {
  original: path.resolve(flag('--original', path.join(root, 'browser-builder/dist/vendor/makerom.mjs'))),
  streamed: path.resolve(flag('--streamed', path.join(root, 'out/browser-tools/wasm/makerom.mjs'))),
};
const stage = path.join(output, 'romfs');
fs.mkdirSync(output, {recursive: true});
const hash = async (file, start = 0, bytes = fs.statSync(file).size) => {
  const sha = createHash('sha256');
  if (bytes) for await (const chunk of fs.createReadStream(file, {start, end: start + bytes - 1})) sha.update(chunk);
  return sha.digest('hex');
};
const mode = flag('--mode');
if (mode) {
  const {default: createMakerom} = await import(pathToFileURL(modules[mode]));
  const logs = [];
  const maker = await createMakerom({noInitialRun: true, print: x => logs.push(x), printErr: x => logs.push(x)});
  maker.FS.mkdirTree('/game'); maker.FS.mount(maker.NODEFS, {root: stage}, '/game');
  maker.FS.mkdirTree('/output'); maker.FS.mount(maker.NODEFS, {root: output}, '/output');
  maker.FS.mkdirTree('/input');
  for (const [name, source] of [['runtime.elf', runtime], ['game.rsf', rsf], ['banner.bnr', path.join(artwork, 'banner.bnr')], ['icon.smdh', path.join(artwork, 'icon.smdh')]]) maker.FS.writeFile(`/input/${name}`, fs.readFileSync(source));
  const started = performance.now();
  const code = maker.callMain(['-f', 'cia', '-o', `/output/${mode}.cia`, '-target', 't', '-rsf', '/input/game.rsf', '-DROMFS_ROOT=/game', '-elf', '/input/runtime.elf', '-icon', '/input/icon.smdh', '-banner', '/input/banner.bnr', '-ver', '1056']);
  fs.writeFileSync(path.join(output, `${mode}.log`), logs.join('\n'));
  assert.equal(code, 0, logs.slice(-10).join('\n'));
  const result = {seconds: (performance.now() - started) / 1000, heapBytes: maker.HEAPU8.byteLength,
    rssBytes: process.memoryUsage().rss, peakRssKiB: process.resourceUsage().maxRSS,
    ciaBytes: fs.statSync(path.join(output, `${mode}.cia`)).size, moduleSHA256: await hash(modules[mode]),
    wasmSHA256: await hash(modules[mode].replace(/\.mjs$/, '.wasm'))};
  fs.writeFileSync(path.join(output, `${mode}.json`), JSON.stringify(result, null, 2));
  console.log(JSON.stringify({mode, ...result}));
  process.exit(0);
}
const list = directory => fs.readdirSync(directory, {withFileTypes: true}).flatMap(entry => {
  const full = path.join(directory, entry.name); assert.ok(!entry.isSymbolicLink());
  return entry.isDirectory() ? list(full) : [full];
}).sort();
assert.ok(!fs.existsSync(stage), 'Use a fresh --out directory.');
fs.mkdirSync(stage);
const inputs = [];
for (const directory of ['art', 'scripts', 'sound']) inputs.push(...list(path.join(data, directory)));
for (const name of fs.readdirSync(data).filter(x => x.endsWith('.rcf') || ['native146.bundle', 'character-occlusion.bin', 'landmark-visibility.bin'].includes(x))) inputs.push(path.join(data, name));
inputs.push(...list(path.join(data, 'movies')).filter(x => x.endsWith('.3dm')));
const manifest = [];
for (const source of inputs.sort()) {
  const relative = path.relative(data, source), target = path.join(stage, relative);
  fs.mkdirSync(path.dirname(target), {recursive: true});fs.linkSync(source, target);
  manifest.push({path: relative.split(path.sep).join('/'), bytes: fs.statSync(source).size, sha256: await hash(source)});
}
const saveFile = path.join(data, 'Save1'), saveBefore = fs.existsSync(saveFile) ? await hash(saveFile) : null;
const report = {started: new Date().toISOString(), evidence: 'Private same-input WASM package parity and memory; no real-phone or console validation.',
  runtimeSHA256: await hash(runtime), inputAssets: manifest.length, inputBytes: manifest.reduce((n, x) => n + x.bytes, 0)};
const save = () => fs.writeFileSync(path.join(output, 'report.json'), JSON.stringify(report, null, 2));
fs.writeFileSync(path.join(output, 'assets.json'), JSON.stringify(manifest));save();
for (const name of ['original', 'streamed']) {
  console.log(`Build ${name} WASM package`);
  const result = spawnSync(process.execPath, [fileURLToPath(import.meta.url), ...args, '--mode', name], {encoding: 'utf8', maxBuffer: 4 * 1024 * 1024});
  fs.writeFileSync(path.join(output, `${name}-process.log`), result.stdout + result.stderr);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  report[name] = JSON.parse(fs.readFileSync(path.join(output, `${name}.json`)));save();
  console.log(JSON.stringify({mode: name, ...report[name]}));
}
const align = x => Math.ceil(x / 64) * 64;
const cia = name => {
  const file = path.join(output, `${name}.cia`), handle = fs.openSync(file, 'r'), header = Buffer.alloc(0x2020);
  fs.readSync(handle, header, 0, header.length, 0);fs.closeSync(handle);
  const ncchOffset = align(align(align(align(header.readUInt32LE(0)) + header.readUInt32LE(8)) + header.readUInt32LE(12)) + header.readUInt32LE(16));
  const ncchBytes = Number(header.readBigUInt64LE(24));
  assert.equal(fs.statSync(file).size, align(ncchOffset + ncchBytes) + header.readUInt32LE(20));
  return {file, ncchOffset, ncchBytes};
};
const first = cia('original'), second = cia('streamed');
report.ncch = {original: await hash(first.file, first.ncchOffset, first.ncchBytes), streamed: await hash(second.file, second.ncchOffset, second.ncchBytes)};
assert.equal(first.ncchBytes, second.ncchBytes);assert.equal(report.ncch.original, report.ncch.streamed, 'Streaming changed NCCH bytes');
assert.ok(report.streamed.heapBytes <= 512 * 1024 ** 2, 'Streaming exceeded 512 MiB WASM heap');
const file = fs.openSync(second.file, 'r');
const read = (position, size) => { const bytes = Buffer.alloc(size);assert.equal(fs.readSync(file, bytes, 0, size, position), size);return bytes; };
try {
  const ncch = read(second.ncchOffset, 0x200);assert.equal(ncch.toString('ascii', 0x100, 0x104), 'NCCH');
  assert.equal(ncch.readBigUInt64LE(0x118), 0x000400000f3a1100n);
  const romfs = second.ncchOffset + ncch.readUInt32LE(0x1b0) * 512;
  const ivfc = read(romfs, 0x60);assert.equal(ivfc.toString('ascii', 0, 4), 'IVFC');
  const block = 2 ** ivfc.readUInt32LE(0x4c);
  const level3 = romfs + Math.ceil((ivfc.readUInt32LE(0x54) + ivfc.readUInt32LE(8)) / block) * block;
  const info = read(level3, 0x28), meta = read(level3, info.readUInt32LE(0x24));
  const directories = meta.subarray(info.readUInt32LE(0xc), info.readUInt32LE(0xc) + info.readUInt32LE(0x10));
  const files = meta.subarray(info.readUInt32LE(0x1c), info.readUInt32LE(0x1c) + info.readUInt32LE(0x20));
  const found = new Map(), visited = new Set();
  const visit = (directory, prefix) => {
    assert.ok(!visited.has(directory));visited.add(directory);
    for (let item = directories.readUInt32LE(directory + 12); item !== 0xffffffff; item = files.readUInt32LE(item + 4)) {
      const name = files.toString('utf16le', item + 32, item + 32 + files.readUInt32LE(item + 28));
      assert.equal(files.readUInt32LE(item), directory);assert.ok(!/[\\/\0]/.test(name));
      found.set(prefix + name, {start: level3 + meta.length + Number(files.readBigUInt64LE(item + 8)), bytes: Number(files.readBigUInt64LE(item + 16))});
    }
    for (let child = directories.readUInt32LE(directory + 8); child !== 0xffffffff; child = directories.readUInt32LE(child + 4)) {
      assert.equal(directories.readUInt32LE(child), directory);
      visit(child, prefix + directories.toString('utf16le', child + 24, child + 24 + directories.readUInt32LE(child + 20)) + '/');
    }
  };
  visit(0, '');assert.equal(found.size, manifest.length);
  for (const asset of manifest) {
    const packed = found.get(asset.path);assert.ok(packed, asset.path);assert.equal(packed.bytes, asset.bytes);
    assert.equal(await hash(second.file, packed.start, packed.bytes), asset.sha256, asset.path);
    assert.equal(await hash(path.join(data, asset.path)), asset.sha256, `Original changed: ${asset.path}`);
  }
  report.verifiedAssets = found.size;
} finally {fs.closeSync(file);}
assert.equal(fs.existsSync(saveFile) ? await hash(saveFile) : null, saveBefore);
const verify = spawnSync(path.join(root, 'out/release-tools/ctrtool'), ['--intype=cia', '--verify', second.file], {encoding: 'utf8', maxBuffer: 4 * 1024 ** 2});
fs.writeFileSync(path.join(output, 'ctrtool.log'), verify.stdout + verify.stderr);
assert.equal(verify.status, 0);assert.deepEqual((verify.stdout + verify.stderr).split('\n').filter(x => /FAIL|ERROR/.test(x) && !/Signature|DigitalSignature/.test(x)), []);
assert.match(verify.stdout + verify.stderr, /TitleVersion:\s+1\.2\.0 \(1056\)/);
report.finished = new Date().toISOString();report.passed = true;report.saveUnchanged = true;
report.heapReductionPercent = (1 - report.streamed.heapBytes / report.original.heapBytes) * 100;
report.timeChangePercent = (report.streamed.seconds / report.original.seconds - 1) * 100;
save();console.log(JSON.stringify(report, null, 2));

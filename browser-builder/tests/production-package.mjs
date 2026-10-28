// Opt-in production-size packaging proof. Uses local assets; never fetches or publishes them.
// node --max-old-space-size=6144 browser-builder/tests/production-package.mjs --data sdcard/3ds/simpsons
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath, pathToFileURL} from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const args = process.argv.slice(2);
const verifyExisting = args.includes('--verify-existing');
const value = (flag, fallback) => {
  const at = args.indexOf(flag);
  if (at < 0) return fallback;
  assert.ok(args[at + 1] && !args[at + 1].startsWith('--'), `${flag} needs a path`);
  return path.resolve(args[at + 1]);
};
const data = value('--data');
assert.ok(data, 'Pass --data with a privately prepared PC game directory (17 converted movies).');
const output = value('--out', path.join(repo, 'out/browser-tools/production-package'));
assert.ok(output.startsWith(path.join(repo, 'out/')), 'Proof outputs must remain under ignored out/.');
const elf = value('--elf', path.join(repo, 'browser-builder/dist/runtime.elf'));
const artwork = value('--artwork', path.join(repo, 'out/distribution-artwork-check'));
const wasmModule = value('--module', path.join(repo, 'out/browser-tools/wasm/makerom.mjs'));
const makerom = value('--makerom', path.join(repo, 'out/release-tools/makerom-v0.18.4'));
const ctrtool = value('--ctrtool', path.join(repo, 'out/release-tools/ctrtool'));
const rsf = path.join(repo, 'tools/3ds/cia/game.rsf');
for (const input of [elf, rsf, wasmModule, makerom, ctrtool, path.join(artwork, 'banner.bnr'), path.join(artwork, 'icon.smdh')]) {
  assert.ok(fs.statSync(input).isFile(), `Missing input: ${input}`);
}
fs.mkdirSync(output, {recursive: true});
const report = verifyExisting
  ? JSON.parse(fs.readFileSync(path.join(output, 'report.json'), 'utf8'))
  : {started: new Date().toISOString(), evidence: 'Local package and data parity; no emulator or console validation.', phases: []};
const save = () => fs.writeFileSync(path.join(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
const phase = name => { report.phases.push({name, at: new Date().toISOString()}); save(); console.log(name); };
const hash = async file => {
  const digest = createHash('sha256');
  for await (const chunk of fs.createReadStream(file)) digest.update(chunk);
  return digest.digest('hex');
};
const list = directory => fs.readdirSync(directory, {withFileTypes: true}).flatMap(entry => {
  const file = path.join(directory, entry.name);
  assert.ok(!entry.isSymbolicLink(), `Unexpected symlink: ${file}`);
  return entry.isDirectory() ? list(file) : [file];
}).sort();
const run = (name, program, arguments_) => {
  const start = performance.now();
  const result = spawnSync(program, arguments_, {encoding: 'utf8', maxBuffer: 16 * 1024 * 1024});
  const log = `${result.stdout || ''}${result.stderr || ''}`;
  fs.writeFileSync(path.join(output, `${name}.log`), log);
  assert.equal(result.status, 0, `${name} failed; see ${name}.log: ${result.error || log.slice(-1000)}`);
  return {seconds: (performance.now() - start) / 1000, log};
};
const stage = path.join(output, 'romfs');
assert.ok(verifyExisting || !fs.existsSync(stage), 'Use a fresh --out directory to avoid stale data.');
fs.mkdirSync(stage, {recursive: verifyExisting});
const stageFile = (source, relative) => {
  const destination = path.join(stage, relative);
  fs.mkdirSync(path.dirname(destination), {recursive: true});
  // Hardlinks avoid duplicating 1.9 GB. Neither packager writes to input files.
  try { fs.linkSync(source, destination); } catch { fs.copyFileSync(source, destination); }
};
try {
  let manifest;
  if (!verifyExisting) {
  phase('Stage only runtime assets and all 17 converted movies');
  for (const directory of ['art', 'scripts', 'sound']) {
    for (const source of list(path.join(data, directory))) stageFile(source, path.relative(data, source));
  }
  for (const name of fs.readdirSync(data).filter(name => name.endsWith('.rcf'))) stageFile(path.join(data, name), name);
  stageFile(path.join(data, 'native146.bundle'), 'native146.bundle');
  assert.ok(fs.existsSync(path.join(stage, 'scripts.rcf')), 'Missing scripts.rcf');
  const movies = fs.readdirSync(path.join(data, 'movies')).filter(name => name.endsWith('.3dm')).sort();
  assert.equal(movies.length, 17, 'Exactly 17 converted movies required');
  for (const name of movies) {
    const source = path.join(data, 'movies', name), header = Buffer.alloc(12), descriptor = fs.openSync(source, 'r');
    try { assert.equal(fs.readSync(descriptor, header, 0, 12, 0), 12); } finally { fs.closeSync(descriptor); }
    assert.deepEqual(header, Buffer.from('SRR3DSMV\x02\0\0\0', 'binary'), `Invalid movie: ${name}`);
    stageFile(source, path.join('movies', name));
  }
  manifest = [];
  for (const file of list(stage)) manifest.push({path: path.relative(stage, file).split(path.sep).join('/'), bytes: fs.statSync(file).size, sha256: await hash(file)});
  report.assets = {count: manifest.length, bytes: manifest.reduce((total, asset) => total + asset.bytes, 0), movies: movies.length};
  report.inputs = {elf: await hash(elf), rsf: await hash(rsf), banner: await hash(path.join(artwork, 'banner.bnr')), icon: await hash(path.join(artwork, 'icon.smdh'))};
  fs.writeFileSync(path.join(output, 'assets.json'), JSON.stringify(manifest, null, 2) + '\n');
  save();

  phase('Run makerom WASM with disk-backed NODEFS input and output');
  const {default: createMakerom} = await import(pathToFileURL(wasmModule));
  const wasmLog = [];
  const maker = await createMakerom({noInitialRun: true, print: line => wasmLog.push(line), printErr: line => wasmLog.push(line)});
  assert.ok(maker.NODEFS, 'Rebuild makerom with NODEFS exported');
  maker.FS.mkdirTree('/game'); maker.FS.mount(maker.NODEFS, {root: stage}, '/game');
  maker.FS.mkdirTree('/proof'); maker.FS.mount(maker.NODEFS, {root: output}, '/proof');
  maker.FS.mkdirTree('/input');
  for (const [name, source] of [['runtime.elf', elf], ['game.rsf', rsf], ['banner.bnr', path.join(artwork, 'banner.bnr')], ['icon.smdh', path.join(artwork, 'icon.smdh')]]) {
    maker.FS.writeFile(`/input/${name}`, fs.readFileSync(source));
  }
  const wasmStart = performance.now();
  const status = maker.callMain(['-f', 'cia', '-o', '/proof/game.cia', '-target', 't', '-rsf', '/input/game.rsf', '-DROMFS_ROOT=/game', '-elf', '/input/runtime.elf', '-icon', '/input/icon.smdh', '-banner', '/input/banner.bnr', '-ver', '1041']);
  fs.writeFileSync(path.join(output, 'makerom-wasm.log'), wasmLog.join('\n') + '\n');
  assert.equal(status, 0, `makerom WASM failed: ${wasmLog.join('\n').slice(-1000)}`);
  report.wasm = {seconds: (performance.now() - wasmStart) / 1000, heapBytes: maker.HEAPU8.length, rssBytes: process.memoryUsage().rss, peakRssBytes: process.resourceUsage().maxRSS * 1024, bytes: fs.statSync(path.join(output, 'game.cia')).size, sha256: await hash(path.join(output, 'game.cia'))};
  save();

  phase('Build native makerom from identical inputs');
  const native = run('makerom-native', makerom, ['-f', 'cia', '-o', path.join(output, 'native.cia'), '-target', 't', '-rsf', rsf, `-DROMFS_ROOT=${stage}`, '-elf', elf, '-icon', path.join(artwork, 'icon.smdh'), '-banner', path.join(artwork, 'banner.bnr'), '-ver', '1041']);
  report.native = {seconds: native.seconds, bytes: fs.statSync(path.join(output, 'native.cia')).size, sha256: await hash(path.join(output, 'native.cia'))};
  assert.equal(report.wasm.bytes, report.native.bytes, 'CIA sizes differ');
  save();
  } else {
    manifest = JSON.parse(fs.readFileSync(path.join(output, 'assets.json'), 'utf8'));
    for (const [name, file] of [['wasm', 'game.cia'], ['native', 'native.cia']]) {
      assert.equal(await hash(path.join(output, file)), report[name].sha256, 'Existing package changed');
    }
  }

  phase('Verify containers with native ctrtool and compare NCCH sections');
  const contents = {};
  for (const [name, cia] of [['wasm', 'game.cia'], ['native', 'native.cia']]) {
    const directory = path.join(output, `${name}-contents`);
    fs.mkdirSync(directory, {recursive: true});
    const logFile = path.join(output, `ctrtool-${name}.log`);
    const check = verifyExisting && list(directory).length === 1 && fs.existsSync(logFile)
      ? {log: fs.readFileSync(logFile, 'utf8')}
      : run(`ctrtool-${name}`, ctrtool, ['--intype=cia', '--verify', `--contents=${directory}/content`, path.join(output, cia)]);
    // Development signatures are expected. Data/content/IVFC failures are not.
    const failures = check.log.split('\n').filter(line => /FAIL|ERROR/.test(line) && !/Signature|DigitalSignature/.test(line));
    assert.deepEqual(failures, [], `${name}: integrity verification failed`);
    assert.match(check.log, /TitleVersion:\s+1\.1\.1 \(1041\)/);
    assert.match(check.log, /Program id:\s+000400000f3a1100/);
    assert.match(check.log, /System mode:\s+prod \(AppMemory: 64MB\)/);
    assert.match(check.log, /CPU Speed \(New3DS\):\s+804MHz/);
    const files = list(directory);
    assert.equal(files.length, 1, 'Expected one NCCH content');
    contents[name] = files[0];
  }
  const sectionHashes = async ncch => {
    const handle = fs.openSync(ncch, 'r'), header = Buffer.alloc(0x200);
    try { fs.readSync(handle, header, 0, header.length, 0); } finally { fs.closeSync(handle); }
    assert.equal(header.toString('ascii', 0x100, 0x104), 'NCCH');
    const result = {};
    for (const [name, field] of [['exefs', 0x1a0], ['romfs', 0x1b0]]) {
      const offset = header.readUInt32LE(field) * 0x200, bytes = header.readUInt32LE(field + 4) * 0x200;
      const digest = createHash('sha256');
      for await (const chunk of fs.createReadStream(ncch, {start: offset, end: offset + bytes - 1})) digest.update(chunk);
      result[name] = {offset, bytes, sha256: digest.digest('hex')};
    }
    return result;
  };
  report.sections = {wasm: await sectionHashes(contents.wasm), native: await sectionHashes(contents.native)};
  assert.deepEqual(report.sections.wasm.exefs, report.sections.native.exefs, 'Native and WASM ExeFS differ');
  assert.equal(report.sections.wasm.romfs.bytes, report.sections.native.romfs.bytes, 'RomFS lengths differ');
  report.romfsByteIdentical = report.sections.wasm.romfs.sha256 === report.sections.native.romfs.sha256;
  for (const name of ['wasm', 'native']) {
    const exefs = path.join(output, `${name}-exefs`);
    fs.mkdirSync(exefs, {recursive: true});
    run(`ctrtool-${name}-exefs`, ctrtool, ['--intype=ncch', `--exefsdir=${exefs}`, '--decompresscode', contents[name]]);
    const code = list(exefs).find(file => /(^|\/)(?:\.code|code)(?:\.bin)?$/.test(file));
    assert.ok(code, 'ctrtool did not extract executable code');
    report[name].codeSha256 = await hash(code);
  }
  assert.equal(report.wasm.codeSha256, report.native.codeSha256, 'Loaded executable code differs');
  save();

  phase('Stream both RomFS filesystems and check every packaged asset against original input');
  // Read existing NCCH file ranges rather than create another two whole-game copies.
  const romfsFiles = (ncch, section) => {
    const descriptor = fs.openSync(ncch, 'r');
    const read = (offset, bytes) => {
      const result = Buffer.alloc(bytes);
      assert.equal(fs.readSync(descriptor, result, 0, bytes, offset), bytes);
      return result;
    };
    try {
      const ivfc = read(section.offset, 0x60);
      assert.equal(ivfc.toString('ascii', 0, 4), 'IVFC');
      const blockSize = 2 ** ivfc.readUInt32LE(0x4c);
      const level2 = section.offset + Math.ceil((ivfc.readUInt32LE(0x54) + ivfc.readUInt32LE(8)) / blockSize) * blockSize;
      const header = read(level2, 0x28);
      assert.equal(header.readUInt32LE(0), 0x28);
      const metadata = read(level2, header.readUInt32LE(0x24));
      const directories = metadata.subarray(header.readUInt32LE(0x0c), header.readUInt32LE(0x0c) + header.readUInt32LE(0x10));
      const files = metadata.subarray(header.readUInt32LE(0x1c), header.readUInt32LE(0x1c) + header.readUInt32LE(0x20));
      const result = new Map(), visitedDirectories = new Set(), visitedFiles = new Set();
      const visit = (directoryOffset, prefix) => {
        assert.ok(!visitedDirectories.has(directoryOffset), 'Directory cycle');
        visitedDirectories.add(directoryOffset);
        let file = directories.readUInt32LE(directoryOffset + 12);
        while (file !== 0xffffffff) {
          assert.ok(!visitedFiles.has(file), 'File cycle'); visitedFiles.add(file);
          assert.equal(files.readUInt32LE(file), directoryOffset, 'File parent mismatch');
          const nameBytes = files.readUInt32LE(file + 28);
          const name = files.toString('utf16le', file + 32, file + 32 + nameBytes);
          assert.ok(name && !/[\\/\0]/.test(name), 'Invalid RomFS filename');
          const bytes = Number(files.readBigUInt64LE(file + 16));
          const offset = level2 + metadata.length + Number(files.readBigUInt64LE(file + 8));
          assert.ok(Number.isSafeInteger(offset + bytes) && offset + bytes <= fs.statSync(ncch).size, 'Asset outside NCCH');
          assert.ok(!result.has(prefix + name), 'Duplicate RomFS filename');
          result.set(prefix + name, {offset, bytes});
          file = files.readUInt32LE(file + 4);
        }
        let child = directories.readUInt32LE(directoryOffset + 8);
        while (child !== 0xffffffff) {
          assert.equal(directories.readUInt32LE(child), directoryOffset, 'Directory parent mismatch');
          const nameBytes = directories.readUInt32LE(child + 20);
          const name = directories.toString('utf16le', child + 24, child + 24 + nameBytes);
          assert.ok(name && !/[\\/\0]/.test(name), 'Invalid RomFS directory');
          visit(child, `${prefix}${name}/`);
          child = directories.readUInt32LE(child + 4);
        }
      };
      visit(0, '');
      return result;
    } finally { fs.closeSync(descriptor); }
  };
  for (const name of ['wasm', 'native']) {
    const packaged = romfsFiles(contents[name], report.sections[name].romfs);
    assert.equal(packaged.size, manifest.length, `${name}: packaged file count differs`);
    for (const asset of manifest) {
      const stored = packaged.get(asset.path);
      assert.ok(stored, `${name}: missing ${asset.path}`);
      assert.equal(stored.bytes, asset.bytes, `${name}: asset length: ${asset.path}`);
      const digest = createHash('sha256');
      if (stored.bytes) {
        for await (const chunk of fs.createReadStream(contents[name], {start: stored.offset, end: stored.offset + stored.bytes - 1})) digest.update(chunk);
      }
      assert.equal(digest.digest('hex'), asset.sha256, `${name}: asset content: ${asset.path}`);
    }
    report[name].verifiedAssets = manifest.length;
    save();
  }
  for (const asset of manifest) {
    // Hardlinks remain read-only in practice: source data must still match too.
    assert.equal(await hash(path.join(data, asset.path)), asset.sha256, `Input changed: ${asset.path}`);
  }
  report.passed = true;
  delete report.error;
  report.finished = new Date().toISOString();
  phase('PASS: production-size WASM/native package, loaded code, and every asset match');
} catch (error) {
  report.passed = false;
  report.error = String(error.stack || error);
  save();
  throw error;
}

/** Production conversion proof against a private installed PC copy. No game data is committed. */
import assert from 'node:assert/strict';
import fs, {openAsBlob} from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {prepareGameAssets, validateDialogue} from '../engine-conversion/conversion.mjs';
import {languageProfile} from '../engine-conversion/languages.mjs';

const [game, language, output, corePath, recipePath] = process.argv.slice(2);
assert.ok(recipePath, 'Usage: localized-media.mjs GAME LANGUAGE OUTPUT FFMPEG_MODULE RECIPE');
const profile = languageProfile(language), files = new Map();
async function visit(folder, relative = '') {
  for (const entry of fs.readdirSync(folder, {withFileTypes: true})) {
    const name = path.join(relative, entry.name), source = path.join(folder, entry.name);
    if (entry.isDirectory()) await visit(source, name);
    else if (entry.isFile()) files.set(name.split(path.sep).join('/'), await openAsBlob(source));
  }
}
await visit(game);
const recipe = JSON.parse(fs.readFileSync(recipePath));
const coverage = await validateDialogue(files.get(profile.archive), recipe.dialogueKeys[language]);
globalThis.self = {location: {href: pathToFileURL(path.resolve(corePath)).href}};
globalThis.importScripts = () => {};
const {default: create} = await import(pathToFileURL(path.resolve(corePath)));
const core = await create({wasmBinary: fs.readFileSync(path.join(path.dirname(corePath), 'ffmpeg-core.wasm'))});
const runFFmpeg = async ({inputs, args, outputs, allowFailure = false}) => {
  const logs = [], result = new Map(); core.reset(); core.setLogger(({message}) => logs.push(message));
  try {
    for (const {name, data} of inputs) core.FS.writeFile('/' + name, data instanceof Uint8Array ? data : new Uint8Array(await data.arrayBuffer()));
    const exitCode = core.exec('-y', ...args);
    assert.ok(allowFailure || exitCode === 0, logs.slice(-12).join('\n'));
    for (const name of outputs) result.set(name, core.FS.readFile('/' + name).slice());
    return {files: result, logs, exitCode};
  } finally {
    core.reset(); for (const name of [...inputs.map(item => item.name), ...outputs]) try { core.FS.unlink('/' + name); } catch {}
  }
};
fs.mkdirSync(output, {recursive: true}); const generated = [];
const prepared = await prepareGameAssets({files, language, phase: 'media', tools: {runFFmpeg},
  async writeOutput(name, blob) {
    const destination = path.join(output, name); fs.mkdirSync(path.dirname(destination), {recursive: true});
    const handle = fs.openSync(destination, 'w');
    try { for await (const chunk of blob.stream()) fs.writeSync(handle, chunk); } finally { fs.closeSync(handle); }
    const hash = createHash('sha256'); for await (const chunk of fs.createReadStream(destination)) hash.update(chunk);
    generated.push({path: name, bytes: blob.size, sha256: hash.digest('hex')});
    return openAsBlob(destination);
  },
  onProgress(event) { console.log(event.detail, event.completed + '/' + event.total); }
});
assert.equal(await prepared.files.get('game-language.txt').text(), language + '\n');
assert.equal(generated.filter(item => item.path.startsWith('sound/3ds-lid/')).length, 10);
assert.ok(prepared.files.has(profile.archive));
for (const other of ['dialog.rcf', 'dialogf.rcf', 'dialogs.rcf', 'dialogg.rcf']) if (other !== profile.archive) assert.ok(!prepared.files.has(other));
for (const movie of prepared.report.movies) {
  assert.equal(movie.language, language); assert.ok(movie.frames > 0 && movie.audioBytes > 0);
}
const report = {passed: true, language, coverage, generated, ...prepared.report};
fs.writeFileSync(path.join(output, 'media-report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({passed: true, language, coverage, movies: report.movies.length, greetings: 10}));

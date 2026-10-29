/** Exercise selection and result state without reading any user files. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const elements = new Map();
function element(id) {
  if (!elements.has(id)) {
    const classes = new Set();
    elements.set(id, {id, hidden: false, disabled: false, value: '', files: [], textContent: '', dataset: {},
      classList: {add: value => classes.add(value), remove: value => classes.delete(value)},
      replaceChildren(...children) { this.children = children; this.value = children[0]?.value || ''; },
      setAttribute() {}, removeAttribute() {}, querySelector() { return {focus() {}}; }, click() {}});
  }
  return elements.get(id);
}
const completed = {version: '1.2', runtimeSHA256: 'runtime', buildID: 'build', bytes: 12, filename: 'old.cia'};
const storage = {async readJSON(name) { return name === 'completed.json' ? completed : {}; },
  async read() { return {size: 12}; }, async clear() {}};
const workers = [];
class Worker {
  constructor() { workers.push(this); }
  postMessage() {}
  terminate() {}
}
const languages = {en: 'English', fr: 'Français', es: 'Español'};
const input = fs.readFileSync(new URL('../app.mjs', import.meta.url), 'utf8').replace(/^import .*;\s*$/gm, '');
const context = vm.createContext({
  document: {getElementById: element, createElement() { return {}; }, addEventListener() {}},
  window: {Worker, WebAssembly}, Worker, WebAssembly,
  navigator: {storage: {async getDirectory() { return {async removeEntry() {}}; }, async persist() {}}},
  fetch: async () => ({async json() { return completed; }}),
  validateInput(files) {
    if ([...files].some(file => file.name === 'bad')) throw new Error('Invalid copy');
    return {languages: ['en', 'fr', 'es'], fileCount: 2, languageName: 'English'};
  },
  readGameImages: async files => {
    if (files[0].name === 'bad.iso') throw new Error('Invalid image');
    return {kind: 'installed', files: new Map(), languages: ['en', 'fr', 'es']};
  },
  languageProfile: code => ({name: languages[code]}),
  createBuildStorage: async () => storage, offerFileDownload() {}, browserProfile: () => ({}),
  keepScreenAwake: async () => null, addEventListener() {}, performance: {now: () => 1},
  setInterval: () => 1, clearInterval() {}, console, Blob,
});
vm.runInContext(input, context, {filename: 'app.mjs'});
await new Promise(resolve => setImmediate(resolve));
assert.equal(element('result').hidden, false, 'The previous completed build should restore');
assert.equal(element('save').disabled, false);
const valid = {name: 'disc', webkitRelativePath: 'Game/disc'};
element('folder').files = [valid]; element('folder').value = 'selected';
element('folder').onchange();
assert.equal(element('folder').value, '', 'A selected folder must be selectable again');
assert.equal(element('result').hidden, true, 'Selecting a copy must hide the previous CIA');
assert.equal(element('save').disabled, true);
assert.match(element('selection').textContent, /English/);
await element('build').onclick();
assert.equal(workers.length, 1);
await workers[0].onmessage({data: {type: 'complete', file: {size: 12}, filename: 'english.cia', report: {}}});
assert.equal(element('result').hidden, false);
element('game-language').value = 'es'; element('game-language').onchange();
assert.equal(element('result').hidden, true, 'Changing language must hide the previous CIA');
assert.equal(element('save').disabled, true);
assert.match(element('selection').textContent, /Español/);
element('iso').value = 'old-disc'; element('folder').value = 'old-folder';
await element('clear').onclick();
assert.equal(element('iso').value, ''); assert.equal(element('folder').value, '');
element('folder').files = [valid]; element('folder').value = 'selected'; element('folder').onchange();
assert.equal(element('build').disabled, false, 'The same copy can be selected after clearing');
element('folder').files = [{name: 'bad', webkitRelativePath: 'Bad/bad'}]; element('folder').value = 'bad';
element('folder').onchange();
assert.equal(element('folder').value, '', 'Failed selection must allow the same copy to be retried');
assert.equal(element('build').disabled, true);
assert.equal(element('language-choice').hidden, true, 'Failed selection must hide the previous copy’s languages');
element('iso').files = [{name: 'bad.iso'}]; element('iso').value = 'bad';
await element('iso').onchange();
assert.equal(element('iso').value, '', 'Failed image selection must be retryable');
element('iso').files = [{name: 'game.iso'}]; element('iso').value = 'game';
await element('iso').onchange();
assert.match(element('selection').textContent, /game\.iso · English/);
assert.equal(element('iso').value, '', 'A selected image must be selectable again');
element('game-language').value = 'fr'; element('game-language').onchange();
assert.match(element('selection').textContent, /game\.iso · Français/);
console.log('PASS: previous CIA invalidation, language summary, and same-copy reselect after clearing or failure.');

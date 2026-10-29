import assert from 'node:assert/strict';
import {LANGUAGES, selectLanguage, availableLanguages} from '../engine-conversion/languages.mjs';
import {validateInput, REQUIRED_MOVIES, convertMovie, validateDialogue, validateTextBible} from '../engine-conversion/conversion.mjs';

const names = ['art/frontend/scrooby/bootup.p3d', 'art/frontend/scrooby/frontend.p3d',
  'art/frontend/scrooby/resource/txtbible/srr2.p3d',
  'art/frontend/scrooby/resource/pure3d/logo_anim.p3d', 'scripts/example.mfk', 'sound/example.rsd',
  'scripts.rcf', 'soundfx.rcf', 'carsound.rcf', 'music00.rcf', 'music01.rcf', 'music02.rcf', 'music03.rcf',
  ...REQUIRED_MOVIES.map(name => `movies/${name}.rmv`)];
const base = new Map(names.map(name => [name, new Blob([new Uint8Array([1])])]));
for (const profile of LANGUAGES) {
  const copy = new Map([...base, [profile.archive.toUpperCase(), new Blob(['voice'])]]);
  assert.equal(selectLanguage(copy).code, profile.code);
  assert.equal(validateInput(copy).language, profile.code);
  assert.deepEqual(validateInput(copy).languages, [profile.code]);
  assert.throws(() => selectLanguage(copy, 'invalid'), /Choose/);
}
const multilingual = new Map([...base, ...LANGUAGES.map(item => [item.archive, new Blob(['voice'])])]);
assert.deepEqual(availableLanguages(multilingual).map(item => item.code), ['en', 'fr', 'es']);
for (const profile of LANGUAGES) assert.equal(validateInput(multilingual, profile.code).language, profile.code);
assert.throws(() => validateInput(new Map([...base, ['dialogf.rcf', new Blob()]]), 'es'), /does not include/);

function chunk(type, body = [], children = []) {
  const data = new Uint8Array(12 + body.length + children.length), v = new DataView(data.buffer);
  v.setUint32(0, type, true); v.setUint32(4, 12 + body.length, true); v.setUint32(8, data.length, true);
  data.set(body, 12); data.set(children, 12 + body.length); return data;
}
function bible(language, offset = 0) {
  const body = new Uint8Array(2 + 1 + 12 + 8 + 4), v = new DataView(body.buffer);
  body.set([1, 120, language.charCodeAt(0)]);
  v.setUint32(3, 1, true); v.setUint32(7, 1009, true); v.setUint32(11, 4, true);
  v.setUint32(15, 42, true); v.setUint32(19, offset, true); body[23] = 65;
  return chunk(0xff443350, [], chunk(0x1800d, [], chunk(0x1800e, body)));
}
const biblePath = 'art/frontend/scrooby/resource/txtbible/srr2.p3d';
const copyWithBible = data => new Map([...multilingual, [biblePath, new Blob([data])]]);
assert.deepEqual(await validateTextBible(copyWithBible(bible('F')), 'fr'), {language: 'fr', strings: 1});
await assert.rejects(() => validateTextBible(copyWithBible(bible('F')), 'es'), /no Español text table/);
await assert.rejects(() => validateTextBible(copyWithBible(bible('F', 12)), 'fr'), /incomplete or invalid/);
await assert.rejects(() => validateTextBible(copyWithBible(bible('F').slice(0, 20)), 'fr'), /incomplete or invalid/);

async function movie(language, streams) {
  const calls = [], video = new Uint8Array([0, 0, 1, 0xb3]), pcm = new Uint8Array(128);
  const result = await convertMovie(new Blob([video]), 'movie', async request => {
    calls.push(request);
    if (request.allowFailure) return {exitCode: 1, logs: ['Video: binkvideo', ...Array.from({length: streams}, (_, i) => `Stream #0:${i + 1}: Audio: binkaudio_dct`)], files: new Map()};
    if (!request.outputs.length) return {exitCode: 0, logs: ['frame=2'], files: new Map()};
    return {exitCode: 0, logs: ['frame=2'], files: new Map([[request.outputs[0], request.outputs[0].endsWith('.pcm') ? pcm : video]])};
  }, language);
  return {result, audio: calls.find(item => item.args.includes('-vn'))};
}
for (const profile of LANGUAGES) {
  const {result, audio} = await movie(profile.code, 4);
  assert.equal(result.audioTrack, profile.track);
  assert.ok(audio.args.includes(`0:a:${profile.track}`));
  assert.equal((await movie(profile.code, 1)).result.audioTrack, 0);
}
await assert.rejects(() => movie('es', 2), /no es audio track/);
const rcf = new Uint8Array(80), view = new DataView(rcf.buffer);
rcf.set(new TextEncoder().encode('RADCORE CEMENT LIBRARY'));
view.setUint32(44, 48, true); view.setUint32(48, 1, true);
view.setUint32(64, 12345, true); view.setUint32(68, 76, true); view.setUint32(72, 4, true);
assert.deepEqual(await validateDialogue(new Blob([rcf]), [12345]), {required: 1, present: 1, missing: 0});
await assert.rejects(() => validateDialogue(new Blob([rcf]), [67890]), /sounds missing/);
view.setUint32(72, 1000, true);
await assert.rejects(() => validateDialogue(new Blob([rcf]), [12345]), /incomplete/);
console.log('PASS: copy-language detection, valid multilingual choices, correct movie tracks and complete dialogue-resource checks.');

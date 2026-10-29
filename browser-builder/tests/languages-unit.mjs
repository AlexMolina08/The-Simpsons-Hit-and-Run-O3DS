import assert from 'node:assert/strict';
import {LANGUAGES, selectLanguage, availableLanguages} from '../engine-conversion/languages.mjs';
import {validateInput, REQUIRED_MOVIES, convertMovie, validateDialogue} from '../engine-conversion/conversion.mjs';

const names = ['art/frontend/scrooby/bootup.p3d', 'art/frontend/scrooby/frontend.p3d',
  'art/frontend/scrooby/resource/pure3d/logo_anim.p3d', 'scripts/example.mfk', 'sound/example.rsd',
  'scripts.rcf', 'music00.rcf', 'music01.rcf', 'music02.rcf', 'music03.rcf',
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

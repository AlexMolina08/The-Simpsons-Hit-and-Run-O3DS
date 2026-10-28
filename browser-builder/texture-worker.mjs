import createEtc1 from './vendor/etc1.mjs';

let encoder;
onmessage = async ({data: {width, height, rgb}}) => {
  let source = 0, target = 0;
  try {
    encoder ||= await createEtc1();
    source = encoder._malloc(rgb.length);
    target = encoder._malloc(width * height / 2);
    if (!source || !target) throw new Error('Not enough browser memory to encode a texture.');
    encoder.HEAPU8.set(rgb, source);
    const count = encoder._encodeEtc1(source, width, height, target);
    if (count < 0) throw new Error('Unsupported texture dimensions.');
    const result = encoder.HEAPU8.slice(target, target + count);
    postMessage({result}, [result.buffer]);
  } catch (error) {
    postMessage({error: error.message || String(error)});
  } finally {
    if (source) encoder._free(source);
    if (target) encoder._free(target);
  }
};

import {createTools} from './tools.mjs';
import {convertMovie, archiveMember, validateBannerCue} from './engine-conversion/conversion.mjs';

onmessage = async ({data: {files}}) => {
  const tools = await createTools(event => postMessage(event));
  try {
    const encoded = await Promise.all(Array.from({length: 8}, async (_, index) => {
      const rgb = new Uint8Array(8 * 8 * 3).fill(index * 30);
      const result = await tools.encodeEtc1({width: 8, height: 8, rgb});
      if (result.length !== 32 || rgb[0] !== index * 30) throw new Error('Texture worker result or input preservation failed.');
      return result.length;
    }));
    postMessage({detail: 'Eight texture worker jobs passed. Testing Bink and sound…'});
    const movie = await convertMovie(files.get('sample_bink.rmv'), 'proof', tools.runFFmpeg);
    if (movie.frames !== 576 || movie.audioBytes !== 2457600) throw new Error('Movie frame or audio count differs.');
    const theme = await archiveMember(files.get('music02.rcf'), 'sound\\music\\simpsons_theme.rsd');
    const cue = await tools.runFFmpeg({inputs: [{name: 'theme.rsd', data: theme}], args: [
      '-hide_banner', '-loglevel', 'error', '-i', 'theme.rsd', '-t', '2.8', '-af',
      'loudnorm=I=-20:TP=-2:LRA=7,afade=t=in:d=0.015,afade=t=out:st=2.6:d=0.2',
      '-ar', '32000', '-ac', '2', '-c:a', 'pcm_s16le', 'banner.wav',
    ], outputs: ['banner.wav']});
    const banner = validateBannerCue(cue.files.get('banner.wav'));
    postMessage({passed: true, movie: {frames: movie.frames, audioBytes: movie.audioBytes}, banner, textureWorkerJobs: encoded.length,
      evidence: 'Actual browser WORKERFS movie/array inputs, minimal FFmpeg, nested texture worker pool'});
  } catch (error) { postMessage({error: error.message || String(error)}); }
  finally { tools.releaseConverters(); }
};

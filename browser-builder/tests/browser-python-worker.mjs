import {loadPython} from './worker.mjs';
onmessage = async () => {
  try {
    const python = await loadPython();
    const result = await python.runPythonAsync(`
import json, hashlib
from PIL import Image
from prepare_home_artwork import render_artwork, nodes
assert callable(texture_requests)
assert callable(render_artwork)
assert Image.new('RGB', (8, 8)).tobytes() == bytes(192)
json.dumps({'passed': True, 'python': __import__('sys').version.split()[0], 'pillow': __import__('PIL').__version__, 'textureParser': True, 'artworkParser': True})
`);
    postMessage(JSON.parse(result));
  } catch (error) { postMessage({error: error.message || String(error)}); }
};

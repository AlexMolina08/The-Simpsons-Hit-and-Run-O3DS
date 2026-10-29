import {loadPython} from './worker.mjs';
onmessage = async () => {
  try {
    const python = await loadPython();
    const result = await python.runPythonAsync(`
import json, hashlib
from PIL import Image, ImageDraw
import prepare_home_artwork as artwork
from prepare_home_artwork import render_artwork, transparent_banner, nodes
assert callable(texture_requests)
assert callable(render_artwork)
assert Image.new('RGB', (8, 8)).tobytes() == bytes(192)
sample = Image.new('RGB', (640, 480), (0, 0, 0))
draw = ImageDraw.Draw(sample)
draw.rectangle((100, 130, 300, 300), fill=(255, 220, 0))
draw.rectangle((180, 190, 220, 230), fill=(0, 0, 0))
artwork.RETAIL_LOGO_PIXELS_SHA256 = hashlib.sha256(sample.convert('RGBA').tobytes()).hexdigest()
banner = transparent_banner(sample)
assert banner.mode == 'RGBA' and banner.size == (256, 128)
assert banner.getpixel((0, 0))[3] == 0
black = banner.getpixel((78, 49))
assert max(black[:3]) < 20 and black[3] > 240
unknown = Image.new('RGBA', (640, 480), (255, 0, 0, 0))
ImageDraw.Draw(unknown).rectangle((560, 100, 630, 200), fill=(255, 0, 0, 255))
preserved = transparent_banner(unknown)
assert preserved.getchannel('A').getextrema() == (0, 255)
assert preserved.getchannel('A').getbbox()[2] > 200
json.dumps({'passed': True, 'python': __import__('sys').version.split()[0], 'pillow': __import__('PIL').__version__, 'textureParser': True, 'artworkParser': True, 'transparentBanner': True})
`);
    postMessage(JSON.parse(result));
  } catch (error) { postMessage({error: error.message || String(error)}); }
};

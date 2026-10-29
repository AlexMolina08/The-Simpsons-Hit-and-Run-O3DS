#!/usr/bin/env python3
"""Exercise local-only HOME Menu artwork extraction with original synthetic images."""
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_home_artwork import FRONTEND, LOGO, Node, frontend_logo, png_image, prepare, render_artwork, retail_logo_banner, transparent_banner


def string(value):
    raw = value.encode() + b'\0'
    return bytes([len(raw)]) + raw


def image_node(name, colour, width=4, height=4):
    payload = io.BytesIO()
    Image.new('RGBA', (width, height), colour).save(payload, format='PNG')
    raw = payload.getvalue()
    body = string(name) + struct.pack('<7I', 14000, width, height, 32, 0, 1, 1)
    return Node(0x19001, body, [Node(0x19002, struct.pack('<I', len(raw)) + raw, [])])


tiles = [image_node('gamelogo.png', (255, 0, 0, 255)),
         image_node('gamelogo.png', (0, 0, 255, 255))]
body = string('gamelogo.png') + struct.pack('<2I', 640, 480) + string('')
sprite = Node(0x19005, body + struct.pack('<4I', 4, 2, 2, 1), tiles)
frontend = Node(0xff443350, b'', [sprite]).encode()
logo = Node(0xff443350, b'', [image_node('logo.bmp', (0, 255, 0, 128))]).encode()
source_hashes = [hashlib.sha256(raw).hexdigest() for raw in (frontend, logo)]
assembled = frontend_logo(frontend)
assert assembled.size == (4, 2)
assert assembled.getpixel((0, 0)) == (255, 0, 0, 255)
assert assembled.getpixel((3, 0)) == (0, 0, 255, 255)
result = render_artwork(frontend, logo)
assert render_artwork(memoryview(frontend), memoryview(logo)) == result
assert set(result) == {'banner.png', 'icon.png'}
for name, expected in [('banner.png', (256, 128)), ('icon.png', (48, 48))]:
    with Image.open(io.BytesIO(result[name])) as image:
        assert image.size == expected and image.mode == ('RGBA' if name == 'banner.png' else 'RGB')
        if name == 'icon.png':
            assert image.getpixel((24, 24)) == (0, 128, 0)
assert source_hashes == [hashlib.sha256(raw).hexdigest() for raw in (frontend, logo)]

# Dark lettering enclosed by bright artwork must survive background removal.
sample = Image.new('RGB', (640, 480), (0, 0, 0))
draw = ImageDraw.Draw(sample)
draw.rectangle((100, 130, 300, 300), fill=(255, 220, 0))
draw.rectangle((180, 190, 220, 230), fill=(0, 0, 0))
banner = retail_logo_banner(sample)
banner.thumbnail((256, 128), Image.Resampling.LANCZOS)
assert banner.width <= 256 and banner.height == 128 and banner.mode == 'RGBA'
assert banner.getpixel((0, 0))[3] == 0
black = banner.getpixel((76, 49))
assert max(black[:3]) < 20 and black[3] > 240, black

# Unknown layouts must keep off-centre artwork and existing transparency.
unknown = Image.new('RGBA', (640, 480), (255, 0, 0, 0))
ImageDraw.Draw(unknown).rectangle((560, 100, 630, 200), fill=(255, 0, 0, 255))
preserved = transparent_banner(unknown)
assert preserved.getchannel('A').getextrema() == (0, 255)
assert preserved.getchannel('A').getbbox()[2] > 200

bad = image_node('logo.bmp', 'red')
bad.children[0].body = struct.pack('<I', len(bad.children[0].body)) + bad.children[0].body[4:]
try:
    png_image(bad)
except ValueError:
    pass
else:
    raise AssertionError('Accepted an out-of-bounds PNG payload')
for count in (1, 3):
    malformed = Node(0x19005, body + struct.pack('<4I', 4, 2, count, 1), tiles).encode()
    try:
        frontend_logo(malformed)
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted an invalid retail sprite tile count')

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    for relative, payload in [(FRONTEND, frontend), (LOGO, logo)]:
        target = root / 'input' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payload)
    metadata = prepare(root / 'input', root / 'output')
    assert json.loads((root / 'output/artwork-source.json').read_text()) == metadata
    assert set(metadata['sources']) == {FRONTEND.as_posix(), LOGO.as_posix()}
    assert set(p.name for p in (root / 'output').iterdir()) == {'banner.png', 'icon.png', 'artwork-source.json'}
    for name, payload in result.items():
        assert (root / 'output' / name).read_bytes() == payload
        assert metadata['outputs'][name]['sha256'] == hashlib.sha256(payload).hexdigest()
print('PASS: retail sprite tiles, transparent logo compositing, PNG bounds, local-only outputs and source provenance.')

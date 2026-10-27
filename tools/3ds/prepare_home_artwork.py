#!/usr/bin/env python3
"""Generate private HOME Menu artwork from the user's retail PC game files."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct

from PIL import Image, ImageOps

from skin_inventory import chunks, pstring


FRONTEND = Path('art/frontend/scrooby/frontend.p3d')
LOGO = Path('art/frontend/scrooby/resource/pure3d/logo_anim.p3d')


class Node:
    def __init__(self, kind, body, children):
        self.kind, self.body, self.children = kind, body, children

    def encode(self):
        children = b''.join(child.encode() for child in self.children)
        header = 12 + len(self.body)
        return struct.pack('<III', self.kind, header, header + len(children)) + self.body + children


def nodes(data, start=0, end=None):
    return [Node(kind, data[body:child], nodes(data, child, stop))
            for kind, body, child, stop in chunks(data, start, len(data) if end is None else end)]


def walk(tree):
    for node in tree:
        yield node
        yield from walk(node.children)


def png_image(node):
    """Decode the PNG in an ordinary Pure3D image chunk, with checked bounds."""
    if node.kind != 0x19001:
        raise ValueError('Expected a Pure3D image')
    _, offset = pstring(node.body, 0)
    fields = struct.unpack_from('<7I', node.body, offset)
    width, height = fields[1:3]
    if not 0 < width <= 4096 or not 0 < height <= 4096:
        raise ValueError('Unsupported HOME Menu source image dimensions')
    pieces = [child for child in node.children if child.kind == 0x19002]
    if len(pieces) != 1 or len(pieces[0].body) < 4:
        raise ValueError('Expected one Pure3D PNG payload')
    payload = pieces[0].body
    size, = struct.unpack_from('<I', payload)
    if size != len(payload) - 4 or not payload[4:].startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError('Invalid Pure3D PNG payload')
    with Image.open(io.BytesIO(payload[4:])) as image:
        if image.size != (width, height):
            raise ValueError('Pure3D image dimensions do not match its PNG')
        return image.convert('RGBA')


def frontend_logo(data):
    """Reassemble the original gamelogo sprite without its replicated tile borders."""
    matches = [node for node in walk(nodes(data))
               if node.kind == 0x19005 and pstring(node.body, 0)[0].lower() == 'gamelogo.png']
    if len(matches) != 1:
        raise ValueError('Retail frontend.p3d must contain one gamelogo.png sprite')
    sprite = matches[0]
    _, offset = pstring(sprite.body, 0)
    _, offset = pstring(sprite.body, offset + 8)
    width, height, count, border = struct.unpack_from('<4I', sprite.body, offset)
    pieces = [png_image(child) for child in sprite.children if child.kind == 0x19001]
    if (not 0 < width <= 4096 or not 0 < height <= 4096 or
            not 0 < count <= 128 or len(pieces) != count or border not in (0, 1)):
        raise ValueError('Invalid retail gamelogo sprite layout')
    if count == 1 and pieces[0].size == (width, height):
        border = 0
    canvas = Image.new('RGBA', (width, height))
    index, y = 0, 0
    while y < height - border:
        x, row_height = 0, None
        while x < width - border:
            if index >= count:
                raise ValueError('Retail gamelogo sprite has missing tiles')
            tile = pieces[index]
            tw, th = tile.size
            step_x, step_y = tw - 2 * border, th - 2 * border
            if min(step_x, step_y) <= 0 or row_height not in (None, step_y):
                raise ValueError('Retail gamelogo sprite has invalid tile dimensions')
            canvas.paste(tile.crop((border, border, tw - border, th - border)), (x, y))
            row_height = step_y
            x += step_x
            index += 1
        y += row_height
    if index != count:
        raise ValueError('Retail gamelogo sprite has unused tiles')
    return canvas


def square_logo(data):
    matches = [node for node in walk(nodes(data))
               if node.kind == 0x19001 and pstring(node.body, 0)[0].lower() == 'logo.bmp']
    if len(matches) != 1:
        raise ValueError('Retail logo_anim.p3d must contain one logo.bmp image')
    return png_image(matches[0])


def on_black(image):
    background = Image.new('RGBA', image.size, (0, 0, 0, 255))
    return Image.alpha_composite(background, image).convert('RGB')


def render_artwork(frontend_bytes, logo_bytes):
    """Return generated PNG bytes; usable by both native Python and browser Python."""
    # Pyodide converts Uint8Array to memoryview; the shared P3D reader uses bytes.
    frontend_bytes, logo_bytes = bytes(frontend_bytes), bytes(logo_bytes)
    frontend, square = frontend_logo(frontend_bytes), square_logo(logo_bytes)
    # Keep the retail logo's aspect ratio and the familiar wide HOME Menu framing.
    framed = on_black(frontend).crop((0, frontend.height * 2 // 15,
                                     frontend.width, frontend.height * 5 // 6))
    fitted = ImageOps.contain(framed, (256, 128), Image.Resampling.LANCZOS)
    banner = Image.new('RGB', (256, 128))
    banner.paste(fitted, ((256 - fitted.width) // 2, (128 - fitted.height) // 2))
    icon = ImageOps.pad(on_black(square), (48, 48), Image.Resampling.LANCZOS,
                        color=(0, 0, 0))
    result = {}
    for name, image in (('banner.png', banner), ('icon.png', icon)):
        encoded = io.BytesIO()
        image.save(encoded, format='PNG')
        result[name] = encoded.getvalue()
    return result


def prepare(data, output):
    data, output = Path(data), Path(output)
    source_bytes = {relative: (data / relative).read_bytes() for relative in (FRONTEND, LOGO)}
    artwork = render_artwork(source_bytes[FRONTEND], source_bytes[LOGO])
    output.mkdir(parents=True, exist_ok=True)
    metadata = {'sources': {}, 'outputs': {}}
    for relative, payload in source_bytes.items():
        metadata['sources'][relative.as_posix()] = hashlib.sha256(payload).hexdigest()
    for name, payload in artwork.items():
        path = output / name
        path.write_bytes(payload)
        with Image.open(io.BytesIO(payload)) as image:
            size = list(image.size)
        metadata['outputs'][name] = {'size': size, 'sha256': hashlib.sha256(payload).hexdigest()}
    (output / 'artwork-source.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.data, args.out), indent=2))

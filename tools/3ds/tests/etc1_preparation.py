#!/usr/bin/env python3
"""Check source binding, mip preservation, opacity and native pixel orientation."""
from pathlib import Path
import hashlib
import importlib.util
import struct
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('prepare_etc1', root / 'tools/3ds/prepare_etc1_textures.py')
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)
sys.path.insert(0, str(root / 'tools/3ds'))
from prepare_native_bundle import index_bundle


def fixture(transparent=False):
    payload = bytearray()
    for size in (16, 8):
        image = bytearray(size * size * 2)
        for y in range(size):
            for x in range(size):
                offset = (((y // 8) * (size // 8) + x // 8) * 64 + converter.morton(x & 7, y & 7)) * 2
                pixel = ((x & 15) << 12) | ((y & 15) << 8) | 0x4f
                if transparent and size == 8 and x == 3 and y == 2:
                    pixel &= ~15
                struct.pack_into('<H', image, offset, pixel)
        payload.extend(image)
    words = [0x3158544e, 1, 16, 16, 256, 16, 16, 3, 1, 50, len(payload)]
    prefix = struct.pack('<11I', *words) + hashlib.sha256(b'original encoded source').digest()
    return hashlib.sha256(prefix).hexdigest(), prefix + hashlib.sha256(payload).digest() + payload


key, body = fixture()
words, images = converter.levels(body, key)
assert [image.size for image in images] == [(16, 16), (8, 8)]
assert images[0].getpixel((13, 6)) == (221, 102, 68)
assert images[1].getpixel((3, 2)) == (51, 34, 68)
alpha_key, alpha_body = fixture(True)
assert converter.levels(alpha_body, alpha_key) is None, 'Transparent lower mip must exclude entire texture'
assert converter.world_source('sdcard/3ds/simpsons/art/l2r1.p3d')
assert converter.world_source('sdcard/3ds/simpsons/art/L1_TERRA.p3d')
for source in ('art/l02_fx.p3d', 'art/cars/l2r1.p3d', 'art/frontend/license.p3d', 'art/cards.p3d'):
    assert not converter.world_source(source)
for invalid in (body[:-1], body[:108] + bytes([body[108] ^ 1]) + body[109:]):
    try:
        converter.levels(invalid, key)
        raise AssertionError('Corrupt native payload accepted')
    except ValueError:
        pass
new_key, encoded = converter.convert(body, key, Path('/opt/devkitpro/tools/bin/tex3ds'))
new_words = struct.unpack_from('<11I', encoded)
assert new_words[1] == 2 and new_words[7] == 4
assert [new_words[i] for i in (2, 3, 4, 5, 6, 8, 9)] == [words[i] for i in (2, 3, 4, 5, 6, 8, 9)]
assert encoded[44:76] == body[44:76]
assert len(encoded) == 108 + (16 * 16 + 8 * 8) // 2
assert new_words[10] == len(encoded) - 108
assert hashlib.sha256(encoded[:76]).hexdigest() == new_key and new_key != key
assert hashlib.sha256(encoded[108:]).digest() == encoded[76:108]
assert converter.convert(alpha_body, alpha_key, Path('/opt/devkitpro/tools/bin/tex3ds')) is None
with tempfile.TemporaryDirectory(prefix='srr-etc1-bundle-') as directory:
    folder = Path(directory)
    bundle = folder / 'native146.bundle'
    bundle.write_bytes(struct.pack('<II', 0x314e4233, 1) + (new_key + '.ntx').encode() +
                       struct.pack('<II', len(encoded), zlib.crc32(encoded)) + encoded)
    try:
        index_bundle(bundle, folder / 'unexpected.hpp')
        raise AssertionError('Version-2 candidate accepted without explicit opt-in')
    except ValueError:
        pass
    assert not (folder / 'unexpected.hpp').exists()
    result = index_bundle(bundle, folder / 'bundle_index.hpp', 16, 'native146.bundle', True)
    assert result['allow_etc1'] and result['entries'] == 1
print('PASS: opaque-only whole-texture gate, source-bound version-2 identity, original mip dimensions, native orientation and integrity rejection. Visual/runtime acceptance remains pending.')

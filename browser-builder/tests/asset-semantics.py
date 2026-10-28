#!/usr/bin/env python3
"""Verify that semantic fallback permits PNG packaging differences only."""
from pathlib import Path
import importlib
import io
import struct
import sys
from PIL import Image, PngImagePlugin

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools/3ds'), str(ROOT/'browser-builder/engine-conversion')]
from prepare_character_detail import Node
fingerprint = importlib.import_module('asset-semantics').semantic_fingerprint


def png(pixel, compression, metadata=False):
    output = io.BytesIO()
    text = PngImagePlugin.PngInfo()
    text.add_text('test', 'lossless container metadata')
    Image.new('RGBA', (4, 3), pixel).save(output, format='PNG', compress_level=compression,
                                       pnginfo=text if metadata else None)
    return output.getvalue()


def asset(data, kind=0x19002, tail=b'', material=b'exact material'):
    return Node(0xff443350, b'', [Node(0x11000, material, []),
        Node(kind, struct.pack('<I', len(data)) + data + tail, [])]).encode()


first = asset(png((10, 20, 30, 255), 0))
repacked = asset(png((10, 20, 30, 255), 9, True))
assert first != repacked and fingerprint(first) == fingerprint(repacked)
assert fingerprint(first) != fingerprint(asset(png((11, 20, 30, 255), 9)))
assert fingerprint(first) != fingerprint(asset(png((10, 20, 30, 254), 9)))
assert fingerprint(first) != fingerprint(asset(png((10, 20, 30, 255), 0), material=b'exact materiam'))
assert fingerprint(first) != fingerprint(asset(png((10, 20, 30, 255), 0), tail=b'preserved'))
assert fingerprint(asset(png((10, 20, 30, 255), 0), kind=0x19003)) != \
       fingerprint(asset(png((10, 20, 30, 255), 9, True), kind=0x19003))
assert fingerprint(Node(0xff443350, b'', [Node(1, b'a', []), Node(2, b'b', [])]).encode()) != \
       fingerprint(Node(0xff443350, b'', [Node(2, b'b', []), Node(1, b'a', [])]).encode())
for malformed in (b'', first[:-1], asset(b'\x89PNG\r\n\x1a\nBAD')):
    try:
        fingerprint(malformed)
    except ValueError:
        pass
    else:
        raise AssertionError('Malformed asset accepted')
print('PASS: PNG compression/metadata normalize; RGB/alpha/material/tail/order/non-image changes differ; invalid P3D/PNG fail closed.')

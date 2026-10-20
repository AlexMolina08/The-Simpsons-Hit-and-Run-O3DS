#!/usr/bin/env python3
"""Prepare opt-in ETC1 world payloads from already filtered native mip levels.

This creates a separate version-2 candidate. Original assets and version-1
payloads remain untouched. Runtime support and visual/hardware acceptance are
required before deployment. Transparent pixels and non-world sources exclude
the entire texture, including every mip level.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

from PIL import Image

HEADER = struct.Struct('<11I')


def world_source(source):
    path = Path(source)
    return path.parent.name == 'art' and re.fullmatch(
        r'l[1-7](?:_terra|[irz][0-9].*)\.p3d', path.name.lower()) is not None


def morton(x, y):
    return ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
            ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))


def levels(body, key):
    if len(body) < 108:
        raise ValueError('Truncated native header')
    words = list(HEADER.unpack_from(body))
    if words[:2] != [0x3158544e, 1] or words[7] not in (1, 2, 3):
        raise ValueError('Expected a version-1 native color texture')
    if hashlib.sha256(body[:76]).hexdigest() != key:
        raise ValueError('Native source identity mismatch')
    if hashlib.sha256(body[108:]).digest() != body[76:108]:
        raise ValueError('Native payload hash mismatch')
    width, height, count = words[5], words[6], words[8]
    if (not 8 <= width <= 1024 or width & (width - 1) or
            not 8 <= height <= 1024 or height & (height - 1) or
            count > 7 or min(width, height) >> count < 8):
        raise ValueError('Invalid native mip dimensions')
    if words[10] != len(body) - 108:
        raise ValueError('Native payload size mismatch')
    images = []
    offset = 108
    for level in range(count + 1):
        w, h = width >> level, height >> level
        size = w * h * 2
        if offset + size > len(body):
            raise ValueError('Truncated native mip')
        rgb = bytearray(w * h * 3)
        for y in range(h):
            for x in range(w):
                tiled = ((y // 8) * (w // 8) + x // 8) * 64 + morton(x & 7, y & 7)
                pixel, = struct.unpack_from('<H', body, offset + tiled * 2)
                if words[7] == 1:
                    r, g, b = pixel >> 11, (pixel >> 5) & 63, pixel & 31
                    color = ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31)
                elif words[7] == 2:
                    if not pixel & 1:
                        return None
                    color = tuple((value * 255 + 15) // 31 for value in
                                  (pixel >> 11, (pixel >> 6) & 31, (pixel >> 1) & 31))
                else:
                    if pixel & 15 != 15:
                        return None
                    color = ((pixel >> 12) * 17, ((pixel >> 8) & 15) * 17, ((pixel >> 4) & 15) * 17)
                index = (y * w + x) * 3
                rgb[index:index + 3] = bytes(color)
        images.append(Image.frombytes('RGB', (w, h), bytes(rgb)))
        offset += size
    if offset != len(body):
        raise ValueError('Trailing native mip bytes')
    return words, images


def convert(body, key, encoder, previews=None):
    decoded = levels(body, key)
    if decoded is None:
        return None
    words, images = decoded
    payload = bytearray()
    with tempfile.TemporaryDirectory(prefix='srr-etc1-') as directory:
        root = Path(directory)
        for level, image in enumerate(images):
            source, target = root / f'{level}.png', root / f'{level}.bin'
            image.save(source)
            command = [str(encoder), '-f', 'etc1', '-q', 'high', '-r',
                       '-z', 'none', '-o', str(target)]
            if previews is not None:
                previews.mkdir(parents=True, exist_ok=True)
                image.save(previews / f'{key}-{level}-before.png')
                command += ['-p', str(previews / f'{key}-{level}-etc1.png')]
            subprocess.run(command + [str(source)], check=True)
            encoded = target.read_bytes()
            size = image.width * image.height // 2
            if encoded[:4] != bytes((0, size & 255, (size >> 8) & 255, (size >> 16) & 255)) or len(encoded) != size + 4:
                raise ValueError('Unexpected tex3ds raw ETC1 output')
            payload.extend(encoded[4:])
    words[1], words[7], words[10] = 2, 4, len(payload)
    prefix = HEADER.pack(*words) + body[44:76]
    result = prefix + hashlib.sha256(payload).digest() + payload
    return hashlib.sha256(prefix).hexdigest(), result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path, help='Existing version-1 preparation manifest')
    parser.add_argument('--out', type=Path, required=True, help='New candidate directory; must not exist')
    parser.add_argument('--encoder', type=Path, default=shutil.which('tex3ds') or '/opt/devkitpro/tools/bin/tex3ds')
    parser.add_argument('--limit', type=int, help='Bound opaque variants during visual development')
    parser.add_argument('--previews', action='store_true', help='Save input and decoded ETC1 mip previews for review')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    args.out.mkdir(parents=True, exist_ok=False)
    entries, excluded = {}, 0
    for key, entry in sorted(manifest['payloads'].items()):
        if not world_source(entry['source']):
            continue
        body = (args.manifest.parent / (key + '.ntx')).read_bytes()
        if hashlib.sha256(body).hexdigest() != entry['sha256']:
            raise ValueError('Preparation manifest does not match native payload')
        result = convert(body, key, args.encoder, args.out / 'previews' if args.previews else None)
        if result is None:
            excluded += 1
            continue
        converted_key, encoded = result
        (args.out / (converted_key + '.ntx')).write_bytes(encoded)
        entries[converted_key] = dict(source=entry['source'], original_key=key,
                                     source_sha256=entry['source_sha256'], sha256=hashlib.sha256(encoded).hexdigest(),
                                     bytes=len(encoded), width=entry['width'], height=entry['height'], levels=entry['levels'])
        if args.limit is not None and len(entries) >= args.limit:
            break
    if not entries:
        raise ValueError('No opaque world variants prepared')
    result = dict(version=2, format='ETC1', lossy=True, mip_filtering='preserved existing prepared levels',
                  runtime_acceptance=False, source_manifest_sha256=hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
                  encoder_sha256=hashlib.sha256(args.encoder.read_bytes()).hexdigest(),
                  transparent_variants_excluded=excluded, payloads=entries)
    (args.out / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(payloads=len(entries), transparent_variants_excluded=excluded,
                          bytes=sum(e['bytes'] for e in entries.values()), runtime_acceptance=False)))


if __name__ == '__main__':
    main()

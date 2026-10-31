#!/usr/bin/env python3
"""Prepare a private, exact native-payload cache for one level's streamed art."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

from skin_inventory import chunks

MAGIC, HEADER_BYTES, ENTRY_BYTES = 0x3143534e, 64, 40


def source_hashes(data, start=0, end=None):
    for kind, body, child, stop in chunks(data, start, len(data) if end is None else end):
        if kind == 0x19002:
            length = struct.unpack_from('<I', data, body)[0]
            image = data[body + 4:child]
            if length > len(image):
                raise ValueError('Truncated source image')
            if image.startswith(b'\x89PNG\r\n\x1a\n'):
                yield hashlib.sha256(image[:length]).digest()
            elif image.startswith(b'DDS ') and length >= 128:
                yield hashlib.sha256(image[128:length]).digest()
        yield from source_hashes(data, child, stop)


def prepare(art, bundle, level, output, max_bytes=8 * 1024 * 1024, selected_files=None):
    art, bundle, output = Path(art), Path(bundle), Path(output)
    if level not in range(1, 8) or output.exists():
        raise ValueError('Select a level and a new output directory')
    files = sorted(p for p in art.iterdir() if re.fullmatch(f'l{level}[rz].*\\.p3d', p.name, re.I))
    if selected_files is not None:
        requested = set(selected_files)
        if not requested or not requested <= {p.name for p in files}:
            raise ValueError('Select existing streamed files from this level')
        files = [p for p in files if p.name in requested]
    if not files:
        raise ValueError('No streamed level art found')
    sources, hashes = {}, set()
    for path in files:
        data = path.read_bytes()
        sources[path.name] = hashlib.sha256(data).hexdigest()
        hashes.update(source_hashes(data))
    selected, seen = [], set()
    with bundle.open('rb') as stream:
        magic, count = struct.unpack('<II', stream.read(8))
        if magic != 0x314e4233 or not 0 < count <= 32768:
            raise ValueError('Unsupported native bundle')
        for _ in range(count):
            filename = stream.read(68)
            if not re.fullmatch(b'[0-9a-f]{64}\\.ntx', filename):
                raise ValueError('Invalid native payload name')
            size, crc = struct.unpack('<II', stream.read(8))
            if not 108 <= size <= 8 * 1024 * 1024:
                raise ValueError('Native payload outside bounds')
            payload = stream.read(size)
            if len(payload) != size or zlib.crc32(payload) != crc:
                raise ValueError('Truncated or corrupted native payload')
            key = hashlib.sha256(payload[:76]).digest()
            if key.hex().encode() != filename[:64] or key in seen:
                raise ValueError('Duplicate or incorrect native key')
            seen.add(key)
            if hashlib.sha256(payload[108:]).digest() != payload[76:108]:
                raise ValueError('Native body hash mismatch')
            if struct.unpack_from('<I', payload, 40)[0] != size - 108:
                raise ValueError('Native body length mismatch')
            if payload[44:76] in hashes:
                selected.append((key, payload))
        if stream.read(1):
            raise ValueError('Trailing native bundle bytes')
    selected.sort()
    if not 0 < len(selected) <= 2048:
        raise ValueError('Level cache record count outside bounds')
    table, body, offset = [], [], 0
    for key, payload in selected:
        table.append(key + struct.pack('<II', offset, len(payload)))
        body.append(payload)
        offset += len(payload)
    body = b''.join(table + body)
    total = HEADER_BYTES + len(body)
    if total > max_bytes:
        raise ValueError('Level cache exceeds its normal-memory budget')
    header = struct.pack('<8I', MAGIC, 1, len(selected), ENTRY_BYTES,
                         HEADER_BYTES + ENTRY_BYTES * len(selected), total, level, 0)
    result = header + hashlib.sha256(body).digest() + body
    output.mkdir(parents=True)
    cache = output / f'level{level}.nsc'
    cache.write_bytes(result)
    report = dict(experimental=True, level=level, records=len(selected), bytes=len(result),
                  sha256=hashlib.sha256(result).hexdigest(),
                  bundle_sha256=hashlib.file_digest(bundle.open('rb'), 'sha256').hexdigest(),
                  source_files=sources, payloads_verbatim=True,
                  limitation='Prepared cache only; runtime admission, CPU overlap, pixels and performance remain unverified.')
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--level', type=int, choices=range(1, 8), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--files', nargs='+')
    args = parser.parse_args()
    result = prepare(args.art, args.bundle, args.level, args.output, selected_files=args.files)
    print(json.dumps({key: value for key, value in result.items() if key != 'source_files'}, indent=2))

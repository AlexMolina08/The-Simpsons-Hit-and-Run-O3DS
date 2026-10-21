#!/usr/bin/env python3
"""Rebuild the accepted opaque-world ETC1 bundle from the user's PC artwork."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import zlib

from prepare_native_textures import prepare
from prepare_etc1_textures import convert, world_source
from prepare_native_bundle import index_bundle


def prepare_release(data, output, encoder):
    output.mkdir(parents=True, exist_ok=True)
    profile_path = Path(__file__).with_name('native_texture_profile.json')
    profile = json.loads(profile_path.read_text())
    accepted = profile['source_variant_keys']
    # Regenerate from source on a clean build; cache only an identical artwork set
    # and encoder. An old developer's out/ directory must never be required.
    assets = sorted(path for path in (data / 'art').glob('*.p3d') if world_source(path))
    digest = hashlib.sha256(encoder.read_bytes())
    digest.update(profile_path.read_bytes())
    for path in assets:
        digest.update(path.name.encode())
        digest.update(path.read_bytes())
    for name in ('prepare_native_textures.py', 'prepare_etc1_textures.py', 'prepare_release_textures.py'):
        digest.update(Path(__file__).with_name(name).read_bytes())
    key = digest.hexdigest()
    bundle, stamp = output / 'native146.bundle', output / 'source.json'
    if bundle.is_file() and stamp.is_file():
        cached = json.loads(stamp.read_text())
        if cached.get('source_sha256') == key and cached.get('bundle_sha256') == hashlib.file_digest(bundle.open('rb'), 'sha256').hexdigest():
            return index_bundle(bundle, output / 'bundle_index.hpp', 16, bundle.name, True)
    with tempfile.TemporaryDirectory(prefix='srr-release-textures-') as temporary:
        root = Path(temporary)
        print(f'Preparing {len(assets)} world artwork files...', flush=True)
        manifest = prepare(assets, root / 'native', include_images=False)
        missing = set(accepted) - manifest['payloads'].keys()
        if missing:
            raise ValueError(f'Retail artwork differs from the accepted texture profile: {len(missing)} variants missing')
        entries = []
        for index, source_key in enumerate(accepted):
            result = convert((root / 'native' / (source_key + '.ntx')).read_bytes(), source_key, encoder)
            if result is None:
                raise ValueError('An accepted opaque texture became transparent')
            entries.append(result)
            if (index + 1) % 200 == 0:
                print(f'Textures: {index + 1}/{len(accepted)} prepared', flush=True)
        if not entries:
            raise ValueError('No opaque world textures were prepared')
        with (root / bundle.name).open('wb') as stream:
            stream.write(struct.pack('<II', 0x314e4233, len(entries)))
            for payload_key, body in entries:
                stream.write((payload_key + '.ntx').encode() + struct.pack('<II', len(body), zlib.crc32(body)) + body)
        if hashlib.file_digest((root / bundle.name).open('rb'), 'sha256').hexdigest() != profile['bundle_sha256']:
            raise ValueError('Prepared texture bytes differ from the accepted baseline; check the tex3ds version and retail files')
        shutil.copy2(root / bundle.name, bundle)
    report = index_bundle(bundle, output / 'bundle_index.hpp', 16, bundle.name, True)
    stamp.write_text(json.dumps({'source_sha256': key, 'bundle_sha256': report['sha256']}, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--encoder', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare_release(args.data, args.out, args.encoder)), flush=True)

#!/usr/bin/env python3
"""Check browser post-lighting residency output against the physical fixture."""
import argparse
import importlib.util
from pathlib import Path
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/3ds'))
spec = importlib.util.spec_from_file_location('browser_assets', root/'browser-builder/engine-conversion/v12-assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--expected', type=Path, required=True)
parser.add_argument('--source-art', type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='browser-residency-') as directory:
    work = Path(directory)
    levels = []
    for level in '1234567':
        output = work/level/'world-residency'
        metadata = assets._prepare_resident_level(
            args.art, args.art, work/('prepare-'+level), output, level,
            assets._choose_landmarks(args.source_art, level))
        levels.append(metadata)
        for name in metadata['files']:
            assert (output/name).read_bytes() == (args.expected/name).read_bytes(), name
    import json
    bundle = json.loads((root/'tools/3ds/native_texture_profile.json').read_text())['bundle_sha256']
    final = work/'final'
    assets._finalize_residency(levels, final, bundle)
    for name in ('landmark-visibility.bin', 'asset-admission.bin'):
        assert (final/name).read_bytes() == (args.expected/name).read_bytes(), name
    for bad in (None, levels[:-1], levels+[levels[0]]):
        rejected = work/'rejected'
        try:
            assets._finalize_residency(bad, rejected, bundle)
            raise AssertionError('An incomplete or duplicate set was accepted')
        except ValueError:
            pass
        assert not rejected.exists()
print('PASS: browser post-lighting preparation matches all 88 physical fixture files; incomplete/duplicate maps cannot publish an admission marker.')

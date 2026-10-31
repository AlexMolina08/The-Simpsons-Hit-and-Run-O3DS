#!/usr/bin/env python3
"""Check complete-set parity and transactional failure using the user's artwork."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_world_residency import prepare_all

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--overlay', type=Path)
parser.add_argument('--expected', type=Path, required=True)
args = parser.parse_args()
sources = {}
for folder in (args.art, args.overlay):
    if folder:
        sources.update({p.name: p for p in folder.iterdir()
                        if re.fullmatch(r'l[1-7]([rz].*|_terra)\.p3d', p.name, re.I)})
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.values()}
with tempfile.TemporaryDirectory(prefix='world-residency-set-') as directory:
    root = Path(directory)
    output = root/'complete'
    result = prepare_all(args.art, args.overlay, output)
    assert len(result['levels']) == 7 and len(result['files']) == 88
    assert not (output/'art/l3r5_dam.p3d').exists() and not (output/'art/l6r5_dam.p3d').exists()
    for name, entry in result['files'].items():
        data = (output/name).read_bytes()
        assert len(data) == entry['bytes'] and hashlib.sha256(data).hexdigest() == entry['sha256']
        if name != 'asset-admission.bin':
            assert data == (args.expected/name).read_bytes(), name
    try:
        prepare_all(args.art, args.overlay, output)
        raise AssertionError('Existing output was overwritten')
    except ValueError:
        pass
    incomplete = root/'incomplete'
    incomplete.mkdir()
    for name, source in sources.items():
        if name.lower() != 'l7_terra.p3d':
            (incomplete/name).symlink_to(source.resolve())
    failed = root/'failed'
    try:
        prepare_all(incomplete, None, failed)
        raise AssertionError('Missing seventh level was accepted')
    except ValueError:
        pass
    assert not failed.exists() and not list(root.glob('world-residency-*'))
assert before == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.values()}
print('PASS: 87 files match the hardware fixture; existing outputs and original assets stay intact; late preparation failure publishes no partial set.')

#!/usr/bin/env python3
"""Prepare the complete loose-data subtree while preserving original game files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile

from prepare_world_residency import prepare_all

GENERATOR = 'tools/3ds/prepare_world_residency.py'


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def install(data, native_bundle):
    data, native_bundle = Path(data), Path(native_bundle)
    target = data/'world-residency'
    if target.is_symlink() or (target.exists() and not target.is_dir()):
        raise ValueError('The world-residency target must be a real generated directory')
    with tempfile.TemporaryDirectory(prefix='.world-residency-', dir=data) as temporary:
        work = Path(temporary)
        prepared = work/'prepared'
        report = prepare_all(data/'art', None, prepared,
                             native_bundle_sha256=digest(native_bundle))
        if target.exists():
            # Only replace this tool's complete generated subtree. User files,
            # symlinks, original artwork and saves never belong to that set.
            try:
                previous = json.loads((target/'manifest.json').read_text())
            except (OSError, ValueError) as error:
                raise ValueError('Existing world-residency directory has no valid ownership manifest') from error
            inventory = set(report['files']) | {'manifest.json'}
            contents = list(target.rglob('*'))
            if (previous.get('generated_by') != GENERATOR or
                    any(path.is_symlink() for path in contents) or
                    {path.relative_to(target).as_posix() for path in contents if path.is_file()} != inventory or
                    {path.relative_to(target).as_posix() for path in contents if path.is_dir()} != {'art'}):
                raise ValueError('Existing world-residency directory contains unowned files; nothing was replaced')
            if (previous.get('files') == report['files'] and
                    all(path.stat().st_size == report['files'][name]['bytes'] and
                        digest(path) == report['files'][name]['sha256']
                        for name in report['files'] for path in (target/name,))):
                return {'output': str(target), 'files': len(report['files']), 'changed': False}
            os.rename(target, work/'previous')
        try:
            os.rename(prepared, target)
        except OSError:
            if (work/'previous').exists():
                os.rename(work/'previous', target)
            raise
    return {'output': str(target), 'files': len(report['files']), 'changed': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--native-bundle', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(install(args.data, args.native_bundle), indent=2))

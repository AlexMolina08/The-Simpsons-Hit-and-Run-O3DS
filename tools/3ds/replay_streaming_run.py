#!/usr/bin/env python3
"""Repeat a recorded physical experiment with verified artifacts and explicit overrides."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def prepare(manifest_path, trial, output, overrides, reuse=False):
    record = json.loads(manifest_path.read_text())
    for name, digest in (('artifact', 'sha256'), ('elf', 'elf_sha256')):
        path = Path(record[name])
        if not path.is_file() or hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() != record[digest]:
            raise ValueError(f'Recorded {name} is missing or changed: {path}')
    arguments = []
    keys = set()
    for argument in record['arguments']:
        key = argument.split('=', 1)[0]
        if key in ('bench_trial', 'dev_receiver'):
            continue
        if key in keys:
            raise ValueError(f'Ambiguous recorded argument: {key}')
        keys.add(key)
        arguments.append(argument)
    changed = {}
    for argument in overrides:
        if '=' not in argument or '\n' in argument or '\r' in argument or '\0' in argument:
            raise ValueError('Use one key=value per override')
        key = argument.split('=', 1)[0]
        if key in ('bench_trial', 'dev_receiver') or key in changed:
            raise ValueError(f'Reserved or repeated override: {key}')
        prior = next((a for a in arguments if a.split('=', 1)[0] == key), None)
        changed[key] = dict(before=prior, after=argument)
        if prior is None:
            arguments.append(argument)
        else:
            arguments[arguments.index(prior)] = argument
    command = [sys.executable, str(Path(__file__).with_name('dev.py')),
               '--host', record['host'], '--port', str(record.get('port', 80)),
               'run', record['artifact'], '--elf', record['elf'],
               '--output', str(output), '--keep-candidate', '--timeout', '600']
    if reuse:
        command += ['--reuse-remote', record['remote_path']]
    for argument in arguments + ['bench_trial=' + trial]:
        command += ['--arg', argument]
    return command, dict(parent_manifest=str(manifest_path.resolve()),
        parent_manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        parent_trial=record['trial'], trial=trial, changes=changed,
        limitation='A repeated command does not establish matched movement, camera, workload or visible pixels. Inspect the resulting trace and images.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--trial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--arg', action='append', default=[])
    parser.add_argument('--reuse', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    command, provenance = prepare(args.manifest, args.trial, args.output, args.arg, args.reuse)
    if args.dry_run:
        print(json.dumps(dict(command=command, provenance=provenance), indent=2))
    else:
        if args.output.exists():
            parser.error('Choose a new output directory; preserve existing evidence')
        result = subprocess.run(command)
        if args.output.is_dir():
            (args.output / 'replay-provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        raise SystemExit(result.returncode)

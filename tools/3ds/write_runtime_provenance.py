#!/usr/bin/env python3
"""Record a normal v1.2 build for local browser packaging, without publishing."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(elf, cache, native_index, output):
    settings = cache.read_text()
    for flag in ('NATIVE_BENCH', 'OPTIONAL_DIAG', 'CAUSAL_DIAG', 'MEMORY_DIAG',
                 'COARSE_POOLS', 'RECURRING_PROFILE', 'MAIN_PROFILE',
                 'SUBMIT_PROFILE', 'VEHICLE_PROFILE', 'GPU_SKIN_AB', 'GPU_SKIN_METRICS'):
        if not re.search(r'^SRR2_OLD3DS_' + flag + r':BOOL=OFF$', settings, re.M):
            raise ValueError(f'Normal runtime requires {flag}=OFF')
    if re.search(r'^SRR2_OLD3DS_RUNTIME_PROFILE:BOOL=ON$', settings, re.M):
        raise ValueError('Normal runtime requires runtime profiling disabled')
    dirty = subprocess.check_output(['git', 'status', '--porcelain'],
                                    cwd=ROOT, text=True)
    budget = json.loads(subprocess.check_output([sys.executable,
        str(ROOT/'tools/3ds/check_old3ds_memory.py'), str(elf)], text=True))
    output.mkdir(parents=True, exist_ok=True)
    target = output/'SRR2.elf'
    if target.resolve() != elf.resolve():
        shutil.copy2(elf, target)
    target_cache = output/'CMakeCache.txt'
    if target_cache.resolve() != cache.resolve():
        shutil.copy2(cache, target_cache)
    report = dict(version='1.2', source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        elf=str(target.resolve()), elf_sha256=digest(target),
        cmake_cache_sha256=digest(target_cache), accepted_native_index_sha256=digest(native_index),
        native_bench=False, runtime_profile=False, receiver_hook=False,
        early_opaque_queue=True, visual_features_default=255, streaming_policy=1,
        source_tree_dirty=bool(dirty),
        fixed_startup_budget=budget, release_elf_hardware_exercised=False,
        status='Normal build; partial streaming improvements; remaining scenery gaps documented; not published')
    (output/'runtime-provenance.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--native-index', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.elf, args.cache, args.native_index, args.out), indent=2))

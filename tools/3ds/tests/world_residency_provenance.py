#!/usr/bin/env python3
"""Reject browser/runtime residency mismatches before packaging any assets."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('recipe', root/'browser-builder/scripts/prepare-v12-manifest.py')
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)

with tempfile.TemporaryDirectory(prefix='world-provenance-test-') as directory:
    work = Path(directory)
    elf, cache, native, provenance = (work/name for name in
                                     ('fixture.elf', 'CMakeCache.txt', 'bundle_index.hpp', 'runtime-provenance.json'))
    # A metadata-parser fixture, not an executable or console validation result.
    elf.write_bytes(struct.pack('<16sHH', b'\x7fELF\x01', 2, 40))
    native.write_bytes(b'fixture native index')
    off = ('NATIVE_BENCH', 'OPTIONAL_DIAG', 'CAUSAL_DIAG', 'MEMORY_DIAG', 'COARSE_POOLS',
           'RECURRING_PROFILE', 'MAIN_PROFILE', 'SUBMIT_PROFILE', 'VEHICLE_PROFILE',
           'GPU_SKIN_AB', 'GPU_SKIN_METRICS', 'COLLVOL_VERIFY')
    on = ('VISUAL_UPGRADE', 'DISTANCE_HAZE', 'CHARACTER_FORM', 'NATIVE_TEXTURES',
          'NATIVE_ETC1', 'NATIVE_TEXTURE_BUNDLE', 'PREPARED_AUDIO', 'SPRITE_ATLAS')
    settings = '\n'.join(['SRR2_REGION:STRING=PAL', 'SRR2_MICROPROFILE:BOOL=OFF',
                          'SRR2_OLD3DS_EXPERIMENT_ID:STRING='] +
                         ['SRR2_OLD3DS_'+flag+':BOOL=OFF' for flag in off] +
                         ['SRR2_OLD3DS_'+flag+':BOOL=ON' for flag in on])+'\n'
    metadata = dict(native_bench=False, runtime_profile=False, receiver_hook=False,
                    visual_features_default=255, early_opaque_queue=True, version='1.2',
                    streaming_policy=1, language_config_version=1, languages=['en', 'fr', 'es'],
                    source_tree_dirty=False, source_commit='a'*40, elf=str(elf),
                    elf_sha256=recipe.digest(elf), accepted_native_index_sha256=recipe.digest(native),
                    fixed_startup_budget={'application_budget': 64*1024*1024, 'pass_budget': True})

    def check(extra='', fields=None, valid=True):
        cache.write_text(settings+extra)
        entry = dict(metadata, **(fields or {}), cmake_cache_sha256=recipe.digest(cache))
        provenance.write_text(json.dumps(entry))
        try:
            recipe.validate_runtime(provenance, native)
        except ValueError:
            assert not valid
        else:
            assert valid

    enabled = ('SRR2_OLD3DS_WORLD_RESIDENCY:BOOL=ON\n'
               'SRR2_OLD3DS_RESIDENCY_ROOT:STRING=romfs:/world-residency\n')
    fields = {'world_residency': True, 'world_residency_format': 1}
    check()  # Existing recipes without residency remain supported.
    check(enabled, valid=False)
    check('', fields, valid=False)
    check(enabled, fields)
    check(enabled, dict(fields, world_residency_format=2), valid=False)
    check(enabled.replace('romfs:/world-residency', 'sdmc:/private-test'), fields, valid=False)
    original_elf = elf.read_bytes()
    for marker in (b'NATIVE_MEMORY diagnostic=', b'NATIVE_MEMORY_ALLOCATOR_MAXIMA',
                   b'NATIVE_CAPTURE_END', b'PASSIVE_FRAME ms=', b'NATIVE_RESIDENCY_CPU_PROBE', b'NATIVE_TEXTURE_COALESCED', b'NATIVE_PARKED_REMOVAL', b'NATIVE_SUSPENSION_PROBE', b'NATIVE_MISSION_NPC_PROBE', b'NATIVE_MISSION_MODELS'):
        elf.write_bytes(original_elf+marker)
        check(enabled, dict(fields, elf_sha256=recipe.digest(elf)), valid=False)
    elf.write_bytes(original_elf)
    check(enabled, fields)

print('PASS: legacy runtime support, matching production feature, missing metadata, feature mismatch, unsupported format and private-root rejection.')

#!/usr/bin/env python3
"""Audit the actual original-3DS release ELF and its preprocessed gameplay paths."""
import argparse
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
args = parser.parse_args()
build = args.build.resolve()
cache = (build/'CMakeCache.txt').read_text()
for flag in ('SRR2_MICROPROFILE', *('SRR2_OLD3DS_'+name for name in (
        'NATIVE_BENCH', 'OPTIONAL_DIAG', 'CAUSAL_DIAG', 'MEMORY_DIAG', 'COARSE_POOLS',
        'RECURRING_PROFILE', 'MAIN_PROFILE', 'SUBMIT_PROFILE', 'VEHICLE_PROFILE',
        'GPU_SKIN_AB', 'GPU_SKIN_METRICS', 'COLLVOL_VERIFY'))):
    assert re.search(r'^'+flag+r':BOOL=OFF$', cache, re.M), flag
assert re.search(r'^SRR2_OLD3DS_EXPERIMENT_ID:STRING=$', cache, re.M)
for feature in ('VISUAL_UPGRADE', 'DISTANCE_HAZE', 'CHARACTER_FORM', 'NATIVE_TEXTURES',
                'NATIVE_ETC1', 'NATIVE_TEXTURE_BUNDLE', 'PREPARED_AUDIO', 'SPRITE_ATLAS'):
    assert re.search(r'^SRR2_OLD3DS_'+feature+r':BOOL=ON$', cache, re.M), feature
elf = (build/'code/SRR2.elf').read_bytes()
for marker in (b'OLD3DS_PROFILE', b'OLD3DS_DUMP ', b'OLD3DS_DEV_RETURN', b'bench_route=',
               b'NATIVE_EARLY_QUEUE', b'NATIVE_BUILD ', b'GPU_SKIN_SUMMARY'):
    assert marker not in elf, marker

commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], text=True)
suffixes = ('/main/game.cpp', '/main/win32main.cpp', '/pddi/pica/display.cpp',
            '/pddi/pica/context.cpp', '/simcollision/collisiondetector.cpp',
            '/gameplay/gameplaycontext.cpp', '/RenderManager/WorldRenderLayer.cpp')
checked = set()
for line in commands.splitlines():
    words = shlex.split(line)
    if '-c' not in words:
        continue
    source = words[words.index('-c')+1]
    suffix = next((x for x in suffixes if source.endswith(x)), None)
    if not suffix:
        continue
    compiler = next(i for i, x in enumerate(words) if x.endswith('arm-none-eabi-g++'))
    flags = []
    skip = False
    for word in words[compiler+1:]:
        if skip:
            skip = False
            continue
        if word in ('-c', '-o', '-MF', '-MT', '-MQ'):
            skip = True
            continue
        if word in ('-MD', '-MMD', '-MP'):
            continue
        flags.append(word)
    text = subprocess.check_output([words[compiler], *flags, '-E', '-P', source], cwd=build, text=True)
    assert 'Old3dsDumpReports(' not in text and 's_old3dsReports' not in text, suffix
    assert 'NativeBench::' not in text, suffix
    assert not re.search(r'g_old3ds(?:UpdateStages|RenderPhases)\[[^\]]+\]\s*\+=', text), suffix
    assert not re.search(r'g_picaSkinMetrics\.paletteTicks\s*\+=', text), suffix
    if suffix.endswith('display.cpp'):
        assert 'picaEarlyQueue::Start(&ctx->gxQueue)' in text
        assert 'state.completed.fetch_add' not in text
    if suffix.endswith('collisiondetector.cpp'):
        assert 'svcGetSystemTick(' not in text.split('static void UpdateVolumeForPass', 1)[1].split('// collision distance', 1)[0]
    if suffix == '/pddi/pica/context.cpp':
        assert 'if(stereoWorldPass && !bottomPass)display->SubmitWorldEarly();' in text
    checked.add(suffix)
assert checked == set(suffixes), checked
print('PASS: release ELF has no benchmark/receiver/report paths; actual ARM preprocessing excludes sampled timers and periodic captures.')
print('PASS: accepted visual/asset features remain ON; production early stereo queue remains enabled without benchmark controls or counters.')

#!/usr/bin/env python3
"""Compare the real ARM renderer layout under its producer and consumer flags."""
from pathlib import Path
import argparse
import json
import re
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
args = parser.parse_args()
build = args.build.resolve()
commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], text=True)
sources = {
    'renderer': '/pddi/pica/context.cpp',
    'vehicle': '/redbrick/geometryvehicle.cpp',
    'character': '/character/characterrenderable.cpp',
}
selected = {}
for line in commands.splitlines():
    words = shlex.split(line)
    if '-c' not in words:
        continue
    source = words[words.index('-c') + 1]
    for name, suffix in sources.items():
        if source.endswith(suffix):
            selected[name] = words
assert selected.keys() == sources.keys(), 'Missing renderer or game compile command'

probe = r'''
#include <pddi/pica/context.hpp>
struct RendererLayoutProbe : picaContext {
    static constexpr unsigned WorldPass() {
        return __builtin_offsetof(RendererLayoutProbe, stereoWorldPass);
    }
    static constexpr unsigned BottomPass() {
        return __builtin_offsetof(RendererLayoutProbe, bottomPass);
    }
#ifdef SRR2_OLD3DS_WHEEL_LOD
    static constexpr unsigned WheelScale() {
        return __builtin_offsetof(RendererLayoutProbe, wheelLodPixelScale);
    }
#endif
};
unsigned char renderer_context_size[sizeof(picaContext)];
unsigned char renderer_world_pass[RendererLayoutProbe::WorldPass() + 1];
unsigned char renderer_bottom_pass[RendererLayoutProbe::BottomPass() + 1];
#ifdef SRR2_OLD3DS_WHEEL_LOD
unsigned char renderer_wheel_scale[RendererLayoutProbe::WheelScale() + 1];
#endif
'''
results = {}
with tempfile.TemporaryDirectory(prefix='srr-renderer-abi-') as directory:
    folder = Path(directory)
    source = folder / 'probe.cpp'
    source.write_text(probe)
    for name, words in selected.items():
        compiler_index = next(i for i, word in enumerate(words) if word.endswith('arm-none-eabi-g++'))
        compiler = Path(words[compiler_index])
        # Use each production command's real definitions, include paths and ABI.
        # Keep generated objects/dependency files outside the configured build.
        flags = []
        skip = False
        for word in words[compiler_index + 1:]:
            if skip:
                skip = False
                continue
            if word in ('-c', '-o', '-MF', '-MT', '-MQ'):
                skip = True
                continue
            if word in ('-MD', '-MMD', '-MP') or word.startswith(('-O', '-flto')):
                continue
            flags.append(word)
        output = folder / (name + '.o')
        # Access control is disabled only for this layout probe, not game code.
        subprocess.run([str(compiler), *flags, '-O0', '-fno-lto', '-fno-access-control', '-Wno-invalid-offsetof',
                        '-c', str(source), '-o', str(output)], cwd=build, check=True)
        symbols = subprocess.check_output([str(compiler.with_name('arm-none-eabi-nm')),
                                           '-S', '--defined-only', str(output)], text=True)
        sizes = {match[2]: int(match[1], 16) for match in
                 re.finditer(r'^\S+\s+([0-9a-fA-F]+)\s+\w\s+(renderer_\w+)$', symbols, re.M)}
        assert len(sizes) >= 3, 'Layout probe emitted no usable ARM symbols'
        results[name] = {key: size if key == 'renderer_context_size' else size - 1
                         for key, size in sizes.items()}
print(json.dumps(results, indent=2))
assert all(layout == results['renderer'] for layout in results.values()), \
    'Renderer and game disagree on class size/member offsets; inline access is unsafe'
print('PASS: actual ARM renderer, vehicle and character layouts match, including inline wheel access.')

#!/usr/bin/env python3
"""Legacy map preferences must not restore a second HUD mode."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='srr-dual-settings-') as temporary:
    path = Path(temporary)
    settings = path / 'srr2_settings.ini'
    settings.write_text('notimers=1\nmaptop=1\n')
    source = path / 'test.cpp'
    source.write_text('''
#include <cheats/portsettings.h>
#include <cassert>
int main(){
    assert(PortSettings::NoTimers());
    PortSettings::SetNoTimers(false);
    PortSettings::Save();
    assert(!PortSettings::NoTimers());
}
''')
    binary = path / 'test'
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root / 'code'), str(source),
                    str(root / 'code/cheats/portsettings.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], cwd=path, check=True)
    assert settings.read_text() == 'notimers=0\n'
assert 'MapOnTop' not in (root / 'code/presentation/gui/utility/hudmap.cpp').read_text()
assert 'MENU_ITEM_MAP' not in (root / 'code/presentation/gui/frontend/guiscreencheats.cpp').read_text()
print('PASS: legacy map preference ignored, remaining settings preserved, single lower-screen HUD path.')

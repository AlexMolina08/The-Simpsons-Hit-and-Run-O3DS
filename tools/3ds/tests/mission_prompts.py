#!/usr/bin/env python3
"""Match the 3DS prompt enum against the PC project used by the port."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/presentation/gui/guiscreenprompt.h').read_text()
begin = source.index('enum ePromptQuestion')
enum = source[begin:source.index('};', begin) + 2]
project = root / 'sdcard/3ds/simpsons/art/frontend/scrooby/ingame.p3d'
if not project.is_file():
    raise SystemExit('Missing local PC ingame.p3d fixture; no retail assets are included in this test.')
data = project.read_bytes()
start = data.index(b'MSG_CONFIRM_QUIT')
end = data.index(b'INGAME_MESSAGE_19', start) + len(b'INGAME_MESSAGE_19')
strings = [name.decode() for name in re.findall(rb'(?:MSG_|INGAME_MESSAGE_)[A-Z0-9_()]+', data[start:end])]
assert len(strings) == 75 and len(set(strings[-20:])) == 20
program = '#include <cstdio>\n' + enum + r'''
int main() {
 printf("%d %d %d %d %d %d\n", NUM_PROMPT_QUESTIONS, PROMPT_LOAD_CARD_EMPTY_PC,
        PROMPT_HD_FULL_XBOX, PROMPT_CONFIRM_RESTOREALLDEFAULTS,
        PROMPT_CONFIRM_QUIT_TO_SYSTEM, PROMPT_CONFIRM_SAVE_BEFORE_QUITTOSYSTEM);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-mission-prompts-') as temporary:
    folder = Path(temporary)
    (folder / 'test.cpp').write_text(program)
    for platform in ['RAD_3DS', 'RAD_PC']:
        subprocess.run(['clang++', '-std=c++17', '-D' + platform,
                        str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
        indices = list(map(int, subprocess.check_output([str(folder / 'test')], text=True).split()))
        assert indices == [55, 40, 49, 50, 51, 52], (platform, indices)
        for message in range(20):
            assert strings[indices[0] + message] == f'INGAME_MESSAGE_{message:02}'
        assert strings[indices[1]] == 'MSG_LOAD_CARD_EMPTY_(PC)'
        assert strings[indices[2]] == 'MSG_HARDDISK_FULL_(XBOX)'
        assert strings[indices[3]] == 'MSG_CONFIRM_RESTOREALLDEFAULTS'
        assert strings[indices[4]] == 'MSG_CONFIRM_QUIT_TO_SYSTEM'
        assert strings[indices[5]] == 'MSG_CONFIRM_SAVE_BEFORE_QUIT'
# The former 3DS value selected the exact false warning reported at the Plow King gate.
assert strings[49] == 'MSG_HARDDISK_FULL_(XBOX)' and strings[55] == 'INGAME_MESSAGE_00'
print('PASS: all 20 mission prompts match retail PC indices; truck instruction no longer selects the hard-disk warning; PC enum unchanged.')

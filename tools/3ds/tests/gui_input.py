#!/usr/bin/env python3
"""Check title-menu message routing and the existing camera-input regression harness."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/presentation/gui/frontend/guiscreenmainmenu.cpp').read_text()
start = source.index('            eGuiMessage menuMessage = message;')
end = source.index('            m_pMenu->HandleMessage( menuMessage, param1, param2 );', start)
relay = source[start:end + len('            m_pMenu->HandleMessage( menuMessage, param1, param2 );')]
program = r'''
#include <cassert>
#include <cstdio>
enum eGuiMessage { GUI_MSG_CONTROLLER_UP, GUI_MSG_CONTROLLER_DOWN,
    GUI_MSG_CONTROLLER_LEFT, GUI_MSG_CONTROLLER_RIGHT, GUI_MSG_CONTROLLER_SELECT,
    GUI_MSG_CONTROLLER_BACK };
const unsigned MENU_ITEM_MAIN_MENU=0;
struct Menu {
    unsigned selection=0, first=0, second=0;
    eGuiMessage received=GUI_MSG_CONTROLLER_BACK;
    unsigned GetSelection() { return selection; }
    void HandleMessage(eGuiMessage message,unsigned p1,unsigned p2) { received=message; first=p1; second=p2; }
};
void Relay(Menu* m_pMenu,eGuiMessage message,unsigned param1,unsigned param2) {
''' + relay + r'''
}
int main() {
    for(unsigned selection : {0u,1u}) for(auto input : {GUI_MSG_CONTROLLER_UP,
        GUI_MSG_CONTROLLER_DOWN,GUI_MSG_CONTROLLER_LEFT,GUI_MSG_CONTROLLER_RIGHT,
        GUI_MSG_CONTROLLER_SELECT,GUI_MSG_CONTROLLER_BACK}) {
        Menu menu; menu.selection=selection;
        Relay(&menu,input,123,456);
        const auto expected=selection==0 && input==GUI_MSG_CONTROLLER_UP ? GUI_MSG_CONTROLLER_LEFT :
            selection==0 && input==GUI_MSG_CONTROLLER_DOWN ? GUI_MSG_CONTROLLER_RIGHT : input;
        assert(menu.received==expected && menu.first==123 && menu.second==456);
    }
    puts("PASS: production title-menu vertical routing, other controls and message parameters.");
}
'''
program = '#include <initializer_list>\n' + program
with tempfile.TemporaryDirectory(prefix='srr-gui-input-') as temporary:
    path = Path(temporary)
    (path / 'title.cpp').write_text(program)
    for name, source in [('title', path / 'title.cpp'), ('camera', root / 'tools/3ds/tests/camera_input.cpp')]:
        binary = path / name
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(root),
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

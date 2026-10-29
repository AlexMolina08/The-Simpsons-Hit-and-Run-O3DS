#!/usr/bin/env python3
"""Check actual Unicode controls and hardcoded-label replacement boundaries."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
code = r'''
#include <cassert>
#include <string>
#include <vector>
#include <initializer_list>
#include "libs/scrooby/src/3dscontrols.h"
using namespace Scrooby3DSControls;
std::u16string decode(const char* text) {
    std::vector<unsigned short> buffer(Length(text)+1,0);
    assert(Copy(buffer.data(),text)==Length(text));
    std::u16string value;for(auto c:buffer)if(c)value+=char16_t(c);return value;
}
int main(){
    const unsigned short original[]={u'N',u'O',u'E',u'L',0};
    for(char language:{'E','F','S'}) {
        for(auto key:{"TUTORIAL_000","TUTORIAL_002","TUTORIAL_003","TUTORIAL_004","TUTORIAL_009","TUTORIAL_011","TUTORIAL_013","CHECKING_MEMORY_CARDS_(PC)","PRESS_START","PRESS_START_GC","PRESS_START_PS2","PRESS_START_XBOX"}) {
            auto text=Replacement(Hash(key,1048573),1048573,original,language);
            assert(text && !decode(text).empty() && Length(text)<256);
            if(std::string(key).find("PRESS_START")==0) {
                assert(std::string(text).find("START")!=std::string::npos);
                assert(std::string(text).find("+")==std::string::npos);
            }
        }
        assert(!Replacement(Hash("MISSION_NARRATIVE",1048573),1048573,original,language));
    }
    assert(decode(Replacement(Hash("TUTORIAL_003",1048573),1048573,original,'F')).find(u"ACCÉLÉRER")!=std::u16string::npos);
    assert(decode(Replacement(Hash("TUTORIAL_002",1048573),1048573,original,'S')).find(u"BOTÓN")!=std::u16string::npos);
    assert(decode(Caption("MAIN MENU",'F'))==u"MENU PRINCIPAL");
    assert(decode(Caption("MAIN MENU",'S'))==u"MENÚ PRINCIPAL");
    const char* guide="A: Jump / gas\nB: Run / brake\nX: Interact / car\nY: Attack\nR: Handbrake\nPad: Move / steer";
    assert(decode(Caption(guide,'F')).find(u"accélérer")!=std::u16string::npos);
    assert(decode(Caption(guide,'S')).find(u"Acción")!=std::u16string::npos);
    const char* camera="L + Pad: Camera\nD-pad: Move / steer\nSELECT: Reset car\nL + SELECT: View\nL + R: Horn\nSTART: Pause";
    assert(decode(Caption(camera,'F')).find(u"Caméra")!=std::u16string::npos);
    assert(decode(Caption(camera,'S')).find(u"Cámara")!=std::u16string::npos);
    for(const auto& item:frenchCorrections) {
        std::vector<unsigned short> source(Length(item.key)+1,0);
        Copy(source.data(),item.key);
        assert(std::string(Replacement(0,1048573,source.data(),'F'))==item.text);
        assert(!Replacement(0,1048573,source.data(),'E'));
    }
    assert(!Caption("MAIN MENU",'E') && !Caption("MAIN MENU AVENUE",'F'));
    assert(!Caption("Homer Simpson",'S'));
    unsigned short keycap[]={u'[',0x00c9,u'C',u'H',u'A',u'P',u']',0};
    assert(std::string(Replacement(0,1048573,keycap,'F'))=="B");
}
'''
with tempfile.TemporaryDirectory(prefix='localized-controls-') as folder:
    path = Path(folder)
    (path/'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', '-I', str(root),
                    str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
print('PASS: actual E/F/S controls, accented UTF-16, exact UI captions, preserved narrative and localized keycaps.')

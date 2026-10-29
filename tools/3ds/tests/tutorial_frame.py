#!/usr/bin/env python3
"""Exercise the real tutorial frame setup under PAL and 3DS build flags."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, help='Optional baseline source for a negative control')
options = parser.parse_args()
source = (options.source or root/'code/presentation/gui/ingame/guiscreentutorial.cpp').read_text()
start = source.index('#ifdef SRR2_OLD_3DS\n        // The lower display needs room')
end = source.index('#endif // PAL', start) + len('#endif // PAL')
setup = source[start:end]
harness = r'''
#include <cassert>
#include <cstring>
namespace Scrooby {
struct Group {
    float x=1, y=1;
    int dx=0, dy=0;
    void ScaleAboutCenter(float a, float b, float) { x=a; y=b; }
    void Translate(int a,int b) { dx+=a; dy+=b; }
    void ResetTransformation() { x=1; y=1; dx=0; dy=0; }
};
struct Page {
    Group frame;
    Group* GetGroup(const char* name) { assert(!strcmp(name,"Frame")); return &frame; }
};
}
int main() {
    Scrooby::Page page;
    Scrooby::Page* pPageSmallBoard=&page;
''' + setup + r'''
#if defined(SRR2_OLD_3DS)
    assert(page.frame.x==1.25f && page.frame.y==1.5f);
    assert(page.frame.dx==0 && page.frame.dy==-24);
#elif defined(PAL)
    assert(page.frame.x==1.0f && page.frame.y==1.2f);
    assert(page.frame.dx==0 && page.frame.dy==0);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='tutorial-frame-') as folder:
    file = Path(folder)/'test.cpp'
    file.write_text(harness)
    for flags in (['-DPAL','-DSRR2_OLD_3DS'], ['-DSRR2_OLD_3DS'], ['-DPAL']):
        binary=Path(folder)/('test'+str(len(flags))+flags[-1][-1])
        subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',*flags,str(file),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
print('PASS: PAL retains the 3DS tutorial frame; non-3DS PAL behavior is preserved.')

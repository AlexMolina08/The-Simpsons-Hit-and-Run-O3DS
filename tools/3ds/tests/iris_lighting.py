#!/usr/bin/env python3
"""Verify actual GUI lighting scope and iris material changes preserve world policy."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/scrooby/src/FePure3dObject.cpp').read_text()
a=source.index('#ifdef SRR2_OLD_3DS\n    // GUI models retain');b=source.index('\n#endif',a)+len('\n#endif');draw=source[a:b]
source=(root/'code/presentation/gui/ingame/guiscreeniriswipe.cpp').read_text();a=source.index('#ifdef SRR2_OLD_3DS\n    // The iris is');b=source.index('\n#endif',a)+len('\n#endif');material=source[a:b]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text();policy=next(line.strip() for line in source.splitlines() if 'FlatLighting() && g_picaWorldScaleAllowed' in line)
code='''#include <cassert>
#include <stdexcept>
#include <cstring>
int g_picaWorldScaleAllowed=1;
namespace Old3dsPerformance {bool FlatLighting(){return true;}}
struct tColour{unsigned r,g,b,a;tColour(unsigned r,unsigned g,unsigned b,unsigned a):r(r),g(g),b(b),a(a){}};
enum{PDDI_SP_AMBIENT};
struct tShader{unsigned ambient=16;void SetColour(unsigned parameter,tColour c){assert(parameter==PDDI_SP_AMBIENT && c.r==0 && c.g==0 && c.b==0 && c.a==255);ambient=0;}} shader;
static bool found=true,throwDraw=false;static unsigned drawn=~0u;
namespace p3d{template<class T>T* find(const char* name){assert(!strcmp(name,"iris_black"));return found?&shader:nullptr;}}
struct Drawable{void Display(){bool lit=true;POLICY;drawn=lit?shader.ambient:255;if(throwDraw)throw std::runtime_error("probe");}} drawable;
void Setup(){
MATERIAL
}
void Draw(){auto* m_RuntimeDrawable=&drawable;
DRAW
}
int main(){
 bool lit=true;POLICY;assert(!lit);
 Setup();Draw();
#ifdef SRR2_OLD_3DS
 assert(drawn==0 && g_picaWorldScaleAllowed==1);
 throwDraw=true;try{Draw();assert(false);}catch(const std::runtime_error&){}assert(g_picaWorldScaleAllowed==1);throwDraw=false;
 g_picaWorldScaleAllowed=0;Draw();assert(g_picaWorldScaleAllowed==0 && drawn==0);g_picaWorldScaleAllowed=1;
#else
 assert(drawn==255 && shader.ambient==16);
#endif
 found=false;Setup();
}
'''.replace('POLICY',policy).replace('MATERIAL',material).replace('DRAW',draw)
with tempfile.TemporaryDirectory(prefix='iris-lighting-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 for old in (False,True):
  cmd=['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')]
  if old:cmd+=['-DSRR2_OLD_3DS']
  subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
print('PASS: actual iris material and GUI draw scope stay black, restore nested/exception state, preserve flat world lighting and leave other backends unchanged.')

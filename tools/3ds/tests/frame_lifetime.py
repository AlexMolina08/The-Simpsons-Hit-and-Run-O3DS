#!/usr/bin/env python3
"""Execute production frame closure; active state must not outlive submission."""
from pathlib import Path
import subprocess, tempfile
source=(Path(__file__).resolve().parents[3]/'libs/pure3d/pddi/pica/display.cpp').read_text()
start=source.index('void picaDisplay::EndFrame(void)');end=source.index('\nvoid picaDisplay::SwapBuffers',start)
code=r'''
#include <cassert>
using u64=unsigned long long;
int g_picaInFrame=1;unsigned submits=0,finishes=0;
unsigned g_picaPresentedFrames=0,g_old3dsFramePresented=0;
double g_picaFrameSubmitMs=0;
const unsigned GX_CMDLIST_FLUSH=1,SYSCLOCK_ARM11=268123480;
namespace Old3dsCadence {void Submit(unsigned){}}
u64 svcGetSystemTick(){static u64 n=0;return ++n;}
void C3D_FrameEnd(unsigned flags){assert(flags==GX_CMDLIST_FLUSH);++submits;}
struct picaDisplay {bool inFrame=true,stereoThisFrame=true;
 void FinishTopSegment(){assert(inFrame&&g_picaInFrame);++finishes;}
 void EndFrame();};
'''+source[start:end]+r'''
int main(){picaDisplay d;d.EndFrame();assert(!d.inFrame&&!g_picaInFrame&&!d.stereoThisFrame);
 assert(submits==1&&finishes==1&&g_old3dsFramePresented==1);
 d.EndFrame();assert(submits==1&&finishes==1);
 d.inFrame=true;g_picaInFrame=1;d.EndFrame();assert(submits==2&&!g_picaInFrame);}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-DSRR2_OLD_3DS','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production frame closure, duplicate end and reopen; global active state ends with native submission')

#!/usr/bin/env python3
"""Check the bounded GPU command audit against packed command layouts."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/display.cpp').read_text()
function=s[s.index('static void picaAuditStereoScene('):s.index('extern "C" void picaDumpStereoDiagnostics')]
fixture=r'''
#include <cassert>
using u32=unsigned;
constexpr unsigned GPUREG_VSH_FLOATUNIFORM_CONFIG=0x2c0,GPUREG_VSH_FLOATUNIFORM_DATA=0x2c1;
namespace NativeBench {bool collecting=true,visual=true;bool StereoVisualAudit(){return visual;}}
static unsigned auditedStereoSegments=0,sceneEyeWrites=0,sceneEyeFloat24Writes=0,auditErrors=0;
static u32 lastSceneEye[4]={};
constexpr u32 header(unsigned reg,unsigned extra=0,bool increment=false){return reg|0xf0000|(extra<<20)|(unsigned(increment)<<31);}
'''
main=r'''
int main(){
 const u32 direct[]={0x8000005f,header(0x2c0),11,header(0x2c1,3),22,33,44,0};
 picaAuditStereoScene(direct,8);assert(sceneEyeWrites==1 && sceneEyeFloat24Writes==0 && auditErrors==0);
 assert(lastSceneEye[0]==11 && lastSceneEye[3]==44);
 const u32 crossing[]={0x8000005e,header(0x2c0),1,header(0x2c1,7),2,3,4,5,6,7,8,0};
 picaAuditStereoScene(crossing,12);assert(sceneEyeWrites==2 && lastSceneEye[0]==5 && lastSceneEye[3]==8);
 const u32 constants[]={95,header(0x2c0,3,true),1,2,3,0};
 picaAuditStereoScene(constants,6);assert(sceneEyeFloat24Writes==1);
 const u32 notEye[]={0x8000005e,header(0x2c0),1,header(0x2c1,3),2,3,4,0};
 picaAuditStereoScene(notEye,8);assert(sceneEyeWrites==2);
 const u32 invalid[]={0,header(0x2c1,15)};picaAuditStereoScene(invalid,2);assert(auditErrors==1);
 NativeBench::visual=false;picaAuditStereoScene(direct,8);assert(auditedStereoSegments==5);
 NativeBench::visual=true;NativeBench::collecting=false;picaAuditStereoScene(direct,8);assert(auditedStereoSegments==5);
 NativeBench::collecting=true;
 for(unsigned i=0;i<100;++i)picaAuditStereoScene(direct,8);
 assert(auditedStereoSegments==32 && sceneEyeWrites==29);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-stereo-audit-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+function+main)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: float32/float24, palette crossing, incremental commands, bounds, visual-only and 32-segment limit.')

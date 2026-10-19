#!/usr/bin/env python3
"""Check actual SDK/uniform checkpoint save, restore and alias comparison helpers."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='prepared-checkpoint-') as directory:
 p=Path(directory)
 subprocess.run([sys.executable,str(root/'tools/3ds/tests/native_context.py'),'--emit-source',str(p/'base.cpp')],check=True)
 source=(p/'base.cpp').read_text();source=source[:source.index('int main(){')]
 source='#include <cstdlib>\n#define SRR2_OLD3DS_PREPARED_THREAD\n#define SRR2_OLD3DS_PREPARED_BACKEND\n'+source+r'''
int main(){
 using namespace Candidate;
 void* before=picaNativeAllocateCheckpoint();void* changed=picaNativeAllocateCheckpoint();assert(before&&changed);
 C3D_Tex a={},b={};context.tex[0]=&a;
 for(unsigned trial=0;trial<10000;++trial){
  unsigned id=trial%96;C3D_FVUnif[0][id]={float(trial),2,3,4};C3D_FVUnifDirty[0][id]=true;g_picaNativeUniformMask[0][id/32]|=BIT(id%32);
  C3Di_FVUnifEverDirty[0][id]=bool(trial%2);C3D_IVUnif[1][trial%4]=trial;C3D_IVUnifDirty[1][trial%4]=true;
  C3D_BoolUnifs[0]=trial;C3D_BoolUnifsDirty[0]=true;
  uniformCache.values[0][id]=C3D_FVUnif[0][id];uniformCache.valid[0][id]=true;uniformCache.sent=trial;
  context.flags=trial;context.viewport[0]=trial*7;context.tex[0]=&a;a.param=trial;
  picaNativeSaveCheckpoint(before);
  C3D_FVUnif[0][id].x=-17;C3D_IVUnif[1][trial%4]=0;C3D_BoolUnifs[0]=0;uniformCache.sent=~trial;context.flags=~trial;
  picaNativeSaveCheckpoint(changed);assert(!picaNativeCompareCheckpoint(before,changed));
  picaNativeRestoreCheckpoint(before);picaNativeSaveCheckpoint(changed);assert(picaNativeCompareCheckpoint(before,changed));
  assert(C3D_FVUnif[0][id].x==float(trial)&&C3D_IVUnif[1][trial%4]==trial&&C3D_BoolUnifs[0]==u16(trial)&&uniformCache.sent==trial);
  b=a;context.tex[0]=&b;picaNativeSaveCheckpoint(changed);assert(picaNativeCompareCheckpoint(before,changed));
  b.param^=1;picaNativeSaveCheckpoint(changed);assert(!picaNativeCompareCheckpoint(before,changed));
 }
 picaNativeFreeCheckpoint(before);picaNativeFreeCheckpoint(changed);
 puts("PASS: 10000 production SDK/uniform checkpoints preserve registers, dirty/ever-dirty masks, upload history and SDK state; owned texture alias equality rejects changed metadata. Host ABI fixture; actual ARM layout separately compiled.");
}
'''
 (p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

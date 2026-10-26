#!/usr/bin/env python3
"""Keep the private slice test confined to gameplay and preserve loading policy."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'code/main/game.cpp').read_text()
a=source.index('        g_p3dYieldTimeUs = ( ctx == CONTEXT_GAMEPLAY')
b=source.index('\n#endif',source.index('g_p3dYieldTimeUs=NativeBench::LoaderSliceUs();',a))+len('\n#endif')
assignment=source[a:b]
code=r'''
#include <cassert>
#include <initializer_list>
enum {CONTEXT_GAMEPLAY,CONTEXT_PAUSE,CONTEXT_LOADING_GAMEPLAY,CONTEXT_FRONTEND};
static unsigned selected=0;static bool animation=false;
namespace NativeBench{unsigned LoaderSliceUs(){return selected;}}
bool old3dsNewspaperAnimating(){return animation;}
unsigned budget(unsigned ctx){unsigned g_p3dYieldTimeUs=0;
ASSIGNMENT
return g_p3dYieldTimeUs;}
int main(){
 for(unsigned value:{0u,2000u,4000u,8000u}){
  selected=value;assert(budget(CONTEXT_GAMEPLAY)==(value?value:8000));
  assert(budget(CONTEXT_PAUSE)==8000);
  animation=false;assert(budget(CONTEXT_LOADING_GAMEPLAY)==52000);
  animation=true;assert(budget(CONTEXT_LOADING_GAMEPLAY)==8000);
  assert(budget(CONTEXT_FRONTEND)==52000);
 }
}
'''.replace('ASSIGNMENT',assignment)
with tempfile.TemporaryDirectory(prefix='srr-loader-slice-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-DSRR2_OLD3DS_NATIVE_BENCH',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual private override applies only during gameplay; pause, loading throughput, newspaper entrance and disabled behavior stay unchanged.')

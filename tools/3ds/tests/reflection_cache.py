#!/usr/bin/env python3
"""Compare production reflection cache transitions with the prior boolean protocol."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root/'libs/pure3d/pddi/pica/context.cpp').read_text()
start = text.index('bool picaContext::BindReflectionProgram(bool enabled, bool world)')
end = text.index('\n#endif', start)
method = text[start:end]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <vector>
namespace Candidate {
std::vector<bool> calls;
unsigned short C3D_BoolUnifs[2]={};
#define BIT(n) (1u<<(n))
#define GPU_VERTEX_SHADER 0
void C3D_BoolUnifSet(int,int,bool value){C3D_BoolUnifs[0]=(C3D_BoolUnifs[0]&~BIT(4))|(value?BIT(4):0);}
void picaNativeSetReflectionOutput(bool value){calls.push_back(value);}
class picaContext {
public:
 enum SurfaceState { SurfaceUnknown,SurfaceOrdinary,SurfaceReflection,SurfaceSkin,SurfaceWorld };
 bool skinProgramBound=false,ready=true;
 unsigned char reflectionState=SurfaceUnknown;
 void* ReflectionProgram(){return ready?this:nullptr;}
 bool BindReflectionProgram(bool,bool world=false);
 void Invalidate(){reflectionState=skinProgramBound?SurfaceSkin:SurfaceUnknown;}
 void BindSkin(bool value){skinProgramBound=value;Invalidate();}
 bool Apply(bool eligible){const bool reflective=eligible && ready && !skinProgramBound;
  if(reflectionState!=SurfaceSkin && reflectionState!=(reflective?SurfaceReflection:SurfaceOrdinary))BindReflectionProgram(reflective);
  return reflective;}
};
METHOD
}
struct Reference {
 bool skin=false,ready=true,bound=false,valid=false;std::vector<bool> calls;
 void Invalidate(){valid=false;}
 void BindSkin(bool value){skin=value;bound=false;Invalidate();}
 bool Bind(bool value){if(skin)return false;value=value && ready;if(value!=bound || !valid){calls.push_back(value);bound=value;valid=true;}return value;}
 bool Apply(bool eligible){const bool reflective=eligible && ready && !skin;if(!skin &&(!valid || bound!=reflective))Bind(reflective);return reflective;}
};
int main(){
 Candidate::picaContext candidate;Reference reference;unsigned random=79;
 for(unsigned i=0;i<100000;++i){random=random*1664525u+1013904223u;bool value=(random&0x100)!=0;
  switch(random%6){
   case 0:reference.BindSkin(value);candidate.BindSkin(value);break;
   case 1:reference.Invalidate();candidate.Invalidate();break;
   case 2:reference.ready=candidate.ready=value;break;
   case 3:assert(reference.Bind(value)==candidate.BindReflectionProgram(value));break;
   default:assert(reference.Apply(value)==candidate.Apply(value));break;
  }
  assert(reference.calls==Candidate::calls);
 }
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='srr-reflection-cache-') as name:
    folder = Path(name)
    (folder/'test.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(folder/'test.cpp'), '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
print('PASS: 100000 production cache transitions preserve reflection requests and eligibility across skin switches, invalidation and unavailable programs.')

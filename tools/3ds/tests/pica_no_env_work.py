#!/usr/bin/env python3
"""Prove vehicle reflection parameters are no-ops in the current PICA backend."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
base=(root/'libs/pure3d/pddi/base/baseshader.cpp').read_text()
material=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
header=(root/'libs/pure3d/pddi/pica/material.hpp').read_text()
shade=(root/'libs/pure3d/pddi/pddishade.hpp').read_text()
constants='\n'.join(line for line in re.findall(r'^const unsigned PDDI_SP_.*?;',shade,re.M) if re.match(r'const unsigned (PDDI_SP_(NULL|AMBIENT|DIFFUSE|EMISSIVE|SPECULAR|ENVBLEND|ROTVEC))\s*=',line))
colour=re.search(r'pddiShadeColourTable picaMat::colourTable\[\].*?\n};',material,re.S)[0]
params=re.findall(r'\{(PDDI_SP_\w+)\s*,',colour)
assert 'PDDI_SP_ENVBLEND' not in params
assert 'GetVectorTable' not in header and 'GetMatrixTable' not in header
# Each installed shader name resolves to this one backend implementation.
device=(root/'libs/pure3d/pddi/pica/device.cpp').read_text()
assert 'new picaMat((picaContext*)context)' in device
macro=base[base.index('#define DISPATCH'):base.index('bool pddiBaseShader::SetTexture')]
methods=base[base.index('bool pddiBaseShader::SetColour'):base.index('bool pddiBaseShader::SetMatrix')]
source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#define PDDI_FOURCC(a,b,c,d) (unsigned(a)|(unsigned(b)<<8)|(unsigned(c)<<16)|(unsigned(d)<<24))
'''+constants+r'''
using pddiColour=uint32_t;
struct pddiVector {float x,y,z;};
class pddiBaseShader;
struct pddiShadeColourTable {unsigned param;void(pddiBaseShader::*func)(pddiColour);};
struct pddiShadeVectorTable {unsigned param;void(pddiBaseShader::*func)(const pddiVector&);};
class pddiBaseShader {
public:
 unsigned uid=17;static unsigned newUID;unsigned writes=0;uint32_t payload=0;
 bool SetColour(unsigned,pddiColour);bool SetVector(unsigned,const pddiVector&);
 void Sink(pddiColour x){++writes;payload=x;}
 pddiShadeColourTable* GetColourTable();
 pddiShadeVectorTable* GetVectorTable(){return nullptr;}
};
unsigned pddiBaseShader::newUID=100;
pddiShadeColourTable colours[]={
'''+''.join('{'+p+', '+('nullptr' if p=='PDDI_SP_NULL' else '&pddiBaseShader::Sink')+'},\n' for p in params)+r'''};
pddiShadeColourTable* pddiBaseShader::GetColourTable(){return colours;}
'''+macro+methods+r'''
int main(){
 pddiBaseShader shader;uint32_t random=17;
 for(unsigned i=0;i<100000;++i){
  random=random*1664525u+1013904223u;pddiVector v;uint32_t words[3]={random,random^0x7f800000u,random^0x80000000u};memcpy(&v,words,sizeof(v));
  const auto uid=shader.uid, next=shader.newUID, writes=shader.writes,payload=shader.payload;
  assert(!shader.SetColour(PDDI_SP_ENVBLEND,random));
  assert(!shader.SetVector(PDDI_SP_ROTVEC,v));
  assert(shader.uid==uid && shader.newUID==next && shader.writes==writes && shader.payload==payload);
  if((i&31)==0){assert(shader.SetColour(PDDI_SP_DIFFUSE,random));assert(shader.payload==random && shader.uid!=uid);}
 }
 puts("PASS: 200000 production PICA dispatch no-ops preserve material UID and state; supported diffuse writes remain live. Not native performance evidence.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-pica-env-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

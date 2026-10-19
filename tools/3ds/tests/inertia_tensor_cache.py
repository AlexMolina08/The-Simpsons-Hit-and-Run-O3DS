#!/usr/bin/env python3
"""Compare exact production tensor rotation with cached inputs and output."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/sim/simcommon/symmatrix.cpp').read_text();start=s.index('void SymMatrix::TransformSelf(const Matrix& m)');end=s.index('\nOLD3DS_HOT_PATH',start)
production=s[start:end]
source=r'''
#pragma STDC FENV_ACCESS ON
#include <simphysics/inertiatensorcache.hpp>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include <limits>
struct Matrix {float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};};
struct SymMatrix {
 float xx=0,xy=0,xz=0,yy=0,yz=0,zz=0;
 void TransformSelf(const Matrix&);
};
'''+production+r'''
int main(){
 unsigned state=193,checks=0,hits=0;
 auto random=[&](){return state=state*1664525u+1013904223u;};
 sim::inertiaTensorCache::Cache cache;
 Matrix matrix;SymMatrix tensor;
 for(int round:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(round));
  for(unsigned n=0;n<48000;++n){
   if(n%11==0){tensor.xx=float(random()%8192)/128; tensor.yy=float(random()%8192)/128;tensor.zz=float(random()%8192)/128;}
   if(n%19==0){tensor.xy=float(int(random()%4096)-2048)/1024;tensor.xz=-tensor.xy;tensor.yz=tensor.xy*.5f;}
   if(n%13==0)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)matrix.m[r][c]=float(int(random()%4096)-2048)/1024;
   for(unsigned c=0;c<4;++c)matrix.m[3][c]=float(random()%65536); // Translation is deliberately irrelevant.
   SymMatrix expected=tensor,actual;
   expected.TransformSelf(matrix);hits+=cache.Apply(actual,tensor,matrix);
   assert(!std::memcmp(&actual,&expected,sizeof(actual)));++checks;
   if(n%101==0){auto cloned=cache;actual=tensor;assert(cloned.Apply(actual,actual,matrix));assert(!std::memcmp(&actual,&expected,sizeof(actual)));}
   if(n%23==0){Matrix other=matrix;other.m[0][0]=-other.m[0][0];expected=tensor;expected.TransformSelf(other);cache.Apply(actual,tensor,other);assert(!std::memcmp(&actual,&expected,sizeof(actual)));++checks;}
   if(n%229==0){auto invalid=matrix;invalid.m[1][2]=std::numeric_limits<float>::infinity();expected=tensor;expected.TransformSelf(invalid);assert(!cache.Apply(actual,tensor,invalid));assert(!std::memcmp(&actual,&expected,sizeof(actual)));++checks;}
   if(n%241==0){auto invalid=tensor;invalid.xy=std::numeric_limits<float>::quiet_NaN();expected=invalid;expected.TransformSelf(matrix);assert(!cache.Apply(actual,invalid,matrix));assert(!std::memcmp(&actual,&expected,sizeof(actual)));++checks;}
   if(n%251==0){auto invalid=tensor;invalid.xx=std::numeric_limits<float>::denorm_min();invalid.xy=-0.f;expected=invalid;expected.TransformSelf(matrix);cache.Apply(actual,invalid,matrix);assert(!std::memcmp(&actual,&expected,sizeof(actual)));++checks;}
  }
 }
 assert(hits>100000);printf("PASS: %u production tensor comparisons across four rounding modes; %u exact cache hits, translation, rotation/mass changes, aliases, clones, signed zeros, subnormal and nonfinite fallbacks; 92 bytes per joint. Not native performance evidence.\n",checks,hits);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-inertia-tensor-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/sim'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

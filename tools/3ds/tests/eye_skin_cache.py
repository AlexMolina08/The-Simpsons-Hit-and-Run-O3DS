#!/usr/bin/env python3
"""Compare production eye-palette cache with uncached transforms and invalid input."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
code=r'''
#include <pddi/pica/skincore.hpp>
#include <cassert>
#include <cstdio>
#include <random>
#include <limits>
unsigned products=0;
namespace picaSkin {bool CountedPack(float out[3][4],const float bone[4][4],const float view[4][4]){++products;return PackEyeMatrixValidatedView(out,bone,view);}}
#define PackEyeMatrixValidatedView CountedPack
#include <pddi/pica/skineyecache.hpp>
#undef PackEyeMatrixValidatedView
int main(){
 picaSkin::EyeCache cache;std::mt19937 rng(731929);std::uniform_real_distribution<float>d(-2,2);
 float view[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},bones[64][4][4];
 for(auto& bone:bones)for(auto& row:bone)for(float& x:row)x=d(rng);
 unsigned calls=0;
 for(unsigned i=0;i<100000;++i){
  unsigned slot=i%64;float input[4][4];memcpy(input,bones[slot],64);
  if(i%97==0)input[i%4][i/4%4]=d(rng);
  if(i%499==0)input[i%4][i/4%4]=std::numeric_limits<float>::infinity();
  if(i%587==0)input[i%4][i/4%4]=std::numeric_limits<float>::quiet_NaN();
  if(i%331==0)view[3][i%3]=d(rng);
  if(i%937==0)view[2][3]=.01f;else view[2][3]=0;
  if(i%797==0)input[0][0]=-0.0f;
  float expected[3][4],actual[3][4];
  bool valid=cache.PrepareView(view,(i/10000)%4);
  assert(valid==picaSkin::EyeViewValid(view));
  bool ref=picaSkin::PackEyeMatrix(expected,input,view);
  // Alternate addresses reuse the same slots to exercise collisions and aliases.
  bool got=cache.Pack(actual,input,64u*(slot+(i%2?64:0)));assert(ref==got);
  if(got)assert(!memcmp(expected,actual,sizeof(expected)));++calls;
 }
 unsigned before=products;
 for(unsigned i=0;i<10000;++i){cache.PrepareView(view,0);float out[3][4];assert(cache.Pack(out,bones[0],0));}
 assert(products-before<=1);printf("PASS: %u exact cached/uncached eye-palette comparisons; view/control changes, pointer aliases, collisions, signed zero and nonfinite inputs; repeated bone products 10000 -> %u\n",calls,products-before);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-eye-cache-')as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

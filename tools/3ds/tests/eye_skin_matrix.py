#!/usr/bin/env python3
"""Verify the production eye-palette composition, including rejected input."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
code=r'''
#include <pddi/pica/skincore.hpp>
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
int main(){
 std::mt19937 rng(672451);std::uniform_real_distribution<float>d(-2,2);
 double worst=0;
 for(unsigned n=0;n<100000;++n){
  float bone[4][4],view[4][4],packed[3][4];
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){bone[r][c]=d(rng);view[r][c]=d(rng);}
  view[0][3]=view[1][3]=view[2][3]=0;view[3][3]=1;
  // Bone last columns are deliberately non-affine: the established skin path ignores them.
  assert(picaSkin::PackEyeMatrix(packed,bone,view));
  float position[4]={d(rng),d(rng),d(rng),1},normal[4]={d(rng),d(rng),d(rng),0};
  for(auto input:{position,normal}){
   double local[3]={};for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)local[r]+=double(bone[c][r])*input[c];
   for(unsigned r=0;r<3;++r){
    double reference=double(view[3][r])*input[3],actual=0;
    for(unsigned k=0;k<3;++k)reference+=double(view[k][r])*local[k];
    for(unsigned k=0;k<4;++k)actual+=double(packed[r][k])*input[k];
    worst=std::fmax(worst,std::fabs(reference-actual));
   }
  }
  view[n%3][3]=.01f;assert(!picaSkin::PackEyeMatrix(packed,bone,view));view[n%3][3]=0;
  view[3][3]=0;assert(!picaSkin::PackEyeMatrix(packed,bone,view));view[3][3]=1;
  bone[n%4][(n/4)%4]=std::numeric_limits<float>::infinity();assert(!picaSkin::PackEyeMatrix(packed,bone,view));
 }
 assert(worst<.00001);printf("PASS: 100,000 production eye-palette position/normal cases; max composition error %.9g; projective/nonfinite fallback\n",worst);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-eye-matrix-')as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

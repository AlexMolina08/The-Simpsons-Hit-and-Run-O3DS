#!/usr/bin/env python3
"""Compare cached sparse plane preparation with the retained production reference."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
header=(root/'libs/pure3d/pddi/pica/clipbounds.hpp').read_text()
a=header.index('inline Transform Prepare(',header.index('#else'));b=header.index('{',a);e=b+1;depth=1
while depth:
 depth+=(header[e]=='{')-(header[e]=='}');e+=1
reference=header[a:e].replace('inline Transform Prepare(','inline Transform PrepareReference(')
source=r'''
#define SRR2_OLD3DS_CLIP_PLANE_PREP
#include <pddi/pica/clipbounds.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <limits>
namespace picaClipBounds {
'''+reference+r'''
}
int main(){
 std::mt19937 random(551227);unsigned compared=0,rejected=0;
 unsigned long long terms=0,referenceTerms=0;
 for(unsigned test=0;test<300000;++test){
  float mv[4][4]={{0}},projection[4][4]={{0}},low[3],high[3];
  for(unsigned r=0;r<3;++r){
   for(unsigned c=0;c<4;++c)mv[r][c]=float(int(random()%400)-200)/(c==3 ? 0.1f : 100.0f);
   low[r]=float(int(random()%400)-200);high[r]=low[r]+float(random()%100);
  }
  mv[3][3]=1;
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
   if(test%3==0 || random()%5==0)projection[r][c]=float(int(random()%400)-200)/100.0f;
  if(test%3!=0){
   memset(projection,0,sizeof(projection));
   projection[0][1]=1.3f;projection[1][0]=-1.8f;projection[2][2]=1.001f;projection[2][3]=-.1f;projection[3][2]=1;
  }
  if(test%101==0)projection[random()%4][random()%4]=std::numeric_limits<float>::infinity();
  if(test%103==0)mv[random()%4][random()%4]=std::numeric_limits<float>::quiet_NaN();
  if(test%107==0)mv[3][1]=1;
  auto basis=picaClipBounds::PrepareProjection(projection);
  auto result=picaClipBounds::Prepare(mv,basis);
  auto expected=picaClipBounds::PrepareReference(mv,projection);
  assert(result.valid==expected.valid);
  if(result.valid){
   for(unsigned p=0;p<4;++p)for(unsigned j=0;j<4;++j){
    // Signed zero cannot affect < 0 selection or conservative rejection.
    assert(result.coefficient[p][j]==expected.coefficient[p][j]);
    assert(result.error[p][j]==expected.error[p][j]);
   }
   for(unsigned p=0;p<4;++p)terms+=basis.count[p]*4;
   referenceTerms+=64;
  }
  auto actual=picaClipBounds::Reject(result,low,high);
  assert(actual==picaClipBounds::Reject(expected,low,high));
  rejected+=actual!=0;++compared;
 }
 printf("PASS: %u original/sparse projection preparations; identical coefficients, error bounds and rejection decisions; %u rejections; coefficient products %llu vs %llu reference\n",compared,rejected,terms,referenceTerms);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-plane-prep-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=30)

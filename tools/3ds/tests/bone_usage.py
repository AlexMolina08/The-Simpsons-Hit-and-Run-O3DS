#!/usr/bin/env python3
"""Check production palette selection and matrix preparation against the full loop."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/p3d/anim/polyskin.cpp').read_text()
p=(root/'libs/pure3d/p3d/primgroup.cpp').read_text()
m=(root/'libs/radmath/radmath/matrix.cpp').read_text()
def extract(text,sig):
 a=text.index(sig);b=text.index('{',a)+1;e=b;depth=1
 while depth:depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
loop=s[s.index('#ifdef SRR2_OLD3DS_BONE_USAGE\n    const bool selectiveBones'):s.index('#ifdef SRR2_OLD3DS_SKIN_CLIP',s.index('const bool selectiveBones'))]
code=r'''
#include <p3d/anim/boneusage.hpp>
#include <cassert>
#define SRR2_OLD3DS_BONE_ONCE
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cfenv>
unsigned multiplications=0;
namespace rmt {struct Matrix {float m[4][4];void Mult(const Matrix&,const Matrix&);void Identity(){memset(m,0,64);for(int i=0;i<4;++i)m[i][i]=1;}};}
using rmt::Matrix;
'''+extract(m,'void Matrix::Mult(const Matrix& a, const Matrix& b)').replace('void Matrix::Mult','void rmt::Matrix::Mult').replace('{\n','{\n ++multiplications;\n',1)+r'''
struct tPrimGroupSkinnedPC {unsigned nMatrices;Matrix** matrixPalette;bool CollectBoneUsage(p3dSkin::BoneUsage&,const Matrix*) const;};
'''+extract(p,'bool tPrimGroupSkinnedPC::CollectBoneUsage')+r'''
struct Joint {Matrix inverseWorldMatrix,worldMatrix;};
struct Pose {Joint joint[256];unsigned count;int GetNumJoint(){return int(count);}Joint* GetJoint(unsigned i){assert(i<count);return joint+i;}};
struct Reference {Matrix boneMatrix[256];Pose* skeleton;void* exprAnimOffsets=nullptr;p3dSkin::BoneUsage boneUsage;void Prepare(Pose* p){
'''+loop+r'''
}};
#define SRR2_OLD3DS_BONE_USAGE
unsigned boneUsageCalls=0,boneUsageComputed=0,boneUsageAvoided=0,boneUsageFallbacks=0;
struct Candidate {Matrix boneMatrix[256];Pose* skeleton;void* exprAnimOffsets=nullptr;p3dSkin::BoneUsage boneUsage;void Prepare(Pose* p){
'''+loop+r'''
}};
int main(){
 unsigned rng=81713;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};
 unsigned cases=0,referenceMults=0,candidateMults=0;
 for(int rounding:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  assert(!fesetround(rounding));
  for(unsigned trial=0;trial<3000;++trial){
   Pose pose;pose.count=1+word()%255;
   for(unsigned i=0;i<pose.count;++i)for(auto* matrix:{&pose.joint[i].inverseWorldMatrix,&pose.joint[i].worldMatrix}){
    for(unsigned r=0;r<4;++r){for(unsigned c=0;c<3;++c){unsigned bits=word();memcpy(&matrix->m[r][c],&bits,4);}matrix->m[r][3]=r==3?1:0;}
   }
   Reference a;Candidate b;a.skeleton=b.skeleton=&pose;
   memset(a.boneMatrix,0x5a,sizeof(a.boneMatrix));memset(b.boneMatrix,0x5a,sizeof(b.boneMatrix));
   b.boneUsage.Begin(pose.count);bool used[256]={};Matrix* palette[256];unsigned entries=1+word()%256;
   for(unsigned i=0;i<entries;++i){unsigned index=word()%(pose.count+1);palette[i]=b.boneMatrix+index;used[index]=true;}
   tPrimGroupSkinnedPC group{entries,palette};assert(group.CollectBoneUsage(b.boneUsage,b.boneMatrix));
   assert(b.boneUsage.Ready(pose.count));
   bool fallback=trial%7==0;if(fallback){b.boneUsage.Invalidate();assert(!b.boneUsage.Ready(pose.count));}
   bool deformed=trial%11==0;if(deformed)a.exprAnimOffsets=b.exprAnimOffsets=&pose;
   multiplications=0;a.Prepare(&pose);referenceMults+=multiplications;assert(multiplications==pose.count);
   multiplications=0;b.Prepare(&pose);candidateMults+=multiplications;
   unsigned expected=0;
   for(unsigned i=0;i<pose.count;++i){
    if(fallback||deformed||used[i]){assert(!memcmp(a.boneMatrix+i,b.boneMatrix+i,64));++expected;}
    else {unsigned char poison[64];memset(poison,0x5a,64);assert(!memcmp(poison,b.boneMatrix+i,64));}
   }
   assert(multiplications==expected);assert(!memcmp(a.boneMatrix+pose.count,b.boneMatrix+pose.count,64));++cases;
   auto clone=b.boneUsage;assert(clone.Ready(pose.count)==b.boneUsage.Ready(pose.count));
  }
 }
 Matrix storage[256];p3dSkin::BoneUsage usage;
 for(unsigned n:{0u,256u,300u}){usage.Begin(n);assert(!usage.Ready(n));assert(!usage.Add(storage,storage));}
 for(unsigned kind=0;kind<4;++kind){usage.Begin(33);const void* ptr=kind==0?nullptr:kind==1?reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(storage)+1):kind==2?storage+34:reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(storage)-64);assert(!usage.Add(storage,ptr));assert(!usage.Ready(33));}
 usage.Begin(33);assert(usage.Add(storage,storage+33));assert(!usage.Uses(33));assert(!usage.Uses(256));
 puts("PASS: palette bounds, alignment, identity slot, clone values and fallback guards");
 printf("PASS: %u production matrix preparations in four rounding modes, nonfinite inputs, deformable/full fallback; exact referenced matrices and untouched unused slots; synthetic multiplications %u / %u, not native gains\n",cases,referenceMults,candidateMults);
}
'''
code=code.replace('#include <cfenv>','#include <cfenv>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='srr-bone-usage-') as td:
 d=Path(td);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-frounding-math','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(root/'libs/pure3d'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)

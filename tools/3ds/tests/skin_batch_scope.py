#!/usr/bin/env python3
"""Execute production PolySkin submission with mixed and overridden group types."""
from pathlib import Path
import subprocess,tempfile,sys
clip="--clip" in sys.argv
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/p3d/anim/polyskin.cpp').read_text();a=s.index('void tPolySkin::Display(tPose* p)');b=s.index('\nvoid tPolySkin::DisplayInstanced',a);method=s[a:b]
harness=r'''
#include <cassert>
#include <vector>
#include <typeinfo>
#include <cstdio>
#define SRR2_OLD3DS_GPU_SKIN_BATCH
#define SRR2_OLD3DS_BONE_ONCE
static unsigned draws=0,multiplies=0;
struct Matrix{void Identity(){}void Mult(const Matrix&,const Matrix&){++multiplies;}};
struct Joint{Matrix inverseWorldMatrix,worldMatrix;};
struct Skeleton{int GetNumJoint(){return 2;}Joint j;Joint* GetJoint(int){return &j;}};
struct tPose{Skeleton s;void Evaluate(){}int GetNumJoint(){return 2;}Joint* GetJoint(int i){return s.GetJoint(i);}};
struct pddiExtHardwareSkinning{unsigned begins=0,ends=0;void Begin(){++begins;}void End(){++ends;}};
struct picaContext{unsigned skinBatchDepth=0,begins=0,ends=0;bool bound=false;
 void BeginSkinBatch(){assert(!skinBatchDepth);++skinBatchDepth;++begins;}
 void EndSkinBatch(){assert(skinBatchDepth==1);--skinBatchDepth;++ends;bound=false;}
};
picaContext context;
namespace p3d{picaContext* pddi=&context;struct Context{pddiExtHardwareSkinning hw;
 pddiExtHardwareSkinning* GetHardwareSkinning(){return &hw;}};Context c;Context* context=&c;}
struct Prim{virtual ~Prim(){}virtual void Display(){assert(!context.bound&&!context.skinBatchDepth);++draws;}};
struct tPrimGroupSkinnedPC:Prim{bool ready=true;void Display()override{
 if(ready){assert(context.skinBatchDepth);context.bound=true;}else context.bound=false;++draws;}};
struct Override:tPrimGroupSkinnedPC{void Display()override{
 assert(!context.skinBatchDepth&&!context.bound);++draws;
 // An unknown drawable may invoke another skin or issue ordinary geometry.
 Prim normal;normal.Display();assert(!context.bound);
}};
template<class T>struct List:std::vector<T>{using std::vector<T>::operator=;unsigned Size(){return this->size();}};
struct tPolySkin{Matrix boneMatrix[3];Skeleton sk;Skeleton* skeleton=&sk;List<Prim*> primGroup;void Display(tPose*);};
'''
tests=r'''
int main(){tPose pose;tPolySkin skin;tPrimGroupSkinnedPC a,b,f;f.ready=false;Prim ordinary;Override override;
 skin.Display(nullptr);assert(!draws&&!context.begins);
 skin.primGroup={&a,&b,&f,&ordinary,&a,&override,&b};skin.Display(&pose);
 assert(draws==8&&context.begins==3&&context.ends==3&&!context.bound&&!context.skinBatchDepth);
 assert(multiplies==2&&p3d::c.hw.begins==1&&p3d::c.hw.ends==1);
 skin.primGroup={&ordinary,&override};skin.Display(&pose);
 assert(context.begins==3&&context.ends==3&&!context.bound);
 puts("PASS: production PolySkin display order; exact-type runs, CPU fallback, overridden/ordinary group barriers, null pose and hardware extension pairing");}
'''
if clip:
 harness=harness.replace('#define SRR2_OLD3DS_BONE_ONCE','#define SRR2_OLD3DS_BONE_ONCE\n#define SRR2_OLD3DS_SKIN_CLIP\nnamespace picaSkinClip {struct Bounds{};}\nstruct picaPrimBuffer {picaSkinClip::Bounds bounds;picaSkinClip::Bounds* skinClipBounds=&bounds;};')
 harness=harness.replace('struct Joint{','using pddiMatrix=Matrix;\nstruct Joint{')
 harness=harness.replace('struct picaContext{','struct picaContext{bool clipReject=false;unsigned clipCalls=0;\n bool CullSkinBounds(const picaSkinClip::Bounds*const* groups,unsigned count,const pddiMatrix*,unsigned matrices){assert(groups&&count==2&&matrices==3);++clipCalls;return clipReject;}\n')
 harness=harness.replace('struct tPrimGroupSkinnedPC:Prim{','struct tPrimGroupSkinnedPC:Prim{picaPrimBuffer buffer;picaPrimBuffer* GetBuffer(){return &buffer;}')
 tests=tests.replace(' puts("PASS:',r''' assert(!context.clipCalls);
 skin.primGroup={&a,&b};context.clipReject=true;
 unsigned beforeDraws=draws,beforeMultiplies=multiplies,beforeHardware=p3d::c.hw.begins;
 skin.Display(&pose);assert(context.clipCalls==1&&draws==beforeDraws&&multiplies==beforeMultiplies+2&&p3d::c.hw.begins==beforeHardware);
 a.buffer.skinClipBounds=nullptr;skin.Display(&pose);assert(context.clipCalls==1&&draws==beforeDraws+2);
 a.buffer.skinClipBounds=&a.buffer.bounds;context.clipReject=false;skin.Display(&pose);assert(context.clipCalls==2&&draws==beforeDraws+4);
 skin.primGroup.assign(33,&a);skin.Display(&pose);assert(context.clipCalls==2&&draws==beforeDraws+37);
 puts("PASS:''')
with tempfile.TemporaryDirectory(prefix='srr-skin-batch-') as td:
 p=Path(td);(p/'test.cpp').write_text(harness+method+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

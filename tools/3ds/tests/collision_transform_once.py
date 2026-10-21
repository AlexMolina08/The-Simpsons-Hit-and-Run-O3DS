#!/usr/bin/env python3
"""Compare production fused volume traversal with one scoped transform read."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
# Reuse the existing collision fixtures and real bounds/detector implementations.
existing=(root/'tools/3ds/tests/collision_preparation.py').read_text()
namespace={'__file__':str(root/'tools/3ds/tests/collision_preparation.py')}
exec(existing[:existing.index('with tempfile.TemporaryDirectory')],namespace)
prelude,methods,tests=map(namespace.get,['prelude','methods','tests']);extract=namespace['extract'];volume=namespace['volume']
prelude=prelude.replace('#include <cassert>','#pragma STDC FENV_ACCESS ON\n#include <cassert>\n#include <cfenv>\n#include <cstdlib>\n#include <limits>\n#include <typeinfo>')
prelude=prelude.replace('struct CollisionVolume;','''struct SimState {virtual ~SimState()=default;};
struct SimStateArticulated:SimState {};struct SimStateFlexible:SimState {};struct UnknownState:SimState {};
static unsigned transformReads=0,collisionTransformReads=0,collisionTransformReuses=0;
struct CollisionVolume;''')
prelude=prelude.replace('bool manual=false,flexible=false;Matrix','bool manual=false,flexible=false,unknown=false;unsigned reads=0,customReads=0;SimState base;SimStateFlexible flex;UnknownState custom;Matrix')
prelude=prelude.replace(' bool IsManualUpdate()', ' SimState* GetSimState(){return unknown?static_cast<SimState*>(&custom):(flexible?static_cast<SimState*>(&flex):&base);}\n bool IsManualUpdate()')
prelude=prelude.replace('const Matrix& GetTransform(int i) {static Matrix scratch;', 'const Matrix& GetTransform(int i) {++transformReads;++reads;static Matrix scratch;if(unknown){scratch=transforms[i];scratch.m[0][0]+=float(++customReads)/16;return scratch;}')
prelude=prelude.replace('void UpdatePosRot();','''void UpdatePosRot();
#ifdef SRR2_OLD3DS_COLL_TRANSFORM_ONCE
 void UpdatePosRot(CollisionObject*,SimState*);virtual void SetRotationFromTransform(const Matrix&){SetRotation();}
#endif''')
prelude=prelude.replace('void SetRotation();','''void SetRotation();
#ifdef SRR2_OLD3DS_COLL_TRANSFORM_ONCE
 void SetRotationFromTransform(const Matrix&)override;
#endif
''')
prelude=prelude.replace('void SetRotation();','void SetRotation() override;').replace('void UpdateBBox();','void UpdateBBox() override;')
prelude=prelude.replace('struct BBoxVolume:','''struct CustomCylinder:CylinderVolume {bool flipOwner=false;void SetRotation()override{CylinderVolume::SetRotation();mAxis.x+=1;if(flipOwner && mCollisionObject)mCollisionObject->unknown=true;}};
struct BBoxVolume:''')
methods+='\n#ifdef SRR2_OLD3DS_COLL_TRANSFORM_ONCE\n'+extract(volume,'static inline void CollisionTransformCount(')+'\n'+extract(volume,'void CollisionVolume::UpdatePosRot(CollisionObject* stableOwner, SimState* stableState)')+'\n'
for shape in ['CylinderVolume','OBBoxVolume','WallVolume']:methods+=extract(volume,'void '+shape+'::SetRotationFromTransform(')+'\n'
methods+='#endif\n'
tests=tests.replace('int main(){','int main(int argc,char** argv){assert(argc==2);int modes[]={FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO};assert(!fesetround(modes[atoi(argv[1])]));')
tests=tests.replace('BBoxVolume root,nested;', 'if(run%127==0)obj.transforms[1].m[0][0]=std::numeric_limits<float>::quiet_NaN();if(run%131==0)obj.transforms[2].m[1][0]=std::numeric_limits<float>::infinity();BBoxVolume root,nested;')
tests=tests.replace('obj.manual=run%7==0;','obj.unknown=run%13==0;obj.manual=run%7==0;')
tests=tests.replace('root.UpdateAll();','custom.flipOwner=run%37==0;root.UpdateAll();')
tests=tests.replace('CollisionVolume sphere;','CollisionVolume sphere;CustomCylinder custom;custom.mType=CylinderVolumeType;')
tests=tests.replace('grandchildren.v={&box,&sphere};','grandchildren.v={&box,&sphere,&custom};')
tests=tests.replace('nodes[]={&root,&nested,&cylinder,&box,&wall,&sphere};','nodes[]={&root,&nested,&cylinder,&box,&wall,&sphere,&custom};')
tests=tests.replace('cylinder.mAxisRef={f(),f(),f()};','custom.mAxisRef={f(),f(),f()};cylinder.mAxisRef={f(),f(),f()};')
tests=tests.replace('fold(&cylinder.mAxis,sizeof(Vector));','fold(&custom.mAxis,sizeof(Vector));fold(&cylinder.mAxis,sizeof(Vector));')
tests=tests.replace('printf("%llu %u\\n",(unsigned long long)hash,boundsTests);','printf("%llu %u %u %u\\n",(unsigned long long)hash,boundsTests,transformReads,collisionTransformReuses);')
with tempfile.TemporaryDirectory(prefix='srr-collision-transform-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests);outputs=[]
 for mode in ['reference','once']:
  cmd=['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=address,undefined','-DSRR2_OLD3DS_COLL_PREP',str(p/'test.cpp'),'-o',str(p/mode)]
  if mode=='once':cmd+=['-DSRR2_OLD3DS_COLL_TRANSFORM_ONCE']
  subprocess.run(cmd,check=True)
  outputs.append([subprocess.check_output([str(p/mode),str(round)]).split() for round in range(4)])
 for reference,once in zip(*outputs):
  assert reference[:2]==once[:2],(reference,once)
  assert int(reference[2])-int(once[2])==int(once[3]) and int(once[3])>0,(reference,once)
 print('PASS: 80000 production volume trees and 80000 ordered collision child-list cases across four rounding modes; exact positions/axes/bounds/Updated state and candidate order, nonfinite inputs, manual updates, shared flexible scratch, custom shape overrides, state replacement during traversal and unknown SimState fallback.')
 print('Transform reads per 20000-tree run: '+outputs[0][0][2].decode()+' reference / '+outputs[1][0][2].decode()+' optimized. Not native performance evidence.')

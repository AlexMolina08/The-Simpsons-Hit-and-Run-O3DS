#!/usr/bin/env python3
"""Check scoped transform reuse against production lazy joint evaluation."""
from pathlib import Path
import importlib.util,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('pose_fixture',root/'tools/3ds/tests/pose_reuse.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
code=m.code.split('int main(){')[0]+f'\n#include "{root}/libs/sim/simphysics/jointpipeline.hpp"\n'+r'''
#include <cfenv>
int main(){
 unsigned comparisons=0,fetches=0,reused=0;
 std::mt19937 rng(7439);
 for(int mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(mode));
  for(unsigned scenario=0;scenario<200;++scenario){
   const unsigned count=2+rng()%63;
   tSkeleton skeleton;tPose source;source.skeleton=&skeleton;
   skeleton.joints.resize(count);source.joints.resize(count);
   for(unsigned i=1;i<count;++i){skeleton.joints[i].parentIndex=rng()%i;source.joints[i].objectMatrix.m[3][0]=float(i)*.125f;}
   if(scenario%17==0) source.joints[1].objectMatrix.m[3][0]=-0.0f;
   if(scenario%41==0) source.joints[1].objectMatrix.m[3][0]=std::numeric_limits<float>::infinity();
   if(scenario%43==0) source.joints[1].objectMatrix.m[3][0]=std::numeric_limits<float>::quiet_NaN();
   Optimized::Pose original,candidate;original.Assign(&source);candidate.Assign(&source);
   sim::jointPipeline::View<rmt::Matrix>* active=nullptr;
   for(unsigned step=0;step<25;++step){
    // Writes occur between synchronization stages, never inside a retained view.
    unsigned changed=rng()%count;rmt::Matrix world;world.m[3][0]=float(step)*.25f;
    if(step%3==0){original.m_Joints[changed].SetWorldMatrix(world);candidate.m_Joints[changed].SetWorldMatrix(world);}
    else {original.m_Joints[changed].SetObjectMatrix(world);candidate.m_Joints[changed].SetObjectMatrix(world);}
    sim::jointPipeline::View<rmt::Matrix> view;
    {
     sim::jointPipeline::Scope<rmt::Matrix> scope(active,&view);
     for(unsigned k=0;k<count*4;++k){
      unsigned i=rng()%count;
      const auto& a=original.m_Joints[i].GetWorldMatrix();
      const auto& b=active->Resolve(i,[&]() -> const rmt::Matrix& {++fetches;return candidate.m_Joints[i].GetWorldMatrix();});
      assert(!memcmp(&a,&b,sizeof(a)));++comparisons;
     }
     // Nested synchronization must restore its parent's view, not clear it.
     sim::jointPipeline::View<rmt::Matrix> nested;
     {sim::jointPipeline::Scope<rmt::Matrix> inner(active,&nested);assert(active==&nested);}
     assert(active==&view);
    }
    assert(!active);reused+=view.hits;
    // Subsequent quaternion/object reads must retain original provenance and flags.
    for(unsigned i=0;i<count;++i){auto& a=original.m_Joints[i];auto& b=candidate.m_Joints[i];
     auto am=a.GetObjectMatrix(),bm=b.GetObjectMatrix();assert(!memcmp(&am,&bm,sizeof(am)));
     auto aq=a.GetWorldQuaternion(),bq=b.GetWorldQuaternion();assert(!memcmp(&aq,&bq,sizeof(aq)));
     assert(int(a.m_Dirty)==int(b.m_Dirty));
    }
   }
  }
 }
 // Out-of-capacity queries retain ordinary lazy fetching.
 sim::jointPipeline::View<rmt::Matrix> view;rmt::Matrix matrix;unsigned fallback=0;
 view.Resolve(64,[&]() -> const rmt::Matrix& {++fallback;return matrix;});
 view.Resolve(64,[&]() -> const rmt::Matrix& {++fallback;return matrix;});assert(fallback==2);
 printf("PASS: %u production lazy-joint matrix comparisons; four rounding modes, shared parents, inter-stage writes, quaternion/dirty-state equivalence, nested scope and oversized fallback; %u fetches / %u reused reads. Not native gains.\n",comparisons,fetches,reused);
}
'''
# Keep previous-state storage untouched and retain legacy synchronization ordering.
s=(root/'libs/sim/simphysics/articulatedphysicsobject.cpp').read_text()
assert 'jointPipeline::Scope<Matrix> scope(' in s
assert 'mSimJoints[i]->SetTransform(JointTransform(mSimJoints[i]->mIndex));' in s
with tempfile.TemporaryDirectory(prefix='srr-joint-pipeline-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-frounding-math','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

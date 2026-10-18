#!/usr/bin/env python3
"""Check whether deferred StoreJointState can preserve production pose semantics."""
from pathlib import Path
import importlib.util,subprocess,tempfile,json
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('pose_fixture',root/'tools/3ds/tests/pose_reuse.py');fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
code=fixture.code[:fixture.code.index('int main(){')]
code=code.replace('#define SRR2_OLD3DS_JOINT_WORLD_REUSE','#define SRR2_OLD3DS_JOINT_WORLD_REUSE\n#define SRR2_OLD3DS_JOINT_ROTATION_REUSE')
code+=r'''
int main(){
 tSkeleton skeleton;tPose input;input.skeleton=&skeleton;skeleton.joints.resize(3);input.joints.resize(3);
 skeleton.joints[1].parentIndex=0;skeleton.joints[2].parentIndex=1;
 input.joints[1].objectMatrix.m[3][0]=2;input.joints[2].objectMatrix.m[3][0]=3;
 Optimized::Pose original,deferred,snapshot;original.Assign(&input);deferred.Assign(&input);snapshot.Assign(&input);
 auto copyState=[](Optimized::Joint& target,const Optimized::Joint& source){
  target.m_Object=source.m_Object;target.m_World=source.m_World;target.m_Dirty=source.m_Dirty;
  target.m_WorldCoherent=source.m_WorldCoherent;target.m_RotationCoherent=source.m_RotationCoherent;
 };
 rmt::Matrix root;root.m[3][0]=7;original.m_Joints[0].SetWorldMatrix(root);deferred.m_Joints[0].SetWorldMatrix(root);
 // Legacy StoreJointState evaluates every live world. Proposed snapshots do not.
 rmt::Matrix previous[3];for(unsigned i=0;i<3;++i)previous[i]=original.m_Joints[i].GetWorldMatrix();
 for(unsigned i=0;i<3;++i)copyState(snapshot.m_Joints[i],deferred.m_Joints[i]);
 for(unsigned i=0;i<3;++i){auto value=snapshot.m_Joints[i].GetWorldMatrix();assert(!memcmp(&value,&previous[i],sizeof(value)));}
 // Clean is a supported operation that intentionally discards dirty work. It can
 // observe whether StoreJointState previously populated live world caches.
 original.Clean();deferred.Clean();
 unsigned mismatches=0;
 for(unsigned i=0;i<3;++i){auto a=original.m_Joints[i].GetWorldMatrix(),b=deferred.m_Joints[i].GetWorldMatrix();if(memcmp(&a,&b,sizeof(a))){++mismatches;printf("LAZY_STORE_COUNTEREXAMPLE joint=%u original_x=%.1f deferred_x=%.1f\n",i,a.m[3][0],b.m[3][0]);}}
 assert(mismatches==2);
 puts("PASS: naive deferred world snapshots rejected: previous matrices match, but subsequent production Pose::Clean changes live pose outputs. No game patch applied.");
}
'''
with tempfile.TemporaryDirectory(prefix='lazy-joint-audit-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

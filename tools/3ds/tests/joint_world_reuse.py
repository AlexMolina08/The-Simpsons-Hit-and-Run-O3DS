#!/usr/bin/env python3
"""Differential checks for production joint setters, dependencies and quaternion provenance."""
from pathlib import Path
import importlib.util, subprocess, tempfile, sys
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('pose',root/'tools/3ds/tests/pose_reuse.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
extra=r'''
void jointWorldCases(){
 unsigned referenceProducts=0,reusedProducts=0,comparisons=0;
 for(unsigned scenario=0;scenario<30;++scenario){
  tSkeleton skeleton;tPose source;source.skeleton=&skeleton;unsigned count=40;
  skeleton.joints.resize(count);source.joints.resize(count);
  for(unsigned i=1;i<count;++i){skeleton.joints[i].parentIndex=(i-1)/2;source.joints[i].objectMatrix.m[3][0]=float(i)*.125f;}
  if(scenario==27)source.joints[3].objectMatrix.m[0][1]=-0.0f;
  if(scenario==28)source.joints[3].objectMatrix.m[0][1]=std::numeric_limits<float>::quiet_NaN();
  if(scenario==29)source.joints[3].objectMatrix.m[0][1]=std::numeric_limits<float>::infinity();
  Reference::Pose a;Optimized::Pose b;a.Assign(&source);b.Assign(&source);
  for(unsigned step=0;step<300;++step){
   rmt::Matrix world;world.m[3][0]=float(step/17);world.m[3][1]=float(scenario)*.25f;
   a.m_Joints[0].SetWorldMatrix(world);b.m_Joints[0].SetWorldMatrix(world);
   for(unsigned i=1;i<count;++i){auto& x=a.m_Joints[i];auto& y=b.m_Joints[i];
    rmt::Matrix local=source.joints[i].objectMatrix;
    if(step%19==0&&i==step%count){local.m[3][2]=float(step)*.125f;source.joints[i].objectMatrix=local;}
    x.SetObjectMatrix(local);y.SetObjectMatrix(local);
    if(step%3==0){auto v=local.Row(3);x.SetObjectTranslation(v);y.SetObjectTranslation(v);}
    if(step%7==0){x.SetObjectMatrixRotation(local);y.SetObjectMatrixRotation(local);}
    if(step%11==0){rmt::Quaternion q;q.w=(step%22==0)?-1:1;x.SetObjectQuaternion(q);y.SetObjectQuaternion(q);}
   }
   for(unsigned i=0;i<count;++i)assert(int(a.m_Joints[i].m_Dirty)==int(b.m_Joints[i].m_Dirty));
   if(step%5==0){for(unsigned i=1;i<count;++i){
    Reference::Transform x=a.m_Joints[i].GetWorldTransform();Optimized::Transform y=b.m_Joints[i].GetWorldTransform();
    auto xm=x.GetMatrix(),ym=y.GetMatrix();assert(!memcmp(&xm,&ym,sizeof(xm)));
    x.SetMatrix(xm);y.SetMatrix(ym);a.m_Joints[i].SetWorldTransform(x);b.m_Joints[i].SetWorldTransform(y);
   }}
   if(step%29==0){unsigned i=1+step%(count-1);Reference::Transform x;Optimized::Transform y;
    rmt::Quaternion q;q.w=-1;x.SetQuaternion(q);y.SetQuaternion(q);a.m_Joints[i].SetWorldTransform(x);b.m_Joints[i].SetWorldTransform(y);
    a.m_Joints[i].GetObjectTransform();b.m_Joints[i].GetObjectTransform();}
   if(step%31==0){unsigned i=1+step%(count-1);rmt::Matrix v;v.m[3][2]=float(step);
    a.m_Joints[i].SetWorldMatrixRotation(v);b.m_Joints[i].SetWorldMatrixRotation(v);
    auto t=v.Row(3);a.m_Joints[i].SetWorldTranslation(t);b.m_Joints[i].SetWorldTranslation(t);}
   if(step%37==0){rmt::Quaternion q;q.w=-1;a.m_Joints[5].SetWorldQuaternion(q);b.m_Joints[5].SetWorldQuaternion(q);}
   if(step%43==0){auto& x=a.m_Joints[7];auto& y=b.m_Joints[7];x.SetObjectMatrix(x.GetObjectMatrix());y.SetObjectMatrix(y.GetObjectMatrix());}
   if(step%47==0){unsigned parent=(step/47)%2;a.m_Joints[39].SetParentJoint(&a.m_Joints[parent]);b.m_Joints[39].SetParentJoint(&b.m_Joints[parent]);}
   if(step%41==0){a.Clean();b.Clean();}
   for(unsigned index=0;index<count;++index){unsigned i=step%2?count-1-index:index;auto& x=a.m_Joints[i];auto& y=b.m_Joints[i];
    multiplies=0;auto xm=x.GetWorldTransform().GetMatrix();referenceProducts+=multiplies;
    multiplies=0;auto ym=y.GetWorldTransform().GetMatrix();reusedProducts+=multiplies;assert(!memcmp(&xm,&ym,sizeof(xm)));
    xm=x.GetObjectTransform().GetMatrix();ym=y.GetObjectTransform().GetMatrix();assert(!memcmp(&xm,&ym,sizeof(xm)));
    auto xq=x.GetWorldTransform().GetQuaternion(),yq=y.GetWorldTransform().GetQuaternion();assert(!memcmp(&xq,&yq,sizeof(xq)));
    xq=x.GetObjectTransform().GetQuaternion();yq=y.GetObjectTransform().GetQuaternion();assert(!memcmp(&xq,&yq,sizeof(xq)));
    assert(int(x.m_Dirty)==int(y.m_Dirty));++comparisons;
   }
  }
 }
 referenceProducts=Reference::allProducts;reusedProducts=Optimized::allProducts;
 assert(reusedProducts<referenceProducts);
 printf("PASS: %u joint setter/dependency cases; products %u reference / %u reused; dirty flags and matrix/quaternion bits match\n",comparisons,referenceProducts,reusedProducts);
 assert(sizeof(Optimized::Joint)<=sizeof(Reference::Joint)+8);
}
'''
extra += r'''
void retainedSetterCases(){
 tSkeleton skeleton;tPose source;source.skeleton=&skeleton;const unsigned count=60;
 skeleton.joints.resize(count);source.joints.resize(count);
 for(unsigned i=1;i<count;++i){skeleton.joints[i].parentIndex=(i-1)/2;source.joints[i].objectMatrix.m[3][0]=float(i)*.125f;}
 Reference::Pose a;Optimized::Pose b;
 const unsigned beforeA=Reference::allProducts,beforeB=Optimized::allProducts;
 for(unsigned step=0;step<400;++step){
  a.Assign(&source);b.Assign(&source);
  rmt::Vector root(float(step/23),0,0);
  a.m_Joints[0].SetWorldTranslation(root);b.m_Joints[0].SetWorldTranslation(root);
  for(unsigned i=1;i<count;++i){
   auto translation=source.joints[i].objectMatrix.Row(3);
   a.m_Joints[i].SetObjectTranslation(translation);b.m_Joints[i].SetObjectTranslation(translation);
   rmt::Quaternion q;q.w=step%2 ? -1 : 1;
   if(i%17==0){q.x=float(step%3)*.125f;q.w=sqrtf(1-q.x*q.x);}
   a.m_Joints[i].SetObjectQuaternion(q);b.m_Joints[i].SetObjectQuaternion(q);
  }
  for(unsigned offset=0;offset<count;++offset){
   unsigned i=step%2 ? count-1-offset : offset;
   auto& x=a.m_Joints[i];auto& y=b.m_Joints[i];
   auto xm=x.GetWorldTransform().GetMatrix(),ym=y.GetWorldTransform().GetMatrix();
   assert(!memcmp(&xm,&ym,sizeof(xm)));
   auto xq=x.GetWorldTransform().GetQuaternion(),yq=y.GetWorldTransform().GetQuaternion();
   assert(!memcmp(&xq,&yq,sizeof(xq)));
   xm=x.GetObjectTransform().GetMatrix();ym=y.GetObjectTransform().GetMatrix();
   assert(!memcmp(&xm,&ym,sizeof(xm)));
   xq=x.GetObjectTransform().GetQuaternion();yq=y.GetObjectTransform().GetQuaternion();
   assert(!memcmp(&xq,&yq,sizeof(xq)));
   assert(int(x.m_Dirty)==int(y.m_Dirty));
  }
 }
 const unsigned productsA=Reference::allProducts-beforeA,productsB=Optimized::allProducts-beforeB;
 assert(productsB<=productsA);
#ifdef SRR2_OLD3DS_JOINT_COHERENCE_CHAIN
 assert(productsB<productsA);
#endif
 printf("PASS: 24,000 retained-pose/driver setter comparisons; products %u reference / %u reused; moving roots, rotation changes, source quaternion signs and alternating evaluation order\n",productsA,productsB);
}
'''
extra += r'''
void worldProvenanceCases(){
 for(unsigned sample=0;sample<120;++sample){
  tSkeleton skeleton;tPose source;source.skeleton=&skeleton;
  skeleton.joints.resize(3);source.joints.resize(3);
  skeleton.joints[1].parentIndex=0;skeleton.joints[2].parentIndex=1;
  Reference::Pose a;Optimized::Pose b;a.Assign(&source);b.Assign(&source);
  rmt::Quaternion q;q.x=.13f+float(sample)*.0001f;q.y=.27f;q.z=.31f;q.w=sqrtf(1-q.x*q.x-q.y*q.y-q.z*q.z);
  rmt::Matrix parent;q.ConvertToMatrix(&parent);parent.m[3][0]=13.137f;parent.m[3][1]=7.313f;
  a.m_Joints[0].SetWorldMatrix(parent);b.m_Joints[0].SetWorldMatrix(parent);
  q.x=.23f;q.y=.11f;q.z=.37f;q.w=sqrtf(1-q.x*q.x-q.y*q.y-q.z*q.z);
  rmt::Matrix world;q.ConvertToMatrix(&world);world.m[3][0]=21.337f+float(sample)*.01f;world.m[3][1]=8.937f;
  a.m_Joints[1].SetWorldMatrix(world);b.m_Joints[1].SetWorldMatrix(world);
  auto local=a.m_Joints[1].GetObjectTransform().GetMatrix();
  auto optimizedLocal=b.m_Joints[1].GetObjectTransform().GetMatrix();
  assert(!memcmp(&local,&optimizedLocal,sizeof(local)));
  source.joints[1].objectMatrix=local;
  a.m_Joints[0].GetObjectTransform();b.m_Joints[0].GetObjectTransform();
  a.m_Joints[2].GetWorldTransform();b.m_Joints[2].GetWorldTransform();
  a.Assign(&source);b.Assign(&source);
  for(unsigned i=0;i<3;++i){
   auto reference=a.m_Joints[i].GetWorldTransform().GetMatrix();
   auto optimized=b.m_Joints[i].GetWorldTransform().GetMatrix();
   assert(!memcmp(&reference,&optimized,sizeof(reference)));
   auto rq=a.m_Joints[i].GetWorldTransform().GetQuaternion(),oq=b.m_Joints[i].GetWorldTransform().GetQuaternion();
   assert(!memcmp(&rq,&oq,sizeof(rq)));
  }
 }
 puts("PASS: 120 direct-world/inverse-local pose reassignments preserve product and quaternion provenance");
}
'''
code='#include <limits>\n'+m.code
if '--chain' in sys.argv:
 code=code.replace('#define SRR2_OLD3DS_JOINT_WORLD_REUSE', '#define SRR2_OLD3DS_JOINT_WORLD_REUSE\n#define SRR2_OLD3DS_JOINT_COHERENCE_CHAIN')
if '--rotation' in sys.argv:
 code=code.replace('#define SRR2_OLD3DS_JOINT_WORLD_REUSE', '#define SRR2_OLD3DS_JOINT_WORLD_REUSE\n#define SRR2_OLD3DS_JOINT_ROTATION_REUSE')
 code=code.replace('namespace Optimized {', 'namespace Optimized { static unsigned rotationProducts;')
 code=code.replace('rmt::Matrix result=m_World.GetMatrix();', '++rotationProducts; rmt::Matrix result=m_World.GetMatrix();')
code=code.replace('namespace Reference {', 'namespace Reference { static unsigned allProducts;').replace('namespace Optimized {', 'namespace Optimized { static unsigned allProducts;')
code=code.replace('m_Matrix.Mult(xForm.GetMatrix());', '++allProducts; m_Matrix.Mult(xForm.GetMatrix());')
code=code.replace('int main(){std::mt19937',extra+'\nint main(){jointWorldCases();retainedSetterCases();worldProvenanceCases();std::mt19937',1)
if '--rotation' in sys.argv:
 code=code.replace('jointWorldCases();retainedSetterCases();worldProvenanceCases();std::mt19937', 'jointWorldCases();retainedSetterCases();worldProvenanceCases();assert(Optimized::rotationProducts>0);printf("Partial rotation reuse: %u products skipped\\n",Optimized::rotationProducts);std::mt19937')
with tempfile.TemporaryDirectory(prefix='srr-joint-world-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

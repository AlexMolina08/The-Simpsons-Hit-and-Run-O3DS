#!/usr/bin/env python3
"""Execute production pose assignment and joint dependencies against the reference."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
pose=(root/'libs/poser/src/pose.cpp').read_text();joint=(root/'libs/poser/src/joint.cpp').read_text();matrix=(root/'libs/radmath/radmath/matrix.cpp').read_text()
def extract(source,signature):
 start=source.index(signature);end=source.index('{',start)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <random>
#include <vector>
#define P3DASSERT(x) assert(x)
#define rAssert(x) assert(x)
static unsigned multiplies;
namespace rmt {
struct Vector {float x,y,z;Vector()=default;Vector(float a,float b,float c):x(a),y(b),z(c){}};
struct Matrix {float m[4][4];Matrix(){Identity();}void Identity(){memset(m,0,sizeof(m));for(int i=0;i<4;++i)m[i][i]=1;}
 Vector& Row(int i){return *reinterpret_cast<Vector*>(m[i]);}const Vector& Row(int i)const{return *reinterpret_cast<const Vector*>(m[i]);}
 void FillTranslate(const Vector& v){Row(3)=v;}
 void Mult(const Matrix&,const Matrix&);void Mult(const Matrix& b){Matrix temp;temp.Mult(*this,b);*this=temp;}
 void Transpose(){for(int i=0;i<4;++i)for(int j=i+1;j<4;++j){float t=m[i][j];m[i][j]=m[j][i];m[j][i]=t;}}
 void IdentityProjection(){m[0][3]=m[1][3]=m[2][3]=0;m[3][3]=1;}void InvertOrtho();};
'''+extract(matrix,'void Matrix::Mult(const Matrix& a, const Matrix& b)').replace('{','{++multiplies;',1)+extract(matrix,'void Matrix::InvertOrtho(void)')+r'''
}
struct tRefCounted {template<class T>static void Assign(T*& to,T* from){to=from;}};
struct tSkeleton {struct Joint {int parentIndex;};std::vector<Joint> joints;
 int GetNumJoint(){return joints.size();}Joint* GetJoint(int i){return &joints.at(i);}};
struct tPose {struct Joint {rmt::Matrix objectMatrix;};std::vector<Joint> joints;tSkeleton* skeleton;
 tSkeleton* GetSkeleton(){return skeleton;}Joint* GetJoint(int i){return &joints.at(i);}};
'''
joint_header=(root/'libs/poser/inc/poser/joint.hpp').read_text()
constants=(root/'libs/poser/inc/poser/constants.hpp').read_text()
space=constants[constants.index('enum Space'):constants.index('};',constants.index('enum Space'))+2]
classes=space+joint_header[joint_header.index('class Joint'):joint_header.index('} // namespace poser')].replace('private:', 'public:')+r'''
class Pose {public: bool m_ReuseWorlds=false;tSkeleton* m_Skeleton=nullptr;Joint* m_Joints=nullptr;Joint** m_Children=nullptr;
 ~Pose(){delete[] m_Joints;delete[] m_Children;}
 int GetJointCount(){return m_Skeleton?m_Skeleton->GetNumJoint():0;}void Assign(tPose*);void Clean();};
'''
transform_header=(root/'libs/poser/inc/poser/transform.hpp').read_text()
transform_source=(root/'libs/poser/src/transform.cpp').read_text()
quaternion_source=(root/'libs/radmath/radmath/quaternion.cpp').read_text()
transform_class=transform_header[transform_header.index('class Transform'):transform_header.index('} // namespace poser')]
transform_methods='\n'.join(extract(transform_source,x) for x in ['Transform::Transform()','Transform::Transform(const rmt::Matrix& m)','Transform::Transform(const rmt::Quaternion& q,','Transform::Transform(const rmt::Vector& v)','void Transform::Identity()','void Transform::SetMatrix(const rmt::Matrix& m)','void Transform::SetMatrixRotation(const rmt::Matrix& m)','const rmt::Quaternion& Transform::GetQuaternion() const','void Transform::SetQuaternion(const rmt::Quaternion& q)','void Transform::Invert()'])
quat_decl='struct Quaternion {float x=0,y=0,z=0,w=1;void Identity(){x=y=z=0;w=1;}void BuildFromMatrix(const Matrix&);void ConvertToMatrix(Matrix*)const;};\n'
marker='}\nstruct tRefCounted'
prelude=prelude.replace(marker,quat_decl+extract(quaternion_source,'void Quaternion::BuildFromMatrix(const Matrix& m)')+extract(quaternion_source,'void Quaternion::ConvertToMatrix(Matrix* m) const')+marker,1)
classes=transform_class+transform_methods+classes
methods='\n'.join(extract(joint,s) for s in ['Joint::Joint()', 'Joint::Joint(const Joint& joint)', 'Joint::~Joint()', 'Joint& Joint::operator = (const Joint& joint)', 'void Joint::SetChildJointCount(', 'void Joint::SetChildJointMem(', 'Joint* Joint::GetChildJoint(int i)', 'const Joint* Joint::GetChildJoint(int i) const', 'bool Joint::AddChildJoint(', 'void Joint::Clean()', 'void Joint::SetObjectMatrixRotation(', 'void Joint::SetObjectQuaternion(', 'void Joint::SetWorldMatrixRotation(', 'void Joint::SetWorldTranslation(', 'void Joint::SetWorldQuaternion(', 'const Transform& Joint::GetObjectTransform() const','const Transform& Joint::GetWorldTransform() const','void Joint::SetObjectTransform(','void Joint::SetObjectMatrix(','void Joint::SetObjectTranslation(','void Joint::SetWorldTransform(','void Joint::SetWorldMatrix(','void Joint::NotifyWorldModified(', 'void Joint::ParentWorldModified('])+'\n'+extract(pose,'void Pose::Assign(tPose* p3dPose)')+extract(pose,'void Pose::Clean()')
code=prelude+'\nnamespace Reference {\n'+classes+methods+'\n}\n#define SRR2_OLD3DS_POSE_REUSE\n#define SRR2_OLD3DS_QUATERNION_REUSE\n#define SRR2_OLD3DS_JOINT_WORLD_REUSE\nnamespace Optimized {\n'+classes+methods+r'''
}
int main(){std::mt19937 rng(329);unsigned referenceWork=0,optimizedWork=0,comparisons=0;
 for(unsigned trial=0;trial<1000;++trial){tSkeleton skeleton;tPose source;source.skeleton=&skeleton;unsigned count=2+rng()%80;
  skeleton.joints.resize(count);source.joints.resize(count);
  for(unsigned i=1;i<count;++i){skeleton.joints[i].parentIndex=rng()%i;auto& m=source.joints[i].objectMatrix;
   float angle=float(rng()%20)*.01f;m.m[0][0]=m.m[1][1]=cosf(angle);m.m[0][1]=sinf(angle);m.m[1][0]=-sinf(angle);
   m.m[3][0]=float(rng()%10);m.m[3][1]=float(rng()%10);}
  Reference::Pose a;Optimized::Pose b;a.Assign(&source);b.Assign(&source);
  auto compare=[&](){for(unsigned i=0;i<count;++i){auto& x=a.m_Joints[i];auto& y=b.m_Joints[i];
    auto xm=x.GetWorldTransform().GetMatrix(),ym=y.GetWorldTransform().GetMatrix();assert(!memcmp(&xm,&ym,sizeof(xm)));
    xm=x.GetObjectTransform().GetMatrix();ym=y.GetObjectTransform().GetMatrix();assert(!memcmp(&xm,&ym,sizeof(xm)));
    auto xq=x.GetWorldTransform().GetQuaternion(),yq=y.GetWorldTransform().GetQuaternion();assert(!memcmp(&xq,&yq,sizeof(xq)));
    xq=x.GetObjectTransform().GetQuaternion();yq=y.GetObjectTransform().GetQuaternion();assert(!memcmp(&xq,&yq,sizeof(xq)));++comparisons;}};
  compare();
  for(unsigned step=0;step<32;++step){
   if(step%4==0){rmt::Matrix root;root.m[3][0]=float(step);a.m_Joints[0].SetWorldMatrix(root);b.m_Joints[0].SetWorldMatrix(root);}
   unsigned child=1+rng()%(count-1);
   if(step%5==0){rmt::Vector v{float(step),-.5f,float(child)};a.m_Joints[child].SetObjectTranslation(v);b.m_Joints[child].SetObjectTranslation(v);}
   compare();
   if(step%7==0)source.joints[child].objectMatrix.m[3][1]+=.25f;
   if(step%11==0)source.joints[0].objectMatrix.m[3][0]+=.5f;
   if(step%3==0){rmt::Matrix m=a.m_Joints[child].GetWorldTransform().GetMatrix();m.m[3][2]+=.75f;
    a.m_Joints[child].SetWorldMatrix(m);b.m_Joints[child].SetWorldMatrix(m);}
   // Choreo Clean deliberately removes dirty flags without evaluating worlds.
   if(step%6==0){rmt::Matrix m;m.m[3][2]=float(step)+9;
    a.m_Joints[0].SetWorldMatrix(m);b.m_Joints[0].SetWorldMatrix(m);a.Clean();b.Clean();}
   multiplies=0;a.Assign(&source);for(unsigned i=0;i<count;++i)a.m_Joints[i].GetWorldTransform();referenceWork+=multiplies;
   multiplies=0;b.Assign(&source);for(unsigned i=0;i<count;++i)b.m_Joints[i].GetWorldTransform();optimizedWork+=multiplies;
   compare();
  }
 }
 // A cached world quaternion can retain a different representation of the same
 // rotation after world-to-object conversion. The reference forces matrix-derived
 // quaternion evaluation for non-root joints on their next world read.
 {tSkeleton sk;sk.joints.resize(2);sk.joints[1].parentIndex=0;
  tPose src;src.skeleton=&sk;src.joints.resize(2);Reference::Pose a;Optimized::Pose b;
  a.Assign(&src);b.Assign(&src);a.m_Joints[1].GetWorldTransform();b.m_Joints[1].GetWorldTransform();
  Reference::Transform x;Optimized::Transform y;rmt::Quaternion driver;x.SetQuaternion(driver);y.SetQuaternion(driver);
  a.m_Joints[1].SetWorldTransform(x);b.m_Joints[1].SetWorldTransform(y);
  src.joints[1].objectMatrix=a.m_Joints[1].GetObjectTransform().GetMatrix();b.m_Joints[1].GetObjectTransform();
  a.Assign(&src);b.Assign(&src);
  auto aq=a.m_Joints[1].GetWorldTransform().GetQuaternion(),bq=b.m_Joints[1].GetWorldTransform().GetQuaternion();assert(!memcmp(&aq,&bq,sizeof(aq)));
 }
 assert(optimizedWork<referenceWork);
 printf("PASS: %u production joint world/object matrix and quaternion comparisons over 1,000 hierarchies; matrix products %u reference / %u reused; root, parent, local, world, source mutations and unevaluated Clean\n",comparisons,referenceWork,optimizedWork);
}
'''
if __name__ == '__main__':
 with tempfile.TemporaryDirectory(prefix='srr-pose-') as td:
  p=Path(td);(p/'test.cpp').write_text(code)
  subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)

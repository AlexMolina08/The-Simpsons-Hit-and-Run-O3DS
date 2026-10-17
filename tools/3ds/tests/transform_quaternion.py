#!/usr/bin/env python3
"""Compare production Transform and radmath quaternion conversion, including sign provenance."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
def extract(source,signature):
 a=source.index(signature);b=source.index('{',a)+1;depth=1
 while depth:depth+=(source[b]=='{')-(source[b]=='}');b+=1
 return source[a:b]
t=(root/'libs/poser/src/transform.cpp').read_text();h=(root/'libs/poser/inc/poser/transform.hpp').read_text();q=(root/'libs/radmath/radmath/quaternion.cpp').read_text();m=(root/'libs/radmath/radmath/matrix.cpp').read_text()
cls=h[h.index('class Transform'):h.index('} // namespace poser')]
methods='\n'.join(extract(t,x) for x in ['Transform::Transform()','Transform::Transform(const rmt::Matrix& m)','Transform::Transform(const rmt::Quaternion& q,','Transform::Transform(const rmt::Vector& v)','void Transform::Identity()','void Transform::SetMatrix(const rmt::Matrix& m)','void Transform::SetMatrixRotation(const rmt::Matrix& m)','const rmt::Quaternion& Transform::GetQuaternion() const','void Transform::SetQuaternion(const rmt::Quaternion& q)','void Transform::Invert()'])
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <random>
#include <limits>
#define rAssert(x) assert(x)
static unsigned builds,converts;
namespace rmt {
struct Vector{float x,y,z;Vector()=default;Vector(float a,float b,float c):x(a),y(b),z(c){}};
struct Matrix{float m[4][4];Matrix(){Identity();}void Identity(){memset(m,0,sizeof(m));for(int i=0;i<4;++i)m[i][i]=1;}
 Vector& Row(int i){return *reinterpret_cast<Vector*>(m[i]);}const Vector& Row(int i)const{return *reinterpret_cast<const Vector*>(m[i]);}
 void FillTranslate(const Vector& v){Row(3)=v;}void IdentityProjection(){m[0][3]=m[1][3]=m[2][3]=0;m[3][3]=1;}
 void Transpose(){for(int i=0;i<4;++i)for(int j=i+1;j<4;++j){float t=m[i][j];m[i][j]=m[j][i];m[j][i]=t;}}
 void InvertOrtho();void Mult(const Matrix&,const Matrix&);void Mult(const Matrix& b){Matrix temp;temp.Mult(*this,b);*this=temp;}};
struct Quaternion{float x=0,y=0,z=0,w=1;void Identity(){x=y=z=0;w=1;}void BuildFromMatrix(const Matrix&);void ConvertToMatrix(Matrix*)const;};
'''
build=extract(q,'void Quaternion::BuildFromMatrix(const Matrix& m)').replace('{','{\n++builds;',1)
prelude+=build+extract(q,'void Quaternion::ConvertToMatrix(Matrix* m) const').replace('{','{++converts;',1)+extract(m,'void Matrix::Mult(const Matrix& a, const Matrix& b)')+extract(m,'void Matrix::InvertOrtho(void)')+'}\n'
tests=r'''
int main(){
 static_assert(sizeof(Reference::Transform)==sizeof(Optimized::Transform),"No per-transform memory growth");
 std::mt19937 rng(8123);unsigned comparisons=0,refWork=0,fastWork=0,refConverts=0,fastConverts=0;
 Reference::Transform a;Optimized::Transform b;
 auto setQuaternion=[&](const rmt::Quaternion& q){converts=0;a.SetQuaternion(q);refConverts+=converts;converts=0;b.SetQuaternion(q);fastConverts+=converts;};
 auto compare=[&](){builds=0;rmt::Quaternion qa=a.GetQuaternion();refWork+=builds;builds=0;rmt::Quaternion qb=b.GetQuaternion();fastWork+=builds;
  assert(!memcmp(&qa,&qb,sizeof(qa)));assert(!memcmp(&a.GetMatrix(),&b.GetMatrix(),sizeof(rmt::Matrix)));++comparisons;};
 for(unsigned n=0;n<200000;++n){
  rmt::Matrix value=a.GetMatrix();
  switch(n%10){
   case 0:{rmt::Quaternion q;float angle=float(rng()%6284)*.001f;q.x=sinf(angle*.5f);q.w=cosf(angle*.5f);
    if(n%2){q.x=-q.x;q.w=-q.w;}setQuaternion(q);break;}
   case 1:value.m[3][0]=float(n);a.SetMatrix(value);b.SetMatrix(value);break;
   case 2:a.SetMatrixRotation(value);b.SetMatrixRotation(value);break;
   case 3:{rmt::Vector v{float(n),-.0f,2};a.SetTranslation(v);b.SetTranslation(v);break;}
   case 4:a.SetMatrix(a.GetMatrix());b.SetMatrix(b.GetMatrix());break;
   case 5:{Reference::Transform copy=a;Optimized::Transform other=b;a=copy;b=other;break;}
   case 6:a.Identity();b.Identity();break;
   case 7:a.Invert();b.Invert();break;
   case 8:value.m[0][1]=-value.m[0][1];a.SetMatrix(value);b.SetMatrix(value);break;
   case 9:{Reference::Transform x(value);Optimized::Transform y(value);a.Mult(x);b.Mult(y);break;}
  }compare();
 }
 // Repeated animation assignments must retain the exact conversion result.
 for(unsigned i=0;i<10000;++i){rmt::Quaternion q;q.x=.5f;q.w=.75f;setQuaternion(q);compare();setQuaternion(q);compare();}
 {rmt::Quaternion q;rmt::Vector v{1,2,3};Reference::Transform x(q,v);Optimized::Transform y(q,v);
  assert(!memcmp(&x.GetMatrix(),&y.GetMatrix(),sizeof(rmt::Matrix)));}
 assert(fastConverts<refConverts);
 printf("Quaternion-to-matrix conversions: %u reference / %u reused\n",refConverts,fastConverts);
 // Identity() and a driver-supplied quaternion retain +w. BuildFromMatrix uses
 // -w. An equal matrix assignment must canonicalize that provenance exactly.
 a.Identity();b.Identity();compare();a.SetMatrix(a.GetMatrix());b.SetMatrix(b.GetMatrix());compare();assert(a.GetQuaternion().w==-1);
 // Projection/translation bits do not enter the production quaternion formula.
 for(unsigned i=0;i<1000;++i){rmt::Matrix value=a.GetMatrix();value.m[0][3]=float(i);value.m[3][3]=float(i+1);
  a.SetMatrix(value);b.SetMatrix(value);compare();}
 for(float value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  rmt::Matrix bad;bad.m[0][0]=value;a.SetMatrix(bad);b.SetMatrix(bad);compare();
  a.SetMatrix(bad);b.SetMatrix(bad);compare();a.SetMatrixRotation(bad);b.SetMatrixRotation(bad);compare();
 }
 assert(fastWork<refWork);
 printf("PASS: %u production Transform/quaternion comparisons; %u reference / %u reused matrix-to-quaternion conversions; sign provenance, rotations, translation, self/copy assignment, inversion and no memory growth\n",comparisons,refWork,fastWork);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-transform-quaternion-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+'namespace Reference {\n'+cls+methods+'}\n#define SRR2_OLD3DS_QUATERNION_REUSE\nnamespace Optimized {\n'+cls+methods+'}\n'+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

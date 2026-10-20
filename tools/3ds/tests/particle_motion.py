#!/usr/bin/env python3
"""Run original and production emitter display; compare surviving native quads."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
# The retained implementation is unchanged at HEAD for this source file.
reference=subprocess.check_output(['git','show','HEAD:libs/pure3d/p3d/effects/particleemitter.cpp'],cwd=root,text=True)
candidate=(root/'libs/pure3d/p3d/effects/particleemitter.cpp').read_text()
def display(source,name):
 a=source.index('void tSpriteEmitter::Display()');b=source.index('\nvoid tSpriteEmitter::ResetBiases()',a)
 return source[a:b].replace('tSpriteEmitter::Display()',name+'::Display()')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>
#include <typeinfo>
#include <p3d/effects/particlemotion.hpp>
#include <p3d/effects/particlebounds.hpp>
#include <pddi/pica/particleclip.hpp>
#define P3DASSERT(x) assert(x)
using u32=unsigned;
namespace rmt {
const float PI_BY4=.785398185253143310546875f;
struct Vector {float x=0,y=0,z=0;void Sub(const Vector&a,const Vector&b){x=a.x-b.x;y=a.y-b.y;z=a.z-b.z;}
 void NormalizeSafe(){float m=x*x+y*y+z*z;if(m>0.0000001f){m=1/std::sqrt(m);x*=m;y*=m;z*=m;}else{x=y=z=0;}}};
struct Matrix {void Transform(const Vector&p,Vector*a) const {a->x=m[0][0]*p.x+m[1][0]*p.y+m[2][0]*p.z+m[3][0];a->y=m[0][1]*p.x+m[1][1]*p.y+m[2][1]*p.z+m[3][1];a->z=m[0][2]*p.x+m[1][2]*p.y+m[2][2]*p.z+m[3][2];}float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};};
float ATanCoords(float x,float y){if(x==0)return 0;int q=0;if(x<0){q=1;x=-x;}if(y<0){q^=3;y=-y;}const float a=atanf(y/x);switch(q){case 0:return a;case 1:return 3.141592741012573242f-a;case 2:return 3.141592741012573242f+a;default:return 6.283185482025146484f-a;}}
void SinCos(float a,float*s,float*c){*s=sinf(a);*c=cosf(a);}
}
struct pddiVector {float x,y,z;pddiVector(float a,float b,float c):x(a),y(b),z(c){}};
struct pddiVector2 {float x,y;pddiVector2(float a,float b):x(a),y(b){}};
using pddiColour=unsigned;
struct Quad {float values[20];unsigned colour;};std::vector<Quad> output;
struct pddiPrimStream {unsigned used=0;float values[30]={};unsigned colour=0;
 void Colour(unsigned c){colour=c;}void UV(float x,float y){values[used*5+3]=x;values[used*5+4]=y;}
 void Coord(float x,float y,float z){values[used*5]=x;values[used*5+1]=y;values[used*5+2]=z;if(++used==6){const unsigned corners[4]={0,2,1,4};Quad q={};q.colour=colour;for(unsigned i=0;i<4;++i)memcpy(q.values+i*5,values+corners[i]*5,20);output.push_back(q);used=0;}}} streamStorage;
namespace picaQuad{enum Topology{Particle};}
bool quadAvailable=true,clipAvailable=true,shaderSafe=true;unsigned projectionCalls=0,cameraMatrixReads=0;
struct tCamera {virtual ~tCamera(){};bool CameraToView(const rmt::Vector&a,rmt::Vector*out){if(a.z==0)return false;float inverse=1/a.z;out->x=a.x*inverse*1.3f*.5f;out->y=a.y*inverse*1.7f*.5f;out->z=a.z;return true;}rmt::Matrix matrix;float GetFieldOfView(){return 1.2f;}float GetAspectRatio(){return 1.6666667f;}const rmt::Matrix& GetWorldToCameraMatrix(){++cameraMatrixReads;return matrix;}};
struct tPointCamera:tCamera{};struct ForeignCamera:tPointCamera{};
struct tView{tCamera* camera=nullptr;tCamera* GetCamera(){return camera;}};
struct Context {tView view;tView* GetView(){return &view;}void WorldToView(const rmt::Vector&p,rmt::Vector*out){++projectionCalls;const auto&m=view.camera->matrix.m;rmt::Vector a;a.x=m[0][0]*p.x+m[1][0]*p.y+m[2][0]*p.z+m[3][0];a.y=m[0][1]*p.x+m[1][1]*p.y+m[2][1]*p.z+m[3][1];a.z=m[0][2]*p.x+m[1][2]*p.y+m[2][2]*p.z+m[3][2];if(a.z!=0){float inverse=1/a.z;out->x=a.x*inverse*1.3f*.5f;out->y=a.y*inverse*1.7f*.5f;out->z=a.z;}}};
struct Platform {Context context;Context* GetActiveContext(){return &context;}};
enum pddiCompareMode {PDDI_COMPARE_ALWAYS,PDDI_COMPARE_LESS};enum{PDDI_PRIM_TRIANGLES,PDDI_V_CT};
struct Device {bool write=true;pddiCompareMode compare=PDDI_COMPARE_LESS;bool GetZWrite(){return write;}void SetZWrite(bool v){write=v;}pddiCompareMode GetZCompare(){return compare;}void SetZCompare(pddiCompareMode v){compare=v;}pddiPrimStream* BeginPrims(void*,int,int,int){return &streamStorage;}void EndPrims(pddiPrimStream*){}};
struct Stack {rmt::Matrix matrix;std::vector<rmt::Matrix> history;rmt::Matrix* GetMatrix(){return &matrix;}void PushIdentity(){history.push_back(matrix);matrix={};}void Pop(){assert(!history.empty());matrix=history.back();history.pop_back();}};
namespace p3d{Platform platformStorage;Platform* platform=&platformStorage;Context* context=&platformStorage.context;Stack stackStorage;Stack* stack=&stackStorage;Device device;Device* pddi=&device;}
struct Shader{void* GetShader(){return this;}};
namespace p3dParticleSystemConstants{namespace ParticleAngleMode{enum{MOTION_ALIGNED=1,SPECIFIED=0};}}
struct tSpriteEmitterFactory {unsigned mode=1;float angle=0;bool zWrite=false,zTest=true;unsigned GetAngleMode(){return mode;}float GetAngle(){return angle;}bool GetZWrite(){return zWrite;}bool GetZTest(){return zTest;}};
struct tBaseParticle{rmt::Vector currPos,prevPos;virtual ~tBaseParticle(){}};
struct tSpriteParticle:tBaseParticle{float spin=0,size=.1f;unsigned colour=0;unsigned texFrame=0;};
struct Particles{std::vector<tBaseParticle*> live;int GetNumLiveParticles(){return int(live.size());}tBaseParticle**GetLiveParticles(){return live.data();}};
struct Module{unsigned in=0,out=0;void RenderInit(){++in;}void RenderExit(){++out;}};
struct Emitter{virtual ~Emitter(){};unsigned old3dsBoundsPhase=0;bool old3dsBoundsRejected=false;bool visible=true,local=false;Module*renderModule=nullptr;Particles*particles=nullptr;tSpriteEmitterFactory*factory=nullptr;Shader*shader=nullptr;float texFrameCoords[2]={0,1};};
struct Reference:Emitter{void Display();};struct Candidate:Emitter{void Display();};
using tSpriteEmitter=Candidate;
bool g_ParticleLOD=false;float g_ParticleLODDist=1000;
unsigned particleClipTested=0,particleClipRejected=0;
bool g_old3dsEmitterClip=false;unsigned g_old3dsEmitterClipTests=0,g_old3dsEmitterClipRejected=0,g_old3dsEmitterClipParticles=0,g_old3dsEmitterBoundsParticles=0;bool g_old3dsPreparedParticleMotion=true;unsigned g_old3dsParticleMotionPrepared=0;bool g_old3dsLazyParticleMotion=true;unsigned g_old3dsParticleMotionAligned=0,g_old3dsParticleMotionAvoided=0,g_old3dsParticleMotionFallback=0;
bool picaParticleMotionShaderSafe(void*){return shaderSafe;}
pddiPrimStream*picaBeginQuadStream(void*,unsigned){return quadAvailable?&streamStorage:nullptr;}
bool picaPrepareParticleClip(picaClipBounds::Transform*t){float identity[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},projection[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,1,0}};*t=picaClipBounds::Prepare(identity,projection);return clipAvailable;}
bool picaEmitQuad(pddiPrimStream*,const pddiVector*p,const pddiVector2*uv,unsigned colour,picaQuad::Topology){Quad q={};q.colour=colour;for(unsigned i=0;i<4;++i){q.values[i*5]=p[i].x;q.values[i*5+1]=p[i].y;q.values[i*5+2]=p[i].z;q.values[i*5+3]=uv[i].x;q.values[i*5+4]=uv[i].y;}output.push_back(q);return true;}
'''
tests=r'''
int main(){
 unsigned rng=231171;auto random=[&](){rng=rng*1664525u+1013904223u;return rng;};unsigned total=0,avoided=0;
 for(unsigned scenario=0;scenario<12800;++scenario){
  g_old3dsLazyParticleMotion=scenario%31!=0;g_old3dsPreparedParticleMotion=(scenario%6400)>=3200;g_old3dsEmitterClip=scenario>=6400;
  tPointCamera camera;ForeignCamera foreign;camera.matrix.m[3][2]=foreign.matrix.m[3][2]=5;
  for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col){
   float value=float(int(random()%201)-100)/100.f;camera.matrix.m[row][col]=foreign.matrix.m[row][col]=value;
  }
  p3d::context->view.camera=scenario%7==0?static_cast<tCamera*>(&foreign):&camera;
  tSpriteEmitterFactory factory;factory.mode=scenario%5!=0;factory.angle=float(int(random()%101)-50)/10;factory.zWrite=scenario&1;factory.zTest=scenario&2;
  Module module;Shader shader;Particles array;tSpriteParticle particles[96];
  for(unsigned i=0;i<96;++i){auto&p=particles[i];p.currPos.x=float(int(random()%4001)-2000)/10;p.currPos.y=float(int(random()%4001)-2000)/10;p.currPos.z=float(int(random()%2001)-1000)/10;if(scenario%3==0)p.currPos.x+=10000.f; p.prevPos=p.currPos;p.prevPos.x+=float(int(random()%21)-10)/10;p.prevPos.y+=float(int(random()%21)-10)/10;p.prevPos.z+=float(int(random()%21)-10)/10;p.spin=float(int(random()%101)-50)/10;p.size=float(random()%200)/100;p.colour=i;array.live.push_back(&p);}
  Reference reference;reference.particles=&array;reference.factory=&factory;reference.shader=&shader;reference.renderModule=scenario%11==0?&module:nullptr;reference.local=scenario&1;
  Candidate candidate;static_cast<Emitter&>(candidate)=reference;
  quadAvailable=scenario%13!=0;clipAvailable=scenario%17!=0;shaderSafe=scenario%19!=0;
  for(unsigned repetition=0;repetition<5;++repetition){
  if(scenario%3==0 && repetition)for(auto&p:particles){const float shift=(repetition&1)?-10000.f:10000.f;p.currPos.x+=shift;p.prevPos.x+=shift;}
  const auto initial=std::vector<tSpriteParticle>(particles,particles+96);
  output.clear();projectionCalls=0;cameraMatrixReads=0;reference.Display();const auto expected=output;const auto original=std::vector<tSpriteParticle>(particles,particles+96);const unsigned referenceCalls=projectionCalls,referenceMatrixReads=cameraMatrixReads;
  for(unsigned i=0;i<96;++i)particles[i]=initial[i];
  output.clear();projectionCalls=0;cameraMatrixReads=0;candidate.Display();
  if(scenario%7==0)assert(cameraMatrixReads==referenceMatrixReads);
  if(output.size()!=expected.size()){
   // A failed quad reservation originally submits offscreen triangles. Whole
   // emitter rejection may omit them too. Prove every original primitive lies
   // outside one native plane from its actual corners, independently of bounds.
   assert(g_old3dsEmitterClip && output.empty() && !quadAvailable);
   auto f24=[](float x,bool truncate){if(x==0)return x;int exponent;double m=std::frexp(double(x),&exponent)*131072;return float(std::ldexp(truncate?std::trunc(m):std::round(m),exponent-17));};
   for(const auto&q:expected)for(bool truncate:{false,true}){
    bool outside=false;
    for(unsigned plane=0;plane<6;++plane){bool all=true;
     for(unsigned vertex=0;vertex<4;++vertex){float w=f24(q.values[vertex*5+2],truncate),v=f24(q.values[vertex*5+plane/2],truncate);all=all && w+((plane&1)?-v:v)<0;}
     outside=outside||all;
    }
    assert(outside);
   }
  }
  for(unsigned i=0;i<output.size();++i){assert(output[i].colour==expected[i].colour);assert(!memcmp(output[i].values,expected[i].values,sizeof(output[i].values)));const unsigned id=output[i].colour;assert(!memcmp(&particles[id].spin,&original[id].spin,4));}
  for(unsigned i=0;i<96;++i){assert(!memcmp(&particles[i].currPos,&initial[i].currPos,sizeof(rmt::Vector)));assert(!memcmp(&particles[i].prevPos,&initial[i].prevPos,sizeof(rmt::Vector)));assert(particles[i].size==initial[i].size);}
  assert(p3d::stack->history.empty()&&p3d::pddi->write&&p3d::pddi->compare==PDDI_COMPARE_LESS);
  assert(projectionCalls<=referenceCalls);avoided+=referenceCalls-projectionCalls;total+=96;
  }
 }
 assert(g_old3dsParticleMotionPrepared>1000 && g_old3dsEmitterClipRejected>0 && g_old3dsEmitterClipParticles>0);
 assert(avoided>10000 && g_old3dsParticleMotionAvoided>10000 && g_old3dsParticleMotionFallback>0);
 p3dParticleMotion::View view;float matrix[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 assert(view.Prepare(matrix,1,1));float a[3]={0,0,1},b[3]={0,0,0};assert(!view.FiniteAngle(a,b));b[2]=1;assert(view.FiniteAngle(a,b));a[0]=NAN;assert(!view.FiniteAngle(a,b));matrix[0][0]=INFINITY;assert(!view.Prepare(matrix,1,1));
 printf("PASS: emitter rejects=%u skipped_particles=%u; %u particles; exact visible quad bytes/order/spins, unchanged simulation, modules/custom cameras/shaders/stream fallbacks; %u avoided context projection calls.\n",g_old3dsEmitterClipRejected,g_old3dsEmitterClipParticles,total,avoided);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-particle-motion-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+display(reference,'Reference')+display(candidate,'Candidate')+tests)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-DSRR2_OLD_3DS','-DSRR2_OLD3DS_PARTICLE_CLIP','-DSRR2_OLD3DS_QUAD_STREAM','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

#!/usr/bin/env python3
"""Compare production sprite updates and array dispatch, including custom callbacks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
def method(source,signature):
 start=source.index(signature);body=source.index('{',start);end=body+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
s=(root/'libs/pure3d/p3d/effects/particletype.hpp').read_text()
base=method(s,'    virtual void Update(')
sprite=method(s[s.index('class tSpriteParticle :'):],'    virtual void Update(')
clamp=method(s,'    tColour ColourClamp(')
array=method((root/'libs/pure3d/p3d/effects/particlearray.cpp').read_text(),'void tParticleArray::UpdateParticles(')
code=r'''
#include <cassert>
#include <cfenv>
#include <cstring>
#include <cstdio>
#include <random>
#include <typeinfo>
namespace rmt {struct Vector{float x=0,y=0,z=0;};inline int FtoL(float f){return int(f);}inline float LtoF(int i){return float(i);}}
struct tColour{unsigned c=0; tColour()=default;tColour(int r,int g,int b,int a):c(unsigned(r)|(unsigned(g)<<8)|(unsigned(b)<<16)|(unsigned(a)<<24)){}int Red()const{return c&255;}int Green()const{return(c>>8)&255;}int Blue()const{return(c>>16)&255;}int Alpha()const{return c>>24;}void SetAlpha(unsigned a){c=(c&0xffffff)|(a<<24);}};
namespace p3dParticleSystemConstants{namespace TextureAnimMode{enum{NONE,RANDOM,SPECIFIED};}namespace ColourAnimMode{enum{NONE,BOTH,COLOUR,ALPHA};}}
const unsigned DEFAULT_LOOK_UP_SIZE=256;
struct BaseLookup{float speed,weight;};struct SpriteLookup{int texFrame; tColour colour;float size,spin;};
struct tSpriteEmitterFactory{unsigned texture=0,colour=0;int frames=1,rate=1;unsigned GetTextureAnimationMode(){return texture;}unsigned GetColourAnimationMode(){return colour;}int GetNumTextureFrames(){return frames;}int GetTextureFrameRate(){return rate;}};
struct tBaseEmitter{BaseLookup* baseLookUp;float currGravity,currDrag;tSpriteEmitterFactory* factory;tSpriteEmitterFactory* GetFactory(){return factory;}};
struct tSpriteEmitter:tBaseEmitter{SpriteLookup* spriteLookUp;};
struct tBaseParticle{int lifeIndex=0;float life=0,lifeInit=1,weight=1,weightInit=1; rmt::Vector velocity,velocityInit;float accGravity=0;rmt::Vector currPos,prevPos;virtual ~tBaseParticle()=default;
'''+base+r'''
};
struct tSpriteParticle:tBaseParticle{int texFrame=0,texFrameInit=0;char colourVar=0,alphaVar=0;tColour colour;float size=1,sizeInit=1,spin=0,spinInit=0;
'''+clamp+sprite+r'''
};
struct tParticleArray{int numLiveParticles=0;tBaseParticle** liveParticles;void UpdateParticles(float,float,tBaseEmitter*);};
namespace Reference{
'''+array.replace('tParticleArray::UpdateParticles','UpdateParticles').replace('void UpdateParticles(float','void UpdateParticles(tParticleArray& a, float').replace('numLiveParticles','a.numLiveParticles').replace('liveParticles','a.liveParticles')+r'''
}
#define SRR2_OLD3DS_SPRITE_UPDATE
'''+array+r'''
struct Custom:tSpriteParticle{unsigned calls=0;bool mutate=false;tParticleArray* array=nullptr;tBaseParticle* replacement=nullptr;
 void Update(float a,float b,tBaseEmitter* e)override{++calls;e->currDrag+=.03125f;tSpriteParticle::Update(a,b,e);spin+=.125f;if(mutate){array->liveParticles[2]=replacement;array->numLiveParticles=3;}}};
void equal(const tSpriteParticle& a,const tSpriteParticle& b){
 assert(a.lifeIndex==b.lifeIndex && a.texFrame==b.texFrame && a.texFrameInit==b.texFrameInit && a.colour.c==b.colour.c);
 assert(!memcmp(&a.life,&b.life,4) && !memcmp(&a.weight,&b.weight,4) && !memcmp(&a.accGravity,&b.accGravity,4));
 assert(!memcmp(&a.velocity,&b.velocity,sizeof(a.velocity)) && !memcmp(&a.currPos,&b.currPos,sizeof(a.currPos)) && !memcmp(&a.prevPos,&b.prevPos,sizeof(a.prevPos)));
 assert(!memcmp(&a.size,&b.size,4) && !memcmp(&a.spin,&b.spin,4));
}
int main(){std::mt19937 random(9501);unsigned comparisons=0;
 for(int rounding:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(!fesetround(rounding));
 for(unsigned trial=0;trial<10000;++trial){
  BaseLookup base[256];SpriteLookup lookup[256];
  for(unsigned i=0;i<256;++i){base[i]={float(random()%32)*.0625f,float(random()%32)*.0625f};lookup[i]={int(random()%7),tColour(random()%256,random()%256,random()%256,random()%256),float(random()%16)*.125f,float(random()%32)*.0625f};}
  tSpriteEmitterFactory factory;factory.texture=trial%3;factory.colour=trial%4;factory.frames=trial%7+1;factory.rate=trial%5+1;
  tSpriteEmitter a,b; a.baseLookUp=base;a.spriteLookUp=lookup;a.factory=&factory;a.currGravity=-.01f;a.currDrag=.9375f;b=a;
  tSpriteParticle x[4],y[4];Custom ca,cb; tBaseParticle* pa[5]={x,&ca,x+1,x+2,x+3};tBaseParticle* pb[5]={y,&cb,y+1,y+2,y+3};
  tParticleArray ra{5,pa},rb{5,pb};ca.array=&ra;cb.array=&rb;ca.replacement=x+3;cb.replacement=y+3;ca.mutate=cb.mutate=(trial%3==0);
  for(unsigned i=0;i<4;++i){x[i].lifeInit=trial%5==0?-1.0f:float(1+trial%20);x[i].life=float(trial%15)*.5f;x[i].lifeIndex=trial%256;x[i].spinInit=.25f;x[i].sizeInit=2.5f;x[i].velocityInit={1.25f,-3.5f,.125f};x[i].colour=tColour(150,125,100,75);y[i]=x[i];}
  for(unsigned step=0;step<3;++step){float dt=trial%7==0?0.0f:.016f;Reference::UpdateParticles(ra,dt,.125f,&a);rb.UpdateParticles(dt,.125f,&b);
   assert(ra.numLiveParticles==rb.numLiveParticles && a.currDrag==b.currDrag && ca.calls==cb.calls);equal(ca,cb);for(unsigned i=0;i<4;++i){equal(x[i],y[i]);++comparisons;}}
 }}printf("PASS: %u production particle state comparisons; four rounding modes, finite/infinite/expired life, animation modes, zero dt, custom callbacks and live-array mutations.\n",comparisons);}
'''
with tempfile.TemporaryDirectory(prefix='srr-sprite-update-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-Wno-deprecated-copy-with-user-provided-dtor','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

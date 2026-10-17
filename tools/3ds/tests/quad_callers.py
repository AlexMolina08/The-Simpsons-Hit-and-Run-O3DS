#!/usr/bin/env python3
"""Compare production billboard/sprite emission against their original fallback."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
b=(root/'libs/pure3d/p3d/billboardobject.cpp').read_text()
p=(root/'libs/pure3d/p3d/effects/particleemitter.cpp').read_text()
blocks=[]
start=0
for i in range(3):
 a=b.index('#ifdef SRR2_OLD3DS_QUAD_STREAM\n    if(picaEmitQuad',start)
 e=b.index('\n}\n',a)
 blocks.append(b[a:e]);start=e
particle=p[p.index('#ifdef SRR2_OLD3DS_QUAD_STREAM\n            const pddiVector positions'):p.index('\n        }\n        p3d::pddi->EndPrims',p.index('const pddiVector positions'))]
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <pddi/pica/quadstream.hpp>
namespace p3dBillboardConstants {namespace BillboardMode {enum{NO_AXIS=0};}}
struct Record {float x,y,z,u,v;unsigned colour;};
struct pddiPrimStream {
 std::vector<Record> records;unsigned c=0;float u=0,v=0;
 void Colour(pddiColour x){c=x.c;}void UV(float x,float y){u=x;v=y;}
 void Coord(float x,float y,float z){records.push_back({x,y,z,u,v,c});}
};
bool enabled=true;
extern "C" bool picaEmitQuad(pddiPrimStream* s,const pddiVector* p,const pddiVector2* uv,pddiColour c,picaQuad::Topology t){
 if(!enabled)return false;unsigned short indices[6];assert(picaQuad::Indices(indices,0,t));
 for(unsigned i:indices){s->Colour(c);s->UV(uv[i].x,uv[i].y);s->Coord(p[i].x,p[i].y,p[i].z);}return true;
}
'''
for i,body in enumerate(blocks):
 code+=f'void billboard{i}(pddiPrimStream* stream,pddiVector* v,pddiVector2* t,pddiColour displayColour,bool flip,int billboardMode){{\n'+body+'\n}\n'
sparkles=(root/'code/worldsim/coins/sparkle.cpp').read_text()
if 'picaEmitColourQuad' in sparkles:
 code+=r'''
extern "C" bool picaEmitColourQuad(pddiPrimStream* s,const pddiVector* p,const pddiVector2* uv,const pddiColour* c,picaQuad::Topology t){
 if(!enabled)return false;unsigned short indices[6];assert(picaQuad::Indices(indices,0,t));
 for(unsigned i:indices){s->Colour(c[i]);s->UV(uv[i].x,uv[i].y);s->Coord(p[i].x,p[i].y,p[i].z);}return true;
}
'''
 for name,marker,args in [('worldSparkle','            const pddiVector2 coordinates[4]', 'pddiVector* corners,pddiColour* colour'),('hudSparkle','            const pddiVector positions[4]', 'pddiVector pos,float scale,pddiColour colour')]:
  a=sparkles.index('#ifdef SRR2_OLD3DS_QUAD_STREAM\n'+marker);e=sparkles.index('\n        }\n        p3d::pddi->EndPrims',a)
  code+=f'void {name}(pddiPrimStream* sprite,{args}){{for(unsigned once=0;once<1;++once){{\n'+sparkles[a:e]+'\n}}\n'
 code+='#define SPARKLE_CALLERS\n'
code+='void particle(pddiPrimStream* stream,pddiVector currPos,float x,float y,pddiColour colour,unsigned frame,float* texFrameCoords){struct Particle {pddiColour colour;unsigned texFrame;} storage{colour,frame};auto*current=&storage;for(unsigned once=0;once<1;++once){\n'+particle+'\n}}\n'
code+=r'''
int main(){
 unsigned rng=817;auto value=[&](){rng=rng*1664525u+1013904223u;return float(int(rng%200000)-100000)*0.03125f;};
 for(unsigned trial=0;trial<20000;++trial){
  pddiVector v[4];pddiVector2 uv[4];for(unsigned i=0;i<4;++i){v[i]=pddiVector(value(),value(),value());uv[i]=pddiVector2(value(),value());}
  pddiColour colour(rng);bool flip=trial&1;int mode=(trial>>1)&1;
  for(auto f:{billboard0,billboard1,billboard2}){pddiPrimStream a,b;enabled=false;f(&a,v,uv,colour,flip,mode);enabled=true;f(&b,v,uv,colour,flip,mode);assert(a.records.size()==6&&b.records.size()==6);assert(!memcmp(a.records.data(),b.records.data(),6*sizeof(Record)));}
#ifdef SPARKLE_CALLERS
  pddiColour colours[4]={pddiColour(rng),pddiColour(rng^817239),pddiColour(rng^72837),pddiColour(rng^919)};
  {pddiPrimStream a,b;enabled=false;worldSparkle(&a,v,colours);enabled=true;worldSparkle(&b,v,colours);assert(a.records.size()==6&&b.records.size()==6);assert(!memcmp(a.records.data(),b.records.data(),6*sizeof(Record)));}
  {pddiPrimStream a,b;float scale=value();enabled=false;hudSparkle(&a,v[0],scale,colour);enabled=true;hudSparkle(&b,v[0],scale,colour);assert(a.records.size()==6&&b.records.size()==6);assert(!memcmp(a.records.data(),b.records.data(),6*sizeof(Record)));}
#endif
  pddiPrimStream a,b;float x=value(),y=value(),frames[5]={0,.25,.5,.75,1};enabled=false;particle(&a,v[0],x,y,colour,trial%4,frames);enabled=true;particle(&b,v[0],x,y,colour,trial%4,frames);assert(a.records.size()==6&&b.records.size()==6);assert(!memcmp(a.records.data(),b.records.data(),6*sizeof(Record)));
 }
#ifdef SPARKLE_CALLERS
 puts("PASS: 40000 production world/HUD sparkle comparisons, including per-corner colors and provoking vertex order");
#endif
 puts("PASS: 80000 production emission comparisons: three billboard callers, flip/axis modes, sprite positions/UVs/colors and triangle order");
}
'''
stub=r'''
#pragma once
struct pddiVector {float x,y,z;pddiVector()=default;pddiVector(float a,float b,float c):x(a),y(b),z(c){}};
struct pddiVector2 {union{float x,u;};union{float y,v;};pddiVector2()=default;pddiVector2(float a,float b):x(a),y(b){}};
struct pddiColour {unsigned c;explicit pddiColour(unsigned x):c(x){}};struct pddiPrimStream;struct pddiShader;
'''
with tempfile.TemporaryDirectory(prefix='srr-quad-callers-') as td:
 d=Path(td);(d/'pddi').mkdir();(d/'pddi/pddi.hpp').write_text(stub);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-DSRR2_OLD3DS_QUAD_STREAM','-fsanitize=address,undefined','-I'+td,'-I'+str(root/'libs/pure3d'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)

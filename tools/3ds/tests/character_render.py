#!/usr/bin/env python3
"""Compile actual production skin/stream/matrix code; compare reference and opt."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
context = (root/'libs/pure3d/pddi/pica/context.cpp').read_text()
skin = (root/'libs/pure3d/p3d/primgroup.cpp').read_text()
matrix = (root/'libs/radmath/radmath/matrix.cpp').read_text()

def extract(source, signature):
    start=source.index(signature); opening=source.index('{',start); depth=1; end=opening+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]

polyskin=(root/'libs/pure3d/p3d/anim/polyskin.cpp').read_text()
bone_loop=extract(polyskin,'for(int i = 0; i < p->GetNumJoint(); i++)')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#include <algorithm>
#define PDDIASSERT(x) assert(x)
#define SRR2_OLD_3DS
static bool skinTimingActive=false, expectSkinTiming=false;
enum {O3_SKIN,O3_VERTEXLOCK,O3_VERTEXFLUSH,O3_DEFAULT_VERTS,O3_VERTEX_RELOCK,O3_RESCAN_CANDIDATE};
struct Old3dsOperationScope {
 int operation;
 Old3dsOperationScope(int op,unsigned=1):operation(op){if(op==O3_SKIN){assert(!skinTimingActive);skinTimingActive=true;}}
 ~Old3dsOperationScope(){if(operation==O3_SKIN)skinTimingActive=false;}
};
void Old3dsCount(int,unsigned=1){}
const unsigned PICA_VERTEX_STRIDE=36, PICA_VERTEX_OFF_POS=0, PICA_VERTEX_OFF_NRM=12,
 PICA_VERTEX_OFF_UV=24, PICA_VERTEX_OFF_COL=32;
const unsigned PDDI_V_NORMAL=16, PDDI_V_COLOUR=32;
struct Vector { float x,y,z; void Add(const Vector& b){x+=b.x;y+=b.y;z+=b.z;} };
struct Matrix { float m[4][4]; Vector& Row(int i){return *reinterpret_cast<Vector*>(m[i]);}
 const Vector& Row(int i) const {return *reinterpret_cast<const Vector*>(m[i]);}
 void Mult(const Matrix&,const Matrix&); };
struct Joint { Matrix inverseWorldMatrix, worldMatrix; };
struct Skeleton { Joint joints[8]; Joint* GetJoint(int i){return &joints[i];} };
struct Pose : Skeleton {int GetNumJoint(){return 8;}};
struct pddiColour { unsigned char c[4]; unsigned Red(){return c[0];} unsigned Green(){return c[1];}
 unsigned Blue(){return c[2];} unsigned Alpha(){return c[3];} };
struct pddiVector {float x,y,z;}; struct pddiVector2 {float x,y;};
struct pddiPrimBufferStream { virtual ~pddiPrimBufferStream(){};
 virtual void Position(float,float,float)=0; virtual void Normal(float,float,float)=0; };
class picaPrimBufferStream;
struct picaPrimBuffer {
 float *coord=nullptr,*normal=nullptr,*uv=nullptr;
 unsigned char *colour=nullptr,*buffer=nullptr;
 unsigned total=0, allocated=0, vertexType=0, mem=0, indexCount=0;
 bool transient=false, defaultsValid=false, expandDirty=false;
 int elemState=-1;
 picaPrimBufferStream* stream=nullptr;
 pddiPrimBufferStream* Lock(); void Unlock(pddiPrimBufferStream*);
 unsigned char* LockSkin(unsigned); void SetFormat(int,unsigned); void SetTransient(unsigned char*,unsigned short*,unsigned);
 unsigned short* indices=nullptr; int primType=0;
};
void picaFlushForGpu(const void*,unsigned){if(expectSkinTiming)assert(skinTimingActive);}
struct Shader {void* GetShader(){return nullptr;}};
struct Device {void DrawPrimBuffer(void*,picaPrimBuffer*){}};
namespace p3d { Device device; Device* pddi=&device; }
struct tPrimGroupSkinnedPC {
 struct SkinVertex {Vector position,normal; float weights[3]; unsigned indices[4];};
 int mVertexCount; SkinVertex* mVertices; unsigned mVertexFormat; picaPrimBuffer* mBuffer;
 Matrix** matrixPalette; Shader* mShader;
 void Display();
};
'''
stream=extract(context,'class picaPrimBufferStream : public pddiPrimBufferStream')+';\n'
methods='\n'.join(extract(context,sig) for sig in [
'pddiPrimBufferStream* picaPrimBuffer::Lock()',
'unsigned char* picaPrimBuffer::LockSkin(unsigned count)',
'void picaPrimBuffer::Unlock(pddiPrimBufferStream*)',
'void picaPrimBuffer::SetFormat(pddiPrimType type, unsigned vertexFormat)',
'void picaPrimBuffer::SetTransient(unsigned char* verts, unsigned short* idx, unsigned count)'])
methods=methods.replace('pddiPrimType type','int type')
# The affine translation branch comes directly from Character::Display.
character=(root/'code/worldsim/character/character.cpp').read_text()
a=character.index('#ifdef SRR2_OLD3DS_JOINT_TRANSLATE',character.index('void Character::Display(void)'))
b=character.index('#endif',a)+len('#endif')
joint=character[a:b].replace('pose->GetJoint(i)->worldMatrix','input')
joint='Matrix Shift(const Matrix& input, const Matrix& backToTheOrigin) { Matrix tmp;\n'+joint+'\nreturn tmp; }\n'
tests=r'''
int main(int argc,char** argv) {
 assert(argc==2); FILE* output=fopen(argv[1],"wb"); assert(output);
 std::mt19937 random(90421); std::uniform_real_distribution<float> value(-100,100);
 for(int trial=0;trial<2000;++trial) {
  unsigned count=1+random()%100;
  unsigned format=(trial%2?PDDI_V_NORMAL:0)|(trial%3?1:0)|(trial%5?PDDI_V_COLOUR:0);
  std::vector<unsigned char> bytes(count*36,0xa5);
  picaPrimBuffer buffer; buffer.buffer=bytes.data(); buffer.allocated=count; buffer.mem=bytes.size(); buffer.vertexType=format;
  picaPrimBufferStream stream(&buffer); buffer.stream=&stream;
  Matrix matrices[8]; Matrix* palette[8];
  for(int k=0;k<8;++k) {palette[k]=&matrices[k]; for(auto& row:matrices[k].m) for(float& f:row) f=value(random);}
  std::vector<tPrimGroupSkinnedPC::SkinVertex> vertices(count);
  for(auto& v:vertices) {
   v.position={value(random),value(random),value(random)}; v.normal={value(random),value(random),value(random)};
   for(int k=0;k<3;++k) v.weights[k]=value(random)/100;
   if(trial%4==0) {v.weights[0]=1;v.weights[1]=0;v.weights[2]=0;}
   if(trial%4==1) {v.weights[0]=.5f;v.weights[1]=.5f;v.weights[2]=0;}
   if(trial%4==2) {v.weights[0]=.25f;v.weights[1]=.25f;v.weights[2]=.5f;}
   if(trial%97==0) v.weights[1]=NAN;
   if(trial%101==0) v.position.x=INFINITY;
   for(unsigned& index:v.indices) index=random()%8;
  }
  Shader shader; tPrimGroupSkinnedPC group{(int)count,vertices.data(),format,&buffer,palette,&shader};
  // Multiple frame rewrites exercise retained UV/color and absent defaults.
  buffer.Lock();
  for(unsigned i=0;i<count;++i) {
   if(format&1) {float* uv=(float*)(bytes.data()+i*36+24);uv[0]=.25f;uv[1]=.75f;}
   if(format&PDDI_V_COLOUR) memset(bytes.data()+i*36+32,123,4);
  }
  for(int frame=0;frame<4;++frame) {
   expectSkinTiming=true;group.Display();expectSkinTiming=false;assert(!skinTimingActive); assert(buffer.total==count); assert(buffer.coord==(float*)buffer.buffer);
   fwrite(bytes.data(),1,bytes.size(),output);
  }
  // Format changes must restore newly absent attributes.
  buffer.SetFormat(0,0); buffer.Lock();
  for(unsigned i=0;i<count;++i) {
   float* out=(float*)(bytes.data()+i*36);
   assert(out[3]==0 && out[4]==0 && out[5]==1 && out[6]==0 && out[7]==0);
   for(int k=32;k<36;++k) assert(bytes[i*36+k]==255);
  }
 }
 // Both transient reuse and format changes must restore absent defaults.
 {
  alignas(float) unsigned char bytes[72]; memset(bytes,0xab,sizeof(bytes));
  picaPrimBuffer b; picaPrimBufferStream stream(&b); b.stream=&stream;b.transient=true;
  for(int reuse=0;reuse<10;++reuse) {
   memset(bytes,0xab,sizeof(bytes));b.SetTransient(bytes,nullptr,2);b.SetFormat(0,0);b.Lock();
   for(int i=0;i<2;++i) {float* f=(float*)(bytes+36*i);assert(f[3]==0 && f[5]==1 && f[6]==0);assert(bytes[36*i+32]==255);}
  }
 }
 // Bone preparation uses the extracted production loop in both modes.
 for(int trial=0;trial<1000;++trial) {
  Skeleton skeleton; Pose pose; Matrix actual[8], expected[8];
  for(int i=0;i<8;++i) {
   for(int r=0;r<4;++r) for(int c=0;c<4;++c) {
    skeleton.joints[i].inverseWorldMatrix.m[r][c]=c==3?(r==3?1.f:0.f):value(random);
    pose.joints[i].worldMatrix.m[r][c]=c==3?(r==3?1.f:0.f):value(random);
   }
   expected[i].Mult(skeleton.joints[i].inverseWorldMatrix,pose.joints[i].worldMatrix);
  }
  BuildBones(actual,&skeleton,&pose);assert(memcmp(actual,expected,sizeof(actual))==0);
 }
 // Translation equivalence against the actual affine matrix multiply.
 for(int trial=0;trial<100000;++trial) {
  Matrix input{}, translation{};
  for(int r=0;r<4;++r) for(int c=0;c<3;++c) input.m[r][c]=value(random);
  input.m[3][3]=1;
  for(int i=0;i<4;++i) translation.m[i][i]=1;
  translation.m[3][0]=value(random);translation.m[3][1]=value(random);translation.m[3][2]=value(random);
  Matrix expected; expected.Mult(input,translation); Matrix actual=Shift(input,translation);
  for(int r=0;r<4;++r) for(int c=0;c<4;++c) assert(actual.m[r][c]==expected.m[r][c]);
 }
 fclose(output); puts("2,000 skin cases x 4 frames and 100,000 joint transforms passed");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-character-test-') as directory:
 temp=Path(directory); cpp=temp/'test.cpp'
 cpp.write_text(prelude+extract(matrix,'void Matrix::Mult(const Matrix& a, const Matrix& b)')+stream+methods+'void BuildBones(Matrix* boneMatrix, Skeleton* skeleton, Pose* p) {\n'+bone_loop+'\n}\n'+extract(skin,'void tPrimGroupSkinnedPC::Display(void)')+joint+tests)
 outputs=[]
 for mode in ['reference','optimized']:
  binary=temp/mode; data=temp/(mode+'.bin'); outputs.append(data)
  flags=[] if mode=='reference' else ['-DSRR2_OLD3DS_SKIN_DIRECT','-DSRR2_OLD3DS_DEFAULT_CACHE','-DSRR2_OLD3DS_JOINT_TRANSLATE','-DSRR2_OLD3DS_BONE_ONCE']
  subprocess.run(['clang++','-std=c++11','-O2','-ffp-contract=off','-fsanitize=address,undefined',*flags,str(cpp),'-o',str(binary)],check=True)
  subprocess.run([str(binary),str(data)],check=True)
 assert outputs[0].read_bytes()==outputs[1].read_bytes(), 'Vertex buffer mismatch'
 print('Reference/optimized vertex buffers match byte-for-byte, including NaN/Inf arithmetic, UV/color retention, and format changes.')

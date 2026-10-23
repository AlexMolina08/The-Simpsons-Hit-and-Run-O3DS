#!/usr/bin/env python3
"""Replay production segments through the actual sparse uniform manager."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/display.cpp').read_text()
start = source.index('void picaDisplay::StartTopSegment(void)')
end = source.index('bool picaDisplay::BeginBottomTarget(void)', start)
methods = source[start:end]


def clean(path):
    return '\n'.join(line for line in path.read_text().splitlines()
                     if not line.startswith('#include')).replace('_Static_assert', 'static_assert')


fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using u32=uint32_t;using u16=uint16_t;
enum GPU_SHADER_TYPE{GPU_VERTEX_SHADER,GPU_GEOMETRY_SHADER};
const int C3D_FVUNIF_COUNT=96,C3D_IVUNIF_COUNT=4;
const unsigned GPUREG_VSH_BOOLUNIFORM=0x2b0,GPUREG_GSH_BOOLUNIFORM=0x280;
const unsigned GPUREG_VSH_FLOATUNIFORM_CONFIG=0x2c0,GPUREG_VSH_FLOATUNIFORM_DATA=0x2c1,GPUREG_VSH_INTUNIFORM_I0=0x2b1;
const unsigned GX_CMDLIST_FLUSH=2;
#define BIT(n) (1u<<(n))
#define SRR2_OLD3DS_NATIVE_UNIFORMS
struct C3D_FVec{u32 data[4];};using C3D_IVec=u32;
struct float24Uniform_s{u32 id,data[3];};struct DVLE{GPU_SHADER_TYPE type;};
struct shaderInstance_s{DVLE* dvle;u16 boolUniforms,boolUniformMask;u32 intUniforms[4];float24Uniform_s* float24Uniforms;unsigned intUniformMask,numFloat24Uniforms;};
std::vector<u32> buffers[8];unsigned current=0,recordCursor=0;
struct Submission {u32* data;unsigned words;};std::vector<Submission> queue;
void GPUCMD_AddWrite(unsigned reg,u32 value){
 if(reg==GPUREG_VSH_FLOATUNIFORM_CONFIG){recordCursor=value&127;return;}
 assert(reg==GPUREG_VSH_BOOLUNIFORM || (reg>=GPUREG_VSH_INTUNIFORM_I0&&reg<GPUREG_VSH_INTUNIFORM_I0+4));
}
void GPUCMD_AddWrites(unsigned reg,const u32* values,unsigned count){
 assert(reg==GPUREG_VSH_FLOATUNIFORM_DATA && count%4==0);
 for(unsigned i=0;i<count;i+=4){buffers[current].push_back(recordCursor++);buffers[current].push_back(values[i]);}
}
void GPUCMD_AddIncrementalWrites(unsigned,const u32*,unsigned){assert(false);}
void GX_ProcessCommandList(u32* data,unsigned bytes,unsigned){queue.push_back({data,bytes/4});}
bool C3Di_SplitFrame(u32** data,u32* words){
 if(buffers[current].empty())return false;
 *data=buffers[current].data();*words=unsigned(buffers[current].size());++current;assert(current<8);return true;
}
void C3D_FrameSplit(unsigned flags){u32* data;u32 words;if(C3Di_SplitFrame(&data,&words))GX_ProcessCommandList(data,words*4,flags);}
void C3D_FrameDrawOn(void*){}
void picaWriteEye(float shift){buffers[current].push_back(95);buffers[current].push_back(shift<0?10:shift>0?20:0);}
struct Context {void FlushStreams(){}};
struct picaDisplay {
 bool stereoThisFrame=true;float eyeShift=.02f;Context storage,*context=&storage;void* targetRight=this;
 void StartTopSegment();void FinishTopSegment();
};
'''
uniforms = clean(root / 'libs/pure3d/pddi/pica/nativeuniformsnapshot.h')
uniforms += clean(root / 'libs/pure3d/pddi/pica/nativeuniforms.c')
main = r'''
void C3Di_UpdateContext(){C3D_UpdateUniforms(GPU_VERTEX_SHADER);}
void set(unsigned slot,u32 value){
 for(unsigned c=0;c<4;++c)C3D_FVUnif[0][slot].data[c]=value;
 C3D_FVUnifDirty[0][slot]=true;
}
void draw(unsigned slot,u32 expected){
 set(slot,expected);C3Di_UpdateContext();
 buffers[current].push_back(1000+slot);buffers[current].push_back(expected);
}
int main(){
 for(unsigned slot:{0u,3u,20u,26u,94u}){
  for(auto& buffer:buffers)buffer.clear();queue.clear();current=0;
  picaNativeUniformInvalidate(GPU_VERTEX_SHADER,0,96);set(slot,0);
  picaDisplay display;display.StartTopSegment();
  // The first draw repeats a setup value. The final draw changes that value.
  draw(slot,0);draw(slot,1);display.FinishTopSegment();
  assert(queue.size()==4 && queue[1].data==queue[3].data);
  u32 gpu[96]={};unsigned draws=0,sceneEyeWrites=0;
  for(unsigned q=0;q<queue.size();++q){
   const auto& submission=queue[q];
   for(unsigned i=0;i<submission.words;i+=2){
    const unsigned reg=submission.data[i],value=submission.data[i+1];
    if(reg>=1000){
     if(gpu[reg-1000]!=value)return 77;
     assert(gpu[95]==(draws<2?10u:20u));++draws;
    }else{
     if(reg==95 && (q==1 || q==3))++sceneEyeWrites;
     gpu[reg]=value;
    }
   }
  }
  assert(draws==4 && !sceneEyeWrites);
 }
 picaDisplay mono;mono.stereoThisFrame=false;
 for(unsigned i=0;i<95;++i)uniformCache.valid[0][i]=true;
 const unsigned submissions=unsigned(queue.size());
 mono.StartTopSegment();mono.FinishTopSegment();
 assert(queue.size()==submissions);
 for(unsigned i=0;i<95;++i)assert(uniformCache.valid[0][i]);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-stereo-uniforms-') as directory:
    path = Path(directory)
    for name, body in [('fixed', methods), ('stale', methods.replace(
            'picaNativeUniformInvalidate(GPU_VERTEX_SHADER, 0, 95);', ''))]:
        cpp, binary = path / (name + '.cpp'), path / name
        cpp.write_text(fixture + uniforms + main[:main.index('int main()')]
                       + body + main[main.index('int main()'):])
        subprocess.run(['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(cpp), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)])
        assert result.returncode == (0 if name == 'fixed' else 77), (name, result.returncode)
print('PASS: first-use projection, material, scope and bone uniforms survive both eye replays; '
      'stale-cache control fails; register 95 and mono behavior stay unchanged.')

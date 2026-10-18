#!/usr/bin/env python3
"""Exercise the production ordered recorder/compiler with mocked GPU SDK calls."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
def clean(path):return '\n'.join(line for line in path.read_text().splitlines() if not line.startswith('#include'))+'\n'
with tempfile.TemporaryDirectory(prefix='srr-render-worker-') as directory:
 p=Path(directory)
 subprocess.run([sys.executable,str(root/'tools/3ds/tests/native_context.py'),'--emit-source',str(p/'base.cpp')],check=True)
 base=(p/'base.cpp').read_text();base=base[:base.index('int main(){')]
 base=base.replace('u32 storage[8192]','u32 storage[65536]').replace('gpuCmdBufSize=8192','gpuCmdBufSize=65536')
 base=base.replace('storage+gpuCmdBufOffset','gpuCmdBuf+gpuCmdBufOffset').replace('storage[gpuCmdBufOffset','gpuCmdBuf[gpuCmdBufOffset')
 fixture=r'''
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <climits>
using u8=uint8_t;using s32=int32_t;using Result=int32_t;
constexpr unsigned SYSCLOCK_ARM11=1000000000u;constexpr u64 U64_MAX=~u64(0);
#define R_FAILED(value) ((value)<0)
constexpr unsigned RESET_ONESHOT=0,CUR_THREAD_HANDLE=0;
struct LightEvent {bool ready;};
using Thread=std::thread*;
std::mutex eventMutex;std::condition_variable eventCondition;
void LightEvent_Init(LightEvent* e,unsigned){e->ready=false;}
void LightEvent_Signal(LightEvent* e){std::lock_guard<std::mutex> lock(eventMutex);e->ready=true;eventCondition.notify_all();}
void LightEvent_Wait(LightEvent* e){std::unique_lock<std::mutex> lock(eventMutex);eventCondition.wait(lock,[&]{return e->ready;});e->ready=false;}
const auto mainThread=std::this_thread::get_id();
u64 svcGetSystemTick(){return std::chrono::steady_clock::now().time_since_epoch().count();}
unsigned svcGetProcessorID(){return std::this_thread::get_id()==mainThread?0:1;}
Result APT_GetAppCpuTimeLimit(u32* out){*out=0;return 0;}
Result APT_SetAppCpuTimeLimit(unsigned){return 0;}
Result svcGetThreadPriority(s32* out,unsigned){*out=0x30;return 0;}
Thread threadCreate(void(*fn)(void*),void* arg,size_t,s32,int,bool){return new std::thread(fn,arg);}
void threadJoin(Thread thread,u64){thread->join();}
void threadFree(Thread thread){delete thread;}
#define SRR2_OLD3DS_RENDER_WORKER
namespace Worker {
using namespace Candidate;
'''
 fixture+=clean(root/'libs/pure3d/pddi/pica/renderworker.h')
 fixture+=r'''
u32 alternate[65536];
void __real_GPUCMD_Add(u32 header,const u32* values,u32 count){Candidate::GPUCMD_Add(header,values,count);}
void __real_GPUCMD_AddRawCommands(const u32* words,u32 count){Candidate::GPUCMD_AddRawCommands(words,count);}
void __real_GPUCMD_Split(u32** address,u32* size){
 *address=Candidate::gpuCmdBuf;*size=Candidate::gpuCmdBufOffset;
 Candidate::words.insert(Candidate::words.end(),*address,*address+*size);
 Candidate::gpuCmdBuf=Candidate::gpuCmdBuf==Candidate::storage?alternate:Candidate::storage;Candidate::gpuCmdBufOffset=0;
}
void __real_C3D_FrameSplit(u8 flags){Candidate::GPUCMD_AddWrite(0x318,flags);}
void __real_C3D_FrameEnd(u8 flags){Candidate::GPUCMD_AddWrite(0x319,flags);}
'''
 fixture+=clean(root/'libs/pure3d/pddi/pica/renderworker.c')+'}\n'
 driver=r'''
int main(){
 shaderProgram_s program={};Candidate::picaNativeRegisterPrograms(&program,nullptr);Candidate::g_picaNativeContextEnabled=true;
 Reference::uniformFast=Candidate::uniformFast=true;
 unsigned total=0;u32 referenceAlternate[65536];bool seeded=false;
 for(unsigned frame=0;frame<1200;++frame) {
  Worker::g_picaRenderWorkerMode=frame<100?2:frame<200?1:3;
  Reference::gpuCmdBuf=Reference::storage;Candidate::gpuCmdBuf=Candidate::storage;
  Reference::gpuCmdBufOffset=Candidate::gpuCmdBufOffset=0;Reference::words.clear();Candidate::words.clear();
  Worker::picaRenderWorkerBeginFrame();
  if(frame==200 && Worker::state){Worker::state->published=Worker::state->compiled=Worker::state->committed=UINT_MAX-7u;seeded=true;}
  for(unsigned draw=0;draw<32;++draw) {
   C3D_Context context={};context.flags=C3DiF_Active|C3DiF_DrawUsed|C3DiF_Viewport|C3DiF_Scissor|C3DiF_Effect|C3DiF_TexEnvAll|C3DiF_TexEnvBuf;
   context.program=&program;context.effect.zBuffer=true;context.effect.depthTest=randomValue();
   for(auto& value:context.viewport)value=randomValue();for(auto& value:context.scissor)value=randomValue();
   for(auto& e:context.texEnv)for(auto& value:e.values)value=randomValue();
   // Unsupported context states must fence all earlier compiler records.
   if(!(frame&1) && draw%13==0)context.flags|=C3DiF_FrameBuf;
   Reference::context=Candidate::context=context;
   for(unsigned type=0;type<2;++type) {
    for(unsigned i=0;i<8;++i) {
     unsigned id=(draw*3+i)%96;u32 values[4];for(auto& v:values)v=randomValue();
     memcpy(&Reference::C3D_FVUnif[type][id],values,sizeof(values));memcpy(&Candidate::C3D_FVUnif[type][id],values,sizeof(values));
     Reference::C3D_FVUnifDirty[type][id]=Candidate::C3D_FVUnifDirty[type][id]=true;
     Reference::g_picaNativeUniformMask[type][id>>5]|=BIT(id&31);Candidate::g_picaNativeUniformMask[type][id>>5]|=BIT(id&31);
    }
    if(draw%7==0){u16 value=randomValue();Reference::C3D_BoolUnifs[type]=Candidate::C3D_BoolUnifs[type]=value;Reference::C3D_BoolUnifsDirty[type]=Candidate::C3D_BoolUnifsDirty[type]=true;}
   }
   Reference::C3Di_UpdateContext();
   if(!Worker::picaRenderWorkerContext())Candidate::picaNativeUpdateContext();
   u32 values[300];unsigned count=draw%5==0?300:3;for(unsigned i=0;i<count;++i)values[i]=randomValue();
   const u32 header=GPUCMD_HEADER(draw&1,15,0x100);
   Reference::GPUCMD_Add(header,values,count);Worker::__wrap_GPUCMD_Add(header,values,count);
   u32 raw[]={0x12345678,GPUCMD_HEADER(0,15,0x201)};
   Reference::GPUCMD_AddRawCommands(raw,2);Worker::__wrap_GPUCMD_AddRawCommands(raw,2);
   ++total;
   if(!(frame&1) && draw==11){Reference::GPUCMD_AddWrite(0x318,5);Worker::__wrap_C3D_FrameSplit(5);}
   if(!(frame&1) && draw==17){
    Worker::picaRenderWorkerFence();
    for(unsigned type=0;type<2;++type){Reference::C3Di_DirtyUniforms(type);Candidate::C3Di_DirtyUniforms(type);}
   }
   if(!(frame&1) && draw==23){
    Reference::words.insert(Reference::words.end(),Reference::gpuCmdBuf,Reference::gpuCmdBuf+Reference::gpuCmdBufOffset);
    Reference::gpuCmdBuf=Reference::gpuCmdBuf==Reference::storage?referenceAlternate:Reference::storage;Reference::gpuCmdBufOffset=0;
    u32* address;u32 size;Worker::__wrap_GPUCMD_Split(&address,&size);
   }
  }
  Worker::picaRenderWorkerEndFrame();Reference::GPUCMD_AddWrite(0x319,7);Worker::__wrap_C3D_FrameEnd(7);
  Reference::words.insert(Reference::words.end(),Reference::gpuCmdBuf,Reference::gpuCmdBuf+Reference::gpuCmdBufOffset);
  Candidate::words.insert(Candidate::words.end(),Candidate::gpuCmdBuf,Candidate::gpuCmdBuf+Candidate::gpuCmdBufOffset);
  assert(Reference::words==Candidate::words);
  assert(!memcmp(Reference::C3D_FVUnifDirty,Candidate::C3D_FVUnifDirty,sizeof(Reference::C3D_FVUnifDirty)));
  assert(!memcmp(&Reference::context,&Candidate::context,sizeof(C3D_Context)));
 }
 Worker::picaRenderWorkerStats stats;Worker::picaRenderWorkerGetStats(&stats);
 printf("Queue diagnostics: records=%u raw_calls=%u raw_records=%u notifications=%u capacity_fences=%u worker=%u main=%u\n",stats.records,stats.raw,stats.rawRecords,stats.notifications,stats.capacityFences,stats.workerContexts,stats.mainContexts);fflush(stdout);
 assert(stats.raw>stats.records);
 assert(seeded && stats.workerContexts>100 && stats.mainContexts>100 && stats.comparisons>1000 && !stats.mismatches && stats.capacityFences>100 && stats.peakSlots<=16);
 Worker::picaRenderWorkerShutdown();
 printf("PASS: %u ordered draws across 1200 frames, %u worker contexts, %u main contexts, %u comparisons, %u capacity fences. Raw commands, unsupported state, upload restore, split/end fences, buffer changes and counter wrap match reference. Mock SDK; not physical FPS.\n",total,stats.workerContexts,stats.mainContexts,stats.comparisons,stats.capacityFences);
}
'''
 (p/'test.cpp').write_text(base+fixture+driver)
 flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-sizeof-array-div','-pthread','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined')]
 subprocess.run([*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

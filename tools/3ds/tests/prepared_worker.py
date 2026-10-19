#!/usr/bin/env python3
"""Exercise production coarse-batch ownership with SDK spies and thread sanitizers."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[3]
def clean(path):return '\n'.join(x for x in path.read_text().splitlines() if not x.startswith('#include'))+'\n'
base=subprocess.check_output([sys.executable,str(root/'tools/3ds/tests/prepared_draw.py'),'--emit-source'],text=True)
base=base[:base.index('int main(){')]
base=base.replace('thread_local GPU* active','GPU* active')
base=base.replace('struct pddiBaseContext{', 'struct pddiBaseContext{void SetZRange(float,float){}')
base=base.replace('void ApplyDrawMaterial(pddiBaseShader*);', 'void SetZRange(float,float);void ApplyDrawMaterial(pddiBaseShader*);')
# Commands written by the SDK spy now exercise real private/live buffer handoff.
base=base.replace('void GPUCMD_GetBuffer(u32** buffer,u32* size,u32* offset){static u32 storage[65536];*buffer=storage;*size=active->size;*offset=active->offset;}', 'void GPUCMD_GetBuffer(u32** buffer,u32* size,u32* offset){*buffer=gpuCmdBuf;*size=gpuCmdBufSize;*offset=gpuCmdBufOffset;}')
pos=base.index('constexpr unsigned GPUREG_ATTRIBBUFFER0_OFFSET')
base=base[:pos]+r'''
u32 commandStorage[1048576];u32* gpuCmdBuf=commandStorage;u32 gpuCmdBufSize=1048576,gpuCmdBufOffset=0;
void Emit(unsigned count,unsigned value){assert(gpuCmdBufOffset+count<=gpuCmdBufSize);for(unsigned i=0;i<count;++i)gpuCmdBuf[gpuCmdBufOffset++]=value+i;active->offset+=count;}
'''+base[pos:]
base=base.replace('active->offset+=64','Emit(64,23)').replace('active->offset+=2','Emit(2,value)').replace('active->offset+=16','Emit(16,unsigned(count))')
# Preserve the fixture clock definition, now thread-safe.
base=base.replace('u64 svcGetSystemTick(){static u64 ticks=0;return ++ticks;}', 'u64 svcGetSystemTick(){return std::chrono::steady_clock::now().time_since_epoch().count();}')
shim=r'''
#include <mutex>
#include <condition_variable>
#include <chrono>
using s32=int32_t;using Result=int32_t;
constexpr u64 U64_MAX=~u64(0);
#define R_FAILED(value) ((value)<0)
constexpr unsigned RESET_ONESHOT=0,CUR_THREAD_HANDLE=0;
void* picaNativeAllocateCheckpoint();void picaNativeFreeCheckpoint(void*);void picaNativeSaveCheckpoint(void*);void picaNativeRestoreCheckpoint(const void*);bool picaNativeCompareCheckpoint(const void*,const void*);
struct LightEvent {bool ready=false;};using Thread=std::thread*;
std::mutex eventMutex;std::condition_variable eventCondition;
void LightEvent_Init(LightEvent* e,unsigned){e->ready=false;}
void LightEvent_Signal(LightEvent* e){std::lock_guard<std::mutex> lock(eventMutex);e->ready=true;eventCondition.notify_all();}
void LightEvent_Wait(LightEvent* e){std::unique_lock<std::mutex> lock(eventMutex);eventCondition.wait(lock,[&]{return e->ready;});e->ready=false;}
unsigned svcGetProcessorID(){return 1;}
Result APT_GetAppCpuTimeLimit(u32* out){*out=80;return 0;}
Result APT_SetAppCpuTimeLimit(unsigned){return 0;}
Result svcGetThreadPriority(s32* out,unsigned){*out=0x30;return 0;}
Thread threadCreate(void(*function)(void*),void* argument,size_t,s32,int,bool){return new std::thread(function,argument);}
void threadJoin(Thread thread,u64){thread->join();}void threadFree(Thread thread){delete thread;}
'''
# Types precede source; SDK scalar aliases are introduced by the source itself.
pos=base.index('constexpr unsigned GPUREG_ATTRIBBUFFER0_OFFSET')
base=base[:pos]+shim+base[pos:]
base='#include <chrono>\n'+base
base+=r'''
void* picaNativeAllocateCheckpoint(){return new GPU;}
void picaNativeFreeCheckpoint(void* checkpoint){delete static_cast<GPU*>(checkpoint);}
void picaNativeSaveCheckpoint(void* checkpoint){*static_cast<GPU*>(checkpoint)=*active;}
void picaNativeRestoreCheckpoint(const void* checkpoint){*active=*static_cast<const GPU*>(checkpoint);}
bool picaNativeCompareCheckpoint(const void* left,const void* right){
#ifdef FORCE_CHECKPOINT_MISMATCH
 return false;
#endif
 const auto& a=*static_cast<const GPU*>(left);const auto& b=*static_cast<const GPU*>(right);return a.trace==b.trace && a.booleans==b.booleans && !memcmp(a.vectors,b.vectors,sizeof(a.vectors)) && !memcmp(a.integers,b.integers,sizeof(a.integers));}
'''
worker=clean(root/'libs/pure3d/pddi/pica/preparedworker.cpp')
a=worker.index('    unsigned value;');b=worker.index('\n}',a)
worker=worker[:a]+'    return 0; // Host shim; ARM read-only FPSCR check is separately compiled.'+worker[b:]
# Compile the actual context integration, including its asynchronous branches.
pos=base.index('struct picaContext:picaPreparedUniformCache,pddiBaseContext{')
base=base[:pos]+clean(root/'libs/pure3d/pddi/pica/preparedworker.hpp')+worker+base[pos:]
base=base.replace('picaPreparedExecutor* preparedExecutor=nullptr;bool preparedBatchFailed=false,preparedBatchReady=true;', 'picaPreparedExecutor* preparedExecutor=nullptr;picaPreparedWorker* preparedWorker=nullptr;bool preparedWorkerAttempted=false;void PreparedCommandBuffer(u32**,u32*,u32*);bool preparedBatchFailed=false,preparedBatchReady=true;')
base=base.replace('delete preparedExecutor;delete preparedInputPool;', 'delete preparedWorker;delete preparedExecutor;delete preparedInputPool;')
context=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=context.index('void picaContext::PreparedCommandBuffer(');b=context.index('\n}',a)+2
base+='\n'+context[a:b]+'\n'
a=context.index('void picaContext::SetZRange(');b=context.index('\n}',a)+2
base+='\nvoid C3D_DepthMap(bool enabled,float scale,float offset){active->trace.push_back({32,enabled,bits(scale),bits(offset)}); }\n'+context[a:b]+'\n'
source=('#define FORCE_CHECKPOINT_MISMATCH\n' if '--fault-compare' in sys.argv else '')+'#define SRR2_OLD3DS_PREPARED_THREAD\n'+base+r''' 
int main(){
 constexpr unsigned frames=128,draws=96;
 picaPreparedExecutor executor;picaPreparedInputPool pool;
 auto* worker=picaPreparedWorker::Create(&executor,&pool);assert(worker);
 picaContext context{};context.preparedInputPool=new picaPreparedInputPool;
 GPU expected,actual;std::vector<u32> expectedWords;
 for(unsigned frame=0;frame<frames;++frame){
  for(unsigned arm=0;arm<2;++arm){
   GPU& gpu=arm?actual:expected;gpu=GPU{};active=&gpu;g_picaState={};
   gpuCmdBuf=commandStorage;gpuCmdBufSize=1048576;gpuCmdBufOffset=0;
   context.InvalidateUniformCache();context.lightsDirty=true;context.lighting={};context.eyeLight[0]={};context.screen.stereo=false;context.hwProjection={};
   picaPreparedExecutor* producer=&executor;picaPreparedInputPool* inputs=&pool;pool.Reset();
   executor.uniforms={};executor.Begin();if(arm)worker->Begin();
   auto* savedPool=context.preparedInputPool;context.preparedInputPool=inputs;
   for(unsigned draw=0;draw<draws;++draw){
    if(producer->operations>=picaPreparedExecutor::Capacity-4 || inputs->drawCount==picaPreparedInputPool::Capacity){
     if(arm)worker->Publish(producer,inputs);else{producer->Flush();inputs->Reset();}
     context.preparedInputPool=inputs;
    }
    for(unsigned row=0;row<4;++row)context.stack.value.rows[row]={float(draw+row),float(frame),0,row==3?1.f:0.f};
    picaPreparedDrawInputs material;material.hasMaterial=false;material.hasUniforms=true;
    assert(context.CapturePreparedUniformRecord(material.uniforms,reinterpret_cast<void*>(1),1,false,pddiColour{},pddiColour{},pddiColour{},nullptr));
    if(draw%11==0)producer->AddState({PICA_PREPARED_VIEWPORT,0,0,400,240,0,0});
    producer->AddMaterial(material,*inputs);
    picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),draw+3,36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
    producer->AddGeometry(geometry);
    // Producer keeps changing its source after publication. SDK reads owned input.
    context.stack={};context.hwProjection.r[0].x=1;
    if(arm && draw%9==0){worker->Publish(producer,inputs);context.preparedInputPool=inputs;std::this_thread::yield();}
   }
   if(arm)worker->Finish(producer,inputs);else executor.End();
   context.preparedInputPool=savedPool;
   assert(gpuCmdBuf==commandStorage && !executor.valid);
   if(!arm)expectedWords.assign(commandStorage,commandStorage+gpuCmdBufOffset);
   else{
    if(expected.trace!=actual.trace){fprintf(stderr,"frame=%u traces=%zu/%zu\n",frame,expected.trace.size(),actual.trace.size());for(unsigned i=0;i<std::min(expected.trace.size(),actual.trace.size());++i)if(expected.trace[i]!=actual.trace[i]){for(auto x:expected.trace[i])fprintf(stderr,"%zu ",x);fprintf(stderr," vs ");for(auto x:actual.trace[i])fprintf(stderr,"%zu ",x);fprintf(stderr," first=%u\n",i);break;}return 3;}assert(expected.offset==actual.offset);
    assert(!memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));
    assert(expectedWords.size()==gpuCmdBufOffset);assert(std::equal(expectedWords.begin(),expectedWords.end(),commandStorage));
   }
  }
 }
 printf("PASS: %u draws through production two-arena worker match synchronous SDK calls, registers and committed command words. worker_draws=%u main_draws=%u jobs=%u. SDK spies; not physical GPU/FPS proof.\n",frames*draws,worker->workerDraws,worker->mainDraws,worker->jobs);
 delete worker;
 unsigned integratedDraws=0;
 for(unsigned frame=0;frame<64;++frame){
  for(unsigned arm=0;arm<2;++arm){
   GPU& gpu=arm?actual:expected;gpu=GPU{};active=&gpu;g_picaState={};
   gpuCmdBuf=commandStorage;gpuCmdBufSize=1048576;gpuCmdBufOffset=0;
   picaContext context{};activeOwner=&context;
   context.lighting={};context.eyeLight[0]={};context.screen.stereo=false;context.hwProjection={};
   context.lightsDirty=true;g_picaPreparedBackendEnabled=arm;g_picaPreparedExecution=arm?(frame%2?6:4):1;
   pddiBaseShader::currentUID=~0u;pddiBaseShader::lastShader=nullptr;
   picaMat materials[3];Texture textures[3];
   for(unsigned i=0;i<3;++i){materials[i].context=&context;materials[i].uid=100+i;
    textures[i].tex.data=reinterpret_cast<void*>(uintptr_t(0x14004000+i*4096));
    materials[i].texEnv[0].texture=&textures[i];materials[i].texEnv[0].ambient.c=0xff445566;
    materials[i].texEnv[0].diffuse.c=0xff778899;materials[i].texEnv[0].emissive.c=0;}
   shaderProgram_s program=1;C3D_AttrInfo attributes=1;
   for(unsigned draw=0;draw<draws;++draw){
    for(unsigned row=0;row<4;++row)context.stack.value.rows[row]={float(draw+row),float(frame),0,row==3?1.f:0.f};
    if(draw%13==0){if(arm)assert(context.QueuePreparedProgram(&program,&attributes,true,VSH_FVEC_u_eye,7));
     else{C3D_BindProgram(&program);C3D_FVUnifSet(GPU_VERTEX_SHADER,VSH_FVEC_u_eye,0,7,0,0);C3D_SetAttrInfo(&attributes);g_picaState.bufInfoSet=false;}
     context.InvalidateUniformCache();}
    if(draw%17==0){picaPreparedSkinPalette palette;palette.count=2;for(auto& bone:palette.values)for(auto& row:bone)for(auto& value:row)value=float(draw);
     if(arm)assert(context.QueuePreparedPalette(palette,20));else assert(picaUploadPreparedSkinPalette(palette,20));}
    if(draw%23==0){if(arm)assert(context.QueuePreparedState({PICA_PREPARED_VIEWPORT,0,0,400,240,0,0}));else C3D_SetViewport(0,0,400,240);}
    if(draw%29==0){context.FlushPreparedExecution(true);textures[draw%3].tex.width=128;}
    if(draw%37==0)context.SetZRange(1,1000);
    if(draw%31==0)pddiBaseShader::ClearCurrentShader();
    if(draw%19==0){materials[draw%3].uid+=10;materials[draw%3].texEnv[0].filterMode=draw%5;materials[draw%3].dirty=materials[draw%3].nativeDirty=true;}
    context.ApplyDrawMaterial(&materials[draw%3]);
    picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),draw+3,36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
    if(arm){assert(context.PreparedGeometryRoom());context.QueuePreparedGeometry(geometry);}
    else{picaBindPreparedGeometry(geometry,g_picaState.bufInfoSet);picaSubmitPreparedGeometry(geometry);}
    if(arm && draw%9==0)std::this_thread::yield();
   }
   context.FlushPreparedExecution(true);pddiBaseShader::ClearCurrentShader();
   assert(gpuCmdBuf==commandStorage);
   if(!arm)expectedWords.assign(commandStorage,commandStorage+gpuCmdBufOffset);
   else{
    if(expected.trace!=actual.trace){fprintf(stderr,"integrated frame=%u traces=%zu/%zu\n",frame,expected.trace.size(),actual.trace.size());for(unsigned i=0;i<std::min(expected.trace.size(),actual.trace.size());++i)if(expected.trace[i]!=actual.trace[i]){fprintf(stderr,"first=%u operations=%zu/%zu\n",i,expected.trace[i][0],actual.trace[i][0]);break;}return 4;}
    assert(!memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));
    assert(expectedWords.size()==gpuCmdBufOffset);assert(std::equal(expectedWords.begin(),expectedWords.end(),commandStorage));
    assert(context.preparedWorker && context.preparedWorker->workerDraws+context.preparedWorker->mainDraws==draws);
    integratedDraws+=draws;
   }
  }
 }
 activeOwner=nullptr;
 printf("PASS: %u draws through actual context worker integration match original SDK calls, registers and committed command words, with program, palette, viewport, UID, sampler mutation and foreign-call fences. SDK spies; not physical proof.\n",integratedDraws);
}
'''
if '--emit-source' in sys.argv:print(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='prepared-worker-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-I',str(root/'libs/pure3d'),'-std=c++17','-O1','-g','-fsanitize=thread' if '--tsan' in sys.argv else '-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 result=subprocess.run([str(p/'test')],capture_output=True,text=True,timeout=60)
 print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
 if '--fault-compare' in sys.argv:
  assert result.returncode==1 and 'PREPARED_EXECUTOR_INVARIANT reason=worker_comparison_mismatch' in result.stdout
  print('PASS: injected consumer mismatch joins the worker and exits from main without hanging or committing mismatched output.')
 else:result.check_returncode()

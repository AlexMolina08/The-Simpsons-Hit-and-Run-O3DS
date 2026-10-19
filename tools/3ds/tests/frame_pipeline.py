#!/usr/bin/env python3
"""Check ordered full-frame ownership and simulation overlap using SDK spies."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
def clean(path):return '\n'.join(line for line in path.read_text().splitlines() if not line.startswith('#include'))+'\n'
source=subprocess.check_output([sys.executable,str(root/'tools/3ds/tests/prepared_worker.py'),'--emit-source'],text=True)
source=source[:source.index('int main(){')]
source+='''
#include <atomic>
constexpr unsigned GX_CMDLIST_FLUSH=1;
unsigned prefixCalls=0,pauseCalls=0;
extern "C" void picaNativeFramePrefix(bool){++prefixCalls;gpuCmdBuf+=gpuCmdBufOffset;gpuCmdBufSize-=gpuCmdBufOffset;gpuCmdBufOffset=0;}
extern "C" void picaNativeFrameRetirePrefix(){++pauseCalls;}
extern "C" void picaNativeFrameAbort(){}
void C3Di_RenderQueueWaitDone(){}

float C3D_GetProcessingTime(){return 1;}float C3D_GetDrawingTime(){return 2;}
std::atomic<bool> finishAllowed{true},finishEntered{false};
void C3D_FrameEnd(unsigned flags){
 finishEntered.store(true,std::memory_order_release);
 while(!finishAllowed.load(std::memory_order_acquire))std::this_thread::yield();
 active->trace.push_back({99,flags});Emit(2,99);
}
'''
worker=clean(root/'libs/pure3d/pddi/pica/framepipeline.cpp')
a=worker.index('    unsigned value;');b=worker.index('\n}',a)
worker=worker[:a]+'    return 0; // ARM read-only FPSCR check compiled separately.'+worker[b:]
source+=('#define TEST_FRAME_COMPARE\n' if '--compare' in sys.argv else '')
source+=('#define TEST_FRAME_PREFIX\n' if '--prefix' in sys.argv else '')
source+=clean(root/'libs/pure3d/pddi/pica/framepipeline.hpp')+worker
source+=r'''
void DriverViewport(const void* bytes){u32 values[4];memcpy(values,bytes,sizeof(values));C3D_SetViewport(values[0],values[1],values[2],values[3]);}
int main(){
 picaPreparedExecutor primary;picaPreparedInputPool pool;
 auto* pipeline=picaFramePipeline::Create(&primary,&pool);assert(pipeline);
 picaContext context{};auto* originalPool=context.preparedInputPool;
 GPU expected,actual;std::vector<u32> expectedWords;
 unsigned totalDraws=0,simulationIterations=0;
 for(unsigned frame=0;frame<96;++frame){
  for(unsigned arm=0;arm<2;++arm){
   GPU& gpu=arm?actual:expected;gpu=GPU{};active=&gpu;g_picaState={};
   gpuCmdBuf=commandStorage;gpuCmdBufSize=1048576;gpuCmdBufOffset=0;
   context.InvalidateUniformCache();context.lightsDirty=true;context.lighting={};context.eyeLight[0]={};context.screen.stereo=false;context.hwProjection={};
   auto* producer=&primary;auto* inputs=&pool;pool.Reset();primary.uniforms={};primary.Begin();
   if(arm)pipeline->Begin(producer,inputs);
   context.preparedInputPool=inputs;
   for(unsigned draw=0;draw<96;++draw){
    if(producer->operations>=picaPreparedExecutor::Capacity-4 || inputs->drawCount==picaPreparedInputPool::Capacity){
     if(arm)assert(pipeline->Advance(producer,inputs));else{producer->Flush();inputs->Reset();}
     context.preparedInputPool=inputs;
    }
    for(unsigned row=0;row<4;++row)context.stack.value.rows[row]={float(draw+row),float(frame),0,row==3?1.f:0.f};
    picaPreparedDrawInputs material;material.hasMaterial=false;material.hasUniforms=true;
    assert(context.CapturePreparedUniformRecord(material.uniforms,reinterpret_cast<void*>(1),1,false,pddiColour{},pddiColour{},pddiColour{},nullptr));
    if(draw%11==0)producer->AddState({PICA_PREPARED_VIEWPORT,0,0,400,240,0,0});
    producer->AddMaterial(material,*inputs);
    if(draw%13==0){u32 viewport[4]={draw,frame,400,240};producer->AddDriver(DriverViewport,viewport,sizeof(viewport));memset(viewport,0xff,sizeof(viewport));}
    picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),draw+3,36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
    producer->AddGeometry(geometry);
    context.stack={};context.hwProjection.r[0].x=1;
   }
   if(!arm){primary.End();C3D_FrameEnd(GX_CMDLIST_FLUSH);expectedWords.assign(commandStorage,commandStorage+gpuCmdBufOffset);}
   else{
    finishAllowed.store(false,std::memory_order_release);finishEntered.store(false,std::memory_order_release);
    pipeline->Submit(true,
#ifdef TEST_FRAME_COMPARE
    true
#else
    false
#endif
#ifdef TEST_FRAME_PREFIX
    ,true
#endif
    );assert(pipeline->InFlight() && !pipeline->Recording());
    // No graphics access here. Main advances simulation before the render
    // consumer can complete. Owned transforms and component pages must survive.
    for(unsigned i=0;i<1000;++i){context.stack.value.rows[0].x=float(i);++simulationIterations;}
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!finishEntered.load(std::memory_order_acquire) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    assert(finishEntered.load(std::memory_order_acquire));
    finishAllowed.store(true,std::memory_order_release);pipeline->Join(producer,inputs);
    assert(!pipeline->InFlight() && producer==&primary && inputs==&pool);
    assert(expected.trace==actual.trace && !memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));
    assert(expectedWords.size()==size_t(gpuCmdBuf+gpuCmdBufOffset-commandStorage) && std::equal(expectedWords.begin(),expectedWords.end(),commandStorage));
    totalDraws+=96;
   }
  }
 }
 // Resource/foreign-call fences can consume a partial capture synchronously.
 active=&actual;gpuCmdBuf=commandStorage;gpuCmdBufOffset=0;gpuCmdBufSize=1048576;
 auto* producer=&primary;auto* inputs=&pool;primary.Begin();pipeline->Begin(producer,inputs);
 producer->AddState({PICA_PREPARED_VIEWPORT,0,0,400,240,0,0});
 pipeline->Flush(producer,inputs);assert(!pipeline->InFlight() && !pipeline->Recording());
 assert(pipeline->CompletedMetrics().synchronous==1 && !pipeline->CompletedMetrics().failures);
 assert(pipeline->DrainCount()==0);
 for(unsigned i=0;i<36;++i){
  primary.Begin();pipeline->Begin(producer,inputs);
  producer->AddGeometry({reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),nullptr,3,36,4,0x1000,0x3210,0,false,false,false,false});
  pipeline->Flush(producer,inputs,reinterpret_cast<const void*>(uintptr_t(i+100)));
 }
 assert(pipeline->DrainCount()==32 && pipeline->DrainAt(0).calls==1 && pipeline->DrainAt(31).calls==5);
 unsigned drained=0;for(unsigned i=0;i<pipeline->DrainCount();++i)drained+=pipeline->DrainAt(i).draws;
 assert(drained==36 && pipeline->DrainAt(31).site==nullptr);
#ifdef FORCE_FRAME_COMMAND_FAILURE
 primary.Begin();pipeline->Begin(producer,inputs);
 producer->AddGeometry({reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),nullptr,3,36,4,0x1000,0x3210,0,false,false,false,false});
 gpuCmdBufSize=1;pipeline->Submit(false);pipeline->Join(producer,inputs);
 assert(false);
#endif
#if defined(TEST_FRAME_PREFIX) && !defined(TEST_FRAME_COMPARE)
 assert(prefixCalls==96 && pauseCalls==96);
#else
 assert(prefixCalls==0 && pauseCalls==0);
#endif
 context.preparedInputPool=originalPool;delete pipeline;
 printf("PASS: %u full-frame draws match serial SDK calls, registers and command words; %u simulation iterations run before consumer completion; bounded pages and synchronous fences. Host ownership proof, not FPS.\n",totalDraws,simulationIterations);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-frame-pipeline-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 flags=['-DFORCE_FRAME_COMMAND_FAILURE'] if '--fault' in sys.argv else []
 subprocess.run(['clang++','-I',str(root/'libs/pure3d'),'-std=c++17','-O1','-g','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),'-fno-omit-frame-pointer',*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 result=subprocess.run([str(p/'test')],capture_output=True,text=True,timeout=60)
 print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
 if '--fault' in sys.argv:
  assert result.returncode==1 and 'PREPARED_EXECUTOR_INVARIANT reason=geometry_command_room' in result.stdout
  print('PASS: worker command-capacity failure joins and terminates on main without submitting the partial frame.')
 else:result.check_returncode()

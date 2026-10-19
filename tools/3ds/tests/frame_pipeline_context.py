#!/usr/bin/env python3
"""Compare actual frame-capture context integration with original SDK execution."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
def clean(path):return '\n'.join(x for x in path.read_text().splitlines() if not x.startswith('#include'))+'\n'
def extract(text,signature):
 a=text.index(signature);end=text.index('{',a)+1;level=1
 while level:level+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[a:end]+'\n'
base=subprocess.check_output([sys.executable,str(root/'tools/3ds/tests/prepared_worker.py'),'--emit-source'],text=True)
base=base[:base.index('int main(){')]
base=base.replace('bool StereoActive(){return stereo;}','bool StereoActive(){return stereo;}bool IsWorldTarget(){return false;}')
worker=clean(root/'libs/pure3d/pddi/pica/framepipeline.cpp');a=worker.index('    unsigned value;');b=worker.index('\n}',a)
worker=worker[:a]+'    return 0; // Host shim; ARM read-only FPSCR check compiled separately.'+worker[b:]
marker='struct picaContext:picaPreparedUniformCache,pddiBaseContext{'
base=base.replace(marker,'''constexpr unsigned GX_CMDLIST_FLUSH=1;
extern "C" void picaNativeFramePrefix(bool){gpuCmdBuf+=gpuCmdBufOffset;gpuCmdBufSize-=gpuCmdBufOffset;gpuCmdBufOffset=0;}
extern "C" void picaNativeFrameRetirePrefix(){}
extern "C" void picaNativeFrameAbort(){}
unsigned gpuWaits=0;void C3Di_RenderQueueWaitDone(){++gpuWaits;}

float C3D_GetProcessingTime(){return 1;}float C3D_GetDrawingTime(){return 2;}
void C3D_FrameEnd(unsigned flags){active->trace.push_back({99,flags});Emit(2,99);}
'''+clean(root/'libs/pure3d/pddi/pica/framepipeline.hpp')+worker+marker,1)
needle='picaPreparedWorker* preparedWorker=nullptr;';base=base.replace(needle,'''picaFramePipeline* preparedFrame=nullptr;bool preparedFrameAttempted=false,preparedGpuPending=false;
 void RefreshPreparedOwnership();bool PreparedFrameInFlight()const;bool SubmitPreparedFrame();bool QueuePreparedDriver(void(*)(const void*),const void*,unsigned);
 '''+needle,1)
base=base.replace('delete preparedWorker;delete preparedExecutor;', 'delete preparedFrame;delete preparedWorker;delete preparedExecutor;',1)
context=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
base=base.replace(' picaPreparedDrawInputs* preparedDrawCapture=nullptr;',"""
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
 picaDeferredSkin::Pool* deferredSkinPool=nullptr;picaDeferredSkin::Snapshot* deferredSkin=nullptr;pddiMatrix* deferredSkinScratch=nullptr;
 bool BeginDeferredSkin(picaDeferredSkin::Rest*,pddiMatrix*,const pddiMatrix* const*,const bool*);void EndDeferredSkin();void MaterializeDeferredSkin();bool QueueDeferredSkinPalette(const pddiMatrix* const*,unsigned);void DumpDeferredSkin(FILE*);
#endif
 picaPreparedDrawInputs* preparedDrawCapture=nullptr;""",1)
base=base.replace('~picaContext(){FlushPreparedExecution(true);','~picaContext(){\n#ifdef SRR2_OLD3DS_DEFERRED_SKIN\nEndDeferredSkin();\n#endif\nFlushPreparedExecution(true);',1)
base=base.replace('delete preparedInputPool;}', 'delete preparedInputPool;\n#ifdef SRR2_OLD3DS_DEFERRED_SKIN\ndelete deferredSkinPool;\n#endif\n}',1)
for name in ['bool picaContext::BeginDeferredSkin(', 'void picaContext::EndDeferredSkin(', 'void picaContext::MaterializeDeferredSkin(', 'bool picaContext::QueueDeferredSkinPalette(']:base+='\n#ifdef SRR2_OLD3DS_DEFERRED_SKIN\n'+extract(context,name)+'#endif\n'
base=base.replace('void picaPreparedBackendFence(){if(activeOwner)activeOwner->FlushPreparedExecution(true);}',extract(context,'void picaPreparedBackendFence(').replace('s_picaContext','activeOwner'))
bottom=extract(context,'bool picaContext::BeginBottomScreen(')
assert 'SubmitPreparedScissor(GPU_SCISSOR_DISABLE,0,0,0,0)' in bottom
for name in ['void picaContext::RefreshPreparedOwnership(', 'bool picaContext::PreparedFrameInFlight(', 'bool picaContext::SubmitPreparedFrame(', 'bool picaContext::QueuePreparedDriver(']:base+='\n'+extract(context,name).replace('s_picaContext','activeOwner')
base+='\nstruct picaPrimBuffer{picaContext* context;unsigned short* indices;bool renderExposed=true;unsigned char* LockIndexBuffer();};\n'+extract(context,'unsigned char* picaPrimBuffer::LockIndexBuffer(')
base+='\nnamespace ResourcePressure {\nstruct alignas(16) picaBufferAllocation {picaBufferAllocation* next;};\nstatic picaBufferAllocation* retiredBuffers;static unsigned allocations=0,reclaims=0;\nstatic void* linearAlloc(unsigned size){return ++allocations%2?nullptr:malloc(size);}\nstatic void picaBufferReclaim(){assert(activeOwner && !activeOwner->PreparedFrameInFlight() && !activeOwner->preparedGpuPending);++reclaims;retiredBuffers=nullptr;}\n'+extract(context,'static void* picaBufferAllocate(')+'}\n'
source=('#define SRR2_OLD3DS_DEFERRED_SKIN\n' if '--deferred' in sys.argv else '')+('#define SRR2_OLD3DS_RETAINED_MATERIAL\n' if '--retained' in sys.argv else '')+'#ifdef SRR2_OLD3DS_DEFERRED_SKIN\nbool g_picaDeferredSkinEnabled=true;\n#endif\n#define SRR2_OLD3DS_FRAME_PIPELINE\nnamespace NativeBench {inline bool collecting=true;}\nstatic const void* s_picaPreparedFenceCaller=nullptr;\nbool g_picaFramePrefixEnabled=false;bool g_picaUnexposedResourcePrep=true;bool g_picaPreparedOwnershipPending=false;\n'+base+r'''
int main(){
 GPU expected,actual;std::vector<u32> expectedWords;
 unsigned simulated=0;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
 g_picaRetainedMaterialEnabled=true;
#endif
 for(unsigned frame=0;frame<64;++frame){
  for(unsigned arm=0;arm<2;++arm){
   GPU& gpu=arm?actual:expected;gpu=GPU{};g_picaInFrame=true;active=&gpu;
   g_picaState={};gpuCmdBuf=commandStorage;gpuCmdBufSize=1048576;gpuCmdBufOffset=0;
   picaContext context{};activeOwner=&context;
   context.lighting={};context.eyeLight[0]={};context.screen.stereo=false;context.hwProjection={};
   context.lightsDirty=true;g_picaPreparedBackendEnabled=arm;g_picaPreparedExecution=arm?7:1;
   pddiBaseShader::currentUID=~0u;pddiBaseShader::lastShader=nullptr;
   picaMat materials[3];Texture textures[3];
   for(unsigned i=0;i<3;++i){materials[i].context=&context;materials[i].uid=100+i;textures[i].tex.data=reinterpret_cast<void*>(uintptr_t(0x14004000+i*4096));materials[i].texEnv[0].texture=&textures[i];materials[i].texEnv[0].ambient.c=0xff445566;materials[i].texEnv[0].diffuse.c=0xff778899;materials[i].texEnv[0].emissive.c=0;}
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
   pddiMatrix inverse[2],posed[2],scratch[3];const pddiMatrix* inversePointers[]={&inverse[0],&inverse[1]};const pddiMatrix* posedPointers[]={&posed[0],&posed[1]};const pddiMatrix* palettePointers[]={&scratch[0],&scratch[1],&scratch[2]};bool used[]={true,true};
   for(unsigned i=0;i<2;++i){inverse[i].Identity();inverse[i].rows[3].x=float(i)/4;posed[i].Identity();}
   auto* rest=picaDeferredSkin::Rest::Create(inversePointers,2);assert(rest);
#endif
   shaderProgram_s program=1;C3D_AttrInfo attributes=1;Foreign foreign;foreign.owner=&context;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
   for(unsigned row=0;row<4;++row)context.stack.value.rows[row]={float(row),0,0,row==3?1.f:0.f};
   // A non-retained pass may consume a direct native setter without changing UID.
   // Return through another shader before reusing the original retained version.
   for(unsigned probe=0;probe<4;++probe){
    g_picaRetainedMaterialEnabled=probe==0 || probe==3;
    if(probe==1){materials[0].texEnv[0].filterMode=1;materials[0].dirty=materials[0].nativeDirty=true;}
    context.ApplyDrawMaterial(&materials[probe==2?1:0]);
   }
#endif
   for(unsigned draw=0;draw<96;++draw){
    for(unsigned row=0;row<4;++row)context.stack.value.rows[row]={float(draw+row),float(frame),0,row==3?1.f:0.f};
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    if(draw==35){rest->Release();inverse[0].rows[3].x+=0.25f;rest=picaDeferredSkin::Rest::Create(inversePointers,2);assert(rest);}
#endif
    if(draw%13==0){if(arm)assert(context.QueuePreparedProgram(&program,&attributes,true,VSH_FVEC_u_eye,7));else{C3D_BindProgram(&program);C3D_FVUnifSet(GPU_VERTEX_SHADER,VSH_FVEC_u_eye,0,7,0,0);C3D_SetAttrInfo(&attributes);g_picaState.bufInfoSet=false;}context.InvalidateUniformCache();}
    if(draw%17==0){
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
     picaPreparedSkinPalette palette;palette.count=3;
     float eye[4][4];for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)eye[r][c]=context.stack.value.rows[r][c];
     for(unsigned i=0;i<2;++i){posed[i].Identity();posed[i].rows[3].y=float(draw+i);scratch[i].Mult(inverse[i],posed[i]);}scratch[2].Identity();
     for(unsigned i=0;i<3;++i){float bone[4][4];for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)bone[r][c]=scratch[i].rows[r][c];assert(picaSkin::PackEyeMatrix(palette.values[i],bone,eye));}
     if(arm){assert(context.BeginDeferredSkin(rest,scratch,posedPointers,used));assert(context.QueueDeferredSkinPalette(palettePointers,3));assert(context.QueueDeferredSkinPalette(palettePointers,3));context.EndDeferredSkin();for(auto& matrix:posed)matrix={};for(auto& matrix:scratch)matrix={};}
     else {assert(picaUploadPreparedSkinPalette(palette,20));assert(picaUploadPreparedSkinPalette(palette,20));}
#else
     picaPreparedSkinPalette palette;palette.count=2;for(auto& bone:palette.values)for(auto& row:bone)for(auto& value:row)value=float(draw);if(arm)assert(context.QueuePreparedPalette(palette,20));else assert(picaUploadPreparedSkinPalette(palette,20));
#endif
    }
    if(draw%23==0){if(arm)assert(context.QueuePreparedState({PICA_PREPARED_VIEWPORT,0,0,400,240,0,0}));else C3D_SetViewport(0,0,400,240);}
    if(draw%37==0)context.SetZRange(1,1000);
    if(draw==40){pddiBaseShader::ClearCurrentShader();if(arm)assert(context.preparedFrame->Recording());}
    if(draw==48){picaStateInvalidate();context.SubmitPreparedScissor(1,2,3,20,40);}
    if(draw==72){pddiBaseShader::lastShader=&foreign;pddiBaseShader::ClearCurrentShader();if(arm)assert(!context.preparedFrame->Recording());}
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    g_picaRetainedMaterialEnabled=(draw/12)%2==0;
#endif
    if(draw%19==0){materials[draw%3].texEnv[0].filterMode=draw%5;materials[draw%3].texEnv[0].alphaTest=(frame+draw)%2;materials[draw%3].texEnv[0].alphaCompareMode=(frame+draw)%8;materials[draw%3].texEnv[0].alphaBlendMode=(frame+draw/19)%8;materials[draw%3].texEnv[0].writeMask=(frame+draw)%16;materials[draw%3].texEnv[0].twoSided=(frame+draw/19)%2;materials[draw%3].dirty=materials[draw%3].nativeDirty=true;}
    if(draw==32 || draw==64 || draw==80){context.FlushPreparedExecution(true);auto& t=textures[1];t.tex.width=draw==32?128:64;t.valid=draw!=64;++t.revision;}
    if(draw%11==0){auto& m=materials[draw%3];m.texEnv[0].lit=draw%2;m.texEnv[0].diffuse.c^=0x00553377;m.coloursValid=false;}
    context.ApplyDrawMaterial(&materials[draw%3]);
    picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),draw+3,36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
    if(arm){assert(context.PreparedGeometryRoom());context.QueuePreparedGeometry(geometry);}else{picaBindPreparedGeometry(geometry,g_picaState.bufInfoSet);picaSubmitPreparedGeometry(geometry);}
   }
   if(arm){assert(context.SubmitPreparedFrame());g_picaInFrame=false;assert(g_picaPreparedOwnershipPending);g_picaPreparedBackendEnabled=false;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    rest->Release();rest=nullptr;for(auto& matrix:inverse)matrix={};for(auto& matrix:posed)matrix={};
#endif
    for(unsigned i=0;i<1000;++i){context.stack.value.rows[0].x=float(i);++simulated;}

#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    for(auto& m:materials){m.texEnv[0].diffuse.c=0x12345678;m.dirty=m.nativeDirty=true;m.coloursValid=false;if(m.retainedMaterial){m.retainedMaterial->Release();m.retainedMaterial=nullptr;}}
#endif
    picaPrimBuffer fresh={&context,nullptr,false};assert(fresh.LockIndexBuffer()==nullptr);
    assert(context.PreparedFrameInFlight() && context.preparedGpuPending);
    if(frame%3==0){picaAlphaTest(true,2,44);assert(!context.PreparedFrameInFlight() && !g_picaPreparedOwnershipPending);}
    else if(frame%2==0){
     ResourcePressure::retiredBuffers=reinterpret_cast<ResourcePressure::picaBufferAllocation*>(uintptr_t(1));
     void* allocated=ResourcePressure::picaBufferAllocate(64);assert(allocated);
     free(static_cast<ResourcePressure::picaBufferAllocation*>(allocated)-1);
    }
    picaPrimBuffer writable={&context,nullptr,true};assert(writable.LockIndexBuffer()==nullptr);
    assert(context.preparedFrame && !context.PreparedFrameInFlight() && !context.preparedGpuPending && !g_picaPreparedOwnershipPending);
    const unsigned before=gpuWaits;context.preparedGpuPending=true;context.RefreshPreparedOwnership();assert(g_picaPreparedOwnershipPending);picaPreparedFenceIfOwned();assert(!context.preparedGpuPending && !g_picaPreparedOwnershipPending && gpuWaits==before+1);
   }
   else {C3D_FrameEnd(GX_CMDLIST_FLUSH);if(frame%3==0)picaAlphaTest(true,2,44);}
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
   if(rest)rest->Release();if(arm){assert(context.deferredSkinPool && context.deferredSkinPool->stats.live==0 && context.deferredSkinPool->stats.recipes>0 && context.deferredSkinPool->stats.computedBones>0);}
#endif
   pddiBaseShader::ClearCurrentShader();
   if(!arm)expectedWords.assign(commandStorage,commandStorage+gpuCmdBufOffset);
   else{if(expected.trace!=actual.trace){fprintf(stderr,"frame=%u expected=%zu actual=%zu\n",frame,expected.trace.size(),actual.trace.size());for(unsigned n=0;n<std::min(expected.trace.size(),actual.trace.size());++n)if(expected.trace[n]!=actual.trace[n]){fprintf(stderr,"first=%u\n",n);for(auto x:expected.trace[n])fprintf(stderr,"%zu ",x);fprintf(stderr," vs ");for(auto x:actual.trace[n])fprintf(stderr,"%zu ",x);fprintf(stderr,"\n");break;}}assert(expected.trace==actual.trace);assert(!memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));assert(expectedWords.size()==size_t(gpuCmdBuf+gpuCmdBufOffset-commandStorage) && std::equal(expectedWords.begin(),expectedWords.end(),commandStorage));}
  }
 }
 activeOwner=nullptr;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
 assert(g_picaRetainedMaterialBuilds>0 && g_picaRetainedMaterialReuses>0 && g_picaRetainedMaterialFallbacks==0);
 printf("Retained material builds=%u reuses=%u; colour/lit, UID, texture resize/validity/revision, queued-owner retirement and concurrent producer changes covered.\n",g_picaRetainedMaterialBuilds,g_picaRetainedMaterialReuses);
#endif
 printf("PASS: 6144 draws through actual frame-pipeline context match original calls, uniforms and command words with program/palette/viewport/UID/sampler/depth changes; %u concurrent simulation iterations. Host proof only.\n",simulated);
}
'''
if '--old-retained-invalidation' in sys.argv:source=source.replace('if(retainedMaterial)retainedMaterial->valid=false;','')
if '--prefix' in sys.argv:source=source.replace('bool g_picaFramePrefixEnabled=false;','bool g_picaFramePrefixEnabled=true;')
if '--compare' in sys.argv:source=source.replace('g_picaPreparedExecution=arm?7:1;', 'g_picaPreparedExecution=arm?10:1;')
if '--emit-source' in sys.argv:print(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='srr-frame-context-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-I',str(root/'libs/pure3d'),'-std=c++17','-O1','-g','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),'-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=60)

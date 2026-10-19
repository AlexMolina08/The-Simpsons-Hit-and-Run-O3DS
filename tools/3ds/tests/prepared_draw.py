#!/usr/bin/env python3
"""Exercise production material capture/execute and legacy UID protocol together."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
def clean(s):return '\n'.join(x for x in s.splitlines() if not x.startswith('#include'))+'\n'
def extract(s,signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]+'\n'
source=subprocess.check_output([sys.executable,str(root/'tools/3ds/tests/prepared_uniforms.py'),'--emit-source'],text=True)
source=source[:source.index('unsigned seed=9171;')]
source=source.replace('#include <thread>','#include <thread>\n#include <typeinfo>\n#include <new>\n#define SRR2_OLD3DS_DRAW_PREP')
source=source.replace('void Old3dsCount(unsigned){}','void Old3dsCount(unsigned,unsigned=1){}').replace('void FlushPreparedExecution(bool){}','void FlushPreparedExecution(bool);')
source=source.replace('std::array<u32,6>','std::array<uintptr_t,8>')
material=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
cache=(root/'libs/pure3d/pddi/pica/statecache.hpp').read_text()
prepared=(root/'libs/pure3d/pddi/pica/preparedmaterial.hpp').read_text()
raster=(root/'libs/pure3d/pddi/pica/preparedraster.hpp').read_text()
context=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
rasterCapture=extract(context,'void picaContext::CapturePreparedMaterialRaster(')
rasterTables=context[context.index('static const GPU_TESTFUNC compTable'):context.index('static const GPU_STENCILOP stencilOpTable')]
tables=material[material.index('static const GPU_TEXTURE_FILTER_PARAM'):material.index('#ifdef SRR2_OLD3DS_MATERIAL_WRITES')]
types=['GPU_TEXTURE_FILTER_PARAM','GPU_TEXTURE_WRAP_PARAM','GPU_TESTFUNC','GPU_BLENDEQUATION','GPU_BLENDFACTOR','GPU_CULLMODE','GPU_WRITEMASK','GPU_TEXCOLOR']
constants=sorted(set(re.findall(r'\bGPU_[A-Z0-9_]+',tables+cache+prepared+raster+rasterCapture+rasterTables))-set(types))
prefix=''.join('using '+x+'=int;\n' for x in types)+'enum {'+','.join(x+'='+str(i+1) for i,x in enumerate(constants))+'};\n'
prefix+=r'''
using u16=uint16_t;using u64=uint64_t;using GPU_Primitive_t=int;using GPU_SCISSORMODE=int;using shaderProgram_s=int;using C3D_AttrInfo=int;
struct C3D_Tex{void* data=nullptr;u32 param=0;u16 width=64,height=64;int fmt=0;};
struct C3D_TexEnv{int stage;};
'''
source=source.replace('struct GPU{',prefix+'struct GPU{C3D_TexEnv env[6]={{0},{1},{2},{3},{4},{5}};const C3D_Tex* bound=nullptr;u32 offset=0,size=65536;')
addition=r'''
enum {C3D_Both=0,O3_MATERIAL_PASS=100,O3_STATE_CHANGE,O3_TEX_REQUEST,O3_TEX_CHANGE,PDDI_BLEND_NONE=0};
void C3D_TexSetFilter(C3D_Tex* t,int a,int b){t->param=(t->param&~0xffu)|unsigned(a&15)|unsigned((b&15)<<4);}
void C3D_TexSetWrap(C3D_Tex* t,int a,int b){t->param=(t->param&~0xff00u)|unsigned((a&15)<<8)|unsigned((b&15)<<12);}
void C3D_TexBind(int,C3D_Tex* t){active->bound=t;active->trace.push_back({10,uintptr_t(t->data),t->param,t->width,t->height,uintptr_t(t->fmt)});}
C3D_TexEnv* C3D_GetTexEnv(int i){return &active->env[i];}
void C3D_TexEnvInit(C3D_TexEnv* e){active->trace.push_back({11,uintptr_t(e->stage)});}
void C3D_TexEnvSrc(C3D_TexEnv* e,int,int a,int b,int c){active->trace.push_back({12,uintptr_t(e->stage),uintptr_t(a),uintptr_t(b),uintptr_t(c)});}
void C3D_TexEnvFunc(C3D_TexEnv* e,int,int a){active->trace.push_back({13,uintptr_t(e->stage),uintptr_t(a)});}
void C3D_AlphaTest(bool e,int f,int r){active->trace.push_back({14,e,uintptr_t(f),uintptr_t(r)});}
void C3D_AlphaBlend(int a,int b,int c,int d,int e,int f){active->trace.push_back({15,uintptr_t(a),uintptr_t(b),uintptr_t(c),uintptr_t(d),uintptr_t(e),uintptr_t(f)});}
void C3D_CullFace(int mode){active->trace.push_back({16,uintptr_t(mode)});}
void C3D_DepthTest(bool enabled,int function,int mask){active->trace.push_back({17,enabled,uintptr_t(function),uintptr_t(mask)});}
constexpr unsigned PDDI_WRITE_RED=1,PDDI_WRITE_GREEN=2,PDDI_WRITE_BLUE=4,PDDI_WRITE_ALPHA=8;
struct Render{bool zEnabled=true,zWrite=true,redWrite=true,greenWrite=true,blueWrite=true,alphaWrite=true;unsigned cullMode=0,zCompare=0;};
struct pddiBaseContext{Render render;void EnableZBuffer(bool enabled){render.zEnabled=enabled;}};
constexpr unsigned GPUREG_ATTRIBBUFFER0_OFFSET=0x200,C3D_UNSIGNED_SHORT=1;
struct C3D_BufInfo{const void* vertices;unsigned stride,attributes;u64 permutation;};
void BufInfo_Init(C3D_BufInfo* info){*info={};active->trace.push_back({20});}
void BufInfo_Add(C3D_BufInfo* info,const void* vertices,unsigned stride,unsigned attributes,u64 permutation){*info={vertices,stride,attributes,permutation};active->trace.push_back({21,uintptr_t(vertices),stride,attributes,permutation});}
void C3D_SetBufInfo(const C3D_BufInfo* info){active->trace.push_back({22,uintptr_t(info->vertices),info->stride,info->attributes,info->permutation});}
extern "C" void C3Di_UpdateContext(){active->trace.push_back({23});active->offset+=64;}
void GPUCMD_AddWrite(unsigned reg,unsigned value){active->trace.push_back({24,reg,value});active->offset+=2;}
void C3D_DrawElements(int primitive,int count,int type,const void* indices){active->trace.push_back({25,uintptr_t(primitive),uintptr_t(count),uintptr_t(type),uintptr_t(indices)});active->offset+=16;}
void C3D_DrawArrays(int primitive,int first,int count){active->trace.push_back({26,uintptr_t(primitive),uintptr_t(first),uintptr_t(count)});active->offset+=16;}
void C3D_BindProgram(shaderProgram_s* program){active->trace.push_back({27,uintptr_t(*program)});}
void C3D_SetAttrInfo(C3D_AttrInfo* attributes){active->trace.push_back({28,uintptr_t(*attributes)});}
void C3D_FixedAttribSet(unsigned index,float x,float y,float z,float w){active->trace.push_back({29,index,bits(x),bits(y),bits(z),bits(w)});}
void C3D_SetViewport(u32 x,u32 y,u32 w,u32 h){active->trace.push_back({30,x,y,w,h});}
void C3D_SetScissor(GPU_SCISSORMODE mode,u32 a,u32 b,u32 c,u32 d){active->trace.push_back({31,uintptr_t(mode),a,b,c,d});}
u64 svcGetSystemTick(){static u64 ticks=0;return ++ticks;}
unsigned g_old3dsCmdHighWater=0,g_old3dsCmdSize=0;struct {unsigned maxCommand=0;} g_picaSkinMetrics;
void GPUCMD_GetBuffer(u32** buffer,u32* size,u32* offset){static u32 storage[65536];*buffer=storage;*size=active->size;*offset=active->offset;}
C3D_Tex* picaNativeTexture0(){return const_cast<C3D_Tex*>(active->bound);}
void picaNativeTexture0Alias(C3D_Tex* texture){active->bound=texture;}
class picaContext;void picaPreparedBackendFence();

'''
# Cache helpers use Old3dsCount, declared in the original uniform fixture.
pos=source.index('struct Light{')
source=source[:pos]+addition+clean((root/'libs/pure3d/pddi/pica/preparedbackend.h').read_text())+clean(cache)+'picaStateCache g_picaState={};\n'+clean(prepared)+clean(raster)+tables+rasterTables+r'''
class picaMat;
'''+clean((root/'libs/pure3d/pddi/pica/preparedbackend.h').read_text())+clean((root/'libs/pure3d/pddi/pica/preparedbatch.hpp').read_text())+clean((root/'libs/pure3d/pddi/pica/retainedmaterial.hpp').read_text())+clean((root/'libs/pure3d/pddi/pica/prepareddraw.hpp').read_text())+clean((root/'libs/pure3d/pddi/pica/preparedgeometry.hpp').read_text())+clean((root/'libs/pure3d/pddi/pica/preparedstate.hpp').read_text())+'\n#include <pddi/pica/skincore.hpp>\n'+clean((root/'libs/pure3d/pddi/pica/preparedskin.hpp').read_text())+'#ifdef SRR2_OLD3DS_DEFERRED_SKIN\n'+clean((root/'libs/pure3d/pddi/pica/deferredskin.hpp').read_text())+'#endif\n'+clean((root/'libs/pure3d/pddi/pica/preparedexecutor.hpp').read_text())+r'''
picaPreparedBackendStats preparedBackendStats={};
unsigned g_picaPreparedExecution=1;
bool g_picaPreparedBackendEnabled=false,g_old3dsDrawBlended=false,g_old3dsDrawAlphaTested=false;
class pddiBaseShader{
public:
 static inline unsigned currentUID=~0u;static inline pddiBaseShader* lastShader=nullptr;
 unsigned uid=0;
 virtual ~pddiBaseShader()=default;
 virtual int GetPasses(){return 1;}
 virtual void SetPass(int)=0;virtual void PreRender(){}virtual void PostRender(){}
'''+extract((root/'libs/pure3d/pddi/base/baseshader.hpp').read_text(),'    void SetMaterial(int pass = 0)')+extract((root/'libs/pure3d/pddi/base/baseshader.hpp').read_text().split('    void SetMaterial(int pass = 0)',1)[1],'    static void ClearCurrentShader(void)')+'};\n'+source[pos:]
source=source.replace('struct picaContext:picaPreparedUniformCache{','struct picaContext:picaPreparedUniformCache,pddiBaseContext{')
source=source.replace('Display* display;','Display* display;')
source=source.replace('Lighting* lightingState;', 'Lighting* lightingState;Render* renderState;')
source=source.replace('display=&screen;', 'display=&screen;state.renderState=&render;')
source=source.replace(' Stack stack;Lighting lighting;Display screen;',''' picaPreparedDrawInputs* preparedDrawCapture=nullptr;picaPreparedInputPool* preparedInputPool=nullptr;
 picaPreparedExecutor* preparedExecutor=nullptr;bool preparedBatchFailed=false,preparedBatchReady=true;
 ~picaContext(){FlushPreparedExecution(true);delete preparedExecutor;delete preparedInputPool;}
 bool PreparedBatching()const;void BeginPreparedExecution();bool PreparedGeometryRoom();void QueuePreparedGeometry(const picaPreparedGeometry&);bool QueuePreparedState(const picaPreparedState&);
 bool QueuePreparedPalette(const picaPreparedSkinPalette&,unsigned);bool QueuePreparedProgram(shaderProgram_s*,C3D_AttrInfo*,bool,unsigned,float);bool QueuePreparedLayout(C3D_AttrInfo*,bool);void SubmitPreparedScissor(GPU_SCISSORMODE,u32,u32,u32,u32);
 int matWriteMask=15;

#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
 bool CanRetainMaterial()const;
#endif
 void ApplyDrawMaterial(pddiBaseShader*);void ExecutePreparedUniforms(const picaPreparedUniformView&);
 bool CapturePreparedUniformRecord(picaPreparedUniformRecord&,const void*,unsigned,bool,pddiColour,pddiColour,pddiColour,const float (*)[4],const picaPreparedMaterialUniforms* = nullptr);
 void EnableCulling(bool);void SetMaterialWriteMask(int);void EnableZBuffer(bool);
 void CapturePreparedMaterialRaster(const picaPreparedMaterialPass&,picaPreparedMaterialRaster&);
 Stack stack;Lighting lighting;Display screen;''')
source+=r'''
struct Texture{C3D_Tex tex;unsigned revision=1;unsigned NativeRevision()const{return revision;}bool valid=true;C3D_Tex* GetC3DTexture(){return valid?&tex:nullptr;}};
struct picaTextureEnv{
 Texture* texture=nullptr;int filterMode=0,uvMode=0,alphaCompareMode=0,alphaBlendMode=0,writeMask=15;
 bool alphaTest=false,twoSided=false,lit=false;float alphaRef=0.5f;
 pddiColour ambient,diffuse,emissive;
};
class picaMat:public pddiBaseShader{
public:
 picaTextureEnv texEnv[1];picaContext* context=nullptr;bool dirty=true,nativeDirty=true,coloursValid=false;
 int nativeFilter,nativeWrap,nativeCompare,nativeEquation,nativeSource,nativeDestination,nativeAlphaRef;
 float nativeColours[3][4];
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
 picaRetainedMaterial* retainedMaterial=nullptr;
 picaRetainedMaterial* PrepareRetainedMaterial();
 ~picaMat(){if(retainedMaterial)retainedMaterial->Release();}
#endif
 void PrepareColours();void PrepareNative();void SetDevPass(unsigned);bool CapturePreparedPass(picaPreparedMaterialPass&);static bool PreparedPreviousShader();
 void PreRender() override;void SetPass(int p) override {SetDevPass(p);}
};
'''
for sig in ['bool picaPreparedShaderNeedsFence(', 'bool picaMat::PreparedPreviousShader(', 'void picaMat::PrepareNative(', 'void picaMat::PrepareColours(', 'void picaMat::SetDevPass(', 'bool picaMat::CapturePreparedPass(', 'void picaMat::PreRender(']:source+=extract(material,sig)
source+='#ifdef SRR2_OLD3DS_RETAINED_MATERIAL\nbool g_picaRetainedMaterialEnabled=false;unsigned g_picaRetainedMaterialBuilds=0,g_picaRetainedMaterialReuses=0,g_picaRetainedMaterialFallbacks=0;\n'+extract(material,'picaRetainedMaterial* picaMat::PrepareRetainedMaterial(')+extract(context,'bool picaContext::CanRetainMaterial(')+'#endif\n'
context=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
for sig in ['bool picaContext::PreparedBatching(', 'void picaContext::BeginPreparedExecution(', 'void picaContext::FlushPreparedExecution(', 'bool picaContext::PreparedGeometryRoom(', 'void picaContext::QueuePreparedGeometry(', 'bool picaContext::QueuePreparedState(', 'bool picaContext::QueuePreparedPalette(', 'bool picaContext::QueuePreparedProgram(', 'bool picaContext::QueuePreparedLayout(', 'void picaContext::SubmitPreparedScissor(', 'void picaContext::ApplyDrawMaterial(', 'void picaContext::ExecutePreparedUniforms(', 'void picaContext::CapturePreparedMaterialRaster(', 'bool picaContext::CapturePreparedUniformRecord(', 'void picaContext::EnableCulling(', 'void picaContext::SetMaterialWriteMask(', 'void picaContext::EnableZBuffer(']:source+=extract(context,sig)
source+=r'''
picaContext* activeOwner=nullptr;void picaPreparedBackendFence(){if(activeOwner)activeOwner->FlushPreparedExecution(true);}
bool picaPreparedCaptureState(unsigned kind,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e,unsigned f){return activeOwner && activeOwner->PreparedBatching() && activeOwner->QueuePreparedState({kind,a,b,c,d,e,f});}
unsigned seed=713;unsigned randomValue(){seed=1664525u*seed+1013904223u;return seed;}
float scalar(){return float(int(randomValue()%20001)-10000)/100.f;}
struct ShaderState{unsigned uid=~0u;pddiBaseShader* shader=nullptr;};
void load(const ShaderState& s){pddiBaseShader::currentUID=s.uid;pddiBaseShader::lastShader=s.shader;}
ShaderState save(){return {pddiBaseShader::currentUID,pddiBaseShader::lastShader};}
// A foreign shader uses the untouched path and mutates SDK uniforms, like a UI
// effect. PostRender remains observable even when entering a prepared material.
struct Foreign:public pddiBaseShader{
 picaContext* owner=nullptr;picaMat* nested=nullptr;bool recursing=false;
 void SetPass(int) override{active->trace.push_back({18});}
 void PreRender() override{C3D_FVUnifSet(0,95,1,2,3,4);}
 void PostRender() override{active->trace.push_back({19});if(nested&&!recursing){recursing=true;owner->ApplyDrawMaterial(nested);recursing=false;}}
};
int main(){
 picaContext reference,producer;GPU expected,actual;
 picaMat refMaterials[3],actualMaterials[3];Texture refTextures[3],actualTextures[3];
 Foreign refForeign,actualForeign;refForeign.uid=actualForeign.uid=10;
 refForeign.owner=&reference;actualForeign.owner=&producer;refForeign.nested=&refMaterials[2];actualForeign.nested=&actualMaterials[2];
 for(unsigned i=0;i<3;++i){refMaterials[i].context=&reference;actualMaterials[i].context=&producer;refMaterials[i].uid=actualMaterials[i].uid=i+1;}
 picaStateCache referenceState={},actualState={};ShaderState referenceShader,actualShader;
 unsigned writes=0;
 for(unsigned n=0;n<40000;++n){
  const unsigned selected=randomValue()%3;
  if(n%3==0){auto& e=refMaterials[selected].texEnv[0];
   const unsigned texture=randomValue()%4;e.texture=texture<3?&refTextures[texture]:nullptr;
   e.filterMode=randomValue()%5;e.uvMode=randomValue()%3;e.alphaTest=randomValue()%2;
   e.alphaCompareMode=randomValue()%8;e.alphaBlendMode=randomValue()%8;e.alphaRef=float(int(randomValue()%400)-70)/255.f;
   e.twoSided=randomValue()%2;e.writeMask=randomValue()%16;e.lit=randomValue()%2;
   e.ambient.c=randomValue();e.diffuse.c=randomValue();e.emissive.c=randomValue();
   actualMaterials[selected].texEnv[0]=e;actualMaterials[selected].texEnv[0].texture=texture<3?&actualTextures[texture]:nullptr;
   refMaterials[selected].dirty=actualMaterials[selected].dirty=true;
   refMaterials[selected].nativeDirty=actualMaterials[selected].nativeDirty=true;
   refMaterials[selected].coloursValid=actualMaterials[selected].coloursValid=false;
  }
  if(n%7==0){unsigned i=randomValue()%3;auto& t=refTextures[i];t.tex.data=reinterpret_cast<void*>(uintptr_t(randomValue()|1u));t.tex.param=randomValue();t.tex.width=randomValue()%1024;t.tex.height=randomValue()%1024;t.tex.fmt=randomValue()%8;t.valid=randomValue()%5!=0;actualTextures[i]=t;}
  if(n%5==0){for(unsigned r=0;r<4;++r)reference.stack.value.rows[r]={scalar(),scalar(),scalar(),r==3?1.f:0.f};producer.stack.value=reference.stack.value;}
  if(n%17==0){for(auto& row:reference.hwProjection.r)row={scalar(),scalar(),scalar(),scalar()};producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.InvalidateUniformCache();producer.InvalidateUniformCache();}
  if(n%43==0){reference.hwProjection.r[0].x=scalar();producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.projUploaded=producer.projUploaded=false;}
  if(n%11==0){for(unsigned i=0;i<4;++i){auto& l=reference.lighting.light[i];l.enabled=randomValue()%2;l.colour.c=randomValue();l.attenA=scalar();l.attenB=scalar();l.attenC=scalar();reference.eyeLight[i]={float(randomValue()%2),scalar(),scalar(),scalar()};}producer.lighting=reference.lighting;memcpy(producer.eyeLight,reference.eyeLight,sizeof(reference.eyeLight));reference.lightsDirty=producer.lightsDirty=true;}
  reference.lighting.ambient.c=producer.lighting.ambient.c=randomValue();
  reference.skinProgramBound=producer.skinProgramBound=n%13==0;
  reference.stereoOn=producer.stereoOn=n%19==0;reference.screen.stereo=producer.screen.stereo=n%23==0;
  if(n%37==0){referenceState={};actualState={};reference.InvalidateUniformCache();producer.InvalidateUniformCache();referenceShader={};actualShader={};}
  if(n%13==0){reference.render.zEnabled=randomValue()%2;reference.render.zWrite=randomValue()%2;reference.render.redWrite=randomValue()%2;reference.render.greenWrite=randomValue()%2;reference.render.blueWrite=randomValue()%2;reference.render.alphaWrite=randomValue()%2;reference.render.zCompare=randomValue()%8;reference.render.cullMode=randomValue()%3;producer.render=reference.render;}
  g_picaInFrame=n%101!=0;
  expected.trace.clear();actual.trace.clear();
  active=&expected;activeOwner=&reference;g_picaState=referenceState;load(referenceShader);g_picaPreparedBackendEnabled=false;
  if(n%29==0)reference.ApplyDrawMaterial(&refForeign);else reference.ApplyDrawMaterial(&refMaterials[selected]);
  referenceState=g_picaState;referenceShader=save();
  active=&actual;activeOwner=&producer;g_picaState=actualState;load(actualShader);g_picaPreparedBackendEnabled=n%31!=0;
  if(n%29==0)producer.ApplyDrawMaterial(&actualForeign);else producer.ApplyDrawMaterial(&actualMaterials[selected]);
  actualState=g_picaState;actualShader=save();
  if(expected.trace!=actual.trace){fprintf(stderr,"trace differs draw %u: %zu vs %zu\n",n,expected.trace.size(),actual.trace.size());return 1;}
  assert(!memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));assert(!memcmp(expected.integers,actual.integers,sizeof(expected.integers)));assert(expected.booleans==actual.booleans);
  assert(referenceState.alphaTestValid==actualState.alphaTestValid);assert(referenceState.alphaTestEnable==actualState.alphaTestEnable);
  assert(referenceState.alphaTestFunc==actualState.alphaTestFunc);assert(referenceState.alphaTestRef==actualState.alphaTestRef);
  assert(referenceState.blendValid==actualState.blendValid);assert(referenceState.blendColorEq==actualState.blendColorEq);assert(referenceState.blendAlphaEq==actualState.blendAlphaEq);
  assert(referenceState.blendSrcClr==actualState.blendSrcClr);assert(referenceState.blendDstClr==actualState.blendDstClr);assert(referenceState.blendSrcAlpha==actualState.blendSrcAlpha);assert(referenceState.blendDstAlpha==actualState.blendDstAlpha);
  assert(referenceState.env0Mode==actualState.env0Mode);assert(referenceState.passStagesSet==actualState.passStagesSet);
  assert(reference.modelViewUploaded==producer.modelViewUploaded);assert(reference.projUploaded==producer.projUploaded);assert(reference.matUploaded==producer.matUploaded);assert(reference.lightsUploaded==producer.lightsUploaded);assert(reference.miscUploaded==producer.miscUploaded);
  assert(referenceShader.uid==actualShader.uid);assert(reference.matWriteMask==producer.matWriteMask);
  assert(referenceState.cullValid==actualState.cullValid);assert(referenceState.cull==actualState.cull);
  assert(referenceState.depthValid==actualState.depthValid);assert(referenceState.depthEnable==actualState.depthEnable);assert(referenceState.depthFunc==actualState.depthFunc);assert(referenceState.depthMask==actualState.depthMask);
  for(unsigned i=0;i<3;++i){assert(refTextures[i].tex.param==actualTextures[i].tex.param);assert(refMaterials[i].dirty==actualMaterials[i].dirty);}
  if(expected.bound&&actual.bound){assert(expected.bound->data==actual.bound->data);assert(expected.bound->param==actual.bound->param);}
  assert(!producer.preparedDrawCapture);writes+=actual.trace.size();
 }

 activeOwner=nullptr;
 unsigned orderedDraws=0;
 {
  GPU orderedExpected,orderedActual;shaderProgram_s normalProgram=1,skinProgram=2;C3D_AttrInfo normalAttributes=1,skinAttributes=2,packedAttributes=3;
  picaStateCache refState={},outState={};ShaderState refShader,outShader;
  reference.InvalidateUniformCache();producer.InvalidateUniformCache();
  reference.lightsDirty=producer.lightsDirty=true;g_picaInFrame=true;
  for(unsigned frame=0;frame<256;++frame){
   orderedExpected.trace.clear();orderedActual.trace.clear();orderedExpected.offset=orderedActual.offset=0;
   orderedExpected.size=orderedActual.size=frame%2?4096:65536;
   for(unsigned draw=0;draw<32;++draw){
    const unsigned index=draw%3;
    auto& e=refMaterials[index].texEnv[0];
    if(draw%3==0){e.ambient.c=randomValue();e.diffuse.c=randomValue();e.emissive.c=randomValue();e.lit=draw%2;e.filterMode=randomValue()%5;e.uvMode=randomValue()%3;e.twoSided=randomValue()%2;e.writeMask=randomValue()%16;e.texture=&refTextures[index];
     auto& candidate=actualMaterials[index];candidate.texEnv[0]=e;candidate.texEnv[0].texture=&actualTextures[index];
     refMaterials[index].dirty=candidate.dirty=true;refMaterials[index].nativeDirty=candidate.nativeDirty=true;refMaterials[index].coloursValid=candidate.coloursValid=false;}
    for(unsigned row=0;row<4;++row)reference.stack.value.rows[row]={scalar(),scalar(),scalar(),row==3?1.f:0.f};producer.stack.value=reference.stack.value;
    if(draw==15){reference.hwProjection.r[0].x=scalar();producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.projUploaded=producer.projUploaded=false;}
    picaPreparedSkinPalette palette;palette.count=2;for(unsigned b=0;b<2;++b)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)palette.values[b][r][c]=float(b*12+r*4+c+draw);
    picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),unsigned(3+draw),36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
    active=&orderedExpected;activeOwner=&reference;g_picaState=refState;load(refShader);g_picaPreparedBackendEnabled=false;g_picaPreparedExecution=1;
    if(draw==2 || draw==6){const bool skin=draw==2;C3D_BindProgram(skin?&skinProgram:&normalProgram);if(!skin)C3D_FVUnifSet(GPU_VERTEX_SHADER,VSH_FVEC_u_eye,0,7,0,0);C3D_SetAttrInfo(skin?&skinAttributes:&normalAttributes);g_picaState.bufInfoSet=false;reference.skinProgramBound=skin;reference.InvalidateUniformCache();}
    if(draw>=2&&draw<6)assert(picaUploadPreparedSkinPalette(palette,20));
    if(draw==10 || draw==12){C3D_SetAttrInfo(draw==10?&packedAttributes:&normalAttributes);if(draw==10)C3D_FixedAttribSet(1,0,0,1,0);g_picaState.bufInfoSet=false;}
    if(draw==4)C3D_SetViewport(2,3,200,400);
    if(draw==5)reference.SubmitPreparedScissor(1,2,3,100,200);
    if(draw==17){picaAlphaTest(true,GPU_GEQUAL,127);picaAlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);picaCullFace(GPU_CULL_NONE);}
    if(draw==25)picaStateInvalidate();
    if(draw==24)pddiBaseShader::ClearCurrentShader();
    if(draw==19){reference.FlushPreparedExecution(true);refTextures[index].tex.width=128;}
    if(draw==7){reference.render.zWrite=!reference.render.zWrite;reference.EnableZBuffer(reference.render.zEnabled);}
    if(draw==11)reference.ApplyDrawMaterial(&refForeign);else reference.ApplyDrawMaterial(&refMaterials[index]);
    if(draw==23){reference.FlushPreparedExecution(true);g_picaState.bufInfoSet=false;active->trace.push_back({28});}
    picaBindPreparedGeometry(geometry,g_picaState.bufInfoSet);picaSubmitPreparedGeometry(geometry);
    refState=g_picaState;refShader=save();
    active=&orderedActual;activeOwner=&producer;g_picaState=outState;load(outShader);g_picaPreparedBackendEnabled=true;g_picaPreparedExecution=3;
    if(draw==2 || draw==6){const bool skin=draw==2;assert(producer.QueuePreparedProgram(skin?&skinProgram:&normalProgram,skin?&skinAttributes:&normalAttributes,!skin,VSH_FVEC_u_eye,7));producer.skinProgramBound=skin;producer.InvalidateUniformCache();}
    if(draw>=2&&draw<6){assert(producer.QueuePreparedPalette(palette,20));memset(palette.values,0x57,sizeof(palette.values));}
    if(draw==10 || draw==12)assert(producer.QueuePreparedLayout(draw==10?&packedAttributes:&normalAttributes,draw==10));
    if(draw==4)assert(producer.QueuePreparedState({PICA_PREPARED_VIEWPORT,2,3,200,400,0,0}));
    if(draw==5)producer.SubmitPreparedScissor(1,2,3,100,200);
    if(draw==17){picaAlphaTest(true,GPU_GEQUAL,127);picaAlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);picaCullFace(GPU_CULL_NONE);}
    if(draw==25)picaStateInvalidate();
    if(draw==24)pddiBaseShader::ClearCurrentShader();
    if(draw==19){producer.FlushPreparedExecution(true);actualTextures[index].tex.width=128;}
    if(draw==7){producer.render.zWrite=!producer.render.zWrite;producer.EnableZBuffer(producer.render.zEnabled);}
    if(draw==11)producer.ApplyDrawMaterial(&actualForeign);else producer.ApplyDrawMaterial(&actualMaterials[index]);
    if(draw==23){producer.FlushPreparedExecution(true);g_picaState.bufInfoSet=false;active->trace.push_back({28});}
    assert(producer.PreparedGeometryRoom());producer.QueuePreparedGeometry(geometry);
    outState=g_picaState;outShader=save();++orderedDraws;
   }
   active=&orderedActual;activeOwner=&producer;g_picaState=outState;producer.FlushPreparedExecution(true);outState=g_picaState;
   if(orderedExpected.trace!=orderedActual.trace){fprintf(stderr,"ordered trace differs frame %u: %zu vs %zu\n",frame,orderedExpected.trace.size(),orderedActual.trace.size());for(unsigned i=0;i<orderedExpected.trace.size()&&i<orderedActual.trace.size();++i)if(orderedExpected.trace[i]!=orderedActual.trace[i]){fprintf(stderr,"first difference %u: %zu vs %zu\n",i,orderedExpected.trace[i][0],orderedActual.trace[i][0]);break;}return 2;}
   assert(orderedExpected.booleans==orderedActual.booleans);assert(orderedExpected.offset==orderedActual.offset);
   assert(!memcmp(orderedExpected.vectors,orderedActual.vectors,sizeof(orderedExpected.vectors)));assert(!memcmp(orderedExpected.integers,orderedActual.integers,sizeof(orderedExpected.integers)));
   assert(refState.bufInfoSet==outState.bufInfoSet);assert(refShader.uid==outShader.uid);assert(!producer.preparedExecutor->valid);
   if(orderedExpected.bound&&orderedActual.bound){assert(orderedExpected.bound->data==orderedActual.bound->data);assert(orderedExpected.bound->param==orderedActual.bound->param);}
  }
  printf("PASS: %u ordered material/uniform/raster/geometry draws match immediate SDK calls and final state; capacity flushes, projection/material mutations, foreign callbacks, program/layout/palette/view operations, resource/depth fences and command headroom. serial_batches=%u. Host SDK spies; not GPU/FPS/thread proof.\n",orderedDraws,producer.preparedExecutor->flushes);
 }

 activeOwner=nullptr;
 unsigned threadedDraws=0;
 for(unsigned batch=0;batch<128;++batch){
  GPU expectedJob,actualJob;picaStateCache refState={},outState={};ShaderState refShader,outShader;
  reference.InvalidateUniformCache();producer.InvalidateUniformCache();reference.lightsDirty=producer.lightsDirty=true;
  producer.preparedInputPool->Reset();g_picaInFrame=true;
  for(unsigned draw=0;draw<8;++draw){
   const unsigned index=draw%3;
   for(unsigned row=0;row<4;++row)reference.stack.value.rows[row]={scalar(),scalar(),scalar(),row==3?1.f:0.f};producer.stack.value=reference.stack.value;
   picaPreparedGeometry geometry={reinterpret_cast<const unsigned char*>(uintptr_t(0x14001000)),reinterpret_cast<const unsigned short*>(uintptr_t(0x14002000)),unsigned(3+draw),36,4,0x1000,0x3210,0,bool(draw%2),false,false,false};
   active=&expectedJob;activeOwner=&reference;g_picaState=refState;load(refShader);g_picaPreparedBackendEnabled=false;g_picaPreparedExecution=1;
   reference.ApplyDrawMaterial(&refMaterials[index]);picaBindPreparedGeometry(geometry,g_picaState.bufInfoSet);picaSubmitPreparedGeometry(geometry);
   refState=g_picaState;refShader=save();
   active=&actualJob;activeOwner=&producer;g_picaState=outState;load(outShader);g_picaPreparedBackendEnabled=true;g_picaPreparedExecution=3;
   producer.ApplyDrawMaterial(&actualMaterials[index]);assert(producer.PreparedGeometryRoom());producer.QueuePreparedGeometry(geometry);
   outState=g_picaState;outShader=save();
  }
  auto* executor=producer.preparedExecutor;assert(executor->operations==16);
  std::thread consumer([&]{active=&actualJob;executor->Flush();});
  const auto savedStack=producer.stack;const auto savedLighting=producer.lighting;const auto savedProjection=producer.hwProjection;const auto savedState=producer.state;auto* savedDisplay=producer.display;auto* savedPool=producer.preparedInputPool;
  const auto savedEnvironment=actualMaterials[0].texEnv[0];Texture savedTextures[3];for(unsigned i=0;i<3;++i)savedTextures[i]=actualTextures[i];
  producer.stack={};producer.lighting={};producer.hwProjection={};producer.state={};producer.display=nullptr;producer.preparedInputPool=nullptr;
  actualMaterials[0].texEnv[0]={};for(auto& texture:actualTextures){texture.tex={};texture.valid=false;}
  consumer.join();
  producer.stack=savedStack;producer.lighting=savedLighting;producer.hwProjection=savedProjection;producer.state=savedState;producer.display=savedDisplay;producer.preparedInputPool=savedPool;
  actualMaterials[0].texEnv[0]=savedEnvironment;for(unsigned i=0;i<3;++i)actualTextures[i]=savedTextures[i];
  active=&actualJob;activeOwner=&producer;producer.FlushPreparedExecution(true);
  assert(expectedJob.trace==actualJob.trace);assert(expectedJob.booleans==actualJob.booleans);assert(expectedJob.offset==actualJob.offset);
  assert(!memcmp(expectedJob.vectors,actualJob.vectors,sizeof(expectedJob.vectors)));assert(!memcmp(expectedJob.integers,actualJob.integers,sizeof(expectedJob.integers)));
  assert(!executor->valid);threadedDraws+=8;
 }
 printf("PASS: %u complete material/uniform/raster/geometry draws execute on a host consumer thread after producer context, material and texture metadata mutation. Owned input/cache and texture shadow proof under SDK spies; no real SDK concurrency or FPS claim.\n",threadedDraws);
 activeOwner=nullptr;g_picaPreparedExecution=1;
 unsigned pooledDraws=0;
 for(unsigned batch=0;batch<128;++batch){
  g_picaInFrame=true;
  reference.InvalidateUniformCache();producer.InvalidateUniformCache();
  reference.lightsDirty=producer.lightsDirty=true;
  auto* pool=producer.preparedInputPool;assert(pool);pool->Reset();
  std::array<picaPreparedUniformRecord,picaPreparedInputPool::Capacity> records;
  pddiColour ambient,diffuse,emissive;ambient.c=0x731b2843;diffuse.c=0xff88aabb;emissive.c=0x87003443;
  GPU expectedBatch,actualBatch;
  for(unsigned draw=0;draw<records.size();++draw){
   for(unsigned row=0;row<4;++row)reference.stack.value.rows[row]={scalar(),scalar(),scalar(),row==3?1.f:0.f};producer.stack.value=reference.stack.value;
   active=&expectedBatch;reference.SetVertexUniforms(true,ambient,diffuse,emissive,nullptr);
   assert(producer.CapturePreparedUniformRecord(records[draw],&actualMaterials[0],1,true,ambient,diffuse,emissive,nullptr));
  }
  assert(pool->viewCount==1 && pool->lightCount==1 && pool->materialCount==1 && pool->drawCount==16);
  picaPreparedUniformCache ownedCache;
  std::thread consumer([&]{active=&actualBatch;for(const auto& record:records)picaApplyPreparedUniforms(pool->View(record),ownedCache);});
  const auto savedStack=producer.stack;const auto savedLighting=producer.lighting;const auto savedProjection=producer.hwProjection;const auto savedState=producer.state;auto* savedDisplay=producer.display;
  producer.stack={};producer.lighting={};producer.hwProjection={};producer.state={};producer.display=nullptr;producer.preparedInputPool=nullptr;
  consumer.join();producer.stack=savedStack;producer.lighting=savedLighting;producer.hwProjection=savedProjection;producer.state=savedState;producer.display=savedDisplay;producer.preparedInputPool=pool;
  assert(expectedBatch.trace==actualBatch.trace);assert(expectedBatch.booleans==actualBatch.booleans);
  assert(!memcmp(expectedBatch.vectors,actualBatch.vectors,sizeof(expectedBatch.vectors)));assert(!memcmp(expectedBatch.integers,actualBatch.integers,sizeof(expectedBatch.integers)));
  pooledDraws+=records.size();
 }
 printf("PASS: %u production pooled uniform draws match legacy after producer poisoning on a consumer thread; 1 view/light/material per 16 draws. record_bytes=%zu flat_bytes=%zu pool_bytes=%zu. Host ownership/SDK spy proof; not FPS.\n",pooledDraws,sizeof(picaPreparedUniformRecord),sizeof(picaPreparedUniformInput),sizeof(picaPreparedInputPool));
 printf("PASS: 40000 production combined material captures/executions match legacy SDK call order, uniform registers, UID, material dirty state, cull/depth/write masks and sampler writes; %u calls. Includes fallback, nested foreign PostRender, no-frame and cache transitions. Mock SDK; geometry/palette submission not covered.\n",writes);
}
'''
source='#include \"'+str(root/'code/main/old3dshot.h')+'\"\n'+source
if '--emit-source' in sys.argv:print(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='prepared-draw-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-I',str(root/'libs/pure3d'),'-std=c++17','-O1','-g','-fsanitize=thread' if '--tsan' in sys.argv else '-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

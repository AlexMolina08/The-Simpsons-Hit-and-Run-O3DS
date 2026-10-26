/* Adapted from Citro3D 1.7.1 (fincs), commit 9f21cf7.
 * Original license: citro3d_171/LICENSE.
 * Alteration: compile common state writes into one bounded raw command block.
 * Rare state retains upstream execution. Ordering and context flags are retained.
 */
#include "nativecontext.h"
#include "nativecommands.h"
#include "nativecontextsnapshot.h"
#include "citro3d_171/internal.h"
#include <string.h>
#include <stddef.h>

extern void __real_C3Di_UpdateContext(void);
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
extern void __wrap_C3D_UpdateUniforms(GPU_SHADER_TYPE type);
#define nativeUpdateUniforms __wrap_C3D_UpdateUniforms
#else
#define nativeUpdateUniforms C3D_UpdateUniforms
#endif
extern bool g_picaNativeTrackedFlush;
extern bool picaNativeUniformBatchReady(void);
extern bool picaNativeUniformsPending(GPU_SHADER_TYPE type);
picaNativeCommandBlock* g_picaNativeCommandBlock;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
bool g_picaNativeContextEnabled;
#else
bool g_picaNativeContextEnabled=true;
#endif
static unsigned fastCalls,fallbackCalls,compiledWords;
static const shaderProgram_s* normalProgram;
static const shaderProgram_s* skinProgram;
static const shaderProgram_s* reflectionProgram;
static const shaderProgram_s* contactProgram;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
static u32 reflectionOutputMap[2],reflectionOutputClock[2];
static bool reflectionOutputsConfigured,reflectionOutputRequested;
static bool sparseReflectionOutputs=true,reflectionOutputValid,reflectionOutputApplied;
static bool reflectionOutputPending=true;
static const shaderProgram_s* reflectionOutputProgram;
static const shaderProgram_s* reflectionEntryPrograms[2];
static unsigned reflectionEntries[2][2];
static bool reflectionEntriesConfigured,reflectionEntryApplied;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned reflectionOutputWrites[2];
static unsigned reflectionEntryWrites[2];
#endif
void picaNativeConfigureReflectionEntries(const shaderProgram_s* mono,unsigned monoOrdinary,unsigned monoReflected,
                                         const shaderProgram_s* stereo,unsigned stereoOrdinary,unsigned stereoReflected)
{
    reflectionEntryPrograms[0]=mono;reflectionEntryPrograms[1]=stereo;
    reflectionEntries[0][0]=monoOrdinary;reflectionEntries[0][1]=monoReflected;
    reflectionEntries[1][0]=stereoOrdinary;reflectionEntries[1][1]=stereoReflected;
    reflectionEntriesConfigured=mono!=NULL;reflectionOutputValid=false;reflectionOutputPending=true;
}
void picaNativeReflectionEntryStats(unsigned* ordinary,unsigned* reflected,unsigned* enabled)
{
    *enabled=reflectionEntriesConfigured;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    *ordinary=reflectionEntryWrites[0];*reflected=reflectionEntryWrites[1];
#else
    *ordinary=*reflected=0;
#endif
}
void picaNativeConfigureReflectionOutputs(unsigned ordinaryMap,unsigned ordinaryClock,unsigned reflectedMap,unsigned reflectedClock)
{
    reflectionOutputMap[0]=ordinaryMap;reflectionOutputClock[0]=ordinaryClock;
    reflectionOutputMap[1]=reflectedMap;reflectionOutputClock[1]=reflectedClock;
    reflectionOutputsConfigured=true;reflectionOutputValid=false;reflectionOutputPending=true;
}
void picaNativeSetReflectionOutput(bool reflected)
{
    if(reflectionOutputRequested!=reflected)reflectionOutputPending=true;
    reflectionOutputRequested=reflected;
}
void picaNativeInvalidateReflectionOutputs(void){reflectionOutputValid=false;reflectionOutputPending=true;}
void picaNativeEnableSparseReflectionOutputs(bool enabled)
{sparseReflectionOutputs=enabled;reflectionOutputValid=false;reflectionOutputPending=true;}
void picaNativeReflectionOutputStats(unsigned* ordinary,unsigned* reflected,unsigned* enabled)
{
    *enabled=reflectionOutputsConfigured && sparseReflectionOutputs;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    *ordinary=reflectionOutputWrites[0];*reflected=reflectionOutputWrites[1];
#else
    *ordinary=*reflected=0;
#endif
}
#endif
void picaNativeRegisterContactProgram(const shaderProgram_s* contact) { contactProgram=contact; }
void picaNativeRegisterReflectionProgram(const shaderProgram_s* reflection)
{
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(reflectionProgram!=reflection){reflectionOutputValid=false;reflectionOutputPending=true;}
#endif
    reflectionProgram=reflection;
}
void picaNativeBindTexture1(C3D_Tex* texture)
{
    C3D_Context* ctx=C3Di_GetContext();
    if(!(ctx->flags&C3DiF_Active))return;
    // Citro3D 1.7.1 dereferences unit-1 textures before checking for NULL.
    // Retain its 2D-only validation and dirty-state protocol while allowing
    // an actual unbind, so ordinary materials do not sample another texture.
    if(texture && ((texture->param>>28)&7u)!=0)return;
    ctx->tex[1]=texture;
    ctx->flags|=C3DiF_Tex(1);
}
void picaNativeRegisterPrograms(const shaderProgram_s* normal,const shaderProgram_s* skin)
{
    normalProgram=normal;skinProgram=skin;
}
typedef picaNativeCommandBlock Packet;
static inline void writeOne(Packet* p,u32 reg,u32 mask,u32 value)
{
    p->data[p->count++]=value;
    p->data[p->count++]=GPUCMD_HEADER(0,mask,reg);
}
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
static __attribute__((cold,noinline)) void reflectionOutputs(Packet* p,const C3D_Context* ctx)
{
    reflectionOutputProgram=ctx->program;
    reflectionOutputPending=false;
    if(!reflectionOutputsConfigured || ctx->program!=reflectionProgram || !ctx->program || ctx->program->geometryShader)
    {
        reflectionOutputValid=false;
        // A temporarily foreign geometry stage on an owned root remains dirty.
        reflectionOutputPending=ctx->program && ctx->program==reflectionProgram && ctx->program->geometryShader;
        return;
    }
    if(reflectionEntriesConfigured && ctx->program!=reflectionEntryPrograms[0] && ctx->program!=reflectionEntryPrograms[1])
    {reflectionOutputValid=false;return;}
    const bool reflected=!sparseReflectionOutputs || reflectionOutputRequested;
    if(reflectionEntriesConfigured && (!reflectionOutputValid || reflectionEntryApplied!=reflectionOutputRequested))
    {
        const unsigned family=ctx->program==reflectionEntryPrograms[1]?1:0;
        writeOne(p,GPUREG_VSH_ENTRYPOINT,15,0x7fff0000u|reflectionEntries[family][reflectionOutputRequested]);
        reflectionEntryApplied=reflectionOutputRequested;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        ++reflectionEntryWrites[reflectionOutputRequested];
#endif
    }
    if(reflectionOutputValid && reflectionOutputApplied==reflected)return;
    // Use the SDK-derived maps. Both programs keep position/color/UV0 and the
    // same three output registers; only reflective draws consume UV1 in O2.zw.
    // Run after SDK program configuration, which otherwise restores its map.
    writeOne(p,GPUREG_SH_OUTMAP_O2,15,reflectionOutputMap[reflected]);
    writeOne(p,GPUREG_SH_OUTATTR_CLOCK,15,reflectionOutputClock[reflected]);
    reflectionOutputApplied=reflected;reflectionOutputValid=true;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    ++reflectionOutputWrites[reflected];
#endif
}
static inline void updateReflectionOutputs(Packet* p,const C3D_Context* ctx)
{
    // Most draws retain the same program and material. Avoid revalidating the
    // immutable entry metadata until a request, program or SDK state changes.
    if(reflectionOutputPending || reflectionOutputProgram!=ctx->program)
        reflectionOutputs(p,ctx);
}
#endif
static inline void writeMany(Packet* p,u32 reg,const u32* values,unsigned count)
{
    picaNativeCommandAdd(p,GPUCMD_HEADER(1,15,reg),values,count);
}
static void effect(Packet* p,const C3D_Effect* e)
{
    writeOne(p,GPUREG_DEPTHMAP_ENABLE,15,e->zBuffer?1:0);
    writeOne(p,GPUREG_FACECULLING_CONFIG,15,e->cullMode&3);
    writeMany(p,GPUREG_DEPTHMAP_SCALE,&e->zScale,2);
    writeMany(p,GPUREG_FRAGOP_ALPHA_TEST,&e->alphaTest,4);
    writeOne(p,GPUREG_GAS_DELTAZ_DEPTH,8,(u32)GPU_MAKEGASDEPTHFUNC((e->depthTest>>4)&7)<<24);
    writeOne(p,GPUREG_BLEND_COLOR,15,e->blendClr);
    writeOne(p,GPUREG_BLEND_FUNC,15,e->alphaBlend);
    writeOne(p,GPUREG_LOGIC_OP,15,e->clrLogicOp);
    writeOne(p,GPUREG_COLOR_OPERATION,7,e->fragOpMode);
    writeOne(p,GPUREG_FRAGOP_SHADOW,15,e->fragOpShadow);
    writeOne(p,GPUREG_EARLYDEPTH_TEST1,1,e->earlyDepth?1:0);
    writeOne(p,GPUREG_EARLYDEPTH_TEST2,15,e->earlyDepth?1:0);
    writeOne(p,GPUREG_EARLYDEPTH_FUNC,1,e->earlyDepthFunc);
    writeOne(p,GPUREG_EARLYDEPTH_DATA,7,e->earlyDepthRef);
}
static void texture(Packet* p,unsigned unit,C3D_Tex* tex)
{
    u32 values[10]={tex->border,tex->dim,tex->param,tex->lodParam};
    unsigned count=5;
    if(C3Di_TexIs2D(tex))values[4]=osConvertVirtToPhys(tex->data)>>3;
    else {
        count=10;
        for(unsigned i=0;i<6;++i){values[4+i]=osConvertVirtToPhys(tex->cube->data[i])>>3;if(i)values[4+i]&=0x3fffff;}
    }
    const u32 border[]={GPUREG_TEXUNIT0_BORDER_COLOR,GPUREG_TEXUNIT1_BORDER_COLOR,GPUREG_TEXUNIT2_BORDER_COLOR};
    const u32 type[]={GPUREG_TEXUNIT0_TYPE,GPUREG_TEXUNIT1_TYPE,GPUREG_TEXUNIT2_TYPE};
    writeMany(p,border[unit],values,unit?5:count);
    writeOne(p,type[unit],15,tex->fmt);
}
static void compileState(Packet* p,C3D_Context* ctx)
{
    if(ctx->flags&C3DiF_Viewport) {
        writeMany(p,GPUREG_VIEWPORT_WIDTH,ctx->viewport,4);
        writeOne(p,GPUREG_VIEWPORT_XY,15,ctx->viewport[4]);
        ctx->flags&=~C3DiF_Viewport;
    }
    if(ctx->flags&C3DiF_Scissor) {
        writeMany(p,GPUREG_SCISSORTEST_MODE,ctx->scissor,3);
        ctx->flags&=~C3DiF_Scissor;
    }
    if(ctx->flags&C3DiF_AttrInfo) {
        C3D_AttrInfo* a=&ctx->attrInfo;
        writeMany(p,GPUREG_ATTRIBBUFFERS_FORMAT_LOW,a->flags,2);
        writeOne(p,GPUREG_VSH_INPUTBUFFER_CONFIG,11,0xa0000000|(a->attrCount-1));
        writeOne(p,GPUREG_VSH_NUM_ATTR,15,a->attrCount-1);
        writeMany(p,GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW,(const u32*)&a->permutation,2);
        ctx->flags&=~C3DiF_AttrInfo;
    }
    if(ctx->flags&C3DiF_BufInfo) {
        writeOne(p,GPUREG_ATTRIBBUFFERS_LOC,15,ctx->bufInfo.base_paddr>>3);
        writeMany(p,GPUREG_ATTRIBBUFFER0_OFFSET,(const u32*)ctx->bufInfo.buffers,sizeof(ctx->bufInfo.buffers)/4);
        ctx->flags&=~C3DiF_BufInfo;
    }
    if(ctx->flags&C3DiF_Effect){effect(p,&ctx->effect);ctx->flags&=~C3DiF_Effect;}
    if(ctx->flags&C3DiF_TexAll) {
        u32 units=0;
        for(unsigned i=0;i<3;++i)if(ctx->tex[i]) {
            units|=BIT(i);
            if(ctx->flags&C3DiF_Tex(i))texture(p,i,ctx->tex[i]);
        }
        ctx->texConfig=(ctx->texConfig&~7u)|units|BIT(16);
        ctx->flags=(ctx->flags&~C3DiF_TexAll)|C3DiF_TexStatus;
    }
    if(ctx->flags&C3DiF_TexStatus) {
        writeOne(p,GPUREG_TEXUNIT_CONFIG,11,ctx->texConfig);
        if(ctx->texConfig&BIT(16)){ctx->texConfig&=~BIT(16);writeOne(p,GPUREG_TEXUNIT_CONFIG,4,BIT(16));}
        writeOne(p,GPUREG_TEXUNIT0_SHADOW,15,ctx->texShadow);
        ctx->flags&=~C3DiF_TexStatus;
    }
    if(ctx->flags&C3DiF_TexEnvBuf) {
        writeOne(p,GPUREG_TEXENV_UPDATE_BUFFER,7,ctx->texEnvBuf);
        writeOne(p,GPUREG_TEXENV_BUFFER_COLOR,15,ctx->texEnvBufClr);
        writeOne(p,GPUREG_FOG_COLOR,15,ctx->fogClr);
        ctx->flags&=~C3DiF_TexEnvBuf;
    }
    if(ctx->flags&C3DiF_TexEnvAll) {
        for(unsigned i=0;i<6;++i)if(ctx->flags&C3DiF_TexEnv(i))
            writeMany(p,GPUREG_TEXENV0_SOURCE+(i>=4?i+2:i)*8,(const u32*)&ctx->texEnv[i],sizeof(C3D_TexEnv)/4);
        ctx->flags&=~C3DiF_TexEnvAll;
    }
    if(ctx->flags&C3DiF_LightEnv) {
        writeOne(p,GPUREG_LIGHTING_ENABLE0,15,0);
        writeOne(p,GPUREG_LIGHTING_ENABLE1,15,1);
        ctx->flags&=~C3DiF_LightEnv;
    }
}
bool picaNativeNormalProgramBound(void)
{
    const shaderProgram_s* program=C3Di_GetContext()->program;
    return program && (program==normalProgram || program==reflectionProgram);
}
static __attribute__((cold,noinline)) void fallbackContext(C3D_Context* ctx)
{
    (void)ctx;
    if(g_picaNativeContextEnabled)++fallbackCalls;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    const bool programConfigured=(ctx->flags&C3DiF_Program)!=0;
#endif
    __real_C3Di_UpdateContext();
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(reflectionEntriesConfigured && ctx->program==reflectionProgram && ctx->program && !ctx->program->geometryShader)
    {
        // SDK configuration selects DVLE 0: ordinary entry, UV0 map/clock.
        // Other SDK state updates leave these registers untouched.
        if(programConfigured){reflectionOutputValid=true;reflectionOutputApplied=false;reflectionEntryApplied=false;reflectionOutputPending=true;}
    }
    else {reflectionOutputValid=false;reflectionOutputPending=true;}
    if(programConfigured)reflectionOutputPending=true;
    // Owned draw entrypoints reserve 2,048 words before reaching here.
    // Foreign or exhausted buffers retain the SDK path without extra writes.
    if(gpuCmdBuf && gpuCmdBufOffset<=gpuCmdBufSize && gpuCmdBufSize-gpuCmdBufOffset>=6)
    {
        Packet outputs={gpuCmdBuf+gpuCmdBufOffset,0};
        updateReflectionOutputs(&outputs,ctx);gpuCmdBufOffset+=outputs.count;
    }
#endif
}
void picaNativeUpdateContext(void)
{
    C3D_Context* ctx=C3Di_GetContext();
    const u32 common=C3DiF_Active|C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|
        C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|
        C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|
        C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
    // Only backend-owned programs use the tracked float-uniform API. Foreign
    // clients retain the SDK scan even when their state otherwise looks common.
    if(!g_picaNativeContextEnabled || !ctx->program ||
       (ctx->program!=normalProgram && ctx->program!=skinProgram && ctx->program!=reflectionProgram && ctx->program!=contactProgram) ||
       !picaNativeUniformBatchReady() ||
       !gpuCmdBuf || gpuCmdBufOffset>gpuCmdBufSize || gpuCmdBufSize-gpuCmdBufOffset<2048 ||
       !(ctx->flags&C3DiF_Active) ||
       (ctx->flags&~common) || ctx->lightEnv || ctx->fixedAttribDirty ||
       (ctx->program && ctx->program->geometryShader) ||
       (ctx->texEnvBuf&7)==GPU_GAS ||
       ((ctx->flags&C3DiF_FogLut) && (ctx->texEnvBuf&7)!=GPU_NO_FOG)) {
        fallbackContext(ctx);
        return;
    }
    Packet p; p.count=0;p.data=gpuCmdBuf+gpuCmdBufOffset;
    compileState(&p,ctx);
    // Both stages have at most 96 dirty vectors, four integers, one bool word
    // and eight constants each. Even separate vector commands plus common
    // state fit inside the 2,048 words reserved before state mutation.
    g_picaNativeCommandBlock=&p;
    g_picaNativeTrackedFlush=true;
    if(picaNativeUniformsPending(GPU_VERTEX_SHADER))nativeUpdateUniforms(GPU_VERTEX_SHADER);
    if(picaNativeUniformsPending(GPU_GEOMETRY_SHADER))nativeUpdateUniforms(GPU_GEOMETRY_SHADER);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    updateReflectionOutputs(&p,ctx);
#endif
    g_picaNativeTrackedFlush=false;
    g_picaNativeCommandBlock=NULL;
    gpuCmdBufOffset+=p.count;
    ++fastCalls;compiledWords+=p.count;
}
bool picaNativeCanCaptureContext(void)
{
    const C3D_Context* ctx=C3Di_GetContext();
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    // The recorded-draw format predates this live output routing. Keep these
    // owned draws synchronous rather than retaining borrowed routing state.
    if(reflectionOutputsConfigured && ctx->program==reflectionProgram)return false;
#endif
    const u32 common=C3DiF_Active|C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|
        C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|
        C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|
        C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
    return g_picaNativeContextEnabled && ctx->program &&
        (ctx->program==normalProgram || ctx->program==skinProgram || ctx->program==reflectionProgram || ctx->program==contactProgram) &&
        picaNativeUniformBatchReady() && (ctx->flags&C3DiF_Active) &&
        !(ctx->flags&~common) && !ctx->lightEnv && !ctx->fixedAttribDirty &&
        !ctx->program->geometryShader && (ctx->texEnvBuf&7)!=GPU_GAS &&
        (!(ctx->flags&C3DiF_FogLut) || (ctx->texEnvBuf&7)==GPU_NO_FOG);
}
void picaNativeCaptureContext(picaNativeContextSnapshot* s)
{
    C3D_Context* ctx=C3Di_GetContext();C3D_Context* out=&s->context;
    const u32 flags=out->flags=ctx->flags;s->textureMask=0;
    // Copy only fields read for this dirty-state record. Full SDK contexts
    // contain command queues, framebuffers and fixed attributes unused here.
    if(flags&C3DiF_Viewport)memcpy(out->viewport,ctx->viewport,sizeof(out->viewport));
    if(flags&C3DiF_Scissor)memcpy(out->scissor,ctx->scissor,sizeof(out->scissor));
    if(flags&C3DiF_AttrInfo)out->attrInfo=ctx->attrInfo;
    if(flags&C3DiF_BufInfo)out->bufInfo=ctx->bufInfo;
    if(flags&C3DiF_Effect)out->effect=ctx->effect;
    if(flags&(C3DiF_TexAll|C3DiF_TexStatus)){out->texConfig=ctx->texConfig;out->texShadow=ctx->texShadow;}
    if(flags&C3DiF_TexAll)for(unsigned i=0;i<3;++i)if(ctx->tex[i]) {
        s->textureMask|=1u<<i;
        if(!(flags&C3DiF_Tex(i)))continue;
        s->textures[i]=*ctx->tex[i];
        if(!C3Di_TexIs2D(ctx->tex[i]))s->cubes[i]=*ctx->tex[i]->cube;
    }
    if(flags&C3DiF_TexEnvBuf){out->texEnvBuf=ctx->texEnvBuf;out->texEnvBufClr=ctx->texEnvBufClr;out->fogClr=ctx->fogClr;}
    if(flags&C3DiF_TexEnvAll)for(unsigned i=0;i<6;++i)
        if(flags&C3DiF_TexEnv(i))out->texEnv[i]=ctx->texEnv[i];
    // Consume only fields the current compiler consumes. No command generation
    // or upload-cache mutation occurs on the producer side of this boundary.
    if(ctx->flags&C3DiF_TexAll)ctx->texConfig=(ctx->texConfig&~7u)|s->textureMask|BIT(16);
    if(ctx->flags&(C3DiF_TexAll|C3DiF_TexStatus))ctx->texConfig&=~BIT(16);
    ctx->flags&=~(C3DiF_Viewport|C3DiF_Scissor|C3DiF_AttrInfo|C3DiF_BufInfo|
        C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvBuf|C3DiF_TexEnvAll|C3DiF_LightEnv);
    picaNativeCaptureUniforms(GPU_VERTEX_SHADER,&s->uniforms[0]);
    picaNativeCaptureUniforms(GPU_GEOMETRY_SHADER,&s->uniforms[1]);
}
void picaNativeCompileContext(picaNativeCommandBlock* p,picaNativeContextSnapshot* s,
    picaNativeUniformCache* cache)
{
    C3D_Context* local=&s->context;
    for(unsigned i=0;i<3;++i) {
        local->tex[i]=NULL;
        if(!(s->textureMask&(1u<<i)))continue;
        // Undirty texture slots are tested for presence only; their metadata
        // was deliberately not copied and must not be read.
        if((local->flags&C3DiF_Tex(i)) && !C3Di_TexIs2D(&s->textures[i]))s->textures[i].cube=&s->cubes[i];
        local->tex[i]=&s->textures[i];
    }
    compileState(p,local);
    picaNativeCompileUniforms(p,GPU_VERTEX_SHADER,&s->uniforms[0],cache);
    picaNativeCompileUniforms(p,GPU_GEOMETRY_SHADER,&s->uniforms[1],cache);
}
void picaNativeContextStats(unsigned* fast,unsigned* fallback,unsigned* words)
{*fast=fastCalls;*fallback=fallbackCalls;*words=compiledWords;}

#ifndef SRR2_OLD3DS_SUBMIT_PROFILE
void __wrap_C3Di_UpdateContext(void)
{
    picaNativeUpdateContext();
}
#endif

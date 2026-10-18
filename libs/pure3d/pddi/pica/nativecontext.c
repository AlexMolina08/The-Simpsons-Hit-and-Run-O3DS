/* Adapted from Citro3D 1.7.1 (fincs), commit 9f21cf7.
 * Original license: citro3d_171/LICENSE.
 * Alteration: compile common state writes into one bounded raw command block.
 * Rare state retains upstream execution. Ordering and context flags are retained.
 */
#include "nativecontext.h"
#ifdef SRR2_OLD3DS_OCCLUSION
#include "occlusionmetrics.h"
#endif
#include "nativecommands.h"
#include "nativecontextsnapshot.h"
#ifdef SRR2_OLD3DS_RENDER_WORKER
#include "renderworker.h"
#endif
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
picaNativeCommandBlock* g_picaNativeCommandBlock;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
bool g_picaNativeContextEnabled;
#else
bool g_picaNativeContextEnabled=true;
#endif
static unsigned fastCalls,fallbackCalls,compiledWords;
static const shaderProgram_s* normalProgram;
static const shaderProgram_s* skinProgram;
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
bool picaNativeNormalProgramBound(void){return C3Di_GetContext()->program==normalProgram && normalProgram!=NULL;}
void picaNativeUpdateContext(void)
{
#ifdef SRR2_OLD3DS_RENDER_WORKER
    if(picaRenderWorkerContext())return;
#endif
    C3D_Context* ctx=C3Di_GetContext();
#ifdef SRR2_OLD3DS_OCCLUSION
    if(ctx->program!=normalProgram && ctx->program!=skinProgram)picaVisibilityForeignState();
#endif
    const u32 common=C3DiF_Active|C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|
        C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|
        C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|
        C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
    // Only backend-owned programs use the tracked float-uniform API. Foreign
    // clients retain the SDK scan even when their state otherwise looks common.
    if(!g_picaNativeContextEnabled || !ctx->program ||
       (ctx->program!=normalProgram && ctx->program!=skinProgram) ||
       !picaNativeUniformBatchReady() ||
       !gpuCmdBuf || gpuCmdBufOffset>gpuCmdBufSize || gpuCmdBufSize-gpuCmdBufOffset<2048 ||
       !(ctx->flags&C3DiF_Active) ||
       (ctx->flags&~common) || ctx->lightEnv || ctx->fixedAttribDirty ||
       (ctx->program && ctx->program->geometryShader) ||
       (ctx->texEnvBuf&7)==GPU_GAS ||
       ((ctx->flags&C3DiF_FogLut) && (ctx->texEnvBuf&7)!=GPU_NO_FOG)) {
        if(g_picaNativeContextEnabled)++fallbackCalls;
        __real_C3Di_UpdateContext();return;
    }
    Packet p; p.count=0;p.data=gpuCmdBuf+gpuCmdBufOffset;
    compileState(&p,ctx);
    // Both stages have at most 96 dirty vectors, four integers, one bool word
    // and eight constants each. Even separate vector commands plus common
    // state fit inside the 2,048 words reserved before state mutation.
    g_picaNativeCommandBlock=&p;
    g_picaNativeTrackedFlush=true;
    nativeUpdateUniforms(GPU_VERTEX_SHADER);
    nativeUpdateUniforms(GPU_GEOMETRY_SHADER);
    g_picaNativeTrackedFlush=false;
    g_picaNativeCommandBlock=NULL;
    gpuCmdBufOffset+=p.count;
    ++fastCalls;compiledWords+=p.count;
}
bool picaNativeCanCaptureContext(void)
{
    const C3D_Context* ctx=C3Di_GetContext();
    const u32 common=C3DiF_Active|C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|
        C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|
        C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|
        C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
    return g_picaNativeContextEnabled && ctx->program &&
        (ctx->program==normalProgram || ctx->program==skinProgram) &&
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

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
C3D_Tex* picaNativeTexture0(void){return C3Di_GetContext()->tex[0];}
void picaNativeTexture0Alias(C3D_Tex* texture){C3Di_GetContext()->tex[0]=texture;}
#endif

#ifdef SRR2_OLD3DS_PREPARED_THREAD
#include <stdlib.h>
typedef struct {C3D_Context context;picaNativeUniformCheckpoint uniforms;C3D_Tex texture;bool hasTexture;} PreparedCheckpoint;
void* picaNativeAllocateCheckpoint(void){return calloc(1,sizeof(PreparedCheckpoint));}
void picaNativeFreeCheckpoint(void* checkpoint){free(checkpoint);}
void picaNativeSaveCheckpoint(void* storage)
{
    PreparedCheckpoint* checkpoint=(PreparedCheckpoint*)storage;checkpoint->context=*C3Di_GetContext();
    checkpoint->hasTexture=checkpoint->context.tex[0]!=NULL;
    if(checkpoint->hasTexture)checkpoint->texture=*checkpoint->context.tex[0];
    picaNativeSaveUniformCheckpoint(&checkpoint->uniforms);
}
void picaNativeRestoreCheckpoint(const void* storage)
{
    const PreparedCheckpoint* checkpoint=(PreparedCheckpoint*)storage;*C3Di_GetContext()=checkpoint->context;
    picaNativeRestoreUniformCheckpoint(&checkpoint->uniforms);
}
bool picaNativeCompareCheckpoint(const void* left,const void* right)
{
    const PreparedCheckpoint* a=(const PreparedCheckpoint*)left;const PreparedCheckpoint* b=(const PreparedCheckpoint*)right;
    C3D_Context ac=a->context,bc=b->context;ac.tex[0]=bc.tex[0]=NULL;
    return a->hasTexture==b->hasTexture && (!a->hasTexture || !memcmp(&a->texture,&b->texture,sizeof(a->texture))) &&
        !memcmp(&ac,&bc,sizeof(ac)) && !memcmp(&a->uniforms,&b->uniforms,sizeof(a->uniforms));
}
#endif

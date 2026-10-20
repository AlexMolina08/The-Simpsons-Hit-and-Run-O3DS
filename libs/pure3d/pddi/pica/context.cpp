#include <main/old3dsrecurring.h>
#include <main/old3dspools.h>
#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <main/old3dsmainprofile.h>
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
#include <pddi/pica/framepipeline.hpp>
#include <main/nativebench.h>
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
#include "renderworker.h"
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
#include <pddi/pica/occlusionmetrics.h>
#include <typeinfo>
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include <pddi/pica/trackeduniforms.hpp>
#include <pddi/pica/nativecontext.h>
#endif
#ifdef SRR2_OLD3DS_DRAW_PACKET
#include <pddi/pica/drawpacket.h>
#endif
#include <main/old3dshot.h>
#if defined(SRR2_OLD3DS_RESIDENT_SHADERS) && defined(SRR2_OLD3DS_NATIVE_BENCH)
#include <main/nativebench.h>
#endif
#ifdef SRR2_OLD3DS_SKIN_CLIP
#include <pddi/pica/skinclip.hpp>
#include <cstdlib>
static unsigned skinClipBytes=0,skinClipTested=0,skinClipRejected=0;
#endif
#include <pddi/pica/pica.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/pica/bottomframe.hpp>
#ifdef SRR2_OLD3DS_GPU_LISTS
#include <pddi/pica/nativegpulist.h>
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
#include <pddi/pica/wheellod.hpp>
#endif
#include <pddi/pica/statecache.hpp>
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
#include <pddi/pica/directionallight.hpp>
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
#include <pddi/pica/unlitprojection.hpp>
#include <cstdio>
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
#include <pddi/pica/nonormal.hpp>
#endif
#include <pddi/pica/queuemetrics.hpp>
#include <main/old3dsdiag.h>
extern "C" void C3Di_UpdateContext(void);
#ifdef SRR2_OLD3DS_SKIN_CLIP
static u64 skinClipTicks=0;
static unsigned skinClipCalls=0,skinClipSamples=0;
struct picaSkinClipTimer {
    bool sample;u64 start;
    picaSkinClipTimer():sample((skinClipCalls++&31u)==0),start(sample?svcGetSystemTick():0) {}
    ~picaSkinClipTimer(){if(sample){skinClipTicks+=svcGetSystemTick()-start;++skinClipSamples;}}
};
#endif
#ifdef SRR2_OLD_3DS
// Interval counters consumed by the game loop diagnostics.
unsigned g_old3dsModelViewUploads = 0;
unsigned g_old3dsModelViewReuses = 0;
#endif
static const u32 PICA_BUFINFO_BASE = 0x18000000;
#ifdef SRR2_OLD3DS_DRAW_PREP
// Header is outside all GPU-visible vertex/index ranges. Retirement therefore
// never overwrites bytes an outstanding command might still read. The list
// consumes no separate allocations and is bounded by allocated mesh memory.
struct alignas(16) picaBufferAllocation { picaBufferAllocation* next; };
static picaBufferAllocation* retiredBuffers;
static void picaBufferReclaim();
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
extern "C" void picaReclaimVramTextures();
#endif
extern "C" void C3Di_RenderQueueWaitDone(void);
static void* picaBufferAllocate(unsigned bytes)
{
    if(bytes > ~0u - sizeof(picaBufferAllocation)) return NULL;
    picaBufferAllocation* p = (picaBufferAllocation*)linearAlloc(bytes + sizeof(picaBufferAllocation));
    if(!p && retiredBuffers && !g_picaInFrame)
    {
        // Streaming outside a frame may reclaim completed draws under pressure.
        // Never force a partial presentation or reuse storage in an active frame.
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        // Allocation pressure is a global reclaim, even for a new buffer.
        // Finish command generation before waiting for GPU reads and freeing.
        picaPreparedBackendFence();
#endif
        C3Di_RenderQueueWaitDone();
        picaBufferReclaim();
        p = (picaBufferAllocation*)linearAlloc(bytes + sizeof(picaBufferAllocation));
    }
    return p ? p + 1 : NULL;
}
static void picaBufferRetire(void* data)
{
    if(!data) return;
    picaBufferAllocation* p = (picaBufferAllocation*)data - 1;
    p->next = retiredBuffers;
    retiredBuffers = p;
}
static void picaBufferReclaim()
{
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    picaReclaimVramTextures();
#endif
    // Caller has completed a blocking GPU fence, not merely submitted a frame.
    while(retiredBuffers)
    {
        picaBufferAllocation* p = retiredBuffers;
        retiredBuffers = p->next;
        linearFree(p);
    }
}
extern "C" void C3Di_RenderQueueWaitDone(void);
#endif

#include <pddi/pica/device.hpp>
#include <pddi/pica/display.hpp>
#include <pddi/pica/texture.hpp>
#include <pddi/pica/material.hpp>

#include <pddi/base/debug.hpp>
#include <raddebug.hpp>

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "pddi_shbin.h"
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
static_assert(VSH_FLAG_u_wheelPalette==(1u<<3) && VSH_ULEN_u_wheelMV==16 && VSH_FVEC_u_wheelMV+16<=96,
              "Wheel palette shader register contract");
#endif

extern "C" const unsigned char pddi_pica_shbin[];
extern "C" const unsigned int  pddi_pica_shbin_size;
#ifdef SRR2_OLD3DS_RESIDENT_SHADERS
extern "C" const unsigned char pddi_resident_shbin[];
extern "C" const unsigned int pddi_resident_shbin_size;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
#include "skin_shbin.h"
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
static_assert(VSH_FVEC_u_bones == 20 && VSH_ULEN_u_bones == 75, "Eye-space skin register contract");
static_assert(VSH_FVEC_u_eye_lightPos == 4 && VSH_FVEC_u_lightPos == 8, "Separate skin and normal light layouts");
#elif defined(SRR2_OLD3DS_GPU_SKIN_COMPACT)
static_assert(VSH_FVEC_u_bones == 24 && VSH_ULEN_u_bones == 72, "Compact skin register contract");
static_assert(VSH_FLAG_u_lit == 2, "Compact skin lighting boolean");
#else
static_assert(VSH_FVEC_u_bones == 27 && VSH_ULEN_u_bones == 69, "Skin shader register contract");
#endif
extern "C" const unsigned char pddi_skin_shbin[];
extern "C" const unsigned int pddi_skin_shbin_size;
#endif

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#include <pddi/pica/prepareddraw.hpp>
#include <pddi/pica/preparedgeometry.hpp>
#include <pddi/pica/preparedskin.hpp>
#include <pddi/pica/preparedexecutor.hpp>
#ifdef SRR2_OLD3DS_PREPARED_THREAD
#include <pddi/pica/preparedworker.hpp>
#endif
#include <new>
#include <typeinfo>
bool g_picaPreparedBackendEnabled=false;
unsigned g_picaPreparedExecution=1;
static picaPreparedBackendStats preparedBackendStats={};
void picaPreparedBackendGetStats(picaPreparedBackendStats* output){*output=preparedBackendStats;}
#endif

static const GPU_Primitive_t primTypeTable[5] =
{
    GPU_TRIANGLES,
    GPU_TRIANGLE_STRIP,
    GPU_TRIANGLES,
    GPU_TRIANGLES,
    GPU_TRIANGLES
};

static inline bool PrimIsDrawable(pddiPrimType type)
{
    return (type == PDDI_PRIM_TRIANGLES) || (type == PDDI_PRIM_TRISTRIP);
}

static const GPU_TESTFUNC compTable[8] =
{
    GPU_NEVER,
    GPU_ALWAYS,
    GPU_LESS,
    GPU_LEQUAL,
    GPU_GREATER,
    GPU_GEQUAL,
    GPU_EQUAL,
    GPU_NOTEQUAL
};

static const GPU_CULLMODE cullModeTable[3] =
{
    GPU_CULL_NONE,
    GPU_CULL_FRONT_CCW,
    GPU_CULL_BACK_CCW
};

static const GPU_STENCILOP stencilOpTable[6] =
{
    GPU_STENCIL_KEEP,
    GPU_STENCIL_ZERO,
    GPU_STENCIL_REPLACE,
    GPU_STENCIL_INCR,
    GPU_STENCIL_DECR,
    GPU_STENCIL_INVERT
};

class picaExtGamma : public pddiExtGammaControl
{
public:
    picaExtGamma() { r = g = b = 1.0f; }

    void SetGamma(float ir, float ig, float ib) { r = ir; g = ig; b = ib; }
    void GetGamma(float* outR, float* outG, float* outB) { *outR = r; *outG = g; *outB = b; }

protected:
    float r, g, b;
};

static volatile bool s_picaResumed = false;
static aptHookCookie s_picaAptCookie;

static void picaAptHook(APT_HookType hook, void*)
{
    if(hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP)
    {
        s_picaResumed = true;
    }
}

static picaContext* s_picaContext = NULL;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
extern "C" void C3Di_RenderQueueWaitDone(void);
static const void* s_picaPreparedFenceCaller=nullptr;
bool g_picaFramePrefixEnabled=false;
bool g_picaUnexposedResourcePrep=false;
bool g_picaPreparedOwnershipPending=false;
__attribute__((noinline))
#endif
void picaPreparedBackendFence(){
    if(!s_picaContext)return;
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    const void* previousCaller=s_picaPreparedFenceCaller;
    s_picaPreparedFenceCaller=__builtin_return_address(0);
#endif
    s_picaContext->FlushPreparedExecution(true);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    s_picaPreparedFenceCaller=previousCaller;
    // Mutation during the next update must wait for GPU reads as well as CPU
    // command generation. Ordinary next-frame Begin already performs this wait.
    if(s_picaContext->preparedGpuPending && !g_picaInFrame){
        const u64 start=svcGetSystemTick();
        C3Di_RenderQueueWaitDone();s_picaContext->preparedGpuPending=false;
        if(NativeBench::collecting && s_picaContext->preparedFrame)
            s_picaContext->preparedFrame->NoteJoin(__builtin_return_address(0),0,svcGetSystemTick()-start);
    }
    s_picaContext->RefreshPreparedOwnership();
#endif
}
bool picaPreparedCaptureState(unsigned kind,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e,unsigned f)
{
    return s_picaContext && s_picaContext->PreparedBatching() && s_picaContext->QueuePreparedState({kind,a,b,c,d,e,f});
}
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
void picaContext::DumpUnlitProjectionStats()
{
    std::printf("UNLIT_PROJECTION fast_uniform_calls=%u fallback_uniform_calls=%u\n", fastUnlitCalls, otherProjectionCalls);
}
extern "C" void picaDumpUnlitProjectionStats()
{
    if(s_picaContext) s_picaContext->DumpUnlitProjectionStats();
}
#endif


picaContext::picaContext(picaDevice* dev, picaDisplay* disp)
    : pddiBaseContext((pddiDisplay*)disp, (pddiDevice*)dev)
{
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    clipPrepared = false;
    clipTested = clipRejected = 0;
#endif
    device  = dev;
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    staticQueueActive = false;
    staticQueueCount = 0;
    staticQueuePrevious = NULL;
#endif
    stereoOn = false;
    bottomPass = false;
    s_picaContext = this;
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    RefreshPreparedOwnership();
#endif
    display = disp;

    device->AddRef();
    display->AddRef();
    disp->SetContext(this);

    contextID = 0;
#ifdef SRR2_OLD_3DS
    worldQuad = NULL;
#endif

#ifdef SRR2_OLD3DS_GPU_SKIN
    skinBinary = NULL;
#endif
    defaultShader = NULL;
    clearShader   = NULL;
    matWriteMask  = PDDI_WRITE_ALL;

    extGamma = new picaExtGamma;

#ifdef SRR2_OLD3DS_RESIDENT_SHADERS
    const bool useResident =
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        NativeBench::ResidentShaders();
#else
        true;
#endif
    shaderBinary = useResident ? DVLB_ParseFile((u32*)pddi_resident_shbin, pddi_resident_shbin_size)
                              : DVLB_ParseFile((u32*)pddi_pica_shbin, pddi_pica_shbin_size);
#else
    shaderBinary = DVLB_ParseFile((u32*)pddi_pica_shbin, pddi_pica_shbin_size);
#endif
    PDDIASSERT(shaderBinary != NULL);
    if(shaderBinary == NULL)
    {
        lastError = PDDI_ERR;
        return;
    }
    shaderProgramInit(&shaderProgram);
    shaderProgramSetVsh(&shaderProgram, &shaderBinary->DVLE[0]);
    C3D_BindProgram(&shaderProgram);

    {
        static bool s_hooked = false;
        if(!s_hooked)
        {
            aptHook(&s_picaAptCookie, picaAptHook, NULL);
            s_hooked = true;
        }
    }

#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    noNormalLayout = false;
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    wheelPaletteLayout = false;
#endif
    AttrInfo_Init(&noNormalAttrInfo);
    AttrInfo_AddLoader(&noNormalAttrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddFixed(&noNormalAttrInfo, 1);
    AttrInfo_AddLoader(&noNormalAttrInfo, 2, GPU_FLOAT, 2);
    AttrInfo_AddLoader(&noNormalAttrInfo, 3, GPU_UNSIGNED_BYTE, 4);
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    sparseUnlitProjection = lastFastUnlit = false;
    fastUnlitCalls = otherProjectionCalls = 0;
#endif
    AttrInfo_Init(&attrInfo);
    AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attrInfo, 2, GPU_FLOAT, 2);
    AttrInfo_AddLoader(&attrInfo, 3, GPU_UNSIGNED_BYTE, 4);
    C3D_SetAttrInfo(&attrInfo);
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    AttrInfo_Init(&wheelPaletteAttrInfo);
    AttrInfo_AddLoader(&wheelPaletteAttrInfo,0,GPU_FLOAT,3);
    AttrInfo_AddLoader(&wheelPaletteAttrInfo,1,GPU_FLOAT,4);
    AttrInfo_AddLoader(&wheelPaletteAttrInfo,2,GPU_FLOAT,2);
    AttrInfo_AddLoader(&wheelPaletteAttrInfo,3,GPU_UNSIGNED_BYTE,4);
    C3D_BoolUnifSet(GPU_VERTEX_SHADER,0x6b,false);
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
        wheelPaletteLayout = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout = false;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
#ifdef SRR2_OLD3DS_RESIDENT_SHADERS
    // Both DVLEs share one DVLP. Citro3D changes entry points and attributes
    // without uploading instructions again. Uniform invalidation stays intact.
    skinBinary = useResident ? shaderBinary : DVLB_ParseFile((u32*)pddi_skin_shbin, pddi_skin_shbin_size);
#else
    skinBinary = DVLB_ParseFile((u32*)pddi_skin_shbin, pddi_skin_shbin_size);
#endif
    if(skinBinary) {
        shaderProgramInit(&skinProgram);
#ifdef SRR2_OLD3DS_RESIDENT_SHADERS
        shaderProgramSetVsh(&skinProgram, &skinBinary->DVLE[skinBinary==shaderBinary ? 1 : 0]);
#else
        shaderProgramSetVsh(&skinProgram, &skinBinary->DVLE[0]);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
        C3D_AttrInfo packedAttributes;
        AttrInfo_Init(&packedAttributes);
        AttrInfo_AddLoader(&packedAttributes, 0, GPU_SHORT, 3);
        AttrInfo_AddLoader(&packedAttributes, 1, GPU_SHORT, 3);
        AttrInfo_AddLoader(&packedAttributes, 2, GPU_SHORT, 2);
        AttrInfo_AddLoader(&packedAttributes, 3, GPU_UNSIGNED_BYTE, 4);
        skinAttrInfo = packedAttributes;
#else
        skinAttrInfo = attrInfo;
#endif
        AttrInfo_AddLoader(&skinAttrInfo, 4, GPU_FLOAT, 4);
        AttrInfo_AddLoader(&skinAttrInfo, 5, GPU_BYTE, 4);
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
        skinRigidAttrInfo = packedAttributes;
#else
        skinRigidAttrInfo = attrInfo;
#endif
        AttrInfo_AddLoader(&skinRigidAttrInfo, 4, GPU_BYTE, 4);
        AttrInfo_AddLoader(&skinRigidAttrInfo, 5, GPU_BYTE, 4);
#endif
    }
#endif

#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    picaNativeRegisterPrograms(shaderBinary ? &shaderProgram : NULL,
#ifdef SRR2_OLD3DS_GPU_SKIN
                              skinBinary ? &skinProgram : NULL
#else
                              NULL
#endif
                              );
#endif
    C3D_DepthMap(true, -1.0f, 0.0f);
    C3D_FragOpMode(GPU_FRAGOPMODE_GL);

    for(unsigned i = 0; i < PICA_IMM_BUFFERS; i++)
    {
        immVertexRing[i] = (unsigned char*)linearAlloc(PICA_IMM_VERTEX_BYTES);
        immIndexRing[i]  = (unsigned short*)linearAlloc(PICA_IMM_INDEX_BYTES);
        PDDIASSERT(immVertexRing[i] && immIndexRing[i]);
    }
    immRing       = 0;
    immVertices   = immVertexRing[0];
    immIndices    = immIndexRing[0];
    immRing       = ( immRing + 1 ) % PICA_IMM_BUFFERS;
    immVertices   = immVertexRing[immRing];
    immIndices    = immIndexRing[immRing];
    immVertexUsed = 0;
#ifdef SRR2_OLD3DS_SKIN_RING
    skinVertexUsed = 0;
#endif
    immIndexUsed  = 0;

    memset(lightPos, 0, sizeof(lightPos));
    memset(lightColour, 0, sizeof(lightColour));
    memset(lightAtten, 0, sizeof(lightAtten));
    memset(eyeLight, 0, sizeof(eyeLight));
    for(int i = 0; i < PDDI_MAX_LIGHTS; i++)
        eyeLight[i].z = 1.0f;
    nActiveLights = 0;
    lightsDirty   = true;

    lastMatAmbient.c   = 0;
    lastMatDiffuse.c   = 0;
    lastMatEmissive.c  = 0;
    lastSceneAmbient.c = 0;
    lastLit            = false;
    InvalidateUniformCache();

    fogLutStart = 0.0f;
    fogLutEnd   = 0.0f;
    fogLutValid = false;
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);

    Mtx_Identity(&hwProjection);

    wantViewport[0] = hwViewport[0] = 0;
    wantViewport[1] = hwViewport[1] = 0;
    wantViewport[2] = hwViewport[2] = PICA_SCREEN_HEIGHT;
    wantViewport[3] = hwViewport[3] = PICA_SCREEN_WIDTH;

    DefaultState();

    defaultShader = new picaMat(this);
    defaultShader->AddRef();
    clearShader = new picaMat(this);
    clearShader->AddRef();
    clearShader->SetInt(PDDI_SP_COLOURWRITE, PDDI_WRITE_NONE);

    lastError = PDDI_OK;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    // Display setup can mark a frame active before this constructor finishes.
    // Its remaining direct SDK initialization must retain main-thread ownership.
    preparedBatchReady=true;
#endif
}

picaContext::~picaContext()
{
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    EndDeferredSkin();
#endif
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    picaPreparedBackendFence();
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#ifdef SRR2_OLD3DS_PREPARED_THREAD
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    delete preparedFrame;preparedFrame=nullptr;
#endif
    delete preparedWorker;preparedWorker=nullptr;
#endif
    delete preparedExecutor;preparedExecutor=nullptr;
    delete preparedInputPool;preparedInputPool=nullptr;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    delete deferredSkinPool;deferredSkinPool=nullptr;
#endif
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
    picaRenderWorkerShutdown();
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityShutdown();
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    picaNativeRegisterPrograms(NULL,NULL);
#endif
#ifdef SRR2_OLD3DS_STATIC_BATCH
    ClearStaticBatches();
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    DumpUnlitProjectionStats();
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    FlushStreams();
#ifdef SRR2_OLD3DS_RENDER_WORKER
    picaRenderWorkerEndFrame();
#endif
    display->EndFrame();
    C3Di_RenderQueueWaitDone();
    picaBufferReclaim();
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    picaSkinBeginFrame();
#endif
#endif
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    picaQueueDump();
#endif
    if(bottomStaticShader)bottomStaticShader->Release();
    if(defaultShader)
        defaultShader->Release();
    if(clearShader)
        clearShader->Release();

    delete extGamma;

    for(unsigned i = 0; i < PICA_IMM_BUFFERS; i++)
    {
        if(immVertexRing[i]) linearFree(immVertexRing[i]);
        if(immIndexRing[i])  linearFree(immIndexRing[i]);
    }
    immVertices = NULL;
    immIndices  = NULL;

#ifdef SRR2_OLD3DS_GPU_SKIN
    if(skinBinary) {
        shaderProgramFree(&skinProgram);
        if(skinBinary!=shaderBinary) DVLB_Free(skinBinary);
    }
#endif
    if(shaderBinary)
    {
        shaderProgramFree(&shaderProgram);
        DVLB_Free(shaderBinary);
        shaderBinary = NULL;
    }

    display->SetContext(NULL);
    display->Release();
    device->Release();
    if(s_picaContext==this){s_picaContext=nullptr;
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        g_picaPreparedOwnershipPending=false;
#endif
    }
}

picaStateCache g_picaState;

void picaContext::BeginFrame()
{
    O3_MAIN_SCOPE(FrameBegin);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    visibilityWorld=false;
    picaVisibilityReset(true);
#endif
    pddiBaseContext::BeginFrame();
#ifdef SRR2_OLD3DS_STATIC_BATCH
    if(++staticBatchFrame==0) ClearStaticBatches();
    for(auto& entry:staticBatches)
        if(entry.count && staticBatchFrame-entry.lastFrame>120)
        {staticBatchBytes-=entry.bytes;entry.Reset();}
#endif

    picaStateInvalidate();

    display->BeginFrame();
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(g_picaInFrame)preparedGpuPending=false;
    RefreshPreparedOwnership();
#endif
#ifdef SRR2_OLD3DS_GPU_LISTS
    if(g_picaInFrame)picaGpuListsBeginFrame();
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
    if(g_picaInFrame)picaRenderWorkerBeginFrame();else picaRenderWorkerEndFrame();
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(g_picaInFrame) picaBufferReclaim();
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(g_picaInFrame) picaSkinBeginFrame();
#endif

    InvalidateUniformCache();

    immVertexUsed = 0;
#ifdef SRR2_OLD3DS_SKIN_RING
    skinVertexUsed = 0;
#endif
    immIndexUsed  = 0;

    hwViewport[0] = 0;
    hwViewport[1] = 0;
    hwViewport[2] = PICA_SCREEN_HEIGHT;
    hwViewport[3] = PICA_SCREEN_WIDTH;

    if(s_picaResumed && !display->HasReset())
    {
        s_picaResumed = false;
        C3D_BindProgram(&shaderProgram);
        C3D_SetAttrInfo(&attrInfo);
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
        wheelPaletteLayout = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout = false;
#endif
        C3D_DepthMap(true, -1.0f, 0.0f);
        C3D_FragOpMode(GPU_FRAGOPMODE_GL);
        pddiBaseShader::ClearCurrentShader();
        SyncState(0xffffffff);
        SetupHardwareProjection();
        ApplyFogState();
    }

    if(display->HasReset())
    {
        s_picaResumed = false;
        contextID++;

        C3D_BindProgram(&shaderProgram);
        C3D_SetAttrInfo(&attrInfo);
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
        wheelPaletteLayout = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout = false;
#endif
        C3D_DepthMap(true, -1.0f, 0.0f);
        C3D_FragOpMode(GPU_FRAGOPMODE_GL);
        pddiBaseShader::ClearCurrentShader();
        SyncState(0xffffffff);
        SetupHardwareProjection();
        ApplyFogState();
    }
}

void picaContext::EndFrame()
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!preparedFrame || !preparedFrame->Recording() || (g_picaPreparedExecution!=7 && g_picaPreparedExecution!=10))
#endif
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD_3DS
    EndWorld();
#endif
    pddiBaseContext::EndFrame();

    FlushStreams();
#ifdef SRR2_OLD3DS_RENDER_WORKER
    picaRenderWorkerEndFrame();
#endif
    display->EndFrame();

#ifdef SRR2_OLD3DS_QUEUE_METRICS
    g_picaQueueMetrics.commandWords += (u64)(C3D_GetCmdBufUsage() * (640u * 1024u / 4u));
    picaQueueCapture();
#endif
}

void picaContext::Clear(unsigned bufferMask)
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // Clear below is an ordered draw quad, not an immediate target-memory clear.
    // Its state and ring-backed geometry already use the owned capture path.
    if(!preparedFrame || !preparedFrame->Recording())
#endif
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
#endif
    pddiBaseContext::Clear(bufferMask);

    PushState((pddiStateMask)(PDDI_STATE_RENDER | PDDI_STATE_VIEW));
    SetCamera(0.0f, 1.0f, 1.0f, 1.0f);
    SetProjectionMode(PDDI_PROJECTION_DEVICE);
    EnableZBuffer((bufferMask & PDDI_BUFFER_DEPTH) != 0);
    SetZCompare(PDDI_COMPARE_ALWAYS);
    SetZWrite((bufferMask & PDDI_BUFFER_DEPTH) != 0);
    SetCullMode(PDDI_CULL_NONE);

    PushIdentityMatrix(PDDI_MATRIX_MODELVIEW);

    pddiVector a, b, c, d;
    const float w = (float)display->GetWidth();
    const float h = (float)display->GetHeight();
    const float z = state.viewState->clearDepth;
    a.Set(0.0f, 0.0f, z);
    b.Set(w,    0.0f, z);
    c.Set(0.0f, h,    z);
    d.Set(w,    h,    z);

    pddiColour clearColour = state.viewState->clearColour;

    pddiPrimStream* stream = BeginPrims(
        (bufferMask & PDDI_BUFFER_COLOUR) ? defaultShader : clearShader,
        PDDI_PRIM_TRISTRIP, PDDI_V_C, 4);
    if(stream)
    {
        stream->Vertex(&a, clearColour);
        stream->Vertex(&b, clearColour);
        stream->Vertex(&c, clearColour);
        stream->Vertex(&d, clearColour);
        EndPrims(stream);
    }

    PopMatrix(PDDI_MATRIX_MODELVIEW);
    PopState((pddiStateMask)(PDDI_STATE_RENDER | PDDI_STATE_VIEW));
}

void picaContext::ApplyProjection(pddiMatrix* m)
{
    float t[4][4];
    for(int i = 0; i < 4; i++)
    {
        t[i][0] = m->Row4(i).x;
        t[i][1] = m->Row4(i).y;
        t[i][2] = m->Row4(i).z;
        t[i][3] = m->Row4(i).w;
    }

    if(state.viewState->projectionMode == PDDI_PROJECTION_PERSPECTIVE)
    {
        for(int i = 0; i < 4; i++)
            t[i][2] = -t[i][2];
    }
    else
    {
        for(int i = 0; i < 4; i++)
            t[i][2] *= -0.5f;
        t[3][2] -= 0.5f;
    }

    if(bottomPass)
    {
        float wl = 0.0f, wt = 0.0f, wr = 1.0f, wb = 1.0f;
        if(state.viewState->projectionMode != PDDI_PROJECTION_DEVICE)
        {
            wl = state.viewState->viewWindow.left;
            wt = state.viewState->viewWindow.top;
            wr = state.viewState->viewWindow.right;
            wb = state.viewState->viewWindow.bottom;
        }

        const float su = (bottomDst[2] - bottomDst[0]) / (bottomSrc[2] - bottomSrc[0]);
        const float sv = (bottomDst[3] - bottomDst[1]) / (bottomSrc[3] - bottomSrc[1]);

        const float A = (wr - wl) * su;
        const float B = 2.0f * (bottomDst[0] + (wl + 0.5f * (wr - wl) - bottomSrc[0]) * su) - 1.0f;
        const float C = (wb - wt) * sv;
        const float E = 1.0f - 2.0f * (bottomDst[1] + (wt + 0.5f * (wb - wt) - bottomSrc[1]) * sv);

        for(int i = 0; i < 4; i++)
        {
            t[i][0] = A * t[i][0] + B * t[i][3];
            t[i][1] = C * t[i][1] + E * t[i][3];
        }
    }

    for(int i = 0; i < 4; i++)
    {
        const float x = t[i][0];
        t[i][0] = t[i][1];
        t[i][1] = -x;
    }

    for(int row = 0; row < 4; row++)
    {
        hwProjection.r[row].x = t[0][row];
        hwProjection.r[row].y = t[1][row];
        hwProjection.r[row].z = t[2][row];
        hwProjection.r[row].w = t[3][row];
    }

    projUploaded = false;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    ++preparedProjectionEpoch;
#endif
}

void picaContext::SetupViewport(void)
{
    if(bottomPass)
    {
        wantViewport[0] = 0;
        wantViewport[1] = 0;
        wantViewport[2] = PICA_BOTTOM_HEIGHT;
        wantViewport[3] = PICA_BOTTOM_WIDTH;
        ApplyViewport();
        return;
    }

    const float fw = (float)display->GetWidth();
    const float fh = (float)display->GetHeight();

    int left, top, width, height;

    if(state.viewState->projectionMode == PDDI_PROJECTION_DEVICE)
    {
        left   = 0;
        top    = 0;
        width  = (int)fw;
        height = (int)fh;
    }
    else
    {
        left   = (int)(state.viewState->viewWindow.left * fw);
        top    = (int)(state.viewState->viewWindow.top * fh);
        width  = (int)((state.viewState->viewWindow.right - state.viewState->viewWindow.left) * fw);
        height = (int)((state.viewState->viewWindow.bottom - state.viewState->viewWindow.top) * fh);
    }

    if(width <= 0)  width = 1;
    if(height <= 0) height = 1;

    const int hwX = (int)fh - (top + height);
    const int hwY = (int)fw - (left + width);

    wantViewport[0] = hwX < 0 ? 0 : hwX;
    wantViewport[1] = hwY < 0 ? 0 : hwY;
    wantViewport[2] = height;
    wantViewport[3] = width;
#ifdef SRR2_OLD_3DS
    if(display->IsWorldTarget())
    {
        wantViewport[0] = wantViewport[0] * display->WorldHeight() / PICA_SCREEN_HEIGHT;
        wantViewport[1] = wantViewport[1] * display->WorldWidth() / PICA_SCREEN_WIDTH;
        wantViewport[2] = wantViewport[2] * display->WorldHeight() / PICA_SCREEN_HEIGHT;
        wantViewport[3] = wantViewport[3] * display->WorldWidth() / PICA_SCREEN_WIDTH;
    }
#endif
    ApplyViewport();
}

void picaContext::ApplyViewport(void)
{
    O3_MAIN_SCOPE(Viewport);
    if((hwViewport[0] == wantViewport[0]) && (hwViewport[1] == wantViewport[1]) &&
       (hwViewport[2] == wantViewport[2]) && (hwViewport[3] == wantViewport[3]))
    {
        return;
    }

    hwViewport[0] = wantViewport[0];
    hwViewport[1] = wantViewport[1];
    hwViewport[2] = wantViewport[2];
    hwViewport[3] = wantViewport[3];

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(PreparedBatching() && QueuePreparedState({PICA_PREPARED_VIEWPORT,(u32)hwViewport[0],(u32)hwViewport[1],(u32)hwViewport[2],(u32)hwViewport[3],0,0}))return;
    FlushPreparedExecution(true);
#endif
    C3D_SetViewport((u32)hwViewport[0], (u32)hwViewport[1],
                    (u32)hwViewport[2], (u32)hwViewport[3]);
}

void picaContext::SetupHardwareProjection(void)
{
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
#endif
    pddiMatrix projection;

    switch(state.viewState->projectionMode)
    {
        case PDDI_PROJECTION_DEVICE:
            projection.Identity();
            projection.SetOrthographic(0.0f, (float)display->GetWidth(),
                                       (float)display->GetHeight(), 0.0f,
                                       state.viewState->camera.nearPlane,
                                       state.viewState->camera.farPlane);
            break;

        case PDDI_PROJECTION_ORTHOGRAPHIC:
            projection.Identity();
            projection.SetOrthographic(-0.5f, 0.5f,
                                       -((1.0f / state.viewState->camera.aspect) / 2.0f),
                                        ((1.0f / state.viewState->camera.aspect) / 2.0f),
                                       state.viewState->camera.nearPlane,
                                       state.viewState->camera.farPlane);
            break;

        case PDDI_PROJECTION_PERSPECTIVE:
            projection.Identity();
            projection.SetPerspective(state.viewState->camera.fov,
                                      state.viewState->camera.aspect,
                                      state.viewState->camera.nearPlane,
                                      state.viewState->camera.farPlane);
            break;

        default:
            PDDIASSERTMSG(0, "Bad projection mode", "");
            projection.Identity();
            break;
    }

#ifdef SRR2_OLD3DS_WHEEL_LOD
    // Native top-screen pixel scale, before the PICA landscape rotation.
    wheelLodPixelScale = state.viewState->projectionMode==PDDI_PROJECTION_PERSPECTIVE ?
        fmaxf(fabsf(projection.Row4(0).x)*200.0f,fabsf(projection.Row4(1).y)*120.0f) : 0.0f;
#endif
    stereoOn = (state.viewState->projectionMode == PDDI_PROJECTION_PERSPECTIVE) &&
               (state.viewState->camera.fov < 1.5f);

    ApplyProjection(&projection);
    SetupViewport();
}

void picaContext::LoadHardwareMatrix(pddiMatrixType id)
{
}

void picaContext::SetScissor(pddiRect* rect)
{
    pddiBaseContext::SetScissor(rect);

    int w = display->GetWidth();
    int h = display->GetHeight();

    pddiRect* r = &state.viewState->scissor;

    pddiRect bottomRect;
    if(bottomPass)
    {
        float u0, v0, u1, v1;
        BottomMapPoint((float)r->left / w,  (float)r->top / h,    &u0, &v0);
        BottomMapPoint((float)r->right / w, (float)r->bottom / h, &u1, &v1);
        w = PICA_BOTTOM_WIDTH;
        h = PICA_BOTTOM_HEIGHT;
        bottomRect.left   = (int)(u0 * w);
        bottomRect.top    = (int)(v0 * h);
        bottomRect.right  = (int)(u1 * w + 0.5f);
        bottomRect.bottom = (int)(v1 * h + 0.5f);
        if(bottomRect.left < 0) bottomRect.left = 0;
        if(bottomRect.top < 0) bottomRect.top = 0;
        if(bottomRect.right > w) bottomRect.right = w;
        if(bottomRect.bottom > h) bottomRect.bottom = h;
        r = &bottomRect;
    }

    if((r->left <= 0) && (r->top <= 0) && (r->right >= w) && (r->bottom >= h))
    {
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        SubmitPreparedScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
#else
        C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
#endif
        return;
    }

#ifdef SRR2_OLD_3DS
    if(display->IsWorldTarget() && !bottomPass)
    {
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        SubmitPreparedScissor(GPU_SCISSOR_NORMAL,
#else
        C3D_SetScissor(GPU_SCISSOR_NORMAL,
#endif
            (h - r->bottom) * display->WorldHeight() / h,
            (w - r->right) * display->WorldWidth() / w,
            (h - r->top) * display->WorldHeight() / h,
            (w - r->left) * display->WorldWidth() / w);
        return;
    }
#endif
    #ifdef SRR2_OLD3DS_PREPARED_BACKEND
        SubmitPreparedScissor(GPU_SCISSOR_NORMAL,
#else
        C3D_SetScissor(GPU_SCISSOR_NORMAL,
#endif
                   (u32)(h - r->bottom), (u32)(w - r->right),
                   (u32)(h - r->top),    (u32)(w - r->left));
}

void picaContext::FlushStreams()
{
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_STREAM_FLUSH);
#endif
    if(immVertexUsed) GSPGPU_FlushDataCache(immVertices, immVertexUsed);
    if(immIndexUsed) GSPGPU_FlushDataCache(immIndices, immIndexUsed);
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertexUsed)
        GSPGPU_FlushDataCache(immVertexRing[(immRing + 1) % PICA_IMM_BUFFERS], skinVertexUsed);
#endif
}

#ifdef SRR2_OLD3DS_SKIN_RING
unsigned char* picaContext::AllocSkinVertices(unsigned count)
{
    // Blocking FrameBegin retires the preceding frame before either ring resets.
    // Use the otherwise idle second allocation, leaving the complete immediate
    // effects budget available. Overflow falls back to the persistent CPU path.
    unsigned char* ring = immVertexRing[(immRing + 1) % PICA_IMM_BUFFERS];
    if(!ring || count > (PICA_IMM_VERTEX_BYTES - skinVertexUsed) / PICA_VERTEX_STRIDE)
        return NULL;
    unsigned char* result = ring + skinVertexUsed;
    skinVertexUsed += count * PICA_VERTEX_STRIDE;
    return result;
}
#endif

unsigned char* picaContext::AllocImmVertices(unsigned count)
{
    const unsigned bytes = count * PICA_VERTEX_STRIDE;
    if(!immVertices || bytes > PICA_IMM_VERTEX_BYTES - immVertexUsed)
    {
        return NULL;
    }

    unsigned char* p = immVertices + immVertexUsed;
    immVertexUsed += bytes;
    return p;
}

unsigned short* picaContext::AllocImmIndices(unsigned count)
{
    const unsigned bytes = count * sizeof(unsigned short);
    if((immIndexUsed + bytes) > PICA_IMM_INDEX_BYTES)
    {
        return NULL;
    }

    unsigned short* p = immIndices + (immIndexUsed / sizeof(unsigned short));
    immIndexUsed += bytes;
    return p;
}

class picaPrimStream : public pddiPrimStream
{
public:
    pddiPrimBufferStream* stream;

    void Coord(float x, float y, float z)            { stream->Coord(x, y, z); }
    void Normal(float x, float y, float z)           { stream->Normal(x, y, z); }
    void Colour(pddiColour colour, int channel = 0)  { stream->Colour(colour, channel); }
    void UV(float u, float v, int channel = 0)       { stream->UV(u, v, channel); }
    void Specular(pddiColour colour)                 { stream->Specular(colour); }

    void Vertex(pddiVector* v, pddiColour c)                      { stream->Vertex(v, c); }
    void Vertex(pddiVector* v, pddiVector* n)                     { stream->Vertex(v, n); }
    void Vertex(pddiVector* v, pddiVector2* uv)                   { stream->Vertex(v, uv); }
    void Vertex(pddiVector* v, pddiColour c, pddiVector2* uv)     { stream->Vertex(v, c, uv); }
    void Vertex(pddiVector* v, pddiVector* n, pddiVector2* uv)    { stream->Vertex(v, n, uv); }
} thePrimStream;

static picaPrimBuffer* theImmBuffer = NULL;

pddiPrimStream* picaContext::BeginPrims(pddiShader* mat, pddiPrimType primType, unsigned vertexType, int vertexCount, unsigned pass)
{
    if(!mat)
        mat = defaultShader;

    if(vertexCount <= 0)
        vertexCount = PICA_IMM_DEFAULT_VERTICES;

    pddiBaseContext::BeginPrims(mat, primType, vertexType, vertexCount);

    if(theImmBuffer == NULL)
    {
        theImmBuffer = new picaPrimBuffer(this, primType, vertexType, 0, 0);
        theImmBuffer->AddRef();
    }

    unsigned char* verts = AllocImmVertices((unsigned)vertexCount);
    if(verts == NULL)
    {
        return NULL;
    }

    theImmBuffer->SetFormat(primType, vertexType);
    theImmBuffer->SetTransient(verts, NULL, (unsigned)vertexCount);

    thePrimStream.stream = theImmBuffer->Lock();

#ifdef SRR2_OLD3DS_OCCLUSION
    if(typeid(*mat)!=typeid(picaMat))picaVisibilityForeignState();
#endif
    pddiBaseShader* material = (pddiBaseShader*)mat;
    ADD_STAT(PDDI_STAT_MATERIAL_OPS, !material->IsCurrent());
    {
#ifdef SRR2_OLD_3DS
        Old3dsOperationScope timing(O3_MATERIAL); O3_RECUR(Material);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        ApplyDrawMaterial(material);
#else
        material->SetMaterial();
#endif
    }

    return &thePrimStream;
}

#ifdef SRR2_OLD3DS_QUAD_STREAM
#include <pddi/pica/quadstream.hpp>
#include <typeinfo>
pddiPrimStream* picaContext::BeginQuadStream(pddiShader* shader,unsigned count)
{
    // Preflight both existing, GPU-fenced rings before changing immediate state.
    // A failed reservation leaves the caller's original six-vertex path intact.
    if(!g_picaInFrame || quadIndices || !count || count>picaQuad::MaxQuads ||
       !immVertices || !immIndices ||
       immVertexUsed>PICA_IMM_VERTEX_BYTES || immIndexUsed>PICA_IMM_INDEX_BYTES ||
       count*4*PICA_VERTEX_STRIDE>PICA_IMM_VERTEX_BYTES-immVertexUsed ||
       count*6*sizeof(unsigned short)>PICA_IMM_INDEX_BYTES-immIndexUsed)
    { ++quadFallbacks;return nullptr; }
    const unsigned previousIndexUsed=immIndexUsed;
    unsigned short* indices=AllocImmIndices(count*6);
    pddiPrimStream* stream=BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_CT,int(count*4));
    if(!stream) {immIndexUsed=previousIndexUsed;++quadFallbacks;return nullptr;}
    quadIndices=indices;quadCapacity=count;quadWritten=0;
    return stream;
}
bool picaContext::EmitQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,pddiColour colour,unsigned topology)
{
    if(!quadIndices || stream!=&thePrimStream || quadWritten>=quadCapacity ||
       !picaQuad::Indices(quadIndices+quadWritten*6,quadWritten,topology)) return false;
    // No source pointer survives this call. Each original triangle keeps its order.
    theImmBuffer->AppendQuad(positions,coordinates,colour);
    ++quadWritten;++quadTotal;
    return true;
}
bool picaContext::EmitColourQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,const pddiColour* colours,unsigned topology)
{
    if(!quadIndices || stream!=&thePrimStream || quadWritten>=quadCapacity ||
       !picaQuad::Indices(quadIndices+quadWritten*6,quadWritten,topology)) return false;
    theImmBuffer->AppendColourQuad(positions,coordinates,colours);
    ++quadWritten;++quadTotal;++quadColourTotal;
    return true;
}
extern "C" bool picaEmitColourQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,const pddiColour* colours,picaQuad::Topology topology)
{
    return s_picaContext && s_picaContext->EmitColourQuad(stream,positions,coordinates,colours,unsigned(topology));
}
extern "C" bool picaParticleMotionShaderSafe(pddiShader* shader)
{return shader && typeid(*shader)==typeid(picaMat);}
extern "C" pddiPrimStream* picaBeginQuadStream(pddiShader* shader,unsigned count)
{
    return s_picaContext ? s_picaContext->BeginQuadStream(shader,count) : nullptr;
}
extern "C" bool picaEmitQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,pddiColour colour,picaQuad::Topology topology)
{
    return s_picaContext && s_picaContext->EmitQuad(stream,positions,coordinates,colour,unsigned(topology));
}
#endif

#ifdef SRR2_OLD3DS_SHADOW_MESH
#include <pddi/pica/blobtopology.hpp>
bool picaContext::DrawBlobShadow(pddiShader* shader, const pddiVector* vertices,
                                 pddiColour inside, pddiColour outside)
{
    // Reserve before BeginPrims changes immediate state. The caller can use the
    // original path if either ring is full. No persistent geometry is mutated
    // while a previous GPU submission may still reference it.
    if(immVertexUsed + picaBlob::VertexCount * PICA_VERTEX_STRIDE > PICA_IMM_VERTEX_BYTES ||
       immIndexUsed + picaBlob::IndexCount * sizeof(unsigned short) > PICA_IMM_INDEX_BYTES)
        return false;
    unsigned short* indices = AllocImmIndices(picaBlob::IndexCount);
    picaBlob::Indices(indices);
    pddiPrimStream* stream = BeginPrims(shader, PDDI_PRIM_TRIANGLES, PDDI_V_C, picaBlob::VertexCount);
    if(!stream) return false;
    for(unsigned i = 0; i < picaBlob::VertexCount; ++i)
        stream->Vertex(const_cast<pddiVector*>(vertices + i), i == 0 || (i & 1) ? inside : outside);
    theImmBuffer->SetTransientIndices(indices, picaBlob::IndexCount);
    EndPrims(stream);
    return true;
}
#endif

#ifdef SRR2_OLD3DS_CHARACTER_SHADOW
#include <pddi/pica/blobtopology.hpp>
bool picaContext::DrawCharacterShadow(pddiShader* shader, const pddiVector* vertices,
                                      pddiColour inside, pddiColour outside)
{
    // Use the existing frame-owned rings; each shadow retains its own GPU data.
    if(immVertexUsed + picaCharacterBlob::VertexCount * PICA_VERTEX_STRIDE > PICA_IMM_VERTEX_BYTES ||
       immIndexUsed + picaCharacterBlob::IndexCount * sizeof(unsigned short) > PICA_IMM_INDEX_BYTES)
        return false;
    unsigned short* indices = AllocImmIndices(picaCharacterBlob::IndexCount);
    picaCharacterBlob::Indices(indices);
    pddiPrimStream* stream = BeginPrims(shader, PDDI_PRIM_TRIANGLES, PDDI_V_C, picaCharacterBlob::VertexCount);
    if(!stream) return false;
    for(unsigned i = 0; i < picaCharacterBlob::VertexCount; ++i)
        stream->Vertex(const_cast<pddiVector*>(vertices + i), i == 0 || (i & 1) ? inside : outside);
    theImmBuffer->SetTransientIndices(indices, picaCharacterBlob::IndexCount);
    EndPrims(stream);
    return true;
}
#endif

void picaContext::EndPrims(pddiPrimStream* stream)
{
    pddiBaseContext::EndPrims(stream);

    if(theImmBuffer == NULL)
        return;

#ifdef SRR2_OLD3DS_QUAD_STREAM
    if(quadIndices)
    {
        theImmBuffer->SetQuadIndices(quadIndices,quadWritten*6);
        quadIndices=nullptr;quadCapacity=quadWritten=0;
    }
#endif
    theImmBuffer->Unlock(thePrimStream.stream);
    theImmBuffer->Display();
}

#ifdef SRR2_OLD3DS_CLIP_BOUNDS
void picaContext::DumpClipBoundsStats()
{
#ifdef SRR2_OLD3DS_STATIC_BATCH
    printf("STATIC_BATCH built=%u hits=%u avoided_draw_requests=%u misses=%u failures=%u live=%u peak=%u budget=%u source_cap=%u\n",
        staticBatchBuilt,staticBatchHits,staticBatchAvoided,staticBatchMisses,staticBatchFailures,
        staticBatchBytes,staticBatchPeak,picaStaticBatch::Budget,picaStaticBatch::Slots*picaStaticBatch::GroupLimit);
#endif
    printf("CLIP_BOUNDS tested=%u rejected=%u\n", clipTested, clipRejected);
#ifdef SRR2_OLD3DS_QUAD_STREAM
    printf("QUAD_STREAM quads=%u avoided_vertex_bytes=%llu index_bytes=%llu fallbacks=%u coloured=%u\n",
        quadTotal,(unsigned long long)quadTotal*2*PICA_VERTEX_STRIDE,
        (unsigned long long)quadTotal*6*sizeof(unsigned short),quadFallbacks,quadColourTotal);
#endif
#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
    printf("LIGHT_CAPTURE setup=%u disabled=%u exact_hits=%u computed=%u cache_bytes=%u\n",
        lightSetupCalls,lightDisabledSkips,lightCaptureHits,lightCaptureComputations,(unsigned)sizeof(lightCaptureCache));
#endif
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    printf("DIRECTIONAL_LIGHT prepared_draws=%u general_draws=%u\n",directionalDraws,generalLightDraws);
#endif
#ifdef SRR2_OLD3DS_SKIN_CLIP
    printf("SKIN_CLIP tested=%u rejected=%u metadata_bytes=%u budget=%u\n",skinClipTested,skinClipRejected,skinClipBytes,picaSkinClip::Budget);
    printf("SKIN_CLIP_COST calls=%u samples=%u sampled_ms=%.6f\n",skinClipCalls,skinClipSamples,skinClipTicks*1000.0/SYSCLOCK_ARM11);
#endif
}
extern "C" void picaDumpClipBoundsStats()
{
    if(s_picaContext) s_picaContext->DumpClipBoundsStats();
}
bool picaContext::CullBounds(const float low[3], const float high[3])
{
    if(!g_picaInFrame || bottomPass || display->StereoActive() ||
       state.viewState->projectionMode != PDDI_PROJECTION_PERSPECTIVE) return false;
    const pddiMatrix* mv=state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
#ifdef SRR2_OLD3DS_CLIP_PLANE_PREP
    const bool projectionChanged=!clipPrepared || memcmp(&clipProjection,&hwProjection,sizeof(clipProjection));
    if(projectionChanged || memcmp(&clipModelView,mv,sizeof(clipModelView)))
    {
        if(projectionChanged)
        {
            float projection[4][4];
            for(unsigned row=0;row<4;++row)
            {
                projection[row][0]=hwProjection.r[row].x;projection[row][1]=hwProjection.r[row].y;
                projection[row][2]=hwProjection.r[row].z;projection[row][3]=hwProjection.r[row].w;
            }
            clipProjectionPlanes=picaClipBounds::PrepareProjection(projection);
            clipProjection=hwProjection;
        }
        float modelView[4][4];
        for(unsigned row=0;row<4;++row) for(unsigned col=0;col<4;++col)
            modelView[row][col]=mv->Row4(col)[row];
        clipModelView=*mv;clipPrepared=true;
        clipTransform=picaClipBounds::Prepare(modelView,clipProjectionPlanes);
    }
#else
    if(!clipPrepared || memcmp(&clipModelView,mv,sizeof(clipModelView)) ||
       memcmp(&clipProjection,&hwProjection,sizeof(clipProjection)))
    {
        clipModelView=*mv;clipProjection=hwProjection;clipPrepared=true;
        float modelView[4][4], projection[4][4];
        for(unsigned row=0;row<4;++row) {
            for(unsigned col=0;col<4;++col) modelView[row][col]=mv->Row4(col)[row];
            projection[row][0]=hwProjection.r[row].x;projection[row][1]=hwProjection.r[row].y;
            projection[row][2]=hwProjection.r[row].z;projection[row][3]=hwProjection.r[row].w;
        }
        clipTransform=picaClipBounds::Prepare(modelView,projection);
    }
#endif
    ++clipTested;
    const bool rejected=picaClipBounds::Reject(clipTransform,low,high)!=0;
    clipRejected+=rejected;
    return rejected;
}
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
bool picaContext::PrepareParticleClip(picaClipBounds::Transform& output) const
{
    output={};
    if(!g_picaInFrame || bottomPass || display->StereoActive() ||
       state.viewState->projectionMode!=PDDI_PROJECTION_PERSPECTIVE) return false;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinProgramBound) return false;
#endif
    const pddiMatrix* mv=state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    float modelView[4][4],projection[4][4];
    for(unsigned row=0;row<4;++row)
    {
        for(unsigned col=0;col<4;++col) modelView[row][col]=mv->Row4(col)[row];
        projection[row][0]=hwProjection.r[row].x;projection[row][1]=hwProjection.r[row].y;
        projection[row][2]=hwProjection.r[row].z;projection[row][3]=hwProjection.r[row].w;
    }
    output=picaClipBounds::Prepare(modelView,projection);
    return output.valid;
}
extern "C" bool picaPrepareParticleClip(picaClipBounds::Transform* output)
{
    return output && s_picaContext && s_picaContext->PrepareParticleClip(*output);
}
#endif
void picaPrimBuffer::UpdateClipBounds()
{
    clipBoundsValid=false;
    if(!noNormalAllowed || transient || !buffer || !allocated) return;
    const unsigned stride=noNormalPacked ? picaNoNormal::Stride : PICA_VERTEX_STRIDE;
    for(unsigned i=0;i<allocated;++i) {
        const float* p=reinterpret_cast<const float*>(buffer+i*stride);
        for(unsigned axis=0;axis<3;++axis) {
            if(!std::isfinite(p[axis]) || std::fabs(p[axis])>1000000.0f) return;
            if(!i || p[axis]<clipLow[axis]) clipLow[axis]=p[axis];
            if(!i || p[axis]>clipHigh[axis]) clipHigh[axis]=p[axis];
        }
    }
    clipBoundsValid=true;
}
bool picaPrimBuffer::CullNative()
{
    if(transient || !buffer || !PrimIsDrawable(primType)) return false;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinDrawing) return false;
#endif
    return clipBoundsValid && context->CullBounds(clipLow,clipHigh);
}
#endif

OLD3DS_HOT_PATH
void picaContext::DrawPrimBuffer(pddiShader* mat, pddiPrimBuffer* buffer)
{
    O3_POOL(Recipe,Backend);
    O3_MAIN_SCOPE(DrawPrim); O3_RECUR(Backend);
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_BACKEND);
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    // Entity/geometry callbacks and all simulation still execute. A rejected
    // native draw cannot affect pixels, so its material setup is unnecessary.
    if(static_cast<picaPrimBuffer*>(buffer)->CullNative()) return;
#endif

#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    if(g_o3WorldDepth && g_o3Mode==O3_SUPPRESS) {
        // Keep all upstream geometry/pose/display callbacks and immediate writes.
        // Persistent draws stop before material traversal; streams stop in Display.
        O3ExperimentDraw(static_cast<picaPrimBuffer*>(buffer)->DiagnosticVertexCount()); return;
    }
#endif
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    picaQueueBackendTimer backendTimer;
#endif
    if(!mat)
        mat = defaultShader;

#ifdef SRR2_OLD3DS_OCCLUSION
    const bool visibilityKnown=typeid(*mat)==typeid(picaMat);
    if(!visibilityKnown)picaVisibilityForeignState();
    const bool visibilityHidden=visibilityKnown && VisibilityQuery(static_cast<picaPrimBuffer*>(buffer));
    if(visibilityHidden && g_picaVisibilityMode==1)return;
    const u64 visibilityStart=visibilityHidden && g_picaVisibilityMode==2?svcGetSystemTick():0;
    const unsigned visibilityBeforeDraws=g_old3dsDrawCalls;
#endif
    pddiBaseShader* material = (pddiBaseShader*)mat;
    ADD_STAT(PDDI_STAT_MATERIAL_OPS, !material->IsCurrent());
    {
#ifdef SRR2_OLD_3DS
        Old3dsOperationScope timing(O3_MATERIAL); O3_RECUR(Material);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        ApplyDrawMaterial(material);
#else
        material->SetMaterial();
#endif
    }

    ((picaPrimBuffer*)buffer)->Display();
#ifdef SRR2_OLD3DS_OCCLUSION
    if(visibilityStart)picaVisibilityObserved(svcGetSystemTick()-visibilityStart);
    if(visibilityKnown && g_old3dsDrawCalls!=visibilityBeforeDraws)VisibilityRaster(static_cast<picaPrimBuffer*>(buffer));
#endif
}

#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
bool picaContext::CanQueue(pddiShader* material, pddiPrimBuffer* buffer)
{
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    ++g_picaQueueMetrics.candidates;
#endif
    return g_picaInFrame && !bottomPass && material && buffer &&
        state.renderState->zEnabled && state.renderState->zWrite &&
        !state.stencilState->enabled &&
        (state.renderState->zCompare == PDDI_COMPARE_LESS ||
         state.renderState->zCompare == PDDI_COMPARE_LESSEQUAL) &&
        static_cast<picaMat*>(material)->QueueOpaque() &&
        static_cast<picaPrimBuffer*>(buffer)->QueuePersistent();
}

bool picaPrimBuffer::QueuePersistent()
{
    if(transient || !buffer || !PrimQueueSupported()) return false;
    if(indices)
    {
        if(elemState < 0) elemState = IndicesInRange() ? 1 : 0;
        if(elemState != 1) return false;
    }
    return true;
}

#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
bool picaPrimBuffer::QueueBounds(float* low, float* high)
{
    if(!queueBoundsValid)
    {
        const unsigned count = allocated;
        if(!buffer || !count || transient) return false;
        for(unsigned axis = 0; axis < 3; ++axis)
            queueLow[axis] = queueHigh[axis] = ((const float*)buffer)[axis];
        for(unsigned i = 0; i < count; ++i)
        {
            const float* position = (const float*)(buffer + i *
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
                (noNormalPacked ? picaNoNormal::Stride : PICA_VERTEX_STRIDE)
#else
                PICA_VERTEX_STRIDE
#endif
                );
            for(unsigned axis = 0; axis < 3; ++axis)
            {
                if(!isfinite(position[axis])) return false;
                if(position[axis] < queueLow[axis]) queueLow[axis] = position[axis];
                if(position[axis] > queueHigh[axis]) queueHigh[axis] = position[axis];
            }
        }
        queueBoundsValid = true;
    }
    memcpy(low, queueLow, sizeof(queueLow));
    memcpy(high, queueHigh, sizeof(queueHigh));
    return true;
}

#endif

void picaContext::SubmitMaterialQueue(Submission* entries, unsigned count)
{
    if(!count) return;
    bool opportunity = false;
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    const bool sortEnabled = true;
#else
    const bool sortEnabled = false;
#endif
    (void)sortEnabled;
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    const bool sample = ((++g_picaQueueMetrics.queueSamples & 31u) == 1u);
    g_picaQueueMetrics.queued += count;
    if(count > g_picaQueueMetrics.peakEntries) g_picaQueueMetrics.peakEntries = count;
    for(unsigned i = 0; i < count; ++i)
        g_picaQueueMetrics.originalChanges += i == 0 || entries[i].material != entries[i-1].material;
    {
    picaQueueTimer timer(g_picaQueueMetrics.collectTicks, sample && sortEnabled);
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    // Only build coverage when there is an actual nonconsecutive material match.
    opportunity = picaQueue::HasMaterialOpportunity(entries, count);
    if(opportunity)
    {
        const pddiMatrix* mv = state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
        float modelView[4][4], projection[4][4];
        for(unsigned a = 0; a < 4; ++a)
            for(unsigned b = 0; b < 4; ++b)
            {
                modelView[a][b] = mv->Row4(b)[a];
            }
        // C3D_FVec stores components in w,z,y,x order, not x,y,z,w.
        for(unsigned a = 0; a < 4; ++a)
        {
            projection[a][0] = hwProjection.r[a].x;
            projection[a][1] = hwProjection.r[a].y;
            projection[a][2] = hwProjection.r[a].z;
            projection[a][3] = hwProjection.r[a].w;
        }
        const picaQueue::Projection coverage = picaQueue::PrepareProjection(modelView, projection);
        for(unsigned i = 0; i < count; ++i)
        {
            float low[3], high[3];
            if(entries[i].buffer->QueueBounds(low, high))
                entries[i].rect = picaQueue::ProjectBounds(low, high, coverage);
        }
    }
#endif
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    }
    unsigned moved;
    {
        picaQueueTimer timer(g_picaQueueMetrics.sortTicks, sample && sortEnabled);
        moved = opportunity ? picaQueue::Schedule(entries, count) : 0;
    }
    g_picaQueueMetrics.moved += moved;
    picaQueueTimer submitTimer(g_picaQueueMetrics.submitTicks, sample);
#else
    if(opportunity) picaQueue::Schedule(entries, count);
#endif
    pddiShader* previous = NULL;
    for(unsigned i = 0; i < count; ++i)
        SubmitQueuedOne(entries[i].material, entries[i].buffer, previous);
}

void picaContext::SubmitQueuedPrim(pddiShader* material, picaPrimBuffer* buffer)
{
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    ++g_picaQueueMetrics.queued;
    ++g_picaQueueMetrics.queueSamples;
    if(g_picaQueueMetrics.peakEntries < 1) g_picaQueueMetrics.peakEntries = 1;
    if(material != staticQueuePrevious) ++g_picaQueueMetrics.originalChanges;
#endif
    SubmitQueuedOne(material, buffer, staticQueuePrevious);
}

void picaContext::SubmitQueuedOne(pddiShader* shader, picaPrimBuffer* buffer, pddiShader*& previous)
{
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    if(shader != previous) ++g_picaQueueMetrics.submittedChanges;
#endif
    picaMat* material = static_cast<picaMat*>(shader);
    if(previous && material->CanApplyQueued(static_cast<picaMat*>(previous)))
    {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        picaQueueBackendTimer backendTimer;
#endif
        material->ApplyQueued(static_cast<picaMat*>(previous));
        buffer->Display();
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        if(shader == previous) ++g_picaQueueMetrics.materialReuses;
#endif
    }
    else DrawPrimBuffer(shader, buffer);
    previous = shader;
}
#endif

int picaContext::GetMaxLights(void)
{
    return PDDI_MAX_LIGHTS;
}

void picaContext::SetAmbientLight(pddiColour col)
{
    pddiBaseContext::SetAmbientLight(col);
}

void picaContext::InvalidateUniformCache(void)
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    ++preparedUniformEpoch;
#endif
#ifdef SRR2_OLD_3DS
    modelViewUploaded = false;
#endif
    projUploaded   = false;
    lightsUploaded = false;
    matUploaded    = false;
    miscUploaded   = false;
}

void picaContext::SetupHardwareLight(int handle)
{
    if(handle < 0 || handle >= PDDI_MAX_LIGHTS)
        return;

#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
    ++lightSetupCalls;
    // Disabled eye coordinates are not read by RebuildLightArray. EnableLight
    // always calls this hook again with the enabling call's current transform.
    if(!state.lightingState->light[handle].enabled)
    {
        ++lightDisabledSkips;
        lightsDirty=true;
        return;
    }
#endif
    CaptureEyeLight(handle);
    lightsDirty = true;
}

void picaContext::CaptureEyeLight(int handle)
{
    pddiMatrix* mv = state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    pddiLight*  light = &state.lightingState->light[handle];

#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
    static_assert(sizeof(pddiMatrix)==64 && sizeof(pddiVector)==12,"light capture key layout");
    const pddiVector* source=light->type==PDDI_LIGHT_DIRECTIONAL ? &light->worldDirection : &light->worldPosition;
    unsigned captureControl=0;
#ifdef __arm__
    __asm__ __volatile__("vmrs %0, fpscr" : "=r"(captureControl) : : "memory");
    captureControl &= 0x03c00000u;
#endif
    if(lightCaptureCache[handle].Matches(mv,source,light->type,captureControl))
    {
        ++lightCaptureHits;
        return;
    }
    ++lightCaptureComputations;
#endif
    float vx, vy, vz, type;

    if(light->type == PDDI_LIGHT_DIRECTIONAL)
    {
        const float dx = -light->worldDirection.x;
        const float dy = -light->worldDirection.y;
        const float dz = -light->worldDirection.z;

        vx = dx * mv->Row4(0).x + dy * mv->Row4(1).x + dz * mv->Row4(2).x;
        vy = dx * mv->Row4(0).y + dy * mv->Row4(1).y + dz * mv->Row4(2).y;
        vz = dx * mv->Row4(0).z + dy * mv->Row4(1).z + dz * mv->Row4(2).z;

        const float len = sqrtf(vx * vx + vy * vy + vz * vz);
        if(len > 1.0e-6f)
        {
            vx /= len;
            vy /= len;
            vz /= len;
        }
        type = 0.0f;
    }
    else
    {
        const float px = light->worldPosition.x;
        const float py = light->worldPosition.y;
        const float pz = light->worldPosition.z;

        vx = px * mv->Row4(0).x + py * mv->Row4(1).x + pz * mv->Row4(2).x + mv->Row4(3).x;
        vy = px * mv->Row4(0).y + py * mv->Row4(1).y + pz * mv->Row4(2).y + mv->Row4(3).y;
        vz = px * mv->Row4(0).z + py * mv->Row4(1).z + pz * mv->Row4(2).z + mv->Row4(3).z;

        type = 1.0f;
    }

    eyeLight[handle].x = vx;
    eyeLight[handle].y = vy;
    eyeLight[handle].z = vz;
    eyeLight[handle].w = type;
#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
    lightCaptureCache[handle].Store(mv,source,light->type,captureControl,
        std::isfinite(vx) && std::isfinite(vy) && std::isfinite(vz));
#endif
}

void picaContext::RebuildLightArray(void)
{
    O3_MAIN_SCOPE(LightRebuild);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    ++preparedLightEpoch;
#endif
    nActiveLights = 0;

    for(int i = 0; i < PDDI_MAX_LIGHTS; i++)
    {
        pddiLight* light = &state.lightingState->light[i];
        if(!light->enabled)
            continue;

        const int slot = nActiveLights;

        const float type = eyeLight[i].w;

        lightPos[slot].x = eyeLight[i].x;
        lightPos[slot].y = eyeLight[i].y;
        lightPos[slot].z = eyeLight[i].z;
        lightPos[slot].w = type;

        lightColour[slot].x = (float)light->colour.Red()   / 255.0f;
        lightColour[slot].y = (float)light->colour.Green() / 255.0f;
        lightColour[slot].z = (float)light->colour.Blue()  / 255.0f;
        lightColour[slot].w = 1.0f;

        if(type == 0.0f)
        {
            lightAtten[slot].x = 1.0f;
            lightAtten[slot].y = 0.0f;
            lightAtten[slot].z = 0.0f;
        }
        else
        {
            lightAtten[slot].x = light->attenA;
            lightAtten[slot].y = light->attenB;
            lightAtten[slot].z = light->attenC;

            if((light->attenA == 0.0f) && (light->attenB == 0.0f) && (light->attenC == 0.0f))
                lightAtten[slot].x = 1.0f;
        }
        lightAtten[slot].w = 0.0f;

        nActiveLights++;
    }

    for(int i = nActiveLights; i < PDDI_MAX_LIGHTS; i++)
    {
        lightPos[i].x = 0.0f;
        lightPos[i].y = 0.0f;
        lightPos[i].z = 1.0f;
        lightPos[i].w = 0.0f;

        lightColour[i].x = 0.0f;
        lightColour[i].y = 0.0f;
        lightColour[i].z = 0.0f;
        lightColour[i].w = 0.0f;

        lightAtten[i].x = 1.0f;
        lightAtten[i].y = 0.0f;
        lightAtten[i].z = 0.0f;
        lightAtten[i].w = 0.0f;
    }

#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    directionalOnly = nActiveLights > 0;
    for(int i=0;directionalOnly && i<nActiveLights;++i)
        directionalOnly = picaDirectionalLight::Prepare(lightPos[i],directionalPos[i]);
#endif
    lightsDirty = false;
}

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
bool picaContext::PreparedBatching() const
{return g_picaPreparedBackendEnabled && g_picaInFrame && g_picaPreparedExecution>=3 && preparedBatchReady && !preparedBatchFailed;}

void picaContext::BeginPreparedExecution()
{
    O3_MAIN_SCOPE(BeginPrepared);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(preparedFrame && preparedFrame->InFlight())FlushPreparedExecution(true);
    if(preparedFrame && preparedFrame->Recording())return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
    if(preparedWorker && preparedWorker->Active())return;
#endif
    if(!preparedExecutor)preparedExecutor=new(std::nothrow)picaPreparedExecutor;
    if(!preparedExecutor){preparedBatchFailed=true;return;}
    if(preparedExecutor->valid)return;
    picaPreparedExecutor& executor=*preparedExecutor;
    picaPreparedUniformCache& cache=executor.uniforms;
    cache.modelViewUploaded=modelViewUploaded;cache.projUploaded=projUploaded;
    cache.lightsUploaded=lightsUploaded;cache.matUploaded=matUploaded;
    cache.lastLit=lastLit;cache.miscUploaded=miscUploaded;
    if(modelViewUploaded)cache.lastModelView=lastModelView;
    if(matUploaded){cache.lastMatAmbient=lastMatAmbient;cache.lastMatDiffuse=lastMatDiffuse;cache.lastMatEmissive=lastMatEmissive;cache.lastSceneAmbient=lastSceneAmbient;}
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    cache.sparseUnlitProjection=sparseUnlitProjection;cache.lastFastUnlit=lastFastUnlit;
#endif
    cache.invalidationEpoch=preparedUniformEpoch;cache.lightEpoch=preparedLightEpoch;cache.projectionEpoch=preparedProjectionEpoch;
    executor.Begin();
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    RefreshPreparedOwnership();
    if((g_picaPreparedExecution==7 || g_picaPreparedExecution==9 || g_picaPreparedExecution==10) && !display->StereoActive() && !display->IsWorldTarget()){
        if(!preparedInputPool)preparedInputPool=new(std::nothrow)picaPreparedInputPool;
        if(preparedInputPool && !preparedFrameAttempted){preparedFrameAttempted=true;preparedFrame=picaFramePipeline::Create(preparedExecutor,preparedInputPool);}
        if(preparedFrame)preparedFrame->Begin(preparedExecutor,preparedInputPool);
    }
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
    if(g_picaPreparedExecution==4 || g_picaPreparedExecution==6)
    {
        if(!preparedInputPool)preparedInputPool=new(std::nothrow)picaPreparedInputPool;
        if(preparedInputPool && !preparedWorkerAttempted){preparedWorkerAttempted=true;preparedWorker=picaPreparedWorker::Create(preparedExecutor,preparedInputPool);}
        if(preparedWorker)preparedWorker->Begin(g_picaPreparedExecution==6);
    }
#endif
}

#ifdef SRR2_OLD3DS_FRAME_PIPELINE
__attribute__((noinline))
#endif
void picaContext::FlushPreparedExecution(bool release)
{
    O3_MAIN_SCOPE(FlushPrepared);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!g_picaPreparedOwnershipPending)return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
    picaPreparedExecutor* completedExecutor=nullptr;
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    bool restartFrame=false;
    if(preparedFrame && preparedFrame->InFlight()){
        const void* site=NativeBench::collecting?(s_picaPreparedFenceCaller?s_picaPreparedFenceCaller:__builtin_return_address(0)):nullptr;
        const u64 start=svcGetSystemTick();
        completedExecutor=preparedFrame->Join(preparedExecutor,preparedInputPool);
        const u64 cpuDone=svcGetSystemTick();
        if(preparedGpuPending && !g_picaInFrame){C3Di_RenderQueueWaitDone();preparedGpuPending=false;}
        preparedFrame->NoteJoin(site,cpuDone-start,svcGetSystemTick()-cpuDone);
    }
    else if(preparedFrame && preparedFrame->Recording()){
        if(!release && preparedFrame->Advance(preparedExecutor,preparedInputPool))return;
        const void* site=NativeBench::collecting?(s_picaPreparedFenceCaller?s_picaPreparedFenceCaller:__builtin_return_address(0)):nullptr;
        restartFrame=!release;completedExecutor=preparedFrame->Flush(preparedExecutor,preparedInputPool,site);
    }
#endif
    if(preparedWorker && preparedWorker->Active())
    {
        if(!release){preparedWorker->Publish(preparedExecutor,preparedInputPool);return;}
        completedExecutor=preparedWorker->Finish(preparedExecutor,preparedInputPool);
        preparedBackendStats.batchFlushes+=completedExecutor->flushes;
        preparedBackendStats.batchedDraws+=completedExecutor->submitted;
        preparedBackendStats.workerDraws=preparedWorker->workerDraws;preparedBackendStats.mainDraws=preparedWorker->mainDraws;
        preparedBackendStats.workerJobs=preparedWorker->jobs;
        preparedBackendStats.comparisons=preparedWorker->comparisons;preparedBackendStats.mismatches=preparedWorker->mismatches;
        preparedBackendStats.workerAvailable=1;preparedBackendStats.workerCore=preparedWorker->workerCore;
        preparedBackendStats.initialAllowance=preparedWorker->initialAllowance;preparedBackendStats.actualAllowance=preparedWorker->actualAllowance;
        preparedBackendStats.mainFp=preparedWorker->mainFp;preparedBackendStats.workerFp=preparedWorker->workerFp;
        preparedBackendStats.workerWaitTicks=preparedWorker->waitTicks;preparedBackendStats.workerCommitTicks=preparedWorker->commitTicks;
    }
    if(!completedExecutor && (!preparedExecutor || !preparedExecutor->valid || preparedExecutor->draining)){
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        RefreshPreparedOwnership();
#endif
        return;
    }
    picaPreparedExecutor& executor=completedExecutor?*completedExecutor:*preparedExecutor;
#else
    if(!preparedExecutor || !preparedExecutor->valid || preparedExecutor->draining)return;
    picaPreparedExecutor& executor=*preparedExecutor;
#endif
    const unsigned beforeFlushes=executor.flushes,beforeDraws=executor.submitted;
    const u64 executionStart=svcGetSystemTick();
    executor.Flush();
    preparedBackendStats.batchTicks+=svcGetSystemTick()-executionStart;
    preparedBackendStats.batchFlushes+=executor.flushes-beforeFlushes;
    preparedBackendStats.batchedDraws+=executor.submitted-beforeDraws;
    u32* commands;u32 commandSize,commandOffset;GPUCMD_GetBuffer(&commands,&commandSize,&commandOffset);
    if(commands){if(commandOffset>g_old3dsCmdHighWater)g_old3dsCmdHighWater=commandOffset;g_old3dsCmdSize=commandSize;}
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(commandOffset>g_picaSkinMetrics.maxCommand)g_picaSkinMetrics.maxCommand=commandOffset;
#endif
    picaPreparedUniformCache& cache=executor.uniforms;
    modelViewUploaded=cache.modelViewUploaded;projUploaded=cache.projUploaded;
    lightsUploaded=cache.lightsUploaded;matUploaded=cache.matUploaded;
    lastLit=cache.lastLit;miscUploaded=cache.miscUploaded;
    if(modelViewUploaded)lastModelView=cache.lastModelView;
    if(matUploaded){lastMatAmbient=cache.lastMatAmbient;lastMatDiffuse=cache.lastMatDiffuse;lastMatEmissive=cache.lastMatEmissive;lastSceneAmbient=cache.lastSceneAmbient;}
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    sparseUnlitProjection=cache.sparseUnlitProjection;lastFastUnlit=cache.lastFastUnlit;
    fastUnlitCalls+=cache.fastUnlitCalls;otherProjectionCalls+=cache.otherProjectionCalls;
#endif
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    directionalDraws+=cache.directionalDraws;generalLightDraws+=cache.generalLightDraws;
#endif
    g_old3dsModelViewUploads+=cache.modelViewUploads;g_old3dsModelViewReuses+=cache.modelViewReuses;
    Old3dsCount(O3_MV_UPLOAD,cache.modelViewUploads);Old3dsCount(O3_MV_REUSE,cache.modelViewReuses);
    Old3dsCount(O3_STATE_CHANGE,executor.stateChanges);Old3dsCount(O3_TEX_REQUEST,executor.textureRequests);Old3dsCount(O3_TEX_CHANGE,executor.textureChanges);
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    g_picaQueueMetrics.textures+=executor.textureChanges;
#endif
    cache.modelViewUploads=cache.modelViewReuses=cache.fastUnlitCalls=cache.otherProjectionCalls=cache.directionalDraws=cache.generalLightDraws=0;
    executor.stateChanges=executor.textureRequests=executor.textureChanges=0;
#if defined(SRR2_OLD3DS_RETAINED_MATERIAL) || defined(SRR2_OLD3DS_DEFERRED_SKIN)
    executor.ReleaseOwnedInputs();
#endif
    if(release)executor.End();
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(restartFrame)BeginPreparedExecution();
    RefreshPreparedOwnership();
#endif
}

bool picaContext::PreparedGeometryRoom()
{
    BeginPreparedExecution();
    if(!preparedExecutor)return false;
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(preparedFrame && preparedFrame->Recording()){
        if(preparedExecutor->Full())FlushPreparedExecution(false);
        return !preparedExecutor->Full();
    }
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
    if(preparedWorker && preparedWorker->Active())
    {
        if(preparedWorker->GeometryRoom(preparedExecutor->geometry))return true;
        // Drain conservative reservations before consulting actual remaining room.
        FlushPreparedExecution(true);BeginPreparedExecution();
        if(preparedWorker && preparedWorker->Active())return preparedWorker->GeometryRoom(0);
    }
#endif
    if(!preparedExecutor->HasRoomForGeometry())FlushPreparedExecution(false);
    return preparedExecutor->HasRoomForGeometry();
}

#ifdef SRR2_OLD3DS_PREPARED_THREAD
void picaContext::PreparedCommandBuffer(u32** data,u32* size,u32* offset)
{
    if(preparedWorker && preparedWorker->Active()){preparedWorker->Buffer(data,size,offset);return;}
    GPUCMD_GetBuffer(data,size,offset);
}
#endif

#ifdef SRR2_OLD3DS_FRAME_PIPELINE
void picaContext::RefreshPreparedOwnership()
{
    if(s_picaContext!=this)return;
    g_picaPreparedOwnershipPending=preparedGpuPending ||
        (preparedExecutor && preparedExecutor->valid) ||
        (preparedFrame && (preparedFrame->Recording() || preparedFrame->InFlight())) ||
        (preparedWorker && preparedWorker->Active());
}
bool picaContext::PreparedFrameInFlight()const{return preparedFrame && preparedFrame->InFlight();}
float picaFrameProcessingTime(){return s_picaContext && s_picaContext->PreparedFrameInFlight()?s_picaContext->preparedFrame->CompletedMetrics().processingMs:C3D_GetProcessingTime();}
float picaFrameDrawingTime(){return s_picaContext && s_picaContext->PreparedFrameInFlight()?s_picaContext->preparedFrame->CompletedMetrics().drawingMs:C3D_GetDrawingTime();}
void picaFramePipelineDump(FILE* file)
{
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    if(s_picaContext)s_picaContext->DumpDeferredSkin(file);
#endif
    auto* pipeline=s_picaContext?s_picaContext->preparedFrame:nullptr;
    if(!pipeline){fprintf(file,"NATIVE_FRAME_PIPELINE available=0\n");return;}
    const auto& metrics=pipeline->CompletedMetrics();
    fprintf(file,"NATIVE_FRAME_PREFIX_EXECUTION frames=%u wait_ms=%.6f counters_include_warmup=1\n",metrics.prefixFrames,metrics.prefixWaitTicks*1000.0/SYSCLOCK_ARM11);
    fprintf(file,"NATIVE_FRAME_JOINS measured_route_only=1 site_capacity=32 cpu_includes_handoff=1 gpu_is_following_resource_wait=1\n");
    for(unsigned i=0;i<pipeline->JoinSiteCount();++i){const auto& entry=pipeline->JoinSiteAt(i);
        fprintf(file,"NATIVE_FRAME_JOIN site=%p calls=%u cpu_ms=%.6f gpu_ms=%.6f\n",entry.site,entry.calls,entry.cpuTicks*1000.0/SYSCLOCK_ARM11,entry.gpuTicks*1000.0/SYSCLOCK_ARM11);
    }
    fprintf(file,"NATIVE_FRAME_DRAINS anchor=%p measured_route_only=1 includes_final_async_tail=0 site_capacity=32\n",reinterpret_cast<void*>(&picaPreparedBackendFence));
    for(unsigned i=0;i<pipeline->DrainCount();++i){const auto& entry=pipeline->DrainAt(i);
        fprintf(file,"NATIVE_FRAME_DRAIN site=%p calls=%u draws=%u execution_ms=%.6f\n",entry.site,entry.calls,entry.draws,entry.ticks*1000.0/SYSCLOCK_ARM11);
    }
    fprintf(file,"NATIVE_FRAME_PIPELINE available=1 core=%u main_fp=%u worker_fp=%u initial_allowance=%u actual_allowance=%u batches=%u synchronous=%u presented=%u draws=%u pages=%u maximum_pages=%u failures=%u comparisons=%u mismatches=%u compared_draws=%u execution_ms=%.6f join_ms=%.6f counters_include_warmup=1 one_outstanding_frame=1 period_primary=1 work_excludes_async_tail=1 final_join_before_dump=1\n",pipeline->Core(),pipeline->MainFp(),pipeline->WorkerFp(),pipeline->InitialAllowance(),pipeline->ActualAllowance(),metrics.frames,metrics.synchronous,metrics.presented,metrics.draws,metrics.pages,metrics.maximumPages,metrics.failures,metrics.comparisons,metrics.mismatches,metrics.comparedDraws,metrics.executionTicks*1000.0/SYSCLOCK_ARM11,metrics.joinTicks*1000.0/SYSCLOCK_ARM11);
}
bool picaContext::SubmitPreparedFrame()
{
    if(!preparedFrame || !preparedFrame->Recording() || (g_picaPreparedExecution!=7 && g_picaPreparedExecution!=10))return false;
    preparedFrame->Submit(true,g_picaPreparedExecution==10,g_picaFramePrefixEnabled);preparedGpuPending=true;return true;
}
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
bool g_picaDeferredSkinEnabled=false;
void picaDeferredSkinGetStats(picaDeferredSkinStats* output)
{
    *output=s_picaContext && s_picaContext->deferredSkinPool?s_picaContext->deferredSkinPool->stats:picaDeferredSkinStats{};
}
bool picaContext::BeginDeferredSkin(picaDeferredSkin::Rest* rest,pddiMatrix* scratch,const pddiMatrix* const* pose,const bool* used)
{
    if(!g_picaDeferredSkinEnabled || !rest || !scratch || deferredSkin || !PreparedBatching() ||
       !picaMat::PreparedPreviousShader())return false;
    BeginPreparedExecution();
    if(!preparedFrame || !preparedFrame->Recording())return false;
    if(!deferredSkinPool)deferredSkinPool=new(std::nothrow)picaDeferredSkin::Pool;
    if(!deferredSkinPool)return false;
    deferredSkin=deferredSkinPool->Capture(rest,pose,used,*state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top());
    if(!deferredSkin)return false;
    deferredSkinScratch=scratch;return true;
}
void picaContext::EndDeferredSkin()
{
    auto* snapshot=deferredSkin;deferredSkin=nullptr;deferredSkinScratch=nullptr;
    if(snapshot)snapshot->Release();
}
void picaContext::MaterializeDeferredSkin()
{
    if(!deferredSkin)return;
    for(unsigned i=0;i<deferredSkin->rest->count;++i)if(deferredSkin->used[i])
        deferredSkinScratch[i].Mult(deferredSkin->rest->inverse[i],deferredSkin->pose[i]);
    deferredSkinScratch[deferredSkin->rest->count].Identity();
    ++deferredSkinPool->stats.materialized;EndDeferredSkin();
}
bool picaContext::QueueDeferredSkinPalette(const pddiMatrix* const* matrices,unsigned count)
{
    if(!deferredSkin || !matrices || !count || count>picaSkin::MaxBones)return false;
    picaDeferredSkin::Recipe input={};input.snapshot=deferredSkin;input.count=count;input.first=20;
    const auto base=reinterpret_cast<std::uintptr_t>(deferredSkinScratch);
    for(unsigned i=0;i<count;++i){
        const auto address=reinterpret_cast<std::uintptr_t>(matrices[i]);
        if(address<base || (address-base)%sizeof(pddiMatrix))return false;
        const unsigned joint=(address-base)/sizeof(pddiMatrix);
        if(joint>deferredSkin->rest->count || !deferredSkin->used[joint])return false;
        input.joints[i]=joint;
    }
    if(!PreparedBatching())return false;
    BeginPreparedExecution();
    if(!preparedFrame || !preparedFrame->Recording())return false;
    if(!preparedExecutor->DriverRoom(sizeof(input)))FlushPreparedExecution(false);
    if(!preparedExecutor->DriverRoom(sizeof(input)))return false;
    preparedExecutor->AddDeferredSkin(input);++deferredSkinPool->stats.recipes;return true;
}
void picaContext::DumpDeferredSkin(FILE* file)
{
    const picaDeferredSkin::Stats stats=deferredSkinPool?deferredSkinPool->stats:picaDeferredSkin::Stats{};
    fprintf(file,"NATIVE_DEFERRED_SKIN_STATS captures=%u recipes=%u computed_bones=%u allocated=%u live=%u peak=%u allocation_failures=%u input_fallbacks=%u materialized=%u snapshot_bytes=%u counters_include_warmup=1\n",stats.captures,stats.recipes,stats.computedBones,stats.allocated,stats.live,stats.peak,stats.allocationFailures,stats.inputFallbacks,stats.materialized,(unsigned)sizeof(picaDeferredSkin::Snapshot));
}
#endif
bool picaContext::QueuePreparedDriver(void(*call)(const void*),const void* data,unsigned bytes)
{
    if(!PreparedBatching() || (g_picaPreparedExecution!=7 && g_picaPreparedExecution!=9 && g_picaPreparedExecution!=10))return false;
    BeginPreparedExecution();
    if(!preparedFrame || !preparedFrame->Recording())return false;
    if(!preparedExecutor->DriverRoom(bytes))FlushPreparedExecution(false);
    if(!preparedExecutor->DriverRoom(bytes))return false;
    preparedExecutor->AddDriver(call,data,bytes);return true;
}
#endif

bool picaContext::QueuePreparedState(const picaPreparedState& input)
{
    BeginPreparedExecution();
    if(!preparedExecutor)return false;
    if(preparedExecutor->Full())FlushPreparedExecution(false);
    preparedExecutor->AddState(input);return true;
}

bool picaContext::QueuePreparedPalette(const picaPreparedSkinPalette& input,unsigned first)
{
    BeginPreparedExecution();if(!preparedExecutor)return false;
    if(preparedExecutor->Full() || preparedExecutor->PaletteFull())FlushPreparedExecution(false);
    preparedExecutor->AddPalette(input,first);return true;
}
bool picaContext::QueuePreparedProgram(shaderProgram_s* program,C3D_AttrInfo* attributes,bool resetEye,unsigned eyeRegister,float convergence)
{
    BeginPreparedExecution();if(!preparedExecutor)return false;
    if(preparedExecutor->Full())FlushPreparedExecution(false);
    preparedExecutor->AddProgram(program,attributes,resetEye,eyeRegister,convergence);return true;
}
bool picaContext::QueuePreparedLayout(C3D_AttrInfo* attributes,bool fixedNormal)
{
    BeginPreparedExecution();if(!preparedExecutor)return false;
    if(preparedExecutor->Full())FlushPreparedExecution(false);
    preparedExecutor->AddLayout(attributes,fixedNormal);return true;
}
void picaContext::SubmitPreparedScissor(GPU_SCISSORMODE mode,u32 a,u32 b,u32 c,u32 d)
{
    if(PreparedBatching() && QueuePreparedState({PICA_PREPARED_SCISSOR,unsigned(mode),a,b,c,d,0}))return;
    FlushPreparedExecution(true);C3D_SetScissor(mode,a,b,c,d);
}

void picaContext::QueuePreparedGeometry(const picaPreparedGeometry& input)
{
    BeginPreparedExecution();
    picaPreparedExecutor::Require(preparedExecutor!=nullptr,"geometry_arena_allocation");
    if(preparedExecutor->Full())FlushPreparedExecution(false);
    preparedExecutor->AddGeometry(input);
}

#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
bool picaContext::CanRetainMaterial() const
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    return g_picaRetainedMaterialEnabled && preparedFrame && preparedFrame->Recording();
#else
    return false;
#endif
}
#endif

OLD3DS_HOT_PATH
void picaContext::ApplyDrawMaterial(pddiBaseShader* material)
{
    O3_MAIN_SCOPE(ApplyMaterial);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!g_picaPreparedBackendEnabled){
        picaPreparedFenceIfOwned();material->SetMaterial();return;
    }
#endif
    if(!g_picaPreparedBackendEnabled || !g_picaInFrame || typeid(*material)!=typeid(picaMat))
    {
        picaPreparedDrawInputs* previous=preparedDrawCapture;
        preparedDrawCapture=nullptr;
        FlushPreparedExecution(true);
        ++preparedBackendStats.legacyMaterials;
        picaPreparedImmediateScope immediate;
        material->SetMaterial();
        preparedDrawCapture=previous;
        return;
    }
    if(!preparedInputPool)preparedInputPool=new(std::nothrow)picaPreparedInputPool;
    if(!preparedInputPool)
    {
        material->SetMaterial();
        return;
    }
    const bool immediateProtocol=PreparedBatching() && !picaMat::PreparedPreviousShader();
    if(immediateProtocol)FlushPreparedExecution(true);
    else if(PreparedBatching())BeginPreparedExecution();else FlushPreparedExecution(true);
    // Reserve the material operation before capture. Publishing after capture
    // would switch pools while its component indices still name the old arena.
    if(PreparedBatching() && preparedExecutor && preparedExecutor->Full())FlushPreparedExecution(false);
    picaPreparedDrawInputs input;
    input.materialOwner=static_cast<picaMat*>(material);
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    if(!immediateProtocol && CanRetainMaterial())input.retained=static_cast<picaMat*>(material)->PrepareRetainedMaterial();
#endif
    picaPreparedDrawInputs* previous=preparedDrawCapture;
    preparedDrawCapture=&input;
    // The original UID/pass/PreRender/PostRender protocol still runs once, on
    // main, in order. Only its SDK execution is deferred until capture ends.
    if(immediateProtocol){picaPreparedImmediateScope immediate;material->SetMaterial();}
    else material->SetMaterial();
    preparedDrawCapture=previous;
    if(PreparedBatching())BeginPreparedExecution();
    const picaPreparedMaterialPass& pass=
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
        input.retained?input.retained->pass:
#endif
        input.material;
    if(input.hasMaterial && pass.textureIdentity)
    {
        C3D_Tex* texture=const_cast<C3D_Tex*>(pass.textureIdentity);
        C3D_TexSetFilter(texture,pass.filter,pass.filter);
        C3D_TexSetWrap(texture,pass.wrap,pass.wrap);
    }
    if(PreparedBatching() && preparedExecutor)
    {
        if(preparedExecutor->Full())FlushPreparedExecution(false);
        preparedExecutor->AddMaterial(input,*preparedInputPool);
        preparedBackendStats.materials+=input.hasMaterial;preparedBackendStats.uniforms+=input.hasUniforms;
        return;
    }
    if(input.hasMaterial)
    {
        ++preparedBackendStats.materials;
        picaPreparedMaterialCounters counters;
        picaApplyPreparedMaterialPass(input.material,g_picaState,counters,
            const_cast<C3D_Tex*>(input.material.textureIdentity));
        for(unsigned i=0;i<counters.stateChanges;++i)Old3dsCount(O3_STATE_CHANGE);
        for(unsigned i=0;i<counters.textureRequests;++i)Old3dsCount(O3_TEX_REQUEST);
        for(unsigned i=0;i<counters.textureChanges;++i)Old3dsCount(O3_TEX_CHANGE);
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        g_picaQueueMetrics.textures+=counters.textureChanges;
#endif
        const unsigned rasterChanges=picaApplyPreparedMaterialRaster(input.raster,g_picaState);
        for(unsigned i=0;i<rasterChanges;++i)Old3dsCount(O3_STATE_CHANGE);
    }
    if(input.hasUniforms){++preparedBackendStats.uniforms;ExecutePreparedUniforms(preparedInputPool->View(input.uniforms));}
}

void picaContext::CapturePreparedMaterialRaster(const picaPreparedMaterialPass& material,picaPreparedMaterialRaster& output)
{
    output.cull=material.twoSided?GPU_CULL_NONE:cullModeTable[state.renderState->cullMode];
    output.applyDepth=matWriteMask!=material.writeMask;
    matWriteMask=material.writeMask;
    output.depthEnabled=state.renderState->zEnabled;
    output.depthFunction=output.depthEnabled?compTable[state.renderState->zCompare]:GPU_ALWAYS;
    unsigned mask=0;
    if(state.renderState->redWrite && (matWriteMask&PDDI_WRITE_RED))mask|=GPU_WRITE_RED;
    if(state.renderState->greenWrite && (matWriteMask&PDDI_WRITE_GREEN))mask|=GPU_WRITE_GREEN;
    if(state.renderState->blueWrite && (matWriteMask&PDDI_WRITE_BLUE))mask|=GPU_WRITE_BLUE;
    if(state.renderState->alphaWrite && (matWriteMask&PDDI_WRITE_ALPHA))mask|=GPU_WRITE_ALPHA;
    if(output.depthEnabled && state.renderState->zWrite)mask|=GPU_WRITE_DEPTH;
    output.depthMask=(GPU_WRITEMASK)mask;
}

void picaContext::ExecutePreparedUniforms(const picaPreparedUniformView& input)
{
    Old3dsOperationScope timing(O3_UNIFORMS,1);
    bool ignoredSparse=false,ignoredFast=false;
    unsigned ignoredFastCalls=0,ignoredOtherCalls=0,ignoredDirectional=0,ignoredGeneral=0;
    (void)ignoredSparse;(void)ignoredFast;(void)ignoredFastCalls;(void)ignoredOtherCalls;(void)ignoredDirectional;(void)ignoredGeneral;
    // Capture and legacy invalidation already update these shared flags. Epochs
    // here start current, avoiding a second invalidation at each handoff.
    picaPreparedUniformCacheRefs cache={
        modelViewUploaded,projUploaded,lightsUploaded,matUploaded,lastLit,miscUploaded,
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
        sparseUnlitProjection,lastFastUnlit,
#else
        ignoredSparse,ignoredFast,
#endif
        lastModelView,lastMatAmbient,lastMatDiffuse,lastMatEmissive,lastSceneAmbient,
        input.invalidationEpoch,input.lightEpoch,input.projectionEpoch,g_old3dsModelViewUploads,g_old3dsModelViewReuses,
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
        fastUnlitCalls,otherProjectionCalls,
#else
        ignoredFastCalls,ignoredOtherCalls,
#endif
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
        directionalDraws,generalLightDraws
#else
        ignoredDirectional,ignoredGeneral
#endif
    };
    const unsigned beforeUploads=g_old3dsModelViewUploads,beforeReuses=g_old3dsModelViewReuses;
    picaApplyPreparedUniforms(input,cache);
    if(g_old3dsModelViewUploads!=beforeUploads)Old3dsCount(O3_MV_UPLOAD);
    if(g_old3dsModelViewReuses!=beforeReuses)Old3dsCount(O3_MV_REUSE);
}

OLD3DS_HOT_PATH
bool picaContext::CapturePreparedUniformRecord(picaPreparedUniformRecord& output,const void* materialIdentity,
    unsigned materialUid,bool lit,pddiColour ambient,pddiColour diffuse,pddiColour emissive,const float (*prepared)[4],
    const picaPreparedMaterialUniforms* retained)
{
    O3_MAIN_SCOPE(UniformCapture);
    if(!g_picaInFrame || !preparedInputPool)return false;
    if(lit && lightsDirty){RebuildLightArray();lightsUploaded=false;}
    const picaPreparedInputPool::ViewKey viewKey={preparedProjectionEpoch,stereoOn,display->StereoActive()};
    const picaPreparedInputPool::LightKey lightKey={preparedLightEpoch,state.lightingState->ambient.c};
    // Native prepared colours come from these exact material colours on main.
    // Include values as well as UID: direct native colour writers need not change UID.
    const picaPreparedInputPool::MaterialKey materialKey={materialIdentity,materialUid,ambient.c,diffuse.c,emissive.c,lit,prepared!=nullptr};
    picaPreparedInputPool::Selection selected;
    if(!preparedInputPool->Reserve(viewKey,lightKey,materialKey,selected,retained!=nullptr))
    {
        // Synchronous bridge only: every earlier input has already executed.
        // A future worker must fence/retire the batch before taking this branch.
        FlushPreparedExecution(false);
        preparedInputPool->Reset();
        if(!preparedInputPool->Reserve(viewKey,lightKey,materialKey,selected,retained!=nullptr))return false;
    }
    if(selected.newView)
    {
        ++preparedBackendStats.viewRecords;
        picaPreparedViewUniforms& view=preparedInputPool->views[selected.view];
        view.projection=hwProjection;view.stereo=stereoOn;view.stereoActive=viewKey.active;view.epoch=preparedProjectionEpoch;
    }
    if(selected.newLight)
    {
        ++preparedBackendStats.lightRecords;
        picaPreparedLightUniforms& light=preparedInputPool->lights[selected.light];
        light.sceneAmbient=state.lightingState->ambient;light.epoch=preparedLightEpoch;
        // A first unlit draw must not freeze an empty light set for later lit draws.
        // Dirty lights get a distinct key when a lit draw rebuilds them.
        light.count=nActiveLights;
        memcpy(light.position,lightPos,sizeof(light.position));memcpy(light.colour,lightColour,sizeof(light.colour));
        memcpy(light.attenuation,lightAtten,sizeof(light.attenuation));
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
        light.directional=directionalOnly;memcpy(light.directionalPosition,directionalPos,sizeof(light.directionalPosition));
#else
        light.directional=false;
#endif
    }
    if(selected.newMaterial)
    {
        ++preparedBackendStats.materialUniformRecords;
        picaPreparedMaterialUniforms& material=preparedInputPool->materials[selected.material];
        material.ambient=ambient;material.diffuse=diffuse;material.emissive=emissive;
        material.lit=lit;material.hasPreparedColours=prepared!=nullptr;
        if(prepared)memcpy(material.colours,prepared,sizeof(material.colours));
    }
    output.transform.modelView=*state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    output.transform.invalidationEpoch=preparedUniformEpoch;
#ifdef SRR2_OLD3DS_GPU_SKIN
    output.transform.skin=skinProgramBound;
#else
    output.transform.skin=false;
#endif
    output.view=selected.view;output.light=selected.light;output.material=selected.material;
    return true;
}

bool picaContext::CapturePreparedUniforms(picaPreparedUniformInput& out,bool lit,
    pddiColour ambient,pddiColour diffuse,pddiColour emissive,const float (*prepared)[4])
{
    if(!g_picaInFrame)return false;
    if(lit && lightsDirty){RebuildLightArray();lightsUploaded=false;}
    out.modelView=*state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();out.projection=hwProjection;
    out.ambient=ambient;out.diffuse=diffuse;out.emissive=emissive;out.sceneAmbient=state.lightingState->ambient;
    out.lit=lit;out.stereo=stereoOn;out.stereoActive=display->StereoActive();
#ifdef SRR2_OLD3DS_GPU_SKIN
    out.skin=skinProgramBound;
#else
    out.skin=false;
#endif
    out.invalidationEpoch=preparedUniformEpoch;out.lightEpoch=preparedLightEpoch;out.projectionEpoch=preparedProjectionEpoch;
    out.hasPreparedColours=prepared!=nullptr;if(prepared)memcpy(out.colours,prepared,sizeof(out.colours));
    if(lit) {
        out.lightCount=nActiveLights;
        memcpy(out.lightPosition,lightPos,sizeof(out.lightPosition));memcpy(out.lightColour,lightColour,sizeof(out.lightColour));
        memcpy(out.lightAttenuation,lightAtten,sizeof(out.lightAttenuation));
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
        out.directional=directionalOnly;memcpy(out.directionalPosition,directionalPos,sizeof(out.directionalPosition));
#else
        out.directional=false;
#endif
    } else {out.lightCount=0;out.directional=false;}
    return true;
}
#endif

void picaContext::SetVertexUniforms(bool lit, pddiColour ambient, pddiColour diffuse, pddiColour emissive, const float (*prepared)[4])
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalUniform(O3E_UNIFORM);
#endif
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_UNIFORMS, 1); O3_RECUR(Uniforms);
#endif
    if(!g_picaInFrame)
    {
        InvalidateUniformCache();
        return;
    }


#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
    const unsigned skinRegisterOffset = skinProgramBound ? 4 : 0;
#else
    const unsigned skinRegisterOffset = 0;
#endif
    const unsigned skin_lightPos = VSH_FVEC_u_lightPos - skinRegisterOffset;
    const unsigned skin_lightColour = VSH_FVEC_u_lightColour - skinRegisterOffset;
    const unsigned skin_lightAtten = VSH_FVEC_u_lightAtten - skinRegisterOffset;
    const unsigned skin_matAmbient = VSH_FVEC_u_matAmbient - skinRegisterOffset;
    const unsigned skin_matDiffuse = VSH_FVEC_u_matDiffuse - skinRegisterOffset;
    const unsigned skin_matEmissive = VSH_FVEC_u_matEmissive - skinRegisterOffset;
    const unsigned skin_sceneAmbient = VSH_FVEC_u_sceneAmbient - skinRegisterOffset;

#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
    // Upload alongside the other draw uniforms, after shader/material binding.
    // Do not depend on a preceding program switch having emitted this register.
    if(skinProgramBound)
    {
        static_assert(VSH_FVEC_u_eye_quant == 95, "Packed scales follow the bone palette");
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_eye_quant,
                      1.0f / 8192.0f, 1.0f / 32768.0f, 1.0f / 4096.0f, 1.0f);
    }
#endif

    pddiMatrix* mv = state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();

    if(!projUploaded)
    {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        g_picaQueueMetrics.uniforms += 2;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
        if(!skinProgramBound)
#endif
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_stereoOn,
                      stereoOn ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_proj, &hwProjection);
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        if(!skinProgramBound)
#endif
        {
            float projection[4][4], scale[4];
            for(unsigned row=0;row<4;++row) {
                projection[row][0]=hwProjection.r[row].x;projection[row][1]=hwProjection.r[row].y;
                projection[row][2]=hwProjection.r[row].z;projection[row][3]=hwProjection.r[row].w;
            }
            sparseUnlitProjection = picaUnlitProjection::Parts(projection, scale);
            if(sparseUnlitProjection)
                C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_unlitScale, scale[0],scale[1],scale[2],scale[3]);
        }
#endif
        projUploaded = true;
    }

#ifdef SRR2_OLD_3DS
    // Material changes often draw the same transform repeatedly. Preserve exact
    // matrix bits and invalidate at every frame and render-target transition.
    if(!modelViewUploaded || memcmp(&lastModelView, mv, sizeof(lastModelView)) != 0)
    {
        memcpy(&lastModelView, mv, sizeof(lastModelView));
        modelViewUploaded = true;
        ++g_old3dsModelViewUploads;
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        ++g_picaQueueMetrics.uniforms;
#endif
        Old3dsCount(O3_MV_UPLOAD);
#endif
    C3D_Mtx hwModelView;
    for(int row = 0; row < 4; row++)
    {
        hwModelView.r[row].x = mv->Row4(0)[row];
        hwModelView.r[row].y = mv->Row4(1)[row];
        hwModelView.r[row].z = mv->Row4(2)[row];
        hwModelView.r[row].w = mv->Row4(3)[row];
    }

#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
    if(!skinProgramBound)
#endif
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_mv, &hwModelView);
#ifdef SRR2_OLD_3DS
    }
    else {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        ++g_picaQueueMetrics.uniformReuses;
#endif
        ++g_old3dsModelViewReuses; Old3dsCount(O3_MV_REUSE); }
#endif

#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    // Only mono, unlit affine draws use the reduced transform. Lit, stereo and
    // custom projection paths retain the original instructions and uniforms.
    const bool fastUnlit =
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        !skinProgramBound &&
#endif
        !lit && (!stereoOn || !display->StereoActive()) && sparseUnlitProjection &&
        mv->Row4(0)[3] == 0.0f && mv->Row4(1)[3] == 0.0f &&
        mv->Row4(2)[3] == 0.0f && mv->Row4(3)[3] == 1.0f;
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    if(fastUnlit) ++fastUnlitCalls; else ++otherProjectionCalls;
#endif
    if(!miscUploaded || lit != lastLit
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
       || fastUnlit != lastFastUnlit
#endif
      )
    {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        ++g_picaQueueMetrics.uniforms;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
        if(skinProgramBound)
        {
            // b1 replaces the lighting float; c24..c95 belong solely to bones.
            C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x69, lit);
            if(!lit)
            {
                // Unlit draws still need zero, one and the byte-color scale.
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient, 0, 0, 0, 1);
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive, 0, 0, 0, 0);
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_sceneAmbient, 0, 0, 0, 1.0f / 255.0f);
                matUploaded = false;
            }
        }
        else
#endif
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_misc,
                      lit ? 1.0f : 0.0f,
                      0.0f,
                      1.0f,
                      1.0f / 255.0f);
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        if(!skinProgramBound)
#endif
        {
            static_assert(VSH_FLAG_u_fastUnlit == 2, "Fast unlit uses normal-program boolean b1");
            C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x69, fastUnlit);
        }
        lastFastUnlit = fastUnlit;
#endif
        lastLit = lit;
        miscUploaded = true;
    }

    if(!lit)
        return;

    if(lightsDirty)
    {
        RebuildLightArray();
        lightsUploaded = false;
    }

#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    if(lit) {if(directionalOnly) ++directionalDraws;else ++generalLightDraws;}
#endif
    if(!lightsUploaded)
    {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
        g_picaQueueMetrics.uniforms += 3;
#endif
#ifdef SRR2_OLD3DS_LIGHT_LOOP
        const int lightCount = nActiveLights;
#else
        const int lightCount = PDDI_MAX_LIGHTS;
#endif
        C3D_FVec* pos = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightPos, lightCount);
        C3D_FVec* col = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightColour, lightCount);
        C3D_FVec* att = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightAtten, lightCount);
        for(int i = 0; i < lightCount; i++)
        {
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
            pos[i] = directionalOnly ? directionalPos[i] : lightPos[i];
#else
            pos[i] = lightPos[i];
#endif
            col[i] = lightColour[i];
            att[i] = lightAtten[i];
        }
#ifdef SRR2_OLD3DS_LIGHT_LOOP
        // PICA loop counts encode iterations minus one. Zero lights use the
        // boolean guard rather than wrapping the unsigned count to 255.
        static_assert(VSH_FLAG_u_hasLights == 1, "light boolean register");
        C3D_IVUnifSet(GPU_VERTEX_SHADER, 0x60 + VSH_IVEC_u_lightLoop,
                      nActiveLights ? nActiveLights - 1 : 0, 0, 1, 0);
        C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x68, nActiveLights != 0);
#endif
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
        static_assert(VSH_FLAG_u_directionalOnly == 4,"directional light boolean register");
        C3D_BoolUnifSet(GPU_VERTEX_SHADER,0x6a,directionalOnly);
#endif
        lightsUploaded = true;
    }

    if(matUploaded &&
       ambient.c  == lastMatAmbient.c  &&
       diffuse.c  == lastMatDiffuse.c  &&
       emissive.c == lastMatEmissive.c &&
       state.lightingState->ambient.c == lastSceneAmbient.c)
    {
        return;
    }
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    g_picaQueueMetrics.uniforms += 4;
#endif
    lastMatAmbient   = ambient;
    lastMatDiffuse   = diffuse;
    lastMatEmissive  = emissive;
    lastSceneAmbient = state.lightingState->ambient;
    matUploaded      = true;

    if(prepared)
    {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient,
                      prepared[0][0], prepared[0][1], prepared[0][2], prepared[0][3]);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matDiffuse,
                      prepared[1][0], prepared[1][1], prepared[1][2], prepared[1][3]);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive,
                      prepared[2][0], prepared[2][1], prepared[2][2], prepared[2][3]);
    }
    else
    {
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient,
                  (float)ambient.Red() / 255.0f,
                  (float)ambient.Green() / 255.0f,
                  (float)ambient.Blue() / 255.0f,
                  1.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matDiffuse,
                  (float)diffuse.Red() / 255.0f,
                  (float)diffuse.Green() / 255.0f,
                  (float)diffuse.Blue() / 255.0f,
                  (float)diffuse.Alpha() / 255.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive,
                  (float)emissive.Red() / 255.0f,
                  (float)emissive.Green() / 255.0f,
                  (float)emissive.Blue() / 255.0f,
                  0.0f);
    }
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_sceneAmbient,
                  (float)state.lightingState->ambient.Red() / 255.0f,
                  (float)state.lightingState->ambient.Green() / 255.0f,
                  (float)state.lightingState->ambient.Blue() / 255.0f,
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
                  skinProgramBound ? 1.0f / 255.0f : 1.0f);
#else
                  1.0f);
#endif
}

void picaContext::EnableCulling(bool enable)
{
    picaCullFace(enable ? cullModeTable[state.renderState->cullMode] : GPU_CULL_NONE);
}

void picaContext::SetCullMode(pddiCullMode mode)
{
    pddiBaseContext::SetCullMode(mode);
    picaCullFace(cullModeTable[mode]);
}

void picaContext::SetColourWrite(bool red, bool green, bool blue, bool alpha)
{
    pddiBaseContext::SetColourWrite(red, green, blue, alpha);
    EnableZBuffer(state.renderState->zEnabled);
}

void picaContext::EnableZBuffer(bool enable)
{
    pddiBaseContext::EnableZBuffer(enable);

    unsigned mask = 0;
    if(state.renderState->redWrite   && (matWriteMask & PDDI_WRITE_RED))   mask |= GPU_WRITE_RED;
    if(state.renderState->greenWrite && (matWriteMask & PDDI_WRITE_GREEN)) mask |= GPU_WRITE_GREEN;
    if(state.renderState->blueWrite  && (matWriteMask & PDDI_WRITE_BLUE))  mask |= GPU_WRITE_BLUE;
    if(state.renderState->alphaWrite && (matWriteMask & PDDI_WRITE_ALPHA)) mask |= GPU_WRITE_ALPHA;
    if(enable && state.renderState->zWrite) mask |= GPU_WRITE_DEPTH;

    picaDepthTest(enable, enable ? compTable[state.renderState->zCompare] : GPU_ALWAYS,
                  (GPU_WRITEMASK)mask);
}

void picaContext::SetMaterialWriteMask(int mask)
{
    if(matWriteMask == mask)
        return;
    matWriteMask = mask;
    EnableZBuffer(state.renderState->zEnabled);
}

void picaContext::SetZCompare(pddiCompareMode compareMode)
{
    pddiBaseContext::SetZCompare(compareMode);
    EnableZBuffer(state.renderState->zEnabled);
}

void picaContext::SetZWrite(bool b)
{
    pddiBaseContext::SetZWrite(b);
    EnableZBuffer(state.renderState->zEnabled);
}

void picaContext::SetZBias(float bias)
{
    pddiBaseContext::SetZBias(bias);
}

void picaContext::SetZRange(float n, float f)
{
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
#endif
    pddiBaseContext::SetZRange(n, f);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    struct DepthInput {float scale,offset;};const DepthInput input={-(f-n),n};
    if(QueuePreparedDriver([](const void* bytes){DepthInput value;memcpy(&value,bytes,sizeof(value));C3D_DepthMap(true,value.scale,value.offset);},&input,sizeof(input)))return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
    C3D_DepthMap(true, -(f - n), n);
}

void picaContext::EnableStencilBuffer(bool enable)
{
    pddiBaseContext::EnableStencilBuffer(enable);
    SetupHardwareStencil();
}

void picaContext::SetupHardwareStencil(void)
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    struct StencilInput {bool enabled;GPU_TESTFUNC function;int ref,mask,writeMask;GPU_STENCILOP fail,zfail,pass;};
    StencilInput input={};input.enabled=state.stencilState->enabled;
    if(input.enabled){input.function=compTable[state.stencilState->compare];input.ref=state.stencilState->ref;input.mask=(int)state.stencilState->mask;input.writeMask=(int)state.stencilState->writeMask;
        input.fail=stencilOpTable[state.stencilState->failOp];input.zfail=stencilOpTable[state.stencilState->zFailOp];input.pass=stencilOpTable[state.stencilState->zPassOp];}
    if(QueuePreparedDriver([](const void* bytes){StencilInput value;memcpy(&value,bytes,sizeof(value));
        if(value.enabled){C3D_StencilTest(true,value.function,value.ref,value.mask,value.writeMask);C3D_StencilOp(value.fail,value.zfail,value.pass);}
        else C3D_StencilTest(false,GPU_ALWAYS,0,0xff,0x00);
    },&input,sizeof(input)))return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
    if(state.stencilState->enabled)
    {
        C3D_StencilTest(true,
                        compTable[state.stencilState->compare],
                        state.stencilState->ref,
                        (int)state.stencilState->mask,
                        (int)state.stencilState->writeMask);
        C3D_StencilOp(stencilOpTable[state.stencilState->failOp],
                      stencilOpTable[state.stencilState->zFailOp],
                      stencilOpTable[state.stencilState->zPassOp]);
    }
    else
    {
        C3D_StencilTest(false, GPU_ALWAYS, 0, 0xff, 0x00);
    }
}

void picaContext::SetStencilCompare(pddiCompareMode compare)
{
    pddiBaseContext::SetStencilCompare(compare);
    SetupHardwareStencil();
}

void picaContext::SetStencilRef(int ref)
{
    pddiBaseContext::SetStencilRef(ref);
    SetupHardwareStencil();
}

void picaContext::SetStencilMask(unsigned mask)
{
    pddiBaseContext::SetStencilMask(mask);
    SetupHardwareStencil();
}

void picaContext::SetStencilWriteMask(unsigned mask)
{
    pddiBaseContext::SetStencilWriteMask(mask);
    SetupHardwareStencil();
}

void picaContext::SetStencilOp(pddiStencilOp failOp, pddiStencilOp zFailOp, pddiStencilOp zPassOp)
{
    pddiBaseContext::SetStencilOp(failOp, zFailOp, zPassOp);
    SetupHardwareStencil();
}

void picaContext::SetFillMode(pddiFillMode mode)
{
    pddiBaseContext::SetFillMode(mode);
}

void picaContext::ApplyFogState(void)
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(PreparedFrameInFlight())FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(g_picaPreparedExecution!=7 && g_picaPreparedExecution!=9 && g_picaPreparedExecution!=10)
#endif
    FlushPreparedExecution(true);
#endif
    if(!state.fogState->enabled)
    {
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        const unsigned empty=0;
        if(QueuePreparedDriver([](const void*){C3D_FogGasMode(GPU_NO_FOG,GPU_PLAIN_DENSITY,false);},&empty,sizeof(empty)))return;
        FlushPreparedExecution(true);
#endif
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        return;
    }

    const float start = state.fogState->start;
    const float end   = state.fogState->end;

    if(!fogLutValid || (start != fogLutStart) || (end != fogLutEnd))
    {
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        // SDK state can retain this LUT's address. Encode every earlier draw
        // before changing its backing data, even during frame capture.
        FlushPreparedExecution(true);
#endif
        const float n = state.viewState->camera.nearPlane;
        const float f = state.viewState->camera.farPlane;

        float data[256];
        const float range = (end > start) ? (end - start) : 1.0f;

        for(int i = 0; i < 256; i++)
        {
            const float depth = (float)i / 255.0f;
            const float z = FogLut_CalcZ(depth, n, f);

            float density = (z - start) / range;
            if(density < 0.0f) density = 0.0f;
            if(density > 1.0f) density = 1.0f;

            data[i] = density;
        }

        FogLut_FromArray(&fogLut, data);
        fogLutStart = start;
        fogLutEnd   = end;
        fogLutValid = true;
    }

#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // The LUT is a persistent resource. Its actual data mutations fence above,
    // so ordered leaf calls can retain its address without copying 512 bytes.
    struct FogInput {C3D_FogLut* lut;u32 colour;};
    const FogInput input={&fogLut,((unsigned)state.fogState->colour.Blue()<<16)|((unsigned)state.fogState->colour.Green()<<8)|(unsigned)state.fogState->colour.Red()};
    if(QueuePreparedDriver([](const void* bytes){FogInput value;memcpy(&value,bytes,sizeof(value));
        C3D_FogColor(value.colour);C3D_FogLutBind(value.lut);C3D_FogGasMode(GPU_FOG,GPU_PLAIN_DENSITY,false);
    },&input,sizeof(input)))return;
    FlushPreparedExecution(true);
#endif
    C3D_FogColor(((unsigned)state.fogState->colour.Blue() << 16) |
                 ((unsigned)state.fogState->colour.Green() << 8) |
                  (unsigned)state.fogState->colour.Red());
    C3D_FogLutBind(&fogLut);
    C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
}

void picaContext::EnableFog(bool enable)
{
    pddiBaseContext::EnableFog(enable);
    ApplyFogState();
}

void picaContext::SetFog(pddiColour colour, float start, float end)
{
    pddiBaseContext::SetFog(colour, start, end);
    ApplyFogState();
}

int picaContext::GetMaxTextureDimension(void)
{
    return PICA_MAX_TEXTURE_DIMENSION;
}

pddiExtension* picaContext::GetExtension(unsigned extID)
{
    switch(extID)
    {
        case PDDI_EXT_GAMMACONTROL:
            return extGamma;
    }

    return pddiBaseContext::GetExtension(extID);
}

bool picaContext::VerifyExtension(unsigned extID)
{
    switch(extID)
    {
        case PDDI_EXT_GAMMACONTROL:
            return true;
    }

    return pddiBaseContext::VerifyExtension(extID);
}

void picaContext::BeginTiming(void)
{
    display->BeginTiming();
}

float picaContext::EndTiming(void)
{
    return display->EndTiming();
}

class picaPrimBufferStream : public pddiPrimBufferStream
{
public:
    picaPrimBuffer* buffer;

    picaPrimBufferStream(picaPrimBuffer* b) { buffer = b; }

    void Next(void)
    {
        if(buffer->coord)
            buffer->coord = (float*)((char*)buffer->coord + PICA_VERTEX_STRIDE);
        if(buffer->normal)
            buffer->normal = (float*)((char*)buffer->normal + PICA_VERTEX_STRIDE);
        if(buffer->uv)
            buffer->uv = (float*)((char*)buffer->uv + PICA_VERTEX_STRIDE);
        if(buffer->colour)
            buffer->colour += PICA_VERTEX_STRIDE;

        buffer->total++;
        PDDIASSERT(buffer->total <= buffer->allocated);
    }

    void Position(float x, float y, float z)
    {
        if(buffer->coord && (buffer->total < buffer->allocated))
        {
            buffer->coord[0] = x;
            buffer->coord[1] = y;
            buffer->coord[2] = z;
        }
        Next();
    }

    void Normal(float x, float y, float z)
    {
        if(!buffer->normal || (buffer->total >= buffer->allocated)) return;
        buffer->normal[0] = x;
        buffer->normal[1] = y;
        buffer->normal[2] = z;
    }

    void Colour(pddiColour colour, int channel = 0)
    {
        if(!buffer->colour || (buffer->total >= buffer->allocated)) return;
        buffer->colour[0] = colour.Red();
        buffer->colour[1] = colour.Green();
        buffer->colour[2] = colour.Blue();
        buffer->colour[3] = colour.Alpha();
    }

    void TexCoord1(float u, int channel = 0) {}

    void TexCoord2(float u, float v, int channel = 0)
    {
        if(!buffer->uv || (channel != 0) || (buffer->total >= buffer->allocated)) return;
        buffer->uv[0] = u;
        buffer->uv[1] = v;
    }

    void TexCoord3(float u, float v, float s, int channel = 0) {}
    void TexCoord4(float u, float v, float s, float t, int channel = 0) {}

    void Specular(pddiColour colour) {}
    void SkinIndices(unsigned, unsigned, unsigned, unsigned) {}
    void SkinWeights(float, float, float) {}

    void Vertex(pddiVector* v, pddiColour c)
    {
        Colour(c);
        Position(v->x, v->y, v->z);
    }

    void Vertex(pddiVector* v, pddiVector* n)
    {
        Normal(n->x, n->y, n->z);
        Position(v->x, v->y, v->z);
    }

    void Vertex(pddiVector* v, pddiVector2* uv)
    {
        TexCoord2(uv->x, uv->y);
        Position(v->x, v->y, v->z);
    }

    void Vertex(pddiVector* v, pddiColour c, pddiVector2* uv)
    {
        Colour(c);
        TexCoord2(uv->x, uv->y);
        Position(v->x, v->y, v->z);
    }

    void Vertex(pddiVector* v, pddiVector* n, pddiVector2* uv)
    {
        Normal(n->x, n->y, n->z);
        TexCoord2(uv->x, uv->y);
        Position(v->x, v->y, v->z);
    }
};

#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
void picaContext::SetNoNormalLayout(bool enabled)
{
    if(noNormalLayout == enabled) return;
    noNormalLayout = enabled;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(PreparedBatching() && QueuePreparedLayout(enabled ? &noNormalAttrInfo : &attrInfo,enabled))return;
    FlushPreparedExecution(true);
#endif
    C3D_SetAttrInfo(enabled ? &noNormalAttrInfo : &attrInfo);
    if(enabled) C3D_FixedAttribSet(1, 0.0f, 0.0f, 1.0f, 0.0f);
    g_picaState.bufInfoSet = false;
}
void picaPrimBuffer::ExpandNoNormal()
{
    if(!noNormalPacked) return;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    context->FlushPreparedExecution(true);
#endif
    picaNoNormal::Expand(buffer, allocated);
    noNormalPacked = false;
    GSPGPU_FlushDataCache(buffer, allocated * PICA_VERTEX_STRIDE);
}
#endif

picaPrimBuffer::picaPrimBuffer(picaContext* c, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex
#if defined(SRR2_OLD3DS_WHEEL_GPU_BATCH) || defined(SRR2_OLD3DS_WHEEL_SKIN_BATCH)
    , unsigned stride
#endif
    )
    : context(c)
{
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    #ifdef SRR2_OLD3DS_SKIN_CLIP
    skinClipBounds=NULL;
    #endif
    skinAllocation = NULL; skinReason = picaSkin::NotSelected; skinBoneCount = 0; skinDrawing = false;
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    skinRigid = false;
#endif
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    noNormalAllowed = noNormalPacked = false;
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    clipBoundsValid = false;
#endif
    stream = new picaPrimBufferStream(this);

    total = allocated = 0;
    coord = normal = uv = NULL;
    colour = NULL;
    buffer = NULL;
#ifdef SRR2_OLD3DS_SKIN_RING
    skinVertices = NULL;
#endif
    indices = NULL;
    indexCount = 0;
    expanded = NULL;
    expandedBytes = 0;
    expandDirty = true; elemState = -1;
    mem = 0;
    transient = false;
    defaultsValid = false;
#ifdef SRR2_OLD3DS_DRAW_PREP
    drawPrepared = false;
#endif
#ifdef SRR2_OLD3DS_LOAD_UPLOAD
    uploadPending = false;
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    queueBoundsValid = false;
#endif

    primType = type;
    vertexType = vertexFormat;

    if(nVertex <= 0)
    {
        transient = true;
        return;
    }

    allocated = (unsigned)nVertex;

    indexCount = (nIndex > 0) ? (unsigned)nIndex : allocated;

#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    wheelPaletteBuffer = stride==sizeof(picaWheelLOD::PaletteVertex);
#endif
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    wheelSkinBuffer = stride==picaSkin::PackedStride(true);
#endif
#if defined(SRR2_OLD3DS_WHEEL_GPU_BATCH) || defined(SRR2_OLD3DS_WHEEL_SKIN_BATCH)
    const unsigned vertexBytes = stride * allocated;
#else
    const unsigned vertexBytes = PICA_VERTEX_STRIDE * allocated;
#endif
    const unsigned indexBytes  = ((indexCount * (unsigned)sizeof(unsigned short)) + 3u) & ~3u;

    mem = vertexBytes + indexBytes;
#ifdef SRR2_OLD3DS_DRAW_PREP
    buffer = (unsigned char*)picaBufferAllocate(mem);
#else
    buffer = (unsigned char*)linearAlloc(mem);
#endif
    if(buffer == NULL)
    {
        lastError = PDDI_ERR;
        allocated = 0;
        indexCount = 0;
        return;
    }

    indices = (unsigned short*)(buffer + vertexBytes);

    if(nIndex == 0)
    {
        for(unsigned i = 0; i < indexCount; i++)
            indices[i] = (unsigned short)i;
    }

    context->ADD_STAT(PDDI_STAT_BUFFERED_COUNT, 1);
    context->ADD_STAT(PDDI_STAT_BUFFERED_ALLOC, mem / 1024.0f);
}

#ifdef SRR2_OLD3DS_WHEEL_LOD
#include <pddi/pica/wheellod.hpp>
static inline void picaFlushForGpu(const void* p, unsigned bytes);
static unsigned wheelLodLiveBytes=0,wheelLodPeakBytes=0,wheelLodAllocFailures=0;
extern "C" void picaDumpWheelLodMemory()
{
    printf("WHEEL_LOD_MEMORY live=%u peak=%u failures=%u budget=%u\n",
        wheelLodLiveBytes,wheelLodPeakBytes,wheelLodAllocFailures,picaWheelLOD::Budget);
}
picaPrimBuffer* picaPrimBuffer::BakeWheelCopies(const pddiMatrix* rest,unsigned copies)
{
    if(!rest || copies!=picaWheelLOD::Copies || transient || !buffer || !allocated ||
       allocated>65535/copies || !(vertexType&PDDI_V_NORMAL) || (primType!=PDDI_PRIM_TRIANGLES && primType!=PDDI_PRIM_TRISTRIP) || !IndicesInRange()) return nullptr;
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    if(noNormalPacked) return nullptr;
#endif
    const bool strip=primType==PDDI_PRIM_TRISTRIP;
    const unsigned triangles=picaWheelLOD::TriangleCount(indexCount,strip);
    if(!triangles || triangles>picaWheelLOD::GroupLimit/(copies*6)) return nullptr;
    const unsigned vertices=allocated*copies,count=triangles*3*copies;
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    const unsigned stride=picaSkin::PackedStride(true);
#elif defined(SRR2_OLD3DS_WHEEL_GPU_BATCH)
    const unsigned stride=sizeof(picaWheelLOD::PaletteVertex);
#else
    const unsigned stride=PICA_VERTEX_STRIDE;
#endif
    const unsigned bytes=vertices*stride+((count*2+3)&~3u);
    if(bytes>picaWheelLOD::GroupLimit || wheelLodLiveBytes+bytes>picaWheelLOD::Budget ||
       linearSpaceFree()<picaWheelLOD::Reserve+bytes){++wheelLodAllocFailures;return nullptr;}
    picaPrimBuffer* result=new picaPrimBuffer(context,PDDI_PRIM_TRIANGLES,vertexType,vertices,count
#if defined(SRR2_OLD3DS_WHEEL_GPU_BATCH) || defined(SRR2_OLD3DS_WHEEL_SKIN_BATCH)
        ,stride
#endif
        );
    if(!result->buffer){delete result;++wheelLodAllocFailures;return nullptr;}
    for(unsigned copy=0;copy<copies;++copy)
    {
        float transform[4][4];memcpy(transform,rest+copy,sizeof(transform));
        for(unsigned v=0;v<allocated;++v)
        {
            picaWheelLOD::Vertex source,baked;memcpy(&source,buffer+v*PICA_VERTEX_STRIDE,sizeof(source));
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
            picaSkin::Source rigid={};rigid.weights[0]=1.0f;rigid.indices[0]=copy;
            memcpy(rigid.position,source.position,sizeof(rigid.position));
            memcpy(rigid.normal,source.normal,sizeof(rigid.normal));
            picaSkin::Vertex packed;
            if(!picaSkin::PackVertex(packed,rigid,&source,copies)){delete result;return nullptr;}
            picaSkin::StoreVertex(result->buffer+(copy*allocated+v)*stride,packed,true);
#elif defined(SRR2_OLD3DS_WHEEL_GPU_BATCH)
            const auto packed=picaWheelLOD::Palette(source,copy);
            memcpy(result->buffer+(copy*allocated+v)*stride,&packed,sizeof(packed));
#else
            if(!picaWheelLOD::BakeVertex(baked,source,transform)){delete result;return nullptr;}
            memcpy(result->buffer+(copy*allocated+v)*stride,&baked,sizeof(baked));
#endif
        }
        for(unsigned t=0;t<triangles;++t)
            if(!picaWheelLOD::Triangle(result->indices+(copy*triangles+t)*3,indices,indexCount,allocated,copy,t,strip))
            {delete result;return nullptr;}
    }
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    result->skinRigid=true;
#endif
    result->total=vertices;result->elemState=1;
    result->wheelLodBytes=bytes;wheelLodLiveBytes+=bytes;
    if(wheelLodLiveBytes>wheelLodPeakBytes) wheelLodPeakBytes=wheelLodLiveBytes;
    picaFlushForGpu(result->buffer,result->mem);
#ifdef SRR2_OLD3DS_DRAW_PREP
    result->PrepareDraw();
#endif
    return result;
}
#endif

#ifdef SRR2_OLD3DS_STATIC_BATCH
bool picaPrimBuffer::StaticBatchSource() const
{
    if(transient || !buffer || !indices || !allocated || allocated>65535 ||
       mem>picaStaticBatch::GroupLimit || wheelSourceVersion==~0u || elemState!=1 ||
       (primType!=PDDI_PRIM_TRIANGLES && primType!=PDDI_PRIM_TRISTRIP)) return false;
#ifdef SRR2_OLD3DS_LOAD_UPLOAD
    if(uploadPending) return false;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinAllocation || skinDrawing) return false;
#endif
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices) return false;
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    if(wheelPaletteBuffer) return false;
#endif
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    if(wheelSkinBuffer) return false;
#endif
    return picaStaticBatch::Triangles(indexCount,primType==PDDI_PRIM_TRISTRIP)!=0;
}
unsigned picaPrimBuffer::StaticBatchSize(picaPrimBuffer* const* sources,unsigned count,unsigned& vertices,unsigned& indexTotal)
{
    vertices=indexTotal=0;
    unsigned sourceBytes=0;
    if(count<2 || count>picaStaticBatch::Capacity) return 0;
    for(unsigned i=0;i<count;++i)
    {
        const auto* source=sources[i];
        if(!source || !source->StaticBatchSource()) return 0;
        const unsigned triangles=picaStaticBatch::Triangles(source->indexCount,source->primType==PDDI_PRIM_TRISTRIP);
        if(triangles>picaStaticBatch::GroupLimit/6 || sourceBytes>picaStaticBatch::GroupLimit-source->mem) return 0;
        sourceBytes+=source->mem;
        vertices+=source->allocated;indexTotal+=triangles*3;
    }
    if(vertices>65535 || vertices>picaStaticBatch::GroupLimit/36 || indexTotal>picaStaticBatch::GroupLimit/2) return 0;
    const unsigned bytes=vertices*36+((indexTotal*2+3)&~3u);
    return bytes<=picaStaticBatch::GroupLimit ? bytes : 0;
}
picaPrimBuffer* picaPrimBuffer::BakeStaticBatch(picaPrimBuffer* const* sources,unsigned count)
{
    unsigned vertices,indexTotal;
    const unsigned bytes=StaticBatchSize(sources,count,vertices,indexTotal);
    if(!bytes || linearSpaceFree()<picaStaticBatch::Reserve+bytes) return nullptr;
    auto* result=new picaPrimBuffer(sources[0]->context,PDDI_PRIM_TRIANGLES,
                                   sources[0]->vertexType|PDDI_V_NORMAL,vertices,indexTotal);
    if(!result->buffer) {delete result;return nullptr;}
    unsigned vertexOffset=0,indexOffset=0;
    for(unsigned i=0;i<count;++i)
    {
        const auto* source=sources[i];
        const bool strip=source->primType==PDDI_PRIM_TRISTRIP;
        picaStaticBatch::CopyVertices(result->buffer+vertexOffset*36,source->buffer,source->allocated,
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
                                     source->noNormalPacked
#else
                                     false
#endif
                                     );
        if(!picaStaticBatch::AppendIndices(result->indices+indexOffset,source->indices,source->indexCount,
                                           source->allocated,vertexOffset,strip)) {delete result;return nullptr;}
        vertexOffset+=source->allocated;
        indexOffset+=picaStaticBatch::Triangles(source->indexCount,strip)*3;
    }
    result->total=vertices;result->elemState=1;
    // Individual live source bounds are tested before this draw. A merged box
    // must not replace those tests or introduce a camera-dependent cache key.
    picaFlushForGpu(result->buffer,result->mem);
    result->PrepareDraw();
    return result;
}
void picaContext::ClearStaticBatches()
{
    for(auto& entry:staticBatches) entry.Reset();
    staticBatchBytes=0;
}
bool picaContext::DrawStaticBatch(pddiShader* material,picaPrimBuffer* const* sources,unsigned count)
{
    if(!g_picaInFrame || bottomPass || !material || !state.renderState->zEnabled || !state.renderState->zWrite ||
       state.stencilState->enabled || (state.renderState->zCompare!=PDDI_COMPARE_LESS &&
       state.renderState->zCompare!=PDDI_COMPARE_LESSEQUAL) || !static_cast<picaMat*>(material)->StaticBatchOpaque()) return false;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinProgramBound) return false;
#endif
    unsigned vertices,indexTotal;
    const unsigned bytes=picaPrimBuffer::StaticBatchSize(sources,count,vertices,indexTotal);
    if(!bytes) return false;
    picaStaticBatch::Entry<picaPrimBuffer>* found=nullptr;
    for(auto& entry:staticBatches)
        if(entry.Matches(sources,count)) {found=&entry;break;}
    if(!found)
    {
        ++staticBatchMisses;
        auto* oldest=&staticBatches[0];
        for(auto& entry:staticBatches)
            if(!entry.count || entry.lastFrame<oldest->lastFrame) {oldest=&entry;if(!entry.count) break;}
        staticBatchBytes-=oldest->bytes;oldest->Reset();
        oldest->Remember(sources,count,staticBatchFrame);
        return false;
    }
    if(found->lastFrame!=staticBatchFrame) {++found->seen;found->lastFrame=staticBatchFrame;}
    if(!found->draw)
    {
        // Require repeated use and build at most one small mesh per frame.
        if(found->seen<3 || staticBatchBuildFrame==staticBatchFrame || staticBatchBytes+bytes>picaStaticBatch::Budget) return false;
        staticBatchBuildFrame=staticBatchFrame;
        found->draw=picaPrimBuffer::BakeStaticBatch(sources,count);
        if(!found->draw) {++staticBatchFailures;return false;}
        found->draw->AddRef();found->bytes=bytes;staticBatchBytes+=bytes;++staticBatchBuilt;
        if(staticBatchBytes>staticBatchPeak) staticBatchPeak=staticBatchBytes;
    }
    // No callback or mutable source is crossed. Keep every original clip test,
    // shared-material update and native state boundary live on each submission.
    for(unsigned i=0;i<count;++i) if(sources[i]->CullNative()) return false;
    DrawPrimBuffer(material,found->draw);
    ++staticBatchHits;staticBatchAvoided+=count-1;
    return true;
}
#endif

picaPrimBuffer::~picaPrimBuffer()
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    context->FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityInvalidate();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    wheelLodLiveBytes-=wheelLodBytes;
#endif
#ifdef SRR2_OLD3DS_SKIN_CLIP
    if(skinClipBounds) {std::free(skinClipBounds);skinClipBytes-=sizeof(picaSkinClip::Bounds);}
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinAllocation) {
        // The GPU skin uses the original index allocation. Retire both together.
#ifdef SRR2_OLD3DS_DRAW_PREP
        skinAllocation->nativeBuffer = buffer ? (picaBufferAllocation*)buffer - 1 : NULL;
        skinAllocation->expandedBuffer = expanded ? (picaBufferAllocation*)expanded - 1 : NULL;
        buffer = expanded = NULL;
#else
        skinAllocation->nativeBuffer=buffer; buffer=NULL;
        skinAllocation->expandedBuffer=expanded; expanded=NULL;
#endif
        picaSkinRetire(skinAllocation);
    }
#endif
    delete stream;

    if(!transient)
    {
#ifdef SRR2_OLD3DS_DRAW_PREP
        picaBufferRetire(buffer);
        picaBufferRetire(expanded);
#else
        if(buffer)
            linearFree(buffer);
        if(expanded)
        {
            linearFree(expanded);
        }
#endif

        context->ADD_STAT(PDDI_STAT_BUFFERED_COUNT, -1);
        context->ADD_STAT(PDDI_STAT_BUFFERED_ALLOC, -(float)mem / 1024.0f);
    }
}

#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
void picaContext::SetWheelPaletteLayout(bool enabled)
{
    if(wheelPaletteLayout==enabled) return;
    wheelPaletteLayout=enabled;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(PreparedBatching() && QueuePreparedLayout(enabled ? &wheelPaletteAttrInfo :
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout ? &noNormalAttrInfo :
#endif
        &attrInfo,false))return;
    FlushPreparedExecution(true);
#endif
    C3D_SetAttrInfo(enabled ? &wheelPaletteAttrInfo :
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout ? &noNormalAttrInfo :
#endif
        &attrInfo);
    g_picaState.bufInfoSet=false;
}
bool picaContext::DrawWheelPalette(pddiShader* shader,picaPrimBuffer* buffer,const pddiMatrix* wheels)
{
    if(!g_picaInFrame || bottomPass || !buffer || !buffer->wheelPaletteBuffer) return false;
#ifdef SRR2_OLD3DS_GPU_SKIN
    if(skinProgramBound) return false;
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
    const pddiMatrix& view=*state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    for(unsigned i=0;i<4;++i)
    {
        pddiMatrix eye;eye.Mult(wheels[i],view);
        C3D_Mtx packed;
        for(unsigned row=0;row<4;++row)
        {
            packed.r[row].x=eye.Row4(0)[row];
            packed.r[row].y=eye.Row4(1)[row];
            packed.r[row].z=eye.Row4(2)[row];
            packed.r[row].w=eye.Row4(3)[row];
        }
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,VSH_FVEC_u_wheelMV+4*i,&packed);
    }
    C3D_BoolUnifSet(GPU_VERTEX_SHADER,0x6b,true);
    DrawPrimBuffer(shader,buffer);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
    C3D_BoolUnifSet(GPU_VERTEX_SHADER,0x6b,false);
    return true;
}
#endif

#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
bool picaContext::DrawWheelSkin(pddiShader* shader,picaPrimBuffer* buffer,const pddiMatrix* wheels)
{
    if(!g_picaInFrame || bottomPass || !buffer || !buffer->wheelSkinBuffer || !skinBinary ||
       skinProgramBound || !picaSkinEnabled()
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
       || skinBatchDepth
#endif
       ) return false;
    const pddiMatrix& view=*state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    float copiedView[4][4],packed[4][3][4];
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)copiedView[r][c]=view.Row4(r)[c];
    if(!picaSkin::EyeViewValid(copiedView)) return false;
    for(unsigned i=0;i<4;++i)
    {
        float matrix[4][4];
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)matrix[r][c]=wheels[i].Row4(r)[c];
        if(!picaSkin::PackEyeMatrixValidatedView(packed[i],matrix,copiedView)) return false;
    }
    // Reuse the already established rigid skin layout and program. The immutable
    // wheel buffer owns its packed vertices and indices in one fenced allocation.
    SetSkinLayout(true);
    if(!BindSkinProgram(true)) return false;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    picaPreparedSkinPalette ownedWheels;ownedWheels.count=4;
    memcpy(ownedWheels.values,packed,sizeof(packed));
    if(PreparedBatching() && QueuePreparedPalette(ownedWheels,20))++preparedBackendStats.skinCaptures;
    else
#endif
    {
        C3D_FVec* destination=C3D_FVUnifWritePtr(GPU_VERTEX_SHADER,20,12);
        for(unsigned i=0;i<4;++i)for(unsigned r=0;r<3;++r)
        {
            destination[3*i+r].x=packed[i][r][0];destination[3*i+r].y=packed[i][r][1];
            destination[3*i+r].z=packed[i][r][2];destination[3*i+r].w=packed[i][r][3];
        }
    }
    g_picaSkinMetrics.vectors+=12;
    buffer->skinDrawing=true;
    DrawPrimBuffer(shader,buffer);
    buffer->skinDrawing=false;
    BindSkinProgram(false);
    return true;
}
#endif

void picaPrimBuffer::SetFormat(pddiPrimType type, unsigned vertexFormat)
{
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityInvalidate();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    if(type!=primType || vertexFormat!=vertexType) TouchWheelSource();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    clipBoundsValid = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    ExpandNoNormal();
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(type != primType || vertexFormat != vertexType) drawPrepared = false;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(type!=primType || vertexFormat!=vertexType) MutateGpuSkin();
#endif
    primType   = type;
    if(vertexType != vertexFormat) defaultsValid = false;
    vertexType = vertexFormat;
}

void picaPrimBuffer::SetTransient(unsigned char* verts, unsigned short* idx, unsigned count)
{
    PDDIASSERT(transient);
    defaultsValid = false;
    buffer     = verts;
    indices    = idx;
    allocated  = count;
    indexCount = idx ? count : 0;
    total      = 0;
}

#ifdef SRR2_OLD3DS_QUAD_STREAM
void picaPrimBuffer::AppendQuad(const pddiVector* positions,const pddiVector2* coordinates,pddiColour tint)
{
    // BeginQuadStream owns the capacity and CT format. Write the same native
    // records as Vertex/Colour/UV/Coord, retaining Lock's default normals.
    // Advance stream pointers once so ordinary Unlock still rewinds them.
    PDDIASSERT(transient && vertexType==PDDI_V_CT && total+4<=allocated);
    const unsigned char rgba[4]={(unsigned char)tint.Red(),(unsigned char)tint.Green(),(unsigned char)tint.Blue(),(unsigned char)tint.Alpha()};
    unsigned char* output=buffer+total*PICA_VERTEX_STRIDE;
    for(unsigned i=0;i<4;++i,output+=PICA_VERTEX_STRIDE)
    {
        memcpy(output+PICA_VERTEX_OFF_POS,positions+i,3*sizeof(float));
        memcpy(output+PICA_VERTEX_OFF_UV,coordinates+i,2*sizeof(float));
        memcpy(output+PICA_VERTEX_OFF_COL,rgba,sizeof(rgba));
    }
    total+=4;
    coord=(float*)((unsigned char*)coord+4*PICA_VERTEX_STRIDE);
    uv=(float*)((unsigned char*)uv+4*PICA_VERTEX_STRIDE);
    colour+=4*PICA_VERTEX_STRIDE;
}
void picaPrimBuffer::AppendColourQuad(const pddiVector* positions,const pddiVector2* coordinates,const pddiColour* colours)
{
    // Sparkles retain a distinct color at every corner, including flat-shaded
    // triangles. The generated index order preserves each provoking vertex.
    PDDIASSERT(transient && vertexType==PDDI_V_CT && total+4<=allocated);
    unsigned char* output=buffer+total*PICA_VERTEX_STRIDE;
    for(unsigned i=0;i<4;++i,output+=PICA_VERTEX_STRIDE)
    {
        memcpy(output+PICA_VERTEX_OFF_POS,positions+i,3*sizeof(float));
        memcpy(output+PICA_VERTEX_OFF_UV,coordinates+i,2*sizeof(float));
        output[PICA_VERTEX_OFF_COL]=colours[i].Red();
        output[PICA_VERTEX_OFF_COL+1]=colours[i].Green();
        output[PICA_VERTEX_OFF_COL+2]=colours[i].Blue();
        output[PICA_VERTEX_OFF_COL+3]=colours[i].Alpha();
    }
    total+=4;
    coord=(float*)((unsigned char*)coord+4*PICA_VERTEX_STRIDE);
    uv=(float*)((unsigned char*)uv+4*PICA_VERTEX_STRIDE);
    colour+=4*PICA_VERTEX_STRIDE;
}

#endif

pddiPrimBufferStream* picaPrimBuffer::Lock()
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // Simulation may now run before the preceding render consumer/GPU finish.
    // Join before exposing writable storage during that outside-frame interval.
    if((!g_picaUnexposedResourcePrep || renderExposed) && !g_picaInFrame && (context->PreparedFrameInFlight() || context->preparedGpuPending))picaPreparedBackendFence();
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityInvalidate();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    clipBoundsValid = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    ExpandNoNormal();
#endif
#ifdef SRR2_OLD3DS_SKIN_RING
    skinVertices = NULL;
#endif
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_VERTEXLOCK, allocated);
#endif
    total = 0;

    if(buffer == NULL)
        return stream;

    unsigned char* ptr = buffer;
    coord  = (float*)(ptr + PICA_VERTEX_OFF_POS);
    normal = (vertexType & PDDI_V_NORMAL) ? (float*)(ptr + PICA_VERTEX_OFF_NRM) : NULL;
    uv     = (vertexType & 0xf)           ? (float*)(ptr + PICA_VERTEX_OFF_UV)  : NULL;
    colour = (vertexType & PDDI_V_COLOUR) ? (ptr + PICA_VERTEX_OFF_COL)         : NULL;

#ifdef SRR2_OLD3DS_DEFAULT_CACHE
    // Only absent attributes are written here. Stream writers cannot touch
    // them. Transient allocations and format changes invalidate the cache.
    if(!transient && defaultsValid) return stream;
#endif
#ifdef SRR2_OLD_3DS
    Old3dsCount(O3_DEFAULT_VERTS, allocated);
#endif
    for(unsigned i = 0; i < allocated; i++)
    {
        unsigned char* v = buffer + (i * PICA_VERTEX_STRIDE);
        if(!normal)
        {
            float* n = (float*)(v + PICA_VERTEX_OFF_NRM);
            n[0] = 0.0f; n[1] = 0.0f; n[2] = 1.0f;
        }
        if(!uv)
        {
            float* t = (float*)(v + PICA_VERTEX_OFF_UV);
            t[0] = 0.0f; t[1] = 0.0f;
        }
        if(!colour)
        {
            unsigned char* c = v + PICA_VERTEX_OFF_COL;
            c[0] = 255; c[1] = 255; c[2] = 255; c[3] = 255;
        }
    }

    defaultsValid = true;

    return stream;
}

#ifdef SRR2_OLD3DS_SKIN_DIRECT
unsigned char* picaPrimBuffer::LockSkin(unsigned count)
{
    if(transient || !buffer || count > allocated) return NULL;
    Lock();
    // Unlock rewinds attribute pointers by total; match the regular stream.
    total = count;
    coord = (float*)((unsigned char*)coord + count * PICA_VERTEX_STRIDE);
    if(normal) normal = (float*)((unsigned char*)normal + count * PICA_VERTEX_STRIDE);
    if(uv) uv = (float*)((unsigned char*)uv + count * PICA_VERTEX_STRIDE);
    if(colour) colour += count * PICA_VERTEX_STRIDE;
#ifdef SRR2_OLD3DS_SKIN_RING
    // Each draw owns its output until the immediate ring retires. Static UVs,
    // colours and absent normals remain in the persistent source buffer.
    // Invalid/partial buffers retain the legacy expansion and update path.
    if(count == allocated && g_picaInFrame)
    {
        if(elemState < 0) elemState = IndicesInRange() ? 1 : 0;
        if(elemState == 1)
        {
            skinVertices = context->AllocSkinVertices(count);
            if(skinVertices)
            {
                for(unsigned i = 0; i < count; ++i)
                {
                    unsigned char* dst = skinVertices + i * PICA_VERTEX_STRIDE;
                    const unsigned char* src = buffer + i * PICA_VERTEX_STRIDE;
                    if(!(vertexType & PDDI_V_NORMAL))
                        memcpy(dst + PICA_VERTEX_OFF_NRM, src + PICA_VERTEX_OFF_NRM, 12);
                    memcpy(dst + PICA_VERTEX_OFF_UV, src + PICA_VERTEX_OFF_UV, 12);
                }
                return skinVertices;
            }
        }
    }
#endif
    return buffer;
}
#endif

static inline void picaFlushForGpu(const void* p, unsigned bytes)
{
    if(p != NULL && bytes != 0)
        GSPGPU_FlushDataCache(p, bytes);
}

#ifdef SRR2_OLD3DS_LOAD_UPLOAD
void picaPrimBuffer::EndUpload()
{
    if(!uploadPending) return;
    uploadPending = false;
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    UpdateClipBounds();
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    if(noNormalAllowed && !noNormalPacked && !transient && buffer &&
       !(vertexType & PDDI_V_NORMAL) && IndicesInRange())
    {
        picaNoNormal::Pack(buffer, allocated);
        noNormalPacked = true;
    }
#endif
    // The loader has not published this group or submitted draws from it.
    // This includes the constructor's sequential indices and every vertex field.
    if(!transient) picaFlushForGpu(buffer, mem);
#ifdef SRR2_OLD3DS_DRAW_PREP
    PrepareDraw();
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityBuild();
#endif
}
#endif

void picaPrimBuffer::Unlock(pddiPrimBufferStream*)
{
#ifdef SRR2_OLD3DS_WHEEL_LOD
    TouchWheelSource();
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    queueBoundsValid = false;
#endif
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_VERTEXFLUSH, transient ? 0 : mem);
#endif
    if(coord)
        coord = (float*)((char*)coord - (total * PICA_VERTEX_STRIDE));
    if(normal)
        normal = (float*)((char*)normal - (total * PICA_VERTEX_STRIDE));
    if(uv)
        uv = (float*)((char*)uv - (total * PICA_VERTEX_STRIDE));
    if(colour)
        colour -= (total * PICA_VERTEX_STRIDE);

#ifdef SRR2_OLD3DS_SKIN_RING
    // EndFrame (and every earlier frame split) flushes the ring once. No GPU
    // commands may execute before that flush. Persistent indices are unchanged.
    if(skinVertices) return;
#endif
    if(!transient)
    {
#ifdef SRR2_OLD_3DS
        Old3dsCount(O3_VERTEX_RELOCK);
        if(elemState >= 0) Old3dsCount(O3_RESCAN_CANDIDATE, indexCount);
#endif
#ifdef SRR2_OLD3DS_LOAD_UPLOAD
        if(!uploadPending)
#endif
        picaFlushForGpu(buffer, mem);
        expandDirty = true;
        // Vertex-only updates cannot change index validity. Index writers own
        // invalidation; expanded vertex data must still be rebuilt after this.
#ifndef SRR2_OLD3DS_INDEX_CACHE
        elemState = -1;
#endif
    }
}

unsigned char* picaPrimBuffer::LockIndexBuffer()
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if((!g_picaUnexposedResourcePrep || renderExposed) && !g_picaInFrame && (context->PreparedFrameInFlight() || context->preparedGpuPending))picaPreparedBackendFence();
#endif
    return (unsigned char*)indices;
}

void picaPrimBuffer::UnlockIndexBuffer(int count)
{
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityInvalidate();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    TouchWheelSource();
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    drawPrepared = false;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    MutateGpuSkin();
#endif
    if(count > 0)
        indexCount = (unsigned)count;

    if(!transient)
    {
#ifdef SRR2_OLD3DS_LOAD_UPLOAD
        if(!uploadPending)
#endif
        picaFlushForGpu(indices, indexCount * (unsigned)sizeof(unsigned short));
        expandDirty = true; elemState = -1;
    }
}

void picaPrimBuffer::SetIndices(unsigned short* i, int count)
{
#ifdef SRR2_OLD3DS_OCCLUSION
    VisibilityInvalidate();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    TouchWheelSource();
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    drawPrepared = false;
#endif
    if(!indices)
        return;
    PDDIASSERT(count <= (int)indexCount);
    if(count > (int)indexCount)
        count = (int)indexCount;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    MutateGpuSkin();
#endif
    memcpy(indices, i, count * sizeof(unsigned short));

    if(!transient)
    {
#ifdef SRR2_OLD3DS_LOAD_UPLOAD
        if(!uploadPending)
#endif
        picaFlushForGpu(indices, count * (unsigned)sizeof(unsigned short));
        expandDirty = true; elemState = -1;
    }
}

bool picaPrimBuffer::IndicesInRange(void) const
{
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_INDEXSCAN, indexCount);
#endif
    if(indices == NULL || indexCount == 0 || allocated == 0)
        return false;
    for(unsigned i = 0; i < indexCount; i++)
    {
        if(indices[i] >= allocated)
        {
#ifdef SRR2_OLD_3DS
            Old3dsCount(O3_INDEX_TESTED, i + 1);
#endif
            return false;
        }
    }
#ifdef SRR2_OLD_3DS
    Old3dsCount(O3_INDEX_TESTED, indexCount);
#endif
    return true;
}

bool picaPrimBuffer::BuildExpanded(void)
{
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    ExpandNoNormal();
#endif
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_EXPAND, indexCount);
#endif
    if(!expandDirty && expanded != NULL)
        return true;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    context->FlushPreparedExecution(true);
#endif

    const unsigned needed = indexCount * PICA_VERTEX_STRIDE;
    if(needed == 0)
        return false;

    if(expanded != NULL && expandedBytes < needed)
    {
#ifdef SRR2_OLD3DS_DRAW_PREP
        picaBufferRetire(expanded);
#else
        linearFree(expanded);
#endif
        expanded = NULL;
        expandedBytes = 0;
    }

    if(expanded == NULL)
    {
#ifdef SRR2_OLD3DS_DRAW_PREP
        expanded = (unsigned char*)picaBufferAllocate(needed);
#else
        expanded = (unsigned char*)linearAlloc(needed);
#endif
        if(expanded == NULL)
        {
#ifdef SRR2_OLD_3DS
            // Linear failures are reported separately from the ordinary heap.
            ++g_old3dsLinearFailures;
            if(needed > g_old3dsLinearFailureBytes)
                g_old3dsLinearFailureBytes = needed;
#endif
            return false;
        }
        expandedBytes = needed;
    }

#ifdef SRR2_OLD_3DS
    Old3dsCount(O3_EXPAND_REBUILD);
#endif
    const unsigned char* source = buffer;
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices) source = skinVertices;
#endif
    for(unsigned i = 0; i < indexCount; i++)
    {
        unsigned v = indices[i];
        if(v >= allocated)
            v = 0;
        memcpy(expanded + (i * PICA_VERTEX_STRIDE),
               source + (v * PICA_VERTEX_STRIDE),
               PICA_VERTEX_STRIDE);
    }

    picaFlushForGpu(expanded, needed);
    expandDirty = false;
    return true;
}

#ifdef SRR2_OLD3DS_DRAW_PREP
void picaPrimBuffer::PrepareDraw()
{
    // This record borrows only this buffer's allocation. No frame, material,
    // pose, target or program state is retained. Transient streams stay live.
    drawPrepared = false;
    if(transient || !buffer || !PrimIsDrawable(primType) || !indices) return;
    if(elemState < 0) elemState = IndicesInRange() ? 1 : 0;
    if(elemState != 1) return;
    nativePrimitive = primTypeTable[primType];
    nativeOffset = osConvertVirtToPhys(buffer) - PICA_BUFINFO_BASE;
    nativeCount = indexCount;
    drawPrepared = true;
}
#endif

OLD3DS_HOT_PATH
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
bool picaPrimBuffer::CapturePreparedGeometry(picaPreparedGeometry& output)
{
    O3_MAIN_SCOPE(GeometryCapture);
    const unsigned char* vertexData = buffer;
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices) vertexData = skinVertices;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinDrawing
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
       && !wheelSkinBuffer
#endif
      ) vertexData = (const unsigned char*)(skinAllocation + 1);
#endif
    unsigned count;

    bool useElements = false;
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(!drawPrepared && !transient) PrepareDraw();
    if(drawPrepared)
    {
        useElements = true;
        count = nativeCount;
    }
    else
    {
#endif
    if(indices != NULL
#ifndef SRR2_OLD3DS_SHADOW_MESH
       && !transient
#endif
      )
    {
        if(elemState < 0)
            elemState = IndicesInRange() ? 1 : 0;
        useElements = (elemState == 1);
    }

    if(useElements)
    {
        count = indexCount;
    }
    else if(indices != NULL)
    {
        if(!BuildExpanded())
        {
            return false;
        }
        vertexData = expanded;
        count = indexCount;
#ifdef SRR2_OLD_3DS
        Old3dsCount(O3_EXPANDED_DRAW);
#endif
    }
    else
    {
        count = total ? total : allocated;
    }

#ifdef SRR2_OLD3DS_DRAW_PREP
    }
#endif
    if(count == 0)
        return false;

    output.vertices=vertexData;output.indices=indices;output.indexed=useElements;output.count=count;
    output.skin=false;output.omitNormal=false;output.wheelPalette=false;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    output.skin=skinDrawing;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    output.omitNormal=noNormalPacked && vertexData==buffer;
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    output.wheelPalette=wheelPaletteBuffer;
#endif
    output.stride=PICA_VERTEX_STRIDE;output.attributes=4;output.permutation=0x3210;
    if(output.skin)
    {
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
        output.stride=picaSkin::PackedStride(skinRigid);
#else
        output.stride=picaSkin::VertexStride;
#endif
        output.attributes=6;output.permutation=0x543210;
    }
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    else if(output.omitNormal){output.stride=picaNoNormal::Stride;output.attributes=3;output.permutation=0x320;}
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    else if(output.wheelPalette)output.stride=sizeof(picaWheelLOD::PaletteVertex);
#endif
    output.offset=
#ifdef SRR2_OLD3DS_DRAW_PREP
        drawPrepared && vertexData==buffer?nativeOffset:
#endif
        osConvertVirtToPhys(vertexData)-PICA_BUFINFO_BASE;
    output.primitive=
#ifdef SRR2_OLD3DS_DRAW_PREP
        drawPrepared?nativePrimitive:
#endif
        primTypeTable[primType];
    ++preparedBackendStats.geometryCaptures;
    preparedBackendStats.skinCaptures+=output.skin;
    return true;
}
#endif

void picaPrimBuffer::Display(void)
{
    O3_MAIN_SCOPE(PrimDisplay); O3_RECUR(Submit);
#ifdef SRR2_OLD3DS_OCCLUSION
    context->VisibilityBarrier();
#endif
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_SUBMIT);
#endif
#ifdef SRR2_OLD_3DS
    Old3dsOperationScope timing(O3_SUBMIT, 1);
#endif
    if(!g_picaInFrame)
    {
#ifdef SRR2_OLD_3DS
        ++g_old3dsDrawDroppedNoFrame;
#endif
        return;
    }

    if(!buffer || !PrimIsDrawable(primType))
    {
#ifdef SRR2_OLD_3DS
        ++g_old3dsDrawDroppedNotDrawable;
#endif
        return;
    }

#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // A recorded draw owns its backing allocation before any deferred consumer
    // sees it. Never treat this resource as unpublished again, including A/B flips.
    renderExposed=true;
#endif

    #ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentDraw(indices?indexCount:allocated);
    if(g_o3WorldDepth && g_o3Mode==O3_SUPPRESS) return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    picaPreparedGeometry preparedGeometry;
    const unsigned char* vertexData;
    unsigned count;
    bool useElements=false;
    if(g_picaPreparedBackendEnabled)
    {
        if(!CapturePreparedGeometry(preparedGeometry))return;
        vertexData=preparedGeometry.vertices;count=preparedGeometry.count;useElements=preparedGeometry.indexed;
    }
    else
    {
        vertexData = buffer;
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices) vertexData = skinVertices;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinDrawing
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
       && !wheelSkinBuffer
#endif
      ) vertexData = (const unsigned char*)(skinAllocation + 1);
#endif



#ifdef SRR2_OLD3DS_DRAW_PREP
    if(!drawPrepared && !transient) PrepareDraw();
    if(drawPrepared)
    {
        useElements = true;
        count = nativeCount;
    }
    else
    {
#endif
    if(indices != NULL
#ifndef SRR2_OLD3DS_SHADOW_MESH
       && !transient
#endif
      )
    {
        if(elemState < 0)
            elemState = IndicesInRange() ? 1 : 0;
        useElements = (elemState == 1);
    }

    if(useElements)
    {
        count = indexCount;
    }
    else if(indices != NULL)
    {
        if(!BuildExpanded())
        {
            return;
        }
        vertexData = expanded;
        count = indexCount;
#ifdef SRR2_OLD_3DS
        Old3dsCount(O3_EXPANDED_DRAW);
#endif
    }
    else
    {
        count = total ? total : allocated;
    }

#ifdef SRR2_OLD3DS_DRAW_PREP
    }
#endif
    if(count == 0)
        return;

    }
#else
    const unsigned char* vertexData = buffer;
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices) vertexData = skinVertices;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinDrawing
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
       && !wheelSkinBuffer
#endif
      ) vertexData = (const unsigned char*)(skinAllocation + 1);
#endif
    unsigned count;

    bool useElements = false;
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(!drawPrepared && !transient) PrepareDraw();
    if(drawPrepared)
    {
        useElements = true;
        count = nativeCount;
    }
    else
    {
#endif
    if(indices != NULL
#ifndef SRR2_OLD3DS_SHADOW_MESH
       && !transient
#endif
      )
    {
        if(elemState < 0)
            elemState = IndicesInRange() ? 1 : 0;
        useElements = (elemState == 1);
    }

    if(useElements)
    {
        count = indexCount;
    }
    else if(indices != NULL)
    {
        if(!BuildExpanded())
        {
            return;
        }
        vertexData = expanded;
        count = indexCount;
#ifdef SRR2_OLD_3DS
        Old3dsCount(O3_EXPANDED_DRAW);
#endif
    }
    else
    {
        count = total ? total : allocated;
    }

#ifdef SRR2_OLD3DS_DRAW_PREP
    }
#endif
    if(count == 0)
        return;

#endif

    context->ApplyViewport();
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    // Tiny raster region; retain viewport, vertices, material and CPU traversal.
    if(g_o3WorldDepth && g_o3Mode==O3_COVERAGE)
        C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,4,4);
    else context->SetScissor(&context->state.viewState->scissor);
#endif

    {
        u32* cmdBuf    = NULL;
        u32  cmdSize   = 0;
        u32  cmdOffset = 0;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        const bool batchRoom=!context->PreparedBatching() || context->PreparedGeometryRoom();
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
        context->PreparedCommandBuffer(&cmdBuf,&cmdSize,&cmdOffset);
#else
        GPUCMD_GetBuffer(&cmdBuf, &cmdSize, &cmdOffset);
#endif

        const u32 headroom = 2048;

#ifdef SRR2_OLD_3DS
        // Track command-buffer pressure so a full buffer cannot be mistaken for
        // an optimization. A dropped draw is missing geometry, not saved work.
        if(cmdBuf != NULL)
        {
            if(cmdOffset > g_old3dsCmdHighWater) g_old3dsCmdHighWater = cmdOffset;
            g_old3dsCmdSize = cmdSize;
#ifdef SRR2_OLD3DS_QUEUE_METRICS
            if(cmdOffset > g_picaQueueMetrics.commandHighWater) g_picaQueueMetrics.commandHighWater = cmdOffset;
            g_picaQueueMetrics.commandSize = cmdSize;
#endif
        }
#endif

        if(cmdBuf == NULL || (cmdOffset + headroom) > cmdSize
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
           || !batchRoom
#endif
          )
        {
#ifdef SRR2_OLD_3DS
            ++g_old3dsDrawDroppedNoRoom;
#endif
            return;
        }
    }

#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    const bool omitNormal = noNormalPacked && vertexData == buffer;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(!skinDrawing)
#endif
        context->SetNoNormalLayout(omitNormal);
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(!skinDrawing)
#endif
        context->SetWheelPaletteLayout(wheelPaletteBuffer);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(context->PreparedBatching()){}
    else if(g_picaPreparedBackendEnabled)picaBindPreparedGeometry(preparedGeometry,g_picaState.bufInfoSet);
    else
    {
#endif
    if(!g_picaState.bufInfoSet)
    {
        C3D_BufInfo bufInfo;
        BufInfo_Init(&bufInfo);
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        if(skinDrawing) BufInfo_Add(&bufInfo, vertexData,
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
                                   picaSkin::PackedStride(skinRigid),
#else
                                   picaSkin::VertexStride,
#endif
                                   6, 0x543210);
        else
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        if(omitNormal) BufInfo_Add(&bufInfo, vertexData, picaNoNormal::Stride, 3, 0x320);
        else
#endif
        BufInfo_Add(&bufInfo, vertexData,
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
                    wheelPaletteBuffer ? sizeof(picaWheelLOD::PaletteVertex) :
#endif
                    PICA_VERTEX_STRIDE, 4, 0x3210);
        C3D_SetBufInfo(&bufInfo);
        C3Di_UpdateContext();
        g_picaState.bufInfoSet = true;
    }
    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER0_OFFSET,
#ifdef SRR2_OLD3DS_DRAW_PREP
                    drawPrepared && vertexData == buffer ? nativeOffset :
#endif
                    osConvertVirtToPhys(vertexData) - PICA_BUFINFO_BASE);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    }
#endif

#ifdef SRR2_OLD_3DS
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    ++g_picaQueueMetrics.draws;
    g_picaQueueMetrics.vertices += count;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    ++g_picaSkinMetrics.draws; g_picaSkinMetrics.submittedVertices+=count;
#endif
    ++g_old3dsDrawCalls;
    g_old3dsDrawVerts += count;
    if(g_old3dsActiveEntity >= 0)
    {
        Old3dsCount(useElements ? O3_INDEXED : O3_ARRAY);
        if(transient) Old3dsCount(O3_TRANSIENT);
        Old3dsEntityStats& e = g_old3dsEntities[g_old3dsActiveEntity];
        if(useElements) ++e.indexed; else ++e.arrays;
        e.blended += g_old3dsDrawBlended;
        e.alphaTested += g_old3dsDrawAlphaTested;
    }
#endif

#ifdef SRR2_OLD3DS_DRAW_PREP
    const GPU_Primitive_t primitive = drawPrepared ? nativePrimitive : primTypeTable[primType];
#else
    const GPU_Primitive_t primitive = primTypeTable[primType];
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(context->PreparedBatching())context->QueuePreparedGeometry(preparedGeometry);
    else if(g_picaPreparedBackendEnabled)picaSubmitPreparedGeometry(preparedGeometry);
    else
    {
#endif
#ifdef SRR2_OLD3DS_DRAW_PACKET
    if(useElements)
        picaDrawPacketElements(primitive,(int)count,C3D_UNSIGNED_SHORT,indices);
    else
        picaDrawPacketArrays(primitive,0,(int)count);
#else
    if(useElements)
        C3D_DrawElements(primitive, (int)count, C3D_UNSIGNED_SHORT, indices);
    else
        C3D_DrawArrays(primitive, 0, (int)count);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    }
#endif
}

void picaContext::BottomMapPoint(float u, float v, float* bu, float* bv) const
{
    *bu = bottomDst[0] + (u - bottomSrc[0]) * (bottomDst[2] - bottomDst[0]) / (bottomSrc[2] - bottomSrc[0]);
    *bv = bottomDst[1] + (v - bottomSrc[1]) * (bottomDst[3] - bottomDst[1]) / (bottomSrc[3] - bottomSrc[1]);
}

bool picaContext::BeginBottomScreen(float u0, float v0, float u1, float v1, bool fitFrame)
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(g_picaPreparedExecution!=7 && g_picaPreparedExecution!=9 && g_picaPreparedExecution!=10)
#endif
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
#endif
    if(bottomPass || (u1 <= u0) || (v1 <= v0) || !display->BeginBottomTarget())
        return false;

    bottomSrc[0] = u0; bottomSrc[1] = v0; bottomSrc[2] = u1; bottomSrc[3] = v1;

    const float srcW = (u1 - u0) * 640.0f;
    const float srcH = (v1 - v0) * 480.0f;
    const float availableWidth=fitFrame ? picaBottomFrame::Right-picaBottomFrame::Left : PICA_BOTTOM_WIDTH;
    const float availableHeight=fitFrame ? picaBottomFrame::Bottom-picaBottomFrame::Top : PICA_BOTTOM_HEIGHT;
    float k=availableWidth/srcW;
    if(availableHeight/srcH<k)k=availableHeight/srcH;
    const float dw=srcW*k/PICA_BOTTOM_WIDTH,dh=srcH*k/PICA_BOTTOM_HEIGHT;
    const float cx=(picaBottomFrame::Left+picaBottomFrame::Right)/(2*PICA_BOTTOM_WIDTH);
    const float cy=(picaBottomFrame::Top+picaBottomFrame::Bottom)/(2*PICA_BOTTOM_HEIGHT);
    bottomDst[0]=cx-0.5f*dw;bottomDst[1]=cy-0.5f*dh;
    bottomDst[2]=cx+0.5f*dw;bottomDst[3]=cy+0.5f*dh;

    bottomPass = true;
#ifdef SRR2_OLD3DS_COARSE_POOLS
    bottomPoolPrevious = Old3dsPools::Select(Old3dsPools::Presentation,Old3dsPools::Bottom);
#endif

    picaStateInvalidate();
    InvalidateUniformCache();
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    SubmitPreparedScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
#else
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
#endif
    hwViewport[0] = hwViewport[1] = hwViewport[2] = hwViewport[3] = -1;
    SetupHardwareProjection();
    return true;
}

void picaContext::DrawBottomFrame(bool staticPicture)
{
    if(!bottomPass)return;
    // The surround is one untextured draw. Its geometry is shared between frames.
    const auto& mesh=picaBottomFrame::Bezel();
    if(staticPicture && !bottomStaticAttempted){
        bottomStaticAttempted=true;
        picaTexture* texture=new picaTexture(this);texture->AddRef();
        if(texture->Create(64,64,32,0,1)){
            pddiLockInfo* lock=texture->Lock(0);
            if(lock){
                unsigned seed=0x6d2b79f5;
                for(unsigned y=0;y<64;++y){
                    auto* row=reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock->bits)+y*lock->pitch);
                    for(unsigned x=0;x<64;++x){
                        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
                        unsigned grey=32+(seed>>26)*3;if(y&1)grey=grey*3/4;
                        row[x]=0xff000000u|(grey<<16)|(grey<<8)|grey;
                    }
                }
                texture->Unlock(0);
                bottomStaticShader=new picaMat(this);bottomStaticShader->AddRef();
                bottomStaticShader->SetTexture(PDDI_SP_BASETEX,texture);
                bottomStaticShader->SetInt(PDDI_SP_FILTER,PDDI_FILTER_NONE);
                bottomStaticShader->SetInt(PDDI_SP_UVMODE,PDDI_UV_TILE);
            }
        }
        texture->Release();
    }

    float source[4],destination[4];memcpy(source,bottomSrc,sizeof(source));memcpy(destination,bottomDst,sizeof(destination));
    bottomSrc[0]=bottomSrc[1]=bottomDst[0]=bottomDst[1]=0;
    bottomSrc[2]=bottomSrc[3]=bottomDst[2]=bottomDst[3]=1;
    const auto mask=(pddiStateMask)(PDDI_STATE_RENDER|PDDI_STATE_VIEW|PDDI_STATE_FOG|PDDI_STATE_STENCIL);
    PushState(mask);
    SetCamera(0,1,1,1);SetProjectionMode(PDDI_PROJECTION_DEVICE);
    EnableZBuffer(false);SetZWrite(false);SetCullMode(PDDI_CULL_NONE);
    EnableStencilBuffer(false);EnableFog(false);SetColourWrite(true,true,true,true);
    PushIdentityMatrix(PDDI_MATRIX_MODELVIEW);
    const unsigned phase=unsigned(svcGetSystemTick()*24/SYSCLOCK_ARM11);
    if(staticPicture && bottomStaticShader){
        pddiPrimStream* noise=BeginPrims(bottomStaticShader,PDDI_PRIM_TRISTRIP,PDDI_V_CT,4);
        if(noise){
            const float u=float((phase*17)&63)/64,v=float((phase*29)&63)/64;
            for(unsigned i=0;i<4;++i){
                pddiVector point;point.Set((i&1)?float(display->GetWidth()):0,(i&2)?float(display->GetHeight()):0,0);
                pddiVector2 uv;uv.u=u+((i&1)?5:0);uv.v=v+((i&2)?4:0);
                noise->Vertex(&point,pddiColour(0xffffffff),&uv);
            }
            EndPrims(noise);
        }
    }
    pddiPrimStream* stream=BeginPrims(defaultShader,PDDI_PRIM_TRIANGLES,PDDI_V_C,mesh.used+(staticPicture?12:0));
    if(stream){
        if(staticPicture){
            const float bar=float(phase%80)*3-8;
            for(unsigned strip=0;strip<2;++strip){
                const unsigned corners[6]={0,1,2,2,1,3};
                for(unsigned i=0;i<6;++i){
                    const unsigned c=corners[i];pddiVector point;
                    const float y=bar+strip*8+((c&2)?(strip?1:4):0);
                    point.Set((c&1)?float(display->GetWidth()):0,y*display->GetHeight()/PICA_BOTTOM_HEIGHT,0);
                    stream->Vertex(&point,pddiColour(strip?0xff7b7c83:0xff292731));
                }
            }
        }
        for(unsigned i=0;i<mesh.used;++i){
            const auto& vertex=mesh.vertices[i];pddiVector point;
            point.Set(vertex.position.x*display->GetWidth()/PICA_BOTTOM_WIDTH,
                vertex.position.y*display->GetHeight()/PICA_BOTTOM_HEIGHT,0);
            stream->Vertex(&point,pddiColour(vertex.colour));
        }
        EndPrims(stream);
    }
    PopMatrix(PDDI_MATRIX_MODELVIEW);
    memcpy(bottomSrc,source,sizeof(source));memcpy(bottomDst,destination,sizeof(destination));
    PopState(mask);
}

void picaContext::DrawBottomPanel(int selection,int count)
{
    if(!bottomPass)return;
    float source[4],destination[4];
    memcpy(source,bottomSrc,sizeof(source));memcpy(destination,bottomDst,sizeof(destination));
    bottomSrc[0]=bottomSrc[1]=bottomDst[0]=bottomDst[1]=0;
    bottomSrc[2]=bottomSrc[3]=bottomDst[2]=bottomDst[3]=1;
    const auto mask=(pddiStateMask)(PDDI_STATE_RENDER|PDDI_STATE_VIEW|PDDI_STATE_FOG|PDDI_STATE_STENCIL);
    PushState(mask);PushIdentityMatrix(PDDI_MATRIX_MODELVIEW);
    SetCamera(0,1,1,1);SetProjectionMode(PDDI_PROJECTION_DEVICE);
    EnableZBuffer(false);SetZWrite(false);SetCullMode(PDDI_CULL_NONE);
    EnableStencilBuffer(false);EnableFog(false);SetColourWrite(true,true,true,true);
    const bool menu=count>0 && count<=6;
    const bool selected=menu && selection>=0 && selection<count;
    const int rectangles=menu ? count+2+(selected?2:0) : 1;
    pddiPrimStream* stream=BeginPrims(defaultShader,PDDI_PRIM_TRIANGLES,PDDI_V_C,6*rectangles);
    if(stream){
        const auto rectangle=[&](float left,float top,float right,float bottom,unsigned colour){
            const unsigned corners[6]={0,1,2,2,1,3};
            for(unsigned c:corners){
                pddiVector point;
                point.Set(((c&1)?right:left)*display->GetWidth()/PICA_BOTTOM_WIDTH,
                    ((c&2)?bottom:top)*display->GetHeight()/PICA_BOTTOM_HEIGHT,0);
                stream->Vertex(&point,pddiColour(colour));
            }
        };
        if(menu){
            rectangle(34,201,286,220,0xff201c32);
            rectangle(34,199,286,200,0xff6f658b);
            for(int row=0;row<count;++row){
                const float y=float(picaBottomFrame::MenuTop(count)+row*picaBottomFrame::MenuRowHeight);
                rectangle(35,y,285,y+picaBottomFrame::MenuRowHeight-2,0xff201c32);
            }
        }
        else rectangle(0,0,320,240,0xff201c32);
        if(selected){
            const float y=float(picaBottomFrame::MenuTop(count)+selection*picaBottomFrame::MenuRowHeight);
            rectangle(35,y,285,y+picaBottomFrame::MenuRowHeight-1,0xff554466);
            rectangle(35,y,39,y+picaBottomFrame::MenuRowHeight-1,0xffffd642);
        }
        EndPrims(stream);
    }
    PopMatrix(PDDI_MATRIX_MODELVIEW);
    memcpy(bottomSrc,source,sizeof(source));memcpy(bottomDst,destination,sizeof(destination));
    PopState(mask);
}

extern "C" bool picaBottomScreenInterfaceBegin(void)
{
    return s_picaContext && s_picaContext->BeginBottomScreen(0,0,1,1,false);
}

extern "C" void picaBottomScreenPanel(int selection,int count)
{
    if(s_picaContext)s_picaContext->DrawBottomPanel(selection,count);
}

extern "C" void picaBottomScreenStatic(void)
{
    if(s_picaContext)s_picaContext->DrawBottomFrame(true);
}

extern "C" void picaBottomScreenIdle(void)
{
    if(s_picaContext && s_picaContext->BeginBottomScreen(0,0,1,1)){
        s_picaContext->DrawBottomFrame(true);s_picaContext->EndBottomScreen();
    }
}

extern "C" void picaBottomScreenFrame(void)
{
    if(s_picaContext)s_picaContext->DrawBottomFrame();
}

void picaContext::EndBottomScreen(void)
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(g_picaPreparedExecution!=7 && g_picaPreparedExecution!=9 && g_picaPreparedExecution!=10)
#endif
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
#endif
    if(!bottomPass)
        return;

    bottomPass = false;
    display->EndBottomTarget();
#ifdef SRR2_OLD3DS_COARSE_POOLS
    Old3dsPools::Restore(bottomPoolPrevious); bottomPoolPrevious=-1;
#endif

    picaStateInvalidate();
    InvalidateUniformCache();
    hwViewport[0] = 0;
    hwViewport[1] = 0;
    hwViewport[2] = PICA_SCREEN_HEIGHT;
    hwViewport[3] = PICA_SCREEN_WIDTH;
    SetupHardwareProjection();
    SetScissor(&state.viewState->scissor);
    ApplyFogState();
}

extern "C" bool picaBottomScreenBegin(float u0, float v0, float u1, float v1)
{
    return (s_picaContext != NULL) && s_picaContext->BeginBottomScreen(u0, v0, u1, v1);
}

extern "C" void picaBottomScreenEnd(void)
{
    if(s_picaContext != NULL)
        s_picaContext->EndBottomScreen();
}

#ifdef SRR2_OLD_3DS
void picaContext::BeginWorld(void)
{
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    picaVisibilityReset(false);
    visibilityWorld=true;
#endif
    if(display->BeginWorldTarget())
    {
        // Reserve presentation vertices before world geometry consumes the ring.
        worldQuad = AllocImmVertices(4);
        if(!worldQuad) display->EndWorldTarget();
        hwViewport[0] = hwViewport[1] = hwViewport[2] = hwViewport[3] = -1;
        SetupHardwareProjection();
        SetScissor(&state.viewState->scissor);
    }
}

void picaContext::EndWorld(void)
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!display->IsWorldTarget())return;
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    FlushPreparedExecution(true);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    visibilityWorld=false;
    picaVisibilityReset(false);
#endif
    if(!display->IsWorldTarget()) return;
    // FrameSplit may start the queue before EndFrame flushes the streams.
    FlushStreams();
    if(!display->EndWorldTarget()) return;

    unsigned char* vertices = worldQuad;
    if(vertices)
    {
        const float u = (display->WorldHeight() - 0.5f) / 256.0f;
        const float v = (display->WorldWidth() - 0.5f) / 512.0f;
        const float xy[4][2] = {{-1,-1},{1,-1},{-1,1},{1,1}};
        const float uv[4][2] = {{0.5f/256,0.5f/512},{u,0.5f/512},{0.5f/256,v},{u,v}};
        for(int i=0;i<4;++i)
        {
            unsigned char* vertex = vertices + i * PICA_VERTEX_STRIDE;
            memset(vertex, 0, PICA_VERTEX_STRIDE);
            float* position = (float*)(vertex + PICA_VERTEX_OFF_POS);
            position[0] = xy[i][0]; position[1] = xy[i][1]; position[2] = -0.5f;
            memcpy(vertex + PICA_VERTEX_OFF_UV, uv[i], sizeof(uv[i]));
        }
        C3D_Mtx identity;
        Mtx_Identity(&identity);
        C3D_BindProgram(&shaderProgram);
        C3D_SetAttrInfo(&attrInfo);
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
        wheelPaletteLayout = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
        noNormalLayout = false;
#endif
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_proj, &identity);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_mv, &identity);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_misc, 0, 0, 1, 1);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_stereoOn, 0, 0, 0, 0);
        C3D_SetViewport(0, 0, PICA_SCREEN_HEIGHT, PICA_SCREEN_WIDTH);
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
        C3D_CullFace(GPU_CULL_NONE);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
        C3D_StencilTest(false, GPU_ALWAYS, 0, 0xff, 0x00);
        C3D_AlphaTest(false, GPU_ALWAYS, 0);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        C3D_TexBind(0, display->WorldTexture());
        for(int i=0;i<6;++i)
        {
            C3D_TexEnv* env = C3D_GetTexEnv(i);
            C3D_TexEnvInit(env);
            C3D_TexEnvSrc(env, C3D_Both, i == 0 ? GPU_TEXTURE0 : GPU_PREVIOUS, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        }
        C3D_BufInfo buffers;
        BufInfo_Init(&buffers);
        BufInfo_Add(&buffers, vertices, PICA_VERTEX_STRIDE, 4, 0x3210);
        C3D_SetBufInfo(&buffers);
        C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
    }
    picaStateInvalidate();
    InvalidateUniformCache();
    pddiBaseShader::ClearCurrentShader();
    hwViewport[0] = hwViewport[1] = hwViewport[2] = hwViewport[3] = -1;
    SyncState(0xffffffff);
    SetupHardwareProjection();
    SetScissor(&state.viewState->scissor);
    ApplyFogState();
}

extern "C" void picaWorldBegin(void)
{
    if(s_picaContext) s_picaContext->BeginWorld();
}
extern "C" void picaWorldEnd(void)
{
    if(s_picaContext) s_picaContext->EndWorld();
}
#endif

#ifdef SRR2_OLD3DS_QUEUE_METRICS
#include <stdio.h>
picaQueueMetrics g_picaQueueMetrics = {};
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_AB
bool picaQueue::enabled = false;
#endif
namespace {
struct QueueRecord { picaQueueMetrics metrics; u64 elapsed; };
QueueRecord queueRecords[128];
unsigned queueRecordFirst = 0, queueRecordCount = 0, queueRecordOverwritten = 0;
u64 queueIntervalStart = 0;
bool queueDumped = false;
}
void picaQueueCapture()
{
    if(!queueIntervalStart) queueIntervalStart = svcGetSystemTick();
    if(++g_picaQueueMetrics.frames < 120) return;
    const unsigned slot = (queueRecordFirst + queueRecordCount) % 128;
    if(queueRecordCount == 128)
    {
        queueRecordFirst = (queueRecordFirst + 1) % 128;
        ++queueRecordOverwritten;
    }
    else ++queueRecordCount;
    const u64 now = svcGetSystemTick();
    QueueRecord& record = queueRecords[slot];
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    g_picaQueueMetrics.queueMode = picaQueue::Enabled() ? 1 : 0;
#endif
    record.metrics = g_picaQueueMetrics;
    record.elapsed = now - queueIntervalStart;
    memset(&g_picaQueueMetrics, 0, sizeof(g_picaQueueMetrics));
    queueIntervalStart = now;
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_AB
    picaQueue::enabled = !picaQueue::enabled;
#endif
}
void picaQueueDump()
{
    if(queueDumped) return;
    FILE* file = fopen("sdmc:/3ds/simpsons/material-queue.log", "w");
    if(!file) file = fopen("sd:/3ds/simpsons/material-queue.log", "w");
    if(!file) return;
    fprintf(file, "QUEUE_BUILD id=%s enabled=%u sort=%u capacity=32 snapshot_bytes=%u overwritten=%u\n",
        SRR2_OLD3DS_BUILD_ID,
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
        1u,
#else
        0u,
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
        1u,
#else
        0u,
#endif
        (unsigned)sizeof(queueRecords), queueRecordOverwritten);
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    // Completed records carry their captured mode; the partial tail must use
    // the current mode rather than the zeroed interval's default value.
    g_picaQueueMetrics.queueMode = picaQueue::Enabled() ? 1 : 0;
#endif
    for(unsigned i = 0; i <= queueRecordCount; ++i)
    {
        const picaQueueMetrics& m = i < queueRecordCount ? queueRecords[(queueRecordFirst+i)%128].metrics : g_picaQueueMetrics;
        fprintf(file, "QUEUE interval=%u frames=%u geometries=%u candidates=%u queued=%u barriers=%u capacity_flushes=%u moved=%u original_changes=%u submitted_changes=%u material_reuses=%u draws=%u vertices=%u material_passes=%u textures=%u uniform_blocks=%u uniform_reuses=%u peak_entries=%u cmd_high=%u cmd_size=%u geometry_ticks=%llu geometry_samples=%u collect_ticks=%llu sort_ticks=%llu submit_ticks=%llu queue_calls=%u elapsed_ticks=%llu command_words=%llu backend_calls=%u backend_samples=%u backend_ticks=%llu pipeline_reuses=%u uniform_bypasses=%u opaque_ticks=%llu translucent_ticks=%llu traversal_ticks=%llu queue_enabled=%u partial=%u\n",
            i, m.frames, m.geometries, m.candidates, m.queued, m.barriers, m.capacityFlushes,
            m.moved, m.originalChanges, m.submittedChanges, m.materialReuses,
            m.draws, m.vertices, m.materialPasses, m.textures, m.uniforms, m.uniformReuses,
            m.peakEntries, m.commandHighWater, m.commandSize,
            m.geometryTicks, m.geometrySamples, m.collectTicks, m.sortTicks, m.submitTicks, m.queueSamples,
            i < queueRecordCount ? queueRecords[(queueRecordFirst+i)%128].elapsed : 0ull, m.commandWords, m.backendCalls, m.backendSamples, m.backendTicks, m.pipelineReuses, m.uniformBypasses, m.opaqueTicks, m.translucentTicks, m.traversalTicks, m.queueMode, i == queueRecordCount ? 1u : 0u);
    }
    queueDumped = fflush(file) == 0;
    if(fclose(file) != 0) queueDumped = false;
}
#endif

#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
void picaContext::BeginSkinBatch()
{
    ++skinBatchDepth;
}
void picaContext::SkinBatchBarrier()
{
    if(skinProgramBound) BindSkinProgram(false);
}
void picaContext::EndSkinBatch()
{
    PDDIASSERT(skinBatchDepth);
    --skinBatchDepth;
    SkinBatchBarrier();
}
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
void picaContext::SetSkinLayout(bool rigid)
{
    if(skinRigidLayout == rigid) return;
    skinRigidLayout = rigid;
    if(skinProgramBound)
    {
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        if(PreparedBatching() && QueuePreparedLayout(rigid?&skinRigidAttrInfo:&skinAttrInfo,false))return;
        FlushPreparedExecution(true);
#endif
        C3D_SetAttrInfo(rigid ? &skinRigidAttrInfo : &skinAttrInfo);
        g_picaState.bufInfoSet = false;
    }
}
#endif
bool picaContext::BindSkinProgram(bool enabled)
{
    if (enabled && !skinBinary)
        return false;
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
    if(skinBatchDepth && enabled == skinProgramBound) return true;
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    wheelPaletteLayout = false;
#endif
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    noNormalLayout = false;
#endif
    skinProgramBound = enabled;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    if(PreparedBatching() && QueuePreparedProgram(enabled?&skinProgram:&shaderProgram,enabled?
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
        (skinRigidLayout?&skinRigidAttrInfo:&skinAttrInfo)
#else
        &skinAttrInfo
#endif
        :&attrInfo,
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
        !enabled,
#else
        false,
#endif
        VSH_FVEC_u_eye,PICA_STEREO_CONVERGENCE))
    {
        InvalidateUniformCache();++g_picaSkinMetrics.switches;return true;
    }
    FlushPreparedExecution(true);
#endif
    C3D_BindProgram(enabled ? &skinProgram : &shaderProgram);

#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
    if(!enabled)
    {
        // The compact palette overlaps the normal program's eye register.
        // Old 3DS uses one mono target; restore its frame-begin eye constants.
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_eye,
                      0.0f, PICA_STEREO_CONVERGENCE, 0.0f, 0.0f);
    }
#endif
    C3D_SetAttrInfo(enabled ?
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
        (skinRigidLayout ? &skinRigidAttrInfo : &skinAttrInfo)
#else
        &skinAttrInfo
#endif
        : &attrInfo);
    g_picaState.bufInfoSet = false;
    InvalidateUniformCache();
    ++g_picaSkinMetrics.switches;
    return true;
}
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
void picaPrimBuffer::PrepareGpuSkin(const void *source, unsigned count, unsigned bones,
                                    bool selected, bool pedestrian, bool named)
{
#ifdef SRR2_OLD3DS_SKIN_CLIP
    if(!skinClipBounds && !transient && buffer && count==allocated &&
       bones && bones<=picaSkinClip::MaxBones &&
       skinClipBytes+sizeof(picaSkinClip::Bounds)<=picaSkinClip::Budget)
    {
        const picaSkinClip::Bounds bounds=picaSkinClip::Build(static_cast<const picaSkin::Source*>(source),count,bones);
        if(bounds.valid) {
            skinClipBounds=static_cast<picaSkinClip::Bounds*>(std::malloc(sizeof(bounds)));
            if(skinClipBounds) {*skinClipBounds=bounds;skinClipBytes+=sizeof(bounds);}
        }
    }
#endif
    if (!selected)
        return;
    if (!bones || bones > picaSkin::MaxBones)
    {
        skinReason = picaSkin::Palette;
        return;
    }
    if (transient || !buffer || count != allocated || !PrimIsDrawable(primType) ||
        !IndicesInRange() || !(vertexType & PDDI_V_NORMAL))
    {
        skinReason = picaSkin::Topology;
        return;
    }
    const unsigned char *src = (const unsigned char *)source;
    picaSkin::Vertex probe;
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    bool rigid = true;
#endif
    for (unsigned i = 0; i < count; ++i)
    {
        picaSkin::Source vertex;
        memcpy(&vertex, src + i * sizeof(vertex), sizeof(vertex));
        if (!picaSkin::PackVertex(probe, vertex, buffer + 36 * i, bones))
        {
            skinReason = picaSkin::SourceInvalid;
            return;
        }
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
        rigid = rigid && vertex.weights[0] == 1.0f;
#endif
    }
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    skinRigid = rigid;
#endif
    skinBoneCount = bones;
    skinReason = picaSkin::Ready;
#ifdef SRR2_OLD3DS_GPU_SKIN
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    const unsigned stride = picaSkin::PackedStride(skinRigid);
#else
    const unsigned stride = picaSkin::VertexStride;
#endif
    const unsigned bytes = count * stride;
    // A failed load-time allocation is sticky: never retry allocation in gameplay.
    skinAllocation = picaSkinAllocate(bytes, pedestrian, named);
    if (!skinAllocation)
    {
        skinReason = g_picaSkinAllocationReason;
        return;
    }
    unsigned char *dst = (unsigned char *)(skinAllocation + 1);
    for (unsigned i = 0; i < count; ++i)
    {
        picaSkin::Source vertex;
        memcpy(&vertex, src + i * sizeof(vertex), sizeof(vertex));
        picaSkin::PackVertex(probe, vertex, buffer + 36 * i, bones);
        picaSkin::StoreVertex(dst + stride * i, probe,
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
                              skinRigid
#else
                              false
#endif
                              );
    }
    GSPGPU_FlushDataCache(dst, bytes);
#endif
}
void picaPrimBuffer::MutateGpuSkin()
{
#ifdef SRR2_OLD3DS_SKIN_CLIP
    // Expression/morph/index writers retain their original rendering behavior.
    if(skinClipBounds) {std::free(skinClipBounds);skinClipBounds=NULL;skinClipBytes-=sizeof(picaSkinClip::Bounds);}
#endif
    if (skinReason != picaSkin::NotSelected)
    {
        skinReason = picaSkin::Mutated;
        ++g_picaSkinMetrics.mutations;
        // Keep the allocation until destruction; no writes or frees while GPU reads
        // may still be pending. Mutation fallback remains sticky for this lifetime.
    }
}
#ifdef SRR2_OLD3DS_SKIN_CLIP
bool picaContext::CullSkinBounds(const picaSkinClip::Bounds* const* groups,unsigned groupCount,
                                 const pddiMatrix* matrices,unsigned count)
{
    if(!groups || !groupCount || !matrices || !g_picaInFrame || bottomPass ||
       display->StereoActive() || state.viewState->projectionMode!=PDDI_PROJECTION_PERSPECTIVE) return false;
    picaSkinClipTimer timer;
    picaSkinClip::Bounds bounds={};
    for(unsigned i=0;i<groupCount;++i)
        if(!groups[i] || !picaSkinClip::Merge(bounds,*groups[i])) return false;
    if(bounds.count>count) return false;
    float copied[picaSkinClip::MaxBones][4][4],low[3],high[3];
    for(unsigned i=0;i<bounds.count;++i)
        for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) copied[i][r][c]=matrices[i].Row4(r)[c];
    if(!picaSkinClip::Transform(bounds,copied,bounds.count,low,high)) return false;
    ++skinClipTested;
    const bool rejected=CullBounds(low,high);
    skinClipRejected+=rejected;
    return rejected;
}
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
bool picaPrimBuffer::SubmitGpuSkin(pddiShader* material)
{
            skinDrawing = true;
            context->DrawPrimBuffer(material, this);
            skinDrawing = false;
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
            if(!context->skinBatchDepth)
#endif
            context->BindSkinProgram(false);
            ++g_picaSkinMetrics.gpuGroups;
            g_picaSkinMetrics.gpuVertices += allocated;
            g_picaSkinMetrics.avoidedWriteBytes += (u64)allocated * 24;
            g_picaSkinMetrics.avoidedFlushBytes += mem;
            u32 *commands;
            u32 size, offset;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
            if(!context->PreparedBatching())
#endif
            {
            GPUCMD_GetBuffer(&commands, &size, &offset);
            if (offset > g_picaSkinMetrics.maxCommand)
                g_picaSkinMetrics.maxCommand = offset;
            }
            return true;
}
#endif
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
bool picaPrimBuffer::CanDeferGpuSkin(pddiShader* material,const pddiMatrix* const* matrices,unsigned count,const pddiMatrix* base,unsigned joints) const
{
    if(!material || typeid(*material)!=typeid(picaMat) || skinReason!=picaSkin::Ready ||
       !skinAllocation || !context->skinBinary || !picaSkinEnabled() || !g_picaInFrame ||
       !base || !matrices || !count || count!=skinBoneCount || count>picaSkin::MaxBones || joints>=picaDeferredSkin::MaximumJoints)return false;
    const auto first=reinterpret_cast<std::uintptr_t>(base);
    for(unsigned i=0;i<count;++i){
        const auto address=reinterpret_cast<std::uintptr_t>(matrices[i]);
        if(address<first || (address-first)%sizeof(pddiMatrix) || (address-first)/sizeof(pddiMatrix)>joints)return false;
    }
    return true;
}
#endif
bool picaPrimBuffer::TryGpuSkin(pddiShader *material, const pddiMatrix *const *matrices,
                                unsigned count)
{
    picaSkin::Reason reason = skinReason;
    if (reason == picaSkin::Ready)
    {
        ++g_picaSkinMetrics.eligibleGroups;
        g_picaSkinMetrics.eligibleVertices += allocated;
        if (!picaSkinEnabled())
            reason = picaSkin::Disabled;
    }
#ifdef SRR2_OLD3DS_GPU_SKIN
    if (reason == picaSkin::Ready && !g_picaInFrame)
        reason = picaSkin::NoFrame;
    if (reason == picaSkin::Ready && (!skinAllocation || !context->skinBinary))
        reason = picaSkin::NoShader;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    if(context->deferredSkin && reason!=picaSkin::Ready)context->MaterializeDeferredSkin();
#endif
    if (reason == picaSkin::Ready)
    {
        u64 start = svcGetSystemTick();
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
        if(context->deferredSkin){
            if(CanDeferGpuSkin(material,matrices,count,context->deferredSkinScratch,context->deferredSkin->rest->count)){
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
                context->SetSkinLayout(skinRigid);
#endif
                context->BindSkinProgram(true);
                if(context->QueueDeferredSkinPalette(matrices,count)){
                    g_picaSkinMetrics.vectors+=3*count;
                    g_picaSkinMetrics.paletteTicks+=svcGetSystemTick()-start;
                    return SubmitGpuSkin(material);
                }
            }
            context->MaterializeDeferredSkin();
        }
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
        picaPreparedSkinPalette packed;
        packed.count=count;
#else
        float packed[picaSkin::MaxBones][3][4];
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
        float view[4][4];
        const pddiMatrix* modelView = context->state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
        for(unsigned r = 0; r < 4; ++r)
            for(unsigned c = 0; c < 4; ++c) view[r][c] = modelView->Row4(r)[c];
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_CACHE
        unsigned eyeControl = 0;
#ifdef __arm__
        __asm__ __volatile__("vmrs %0, fpscr" : "=r"(eyeControl) : : "memory");
        eyeControl &= 0x03c00000u;
#endif
        if(!context->skinEyeCache.PrepareView(view, eyeControl)) reason = picaSkin::MatrixInvalid;
#endif
#endif
        if (count != skinBoneCount)
            reason = picaSkin::MatrixInvalid;
        for (unsigned i = 0; reason == picaSkin::Ready && i < count; ++i)
        {
            // Copy values immediately; Pure3D's matrix palette points into shared scratch.
            float m[4][4];
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c)
                    m[r][c] = matrices[i]->Row4(r)[c];
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_CACHE
            if (!context->skinEyeCache.Pack(packed[i], m, reinterpret_cast<std::uintptr_t>(matrices[i])))
#else
            if (!picaSkin::PackEyeMatrix(packed[i], m, view))
#endif
#else
            if (!picaSkin::PackMatrix(packed[i], m))
#endif
                reason = picaSkin::MatrixInvalid;
        }
        if (reason == picaSkin::Ready)
        {
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
            context->SetSkinLayout(skinRigid);
#endif
            context->BindSkinProgram(true);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
            if(g_picaPreparedBackendEnabled)
            {
                const bool uploaded=(context->PreparedBatching() && context->QueuePreparedPalette(packed,
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
                    20
#else
                    VSH_FVEC_u_bones
#endif
                )) || picaUploadPreparedSkinPalette(packed,
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
                    20
#else
                    VSH_FVEC_u_bones
#endif
                );
                if(!uploaded)
                {
                    context->BindSkinProgram(false);
                    ++g_picaSkinMetrics.fallback[picaSkin::MatrixInvalid];
                    g_picaSkinMetrics.paletteTicks+=svcGetSystemTick()-start;
                    return false;
                }
            }
            else
            {
#endif
            C3D_FVec *dst = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER,
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
                                                        20,
#else
                                                        VSH_FVEC_u_bones,
#endif
                                                        3 * count);
            for (unsigned i = 0; i < count; ++i)
                for (unsigned r = 0; r < 3; ++r)
                {
                    dst[3 * i + r].x = packed[i][r][0];
                    dst[3 * i + r].y = packed[i][r][1];
                    dst[3 * i + r].z = packed[i][r][2];
                    dst[3 * i + r].w = packed[i][r][3];
                }
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
            }
#endif
            g_picaSkinMetrics.vectors += 3 * count;
            g_picaSkinMetrics.paletteTicks += svcGetSystemTick() - start;
            return SubmitGpuSkin(material);
        }
        g_picaSkinMetrics.paletteTicks += svcGetSystemTick() - start;
    }
#endif
    ++g_picaSkinMetrics.fallback[reason];
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
    context->SkinBatchBarrier();
#endif
    return false;
}
#endif

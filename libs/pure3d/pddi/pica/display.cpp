#include <main/old3dscadence.h>
#include <pddi/pica/display.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/base/debug.hpp>
#include <main/old3dsdiag.h>
#include <stdio.h>
#include <main/graphics3ds.h>
#include "pddi_shbin.h"

extern "C" void C3Di_UpdateContext(void);
extern "C" void C3Di_RenderQueueWaitDone(void);
extern "C" bool C3Di_SplitFrame(u32** pBuf, u32* pSize);
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
extern "C" void picaNativeUniformInvalidate(GPU_SHADER_TYPE type, int first, int count);
#endif

static const float PICA_STEREO_MAX_SHIFT   = 0.045f;

#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned stereoFrames = 0, stereoSegments = 0;
static float minimumStereoSlider = 1.0f, maximumStereoSlider = 0.0f;
extern "C" void picaDumpStereoDiagnostics(FILE* file)
{
    fprintf(file, "NATIVE_STEREO active_frames=%u replayed_segments=%u minimum_slider=%.4f maximum_slider=%.4f scope=whole_session cpu_scene_traversals_per_segment=1\n",
        stereoFrames, stereoSegments, minimumStereoSlider, maximumStereoSlider);
}
#endif


static void picaWriteEye(float shift)
{
    C3D_FVec v;
    v.x = shift;
    v.y = PICA_STEREO_CONVERGENCE;
    v.z = 0.0f;
    v.w = 0.0f;
    GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000u | VSH_FVEC_u_eye);
    GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)v.c, 4);
#ifdef SRR2_3DS_STEREO_GPU_SKIN
    if(Graphics3DS::Stereo())
    {
        // Slot 95 is outside the complete 25-bone palette. Its xyz packing scales
        // are fixed; w carries the eye shift for both stereo shader companions.
        C3D_FVec packed;
        packed.x = 1.0f / 8192.0f; packed.y = 1.0f / 32768.0f;
        packed.z = 1.0f / 4096.0f; packed.w = shift;
        GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000u | 95u);
        GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)packed.c, 4);
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
        picaNativeUniformInvalidate(GPU_VERTEX_SHADER, 95, 1);
#endif
    }
#endif
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
    picaNativeUniformInvalidate(GPU_VERTEX_SHADER, VSH_FVEC_u_eye, 1);
#endif
}

extern "C" int      g_picaInFrame        = 0;
#ifdef SRR2_OLD_3DS
extern "C" int g_picaWorldScaleAllowed = 0;
extern "C" unsigned g_picaPresentedFrames = 0;
extern "C" double g_picaFrameWaitMs = 0.0;
extern "C" double g_picaFrameSubmitMs = 0.0;
#endif
static int s_c3dRefCount = 0;

extern "C" void (*g_picaYieldHook)(void) = 0;
extern "C" void (*g_picaGpuWaitHook)(int lend) = 0;

#include <SDL.h>

picaDisplay::picaDisplay(pddiDisplayInfo* info)
{
    displayInfo = info;
    mode        = PDDI_DISPLAY_FULLSCREEN;

    winWidth    = PICA_SCREEN_WIDTH;
    winHeight   = PICA_SCREEN_HEIGHT;
    winBitDepth = 32;

    context     = NULL;
    target      = NULL;
    targetRight = NULL;
    targetBottom = NULL;
#ifdef SRR2_OLD_3DS
    memset(&worldTexture, 0, sizeof(worldTexture));
    worldTarget = NULL;
    worldActive = false;
    worldWidth = 400;
    worldHeight = 240;
    renderWidth = 400;
#endif
    stereoThisFrame = false;
    eyeShift    = 0.0f;
    c3dStarted  = false;
    inFrame     = false;
    reset       = true;
    beginTime   = 0.0f;
}

picaDisplay::~picaDisplay()
{
    if(inFrame)
    {
        C3D_FrameEnd(0);
        inFrame = false;
        g_picaInFrame = 0;
    }

#ifdef SRR2_OLD_3DS
    if(worldTarget) C3D_RenderTargetDelete(worldTarget);
    if(worldTexture.data) C3D_TexDelete(&worldTexture);
#endif
    if(target != NULL)
    {
        C3D_RenderTargetDelete(target);
        target = NULL;
    }
    if(targetRight != NULL)
    {
        C3D_RenderTargetDelete(targetRight);
        targetRight = NULL;
    }
    if(targetBottom != NULL)
    {
        C3D_RenderTargetDelete(targetBottom);
        targetBottom = NULL;
    }

    if(c3dStarted)
    {
        c3dStarted = false;
        if(--s_c3dRefCount == 0)
        {
            C3D_Fini();
        }
    }
}

long picaDisplay::ProcessWindowMessage(SDL_Window* wnd, const SDL_WindowEvent* event)
{
    ( void ) wnd;
    ( void ) event;
    return 0;
}

void picaDisplay::SetWindow(SDL_Window* wnd)
{
    ( void ) wnd;
}

bool picaDisplay::InitDisplay(int x, int y, int bpp)
{
    displayInit.xsize = x;
    displayInit.ysize = y;
    displayInit.bpp   = bpp;
    return InitDisplay(&displayInit);
}

bool picaDisplay::InitDisplay(const pddiDisplayInit* init)
{
    displayInit = *init;

    winWidth    = PICA_SCREEN_WIDTH;
    winHeight   = PICA_SCREEN_HEIGHT;
    winBitDepth = 32;

    if(!c3dStarted)
    {
        const size_t cmdBufBytes = 640 * 1024;
        const bool   c3dOk       = ( s_c3dRefCount > 0 )
                                       ? true
                                       : C3D_Init(cmdBufBytes);

        if(!c3dOk)
        {
            return false;
        }

        s_c3dRefCount++;
        c3dStarted = true;
    }

    if(target == NULL)
    {
        target = C3D_RenderTargetCreate(winHeight, winWidth,
                                        GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if(target == NULL)
        {
            return false;
        }

        const u32 transferFlags =
            GX_TRANSFER_FLIP_VERT(0) |
            GX_TRANSFER_OUT_TILED(0) |
            GX_TRANSFER_RAW_COPY(0)  |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
        C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, transferFlags);

#ifndef SRR2_OLD_3DS
        targetRight = C3D_RenderTargetCreate(winHeight, winWidth,
                                             GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if(targetRight != NULL)
        {
            C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, transferFlags);
            gfxSet3D(true);
        }
#else
        // Keep the full 400x240 image. A second eye costs render memory and GPU
        // work that the original hardware needs for the game itself.
        gfxSet3D(false);
#endif

        targetBottom = C3D_RenderTargetCreate(PICA_BOTTOM_HEIGHT, PICA_BOTTOM_WIDTH,
                                              GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if(targetBottom != NULL)
        {
            C3D_RenderTargetSetOutput(targetBottom, GFX_BOTTOM, GFX_LEFT, transferFlags);
        }
    }

#ifdef SRR2_OLD_3DS
    // Optional, tiny SD-side override; the native path requires no extra memory.
    int percent = 100;
    FILE* settings = fopen("old3ds-world-scale.txt", "r");
    if(settings)
    {
        if(fscanf(settings, "%d", &percent) != 1) percent = 100;
        fclose(settings);
    }
    if(percent != 75 && percent != 80) percent = 100;
    if(percent != 100 && worldTarget == NULL)
    {
        // Retain the backend's rotated coordinates so projection and depth
        // conventions remain identical to the native framebuffer.
        if(C3D_TexInitVRAM(&worldTexture, 256, 512, GPU_RGBA8))
        {
            worldTarget = C3D_RenderTargetCreateFromTex(&worldTexture, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH24_STENCIL8);
            if(worldTarget)
            {
                worldWidth = 400 * percent / 100;
                worldHeight = 240 * percent / 100;
                C3D_TexSetFilter(&worldTexture, GPU_LINEAR, GPU_LINEAR);
                C3D_TexSetWrap(&worldTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            }
            else
            {
                C3D_TexDelete(&worldTexture);
                memset(&worldTexture, 0, sizeof(worldTexture));
            }
        }
    }
    printf("OLD3DS_WORLD width=%d height=%d native_ui=1\n", worldWidth, worldHeight);
#endif

#ifdef SRR2_OLD_3DS
    if(!ApplyGraphicsMode()) return false;
#endif
    reset = true;
    return true;
}

#ifdef SRR2_OLD_3DS
bool picaDisplay::ApplyGraphicsMode(void)
{
    if(!Graphics3DS::Pending()) return target != NULL;
    const int requestedWidth = Graphics3DS::RequestedQuality() && Graphics3DS::SupportsWide() ? 800 : 400;
    const bool requestedStereo = Graphics3DS::Stereo();
    // Retire every command referencing these buffers before changing output.
    // FrameSync waits for VBlank; it does not drain an unfinished GPU queue.
    C3Di_RenderQueueWaitDone();
    C3D_FrameSync();
    bool success = true;
    if(requestedWidth != renderWidth || requestedStereo != (targetRight != NULL))
    {
        if(targetRight) { C3D_RenderTargetDelete(targetRight); targetRight = NULL; }
        if(target) { C3D_RenderTargetDelete(target); target = NULL; }
        target = C3D_RenderTargetCreate(winHeight, requestedWidth, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if(target && requestedStereo)
            targetRight = C3D_RenderTargetCreate(winHeight, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        success = target && (!requestedStereo || targetRight);
        if(!success)
        {
            if(targetRight) { C3D_RenderTargetDelete(targetRight); targetRight = NULL; }
            if(target) C3D_RenderTargetDelete(target);
            target = C3D_RenderTargetCreate(winHeight, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        }
        renderWidth = success ? requestedWidth : 400;
    }
    Graphics3DS::Complete(success);
    // gfxSet3D(false) also clears wide mode, so set wide mode last.
    gfxSet3D(Graphics3DS::Stereo());
    if(!Graphics3DS::Stereo()) gfxSetWide(renderWidth == 800);
    const u32 flags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
        GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
    if(target) C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, flags);
    if(targetRight) C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, flags);
    reset = true;
    return target != NULL;
}
#endif

pddiDisplayInfo* picaDisplay::GetDisplayInfo(void)
{
    return displayInfo;
}

int picaDisplay::GetHeight(void) { return winHeight; }
int picaDisplay::GetWidth(void)  { return winWidth; }
int picaDisplay::GetDepth(void)  { return winBitDepth; }

pddiDisplayMode picaDisplay::GetDisplayMode(void) { return mode; }

int picaDisplay::GetNumColourBuffer(void) { return 2; }

unsigned picaDisplay::GetBufferMask(void) { return unsigned(-1); }

unsigned picaDisplay::GetFreeTextureMem(void)
{
    return (unsigned)linearSpaceFree();
}

void picaDisplay::BeginFrame(void)
{
#ifdef SRR2_OLD_3DS
    if(!inFrame && Graphics3DS::CanApply() && !ApplyGraphicsMode()) return;
#endif
    if(!inFrame && target != NULL)
    {
        if(g_picaGpuWaitHook) g_picaGpuWaitHook(1);

#ifdef SRR2_OLD_3DS
        const u64 waitTick = svcGetSystemTick();
#endif
        bool frameOk;
        {
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
            O3ExperimentTimer causalWait(O3E_WAIT);
#endif
            frameOk = C3D_FrameBegin(0);
        }
#ifdef SRR2_OLD_3DS
        g_picaFrameWaitMs += (svcGetSystemTick() - waitTick) * 1000.0 / SYSCLOCK_ARM11;
        // A failed begin submits no frame at all. The old code discarded this
        // result, so a busy GPU was invisible in the diagnostics.
        if(frameOk) ++g_old3dsFrameBeginOk;
        else        ++g_old3dsFrameBeginBusy;
#endif

        if(g_picaGpuWaitHook) g_picaGpuWaitHook(0);

        if(!frameOk)
        {
            return;
        }

        if(!C3D_FrameDrawOn(target))
        {
#ifdef SRR2_OLD_3DS
            ++g_old3dsFrameBeginNoDraw;
#endif
            C3D_FrameEnd(0);
            return;
        }

        const float slider = osGet3DSliderState();
        stereoThisFrame = (targetRight != NULL) && (slider > 0.01f);
        eyeShift = stereoThisFrame ? slider * PICA_STEREO_MAX_SHIFT : 0.0f;

#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(stereoThisFrame)
        {
            ++stereoFrames;
            if(slider < minimumStereoSlider) minimumStereoSlider = slider;
            if(slider > maximumStereoSlider) maximumStereoSlider = slider;
        }
#endif

        StartTopSegment();

        inFrame = true;
        g_picaInFrame = 1;
    }
}

void picaDisplay::StartTopSegment(void)
{
    if(stereoThisFrame)
    {
        if(context) context->FlushStreams();
        C3Di_UpdateContext();
        picaWriteEye(-eyeShift);
        C3D_FrameSplit(GX_CMDLIST_FLUSH);
    }
    else
    {
        picaWriteEye(0.0f);
    }
}

void picaDisplay::FinishTopSegment(void)
{
    if(!stereoThisFrame)
        return;

    if(context) context->FlushStreams();
    u32* sceneBuf = NULL;
    u32  sceneSize = 0;
    if(C3Di_SplitFrame(&sceneBuf, &sceneSize))
    {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        ++stereoSegments;
#endif
        // Old-model EndFrame flushes only its final command segment. Publish
        // this scene segment before either eye consumes it on real hardware.
        GX_ProcessCommandList(sceneBuf, sceneSize * 4, GX_CMDLIST_FLUSH);

        C3D_FrameDrawOn(targetRight);
        C3Di_UpdateContext();
        picaWriteEye(eyeShift);
        C3D_FrameSplit(GX_CMDLIST_FLUSH);

        GX_ProcessCommandList(sceneBuf, sceneSize * 4, 0);
    }
}

bool picaDisplay::BeginBottomTarget(void)
{
    if(!inFrame || targetBottom == NULL)
        return false;

    FinishTopSegment();

    C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0x00FFFFFF);
    C3D_FrameDrawOn(targetBottom);
    picaWriteEye(0.0f);
    return true;
}

void picaDisplay::EndBottomTarget(void)
{
    C3D_FrameDrawOn(target);
    StartTopSegment();
}

void picaDisplay::EndFrame(void)
{
    if(inFrame)
    {
        FinishTopSegment();
        stereoThisFrame = false;

#ifdef SRR2_OLD_3DS
        const u64 submitTick = svcGetSystemTick();
#endif
        // Textures, persistent geometry and immediate rings flush their own
        // modified ranges. Flush the command list without flushing the entire
        // linear heap again on the memory-constrained Old 3DS profile.
#ifdef SRR2_OLD_3DS
        #ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentSubmit();
#endif
        Old3dsCadence::Submit(g_picaPresentedFrames+1);
        C3D_FrameEnd(GX_CMDLIST_FLUSH);
#else
        C3D_FrameEnd(0);
#endif
        inFrame = false;
        g_picaInFrame = 0;
#ifdef SRR2_OLD_3DS
        g_picaFrameSubmitMs += (svcGetSystemTick() - submitTick) * 1000.0 / SYSCLOCK_ARM11;
        ++g_picaPresentedFrames;
        ++g_old3dsFramePresented;
#endif
    }
}

void picaDisplay::SwapBuffers(void)
{
    EndFrame();
    reset = false;
}

unsigned picaDisplay::Screenshot(pddiColour* buffer, int nBytes)
{
    ( void ) buffer;
    ( void ) nBytes;
    return 0;
}

void picaDisplay::BeginTiming(void)
{
    beginTime = (float)SDL_GetTicks();
}

float picaDisplay::EndTiming(void)
{
    return (float)SDL_GetTicks() - beginTime;
}

#ifdef SRR2_OLD_3DS
bool picaDisplay::BeginWorldTarget(void)
{
    if(Graphics3DS::Quality() || Graphics3DS::Stereo() || Graphics3DS::IsNew()) return false;
    if(!inFrame || !worldTarget || !g_picaWorldScaleAllowed) return false;
    C3D_RenderTargetClear(worldTarget, C3D_CLEAR_ALL, 0x000000FF, 0x00FFFFFF);
    worldActive = C3D_FrameDrawOn(worldTarget);
    return worldActive;
}

bool picaDisplay::EndWorldTarget(void)
{
    if(!worldActive) return false;
    // Submit world commands before sampling their render texture.
    if(context) context->FlushStreams();
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    worldActive = false;
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000FF, 0x00FFFFFF);
    C3D_FrameDrawOn(target);
    return true;
}
#endif

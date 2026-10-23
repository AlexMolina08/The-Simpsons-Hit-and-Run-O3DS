#include <main/old3dscadence.h>
#include <pddi/pica/display.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/base/debug.hpp>
#include <main/old3dsdiag.h>
#include <stdio.h>
#include <main/graphics3ds.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/nativebench.h>
#endif
#include "pddi_shbin.h"

extern "C" void C3Di_UpdateContext(void);
extern "C" void C3Di_RenderQueueWaitDone(void);
extern "C" bool C3Di_SplitFrame(u32** pBuf, u32* pSize);
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
extern "C" void picaNativeUniformInvalidate(GPU_SHADER_TYPE type, int first, int count);
#endif

#ifdef SRR2_OLD_3DS
static bool s_indicatorPending = false, s_indicatorOwnsPolicy = false;
static bool s_indicatorPreviousDisable = false;
static u64 s_indicatorReadyTick = 0;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned s_indicatorAttempts = 0, s_indicatorReleases = 0;
static Result s_indicatorLcdResult = -1, s_indicatorWriteResult = -1;
#endif
extern "C" void picaRequest3DIndicator(void)
{
    const Graphics3DS::Model model = Graphics3DS::SystemModel();
    s_indicatorPending = model == Graphics3DS::Old3DS || model == Graphics3DS::Old3DSXL;
}

extern "C" void picaRelease3DIndicator(void)
{
    s_indicatorPending = false;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    ++s_indicatorReleases;
#endif
    if(!s_indicatorOwnsPolicy || R_FAILED(gspLcdInit())) return;
    GSPLCD_SetLedForceOff(s_indicatorPreviousDisable);
    gspLcdExit();
    s_indicatorOwnsPolicy = false;
}

static void picaUpdate3DIndicator(void)
{
    if(!s_indicatorPending) return;
    if(!s_indicatorOwnsPolicy)
    {
        const Result lcdResult = gspLcdInit();
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        ++s_indicatorAttempts;
        s_indicatorLcdResult = lcdResult;
#endif
        if(R_FAILED(lcdResult)) { s_indicatorPending = false; return; }
        s_indicatorPreviousDisable = OS_SharedConfig->led_3d != 0;
        const Result policy = GSPLCD_SetLedForceOff(true);
        gspLcdExit();
        if(R_FAILED(policy)) { s_indicatorPending = false; return; }
        s_indicatorOwnsPolicy = true;
        // LCD applies its final off write asynchronously. Let it settle without
        // stalling rendering, then control only the independent MCU light.
        s_indicatorReadyTick = svcGetSystemTick() + SYSCLOCK_ARM11 / 10;
        return;
    }
    if(svcGetSystemTick() < s_indicatorReadyTick) return;
    s_indicatorPending = false;
    const Result initialized = mcuHwcInit();
    Result written = initialized;
    if(R_SUCCEEDED(initialized))
    {
        const u8 enabled = 1;
        written = MCUHWC_WriteRegister(0x2c, &enabled, sizeof(enabled));
        mcuHwcExit();
    }
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    s_indicatorWriteResult = written;
#endif
    if(R_FAILED(written)) picaRelease3DIndicator();
}
#endif

#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned stereoFrames = 0, stereoSegments = 0;
static float minimumStereoSlider = 1.0f, maximumStereoSlider = 0.0f;
static unsigned measuredStereoFrames = 0, measuredFrames = 0;
static float measuredSliderMinimum = 1.0f, measuredSliderMaximum = 0.0f;
static float measuredConvergenceMinimum = 1000.0f, measuredConvergenceMaximum = 0.0f;
static unsigned stereoSliderBins[16]={};
static float minimumEyeShift=1.0f,maximumEyeShift=0.0f;
static unsigned auditedStereoSegments=0, sceneEyeWrites=0, sceneEyeFloat24Writes=0, auditErrors=0;
static u32 lastSceneEye[4]={};
// Inspect only a bounded number of visual-test command buffers. A scene write
// to register 95 would override both external eye setups during replay.
static void picaAuditStereoScene(const u32* commands,unsigned words)
{
    if(!NativeBench::collecting || !NativeBench::StereoVisualAudit() || auditedStereoSegments>=32)return;
    ++auditedStereoSegments;
    unsigned cursor=0,components=0;bool known=false,float32=false;
    for(unsigned offset=0;offset+1<words;)
    {
        const u32 header=commands[offset+1];
        const unsigned count=1+((header>>20)&0x7ff),reg=header&0x3ff;
        if(count+1>words-offset){++auditErrors;return;}
        for(unsigned i=0;i<count;++i)
        {
            const unsigned target=reg+((header>>31)?i:0);
            const u32 value=commands[offset+(i?i+1:0)];
            if(target==GPUREG_VSH_FLOATUNIFORM_CONFIG)
            {
                known=((header>>16)&15)==15;float32=(value>>31)!=0;
                components=float32?4:3;cursor=(value&0x7f)*components;
            }
            else if(known && target>=GPUREG_VSH_FLOATUNIFORM_DATA && target<GPUREG_VSH_FLOATUNIFORM_DATA+8)
            {
                if(cursor/components==95)
                {
                    if(float32){if(cursor%components==0)++sceneEyeWrites;lastSceneEye[cursor%components]=value;}
                    else if(cursor%components==0)++sceneEyeFloat24Writes;
                }
                ++cursor;
            }
        }
        offset+=(count+2)&~1u;
    }
}
extern "C" void picaDumpStereoDiagnostics(FILE* file)
{
#ifdef SRR2_OLD_3DS
    const Graphics3DS::Model model = Graphics3DS::SystemModel();
    if(model == Graphics3DS::Old3DS || model == Graphics3DS::Old3DSXL)
    {
        u8 led = 0;
        Result result = mcuHwcInit();
        if(R_SUCCEEDED(result))
        {
            result = MCUHWC_ReadRegister(0x2c, &led, sizeof(led));
            mcuHwcExit();
        }
        fprintf(file, "NATIVE_3D_INDICATOR result=%08lX state=%u width=%u stereo=%u after_route=1\n",
            (unsigned long)result, led, Graphics3DS::RenderWidth(), Graphics3DS::Stereo());
        fprintf(file,"NATIVE_3D_INDICATOR_LIFETIME attempts=%u releases=%u owner=%u disable=%u lcd=%08lX write=%08lX\n",
            s_indicatorAttempts,s_indicatorReleases,s_indicatorOwnsPolicy,OS_SharedConfig->led_3d,
            (unsigned long)s_indicatorLcdResult,(unsigned long)s_indicatorWriteResult);
    }
#endif
    fprintf(file, "NATIVE_STEREO active_frames=%u replayed_segments=%u minimum_slider=%.4f maximum_slider=%.4f scope=whole_session cpu_scene_traversals_per_segment=1\n",
        stereoFrames, stereoSegments, minimumStereoSlider, maximumStereoSlider);
    fprintf(file, "NATIVE_STEREO_DEPTH measured_frames=%u stereo_frames=%u slider_min=%.4f slider_max=%.4f convergence_min=%.4f convergence_max=%.4f maximum_background_pair_pixels=12 authored_background_pair_pixels=8 obstruction_foreground_budget_pixels=6 shader_register=95 gpu_bones=25 camera_profiles=1 world_frustum_budget=0 per_vertex_clamp=0\n",
        measuredFrames, measuredStereoFrames, measuredFrames ? measuredSliderMinimum : 0.0f,
        measuredSliderMaximum, measuredFrames ? measuredConvergenceMinimum : 0.0f, measuredConvergenceMaximum);
    fprintf(file,"NATIVE_STEREO_SLIDER shift_min=%.6f shift_max=%.6f bins=16 counts=",measuredFrames?minimumEyeShift:0.0f,maximumEyeShift);
    for(unsigned i=0;i<16;++i)fprintf(file,"%s%u",i?",":"",stereoSliderBins[i]);
    fprintf(file," measured_route_only=1\n");
    fprintf(file,"NATIVE_STEREO_COMMAND_AUDIT segments=%u scene_eye_writes=%u float24_eye_writes=%u errors=%u last_wzyx=%08lx,%08lx,%08lx,%08lx performance_capture=0\n",
        auditedStereoSegments,sceneEyeWrites,sceneEyeFloat24Writes,auditErrors,
        (unsigned long)lastSceneEye[0],(unsigned long)lastSceneEye[1],(unsigned long)lastSceneEye[2],(unsigned long)lastSceneEye[3]);
}
#endif


static void picaWriteEye(float shift)
{
    C3D_FVec v;
    v.x = shift;
    v.y = Graphics3DS::Stereo() ? Graphics3DS::StereoConvergence() : PICA_STEREO_CONVERGENCE;
    v.z = 0.0f;
    v.w = 0.0f;
    GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000u | VSH_FVEC_u_eye);
    GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)v.c, 4);
#ifdef SRR2_3DS_STEREO_GPU_SKIN
    if(Graphics3DS::Stereo())
    {
        // Slot 95 stays outside the complete 25-bone palette. Stereo companions
        // derive position/UV scales from x; z/w hold bias and eye shift.
        C3D_FVec packed;
        packed.x = 1.0f / 32768.0f;
        packed.y = shift < 0.0f ? -shift : shift;
        packed.z = shift * Graphics3DS::StereoConvergence(); packed.w = shift;
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
#ifdef SRR2_OLD_3DS
    picaRelease3DIndicator();
#endif
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
    const bool shadersReady = !requestedStereo || !context || context->StereoShadersReady();
    const bool allocateStereo = requestedStereo && shadersReady;
    // Retire every command referencing these buffers before changing output.
    // FrameSync waits for VBlank; it does not drain an unfinished GPU queue.
    C3Di_RenderQueueWaitDone();
    C3D_FrameSync();
    bool success = shadersReady;
    if(requestedWidth != renderWidth || allocateStereo != (targetRight != NULL))
    {
        if(targetRight) { C3D_RenderTargetDelete(targetRight); targetRight = NULL; }
        if(target) { C3D_RenderTargetDelete(target); target = NULL; }
        target = C3D_RenderTargetCreate(winHeight, requestedWidth, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if(target && allocateStereo)
            targetRight = C3D_RenderTargetCreate(winHeight, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        success = shadersReady && target && (!allocateStereo || targetRight);
        if(!success)
        {
            if(targetRight) { C3D_RenderTargetDelete(targetRight); targetRight = NULL; }
            if(target) C3D_RenderTargetDelete(target);
            target = C3D_RenderTargetCreate(winHeight, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        }
        renderWidth = success ? requestedWidth : 400;
    }
    Graphics3DS::Complete(success);
    if(context) context->PrepareFramePrograms();
    // gfxSet3D(false) also clears wide mode, so set wide mode last.
    gfxSet3D(Graphics3DS::Stereo());
    if(!Graphics3DS::Stereo()) gfxSetWide(renderWidth == 800);
    const u32 flags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
        GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
    if(target) C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, flags);
    if(targetRight) C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, flags);
    picaRequest3DIndicator();
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
    float slider = 0.0f;
    if(!inFrame)
    {
        slider = osGet3DSliderState();
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        slider = NativeBench::StereoSlider(slider);
#endif
#ifdef SRR2_OLD_3DS
        Graphics3DS::UpdateSlider(slider);
        if(Graphics3DS::CanApply() && !ApplyGraphicsMode()) return;
#endif
    }
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

        eyeShift = targetRight && Graphics3DS::Stereo() ? Graphics3DS::StereoShift(slider) : 0.0f;
        stereoThisFrame = eyeShift > 0.0f;

#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(NativeBench::collecting)
        {
            ++measuredFrames;
            const unsigned bin=slider>0.0f?(slider>=1.0f?15:unsigned(slider*16.0f)):0;
            ++stereoSliderBins[bin];
            if(eyeShift<minimumEyeShift)minimumEyeShift=eyeShift;
            if(eyeShift>maximumEyeShift)maximumEyeShift=eyeShift;
            if(stereoThisFrame) ++measuredStereoFrames;
            if(slider < measuredSliderMinimum) measuredSliderMinimum = slider;
            if(slider > measuredSliderMaximum) measuredSliderMaximum = slider;
            const float convergence = Graphics3DS::StereoConvergence();
            if(convergence < measuredConvergenceMinimum) measuredConvergenceMinimum = convergence;
            if(convergence > measuredConvergenceMaximum) measuredConvergenceMaximum = convergence;
        }
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
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
        // The scene must record its first use of each shared uniform even
        // when it matches the left-eye setup. The right eye starts with the
        // scene's final state, which can contain a foreground UI projection.
        // Keep register 95 outside this range: only eye setups may write it.
        picaNativeUniformInvalidate(GPU_VERTEX_SHADER, 0, 95);
#endif
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
        picaAuditStereoScene(sceneBuf,sceneSize);
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
        picaUpdate3DIndicator();
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

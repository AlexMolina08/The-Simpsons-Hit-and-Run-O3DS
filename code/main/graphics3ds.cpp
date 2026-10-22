#include <main/graphics3ds.h>
#include <cheats/portsettings.h>
#include <stdio.h>
#include <string.h>

namespace
{
    Graphics3DS::Model model = Graphics3DS::Unknown;
    bool quality = false, requested = false, stereo = false;
    bool pending = false, allowed = true, remember = false, failed = false;
    unsigned fpsOverride = 0;
    StereoDepth::State stereoDepth;
    unsigned PreferenceGroup()
    {
        return Graphics3DS::IsNew() ? 2 : model == Graphics3DS::Old2DS ? 1 : 0;
    }
}

void Graphics3DS::Initialize(int value, int argc, char** argv)
{
    model = value >= Old3DS && value <= New2DSXL ? Model(value) : Unknown;
    quality = false;
    requested = model != Unknown && PortSettings::GraphicsQuality(PreferenceGroup(), true);
    stereo = false; fpsOverride = 0; failed = false; remember = false; allowed = true;
    stereoDepth.Reset();
    // Diagnostic arguments never change the player's saved preference.
    const auto argument = [](const char* value) {
        if(!strcmp(value, "graphics_quality=0")) requested = false;
        if(!strcmp(value, "graphics_quality=1") && model != Unknown) requested = true;
        if(!strcmp(value, "graphics_stereo=0")) stereo = false;
        if(!strcmp(value, "graphics_stereo=1") && SupportsStereo()) stereo = true;
        if(!strcmp(value, "graphics_fps=60")) fpsOverride = 60;
        if(!strcmp(value, "graphics_fps=30")) fpsOverride = 30;
    };
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    FILE* capture = fopen("sdmc:/3ds/simpsons/native-automation.txt", "r");
    if(capture)
    {
        char line[128];
        while(fgets(line, sizeof(line), capture))
        {
            line[strcspn(line, "\r\n")] = 0;
            argument(line);
        }
        fclose(capture);
    }
#endif
    for(int i = 1; i < argc; ++i) argument(argv[i]);
    if(stereo) requested = false;
    pending = true;
}

Graphics3DS::Model Graphics3DS::SystemModel() { return model; }
bool Graphics3DS::IsNew() { return model == New3DS || model == New3DSXL || model == New2DSXL; }
bool Graphics3DS::SupportsWide() { return model != Old2DS && model != Unknown; }
bool Graphics3DS::SupportsStereo() { return SupportsWide() && model != New2DSXL; }
bool Graphics3DS::Quality() { return quality; }
bool Graphics3DS::RequestedQuality() { return requested; }
bool Graphics3DS::Stereo() { return stereo; }
void Graphics3DS::UpdateStereoCamera(StereoDepth::Camera camera, unsigned elapsedMilliseconds)
{
    if(stereo) stereoDepth.Update(camera, elapsedMilliseconds);
}
float Graphics3DS::StereoShift(float slider) { return stereoDepth.Shift(slider); }
float Graphics3DS::StereoConvergence() { return stereoDepth.convergence; }
bool Graphics3DS::Pending() { return pending; }
void Graphics3DS::AllowChanges(bool allow) { allowed = allow; }
bool Graphics3DS::CanApply() { return pending && allowed; }

void Graphics3DS::RequestQuality(bool enabled)
{
    if(model == Unknown || stereo || (!failed && enabled == requested)) return;
    requested = enabled; pending = true; remember = true; failed = false;
}

void Graphics3DS::Complete(bool success)
{
    quality = success && requested;
    if(!success) { requested = false; stereo = false; failed = true; }
    if(success && remember)
    {
        PortSettings::SetGraphicsQuality(PreferenceGroup(), quality);
        PortSettings::Save();
    }
    pending = remember = false;
    printf("GRAPHICS3DS model=%u quality=%u wide=%u stereo=%u width=%u height=240 target_fps=%u fallback=%u\n",
        unsigned(model), quality, quality && SupportsWide(), stereo, RenderWidth(), TargetFPS(), !success);
}

unsigned Graphics3DS::TargetFPS() { return fpsOverride ? fpsOverride : IsNew() && !quality ? 60 : 30; }
unsigned Graphics3DS::RenderWidth() { return quality && SupportsWide() && !stereo ? 800 : 400; }
bool Graphics3DS::ImprovedLighting() { return quality && model == Old2DS; }
const char* Graphics3DS::Description()
{
    if(model == Unknown) return "STANDARD DISPLAY";
    if(stereo) return "3D DISPLAY TEST";
    if(model == Old2DS) return "ON: RICHER LIGHTING";
    return "ON: SHARPER IMAGE";
}
const char* Graphics3DS::Status()
{
    if(failed) return "QUALITY UNAVAILABLE";
    if(pending) return "APPLYING...";
    if(stereo) return "400x240 PER EYE";
    if(quality && SupportsWide()) return "800x240 / 30 FPS TARGET";
    return IsNew() ? "400x240 / 60 FPS TARGET" : "400x240 / 30 FPS TARGET";
}

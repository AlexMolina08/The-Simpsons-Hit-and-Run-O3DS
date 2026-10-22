#ifndef GRAPHICS3DS_H
#define GRAPHICS3DS_H

// Model values match CFG_SystemModel. Keep this policy testable without libctru.
namespace Graphics3DS
{
    enum Model { Old3DS, Old3DSXL, New3DS, Old2DS, New3DSXL, New2DSXL, Unknown };
    void Initialize(int model, int argc = 0, char** argv = 0);
    Model SystemModel();
    bool IsNew();
    bool SupportsWide();
    bool SupportsStereo();
    bool Quality();
    bool RequestedQuality();
    bool Stereo();
    bool Pending();
    void AllowChanges(bool allow);
    bool CanApply();
    void RequestQuality(bool enabled);
    void Complete(bool success);
    unsigned TargetFPS();
    unsigned RenderWidth();
    bool ImprovedLighting();
    const char* Description();
    const char* Status();
}
#endif

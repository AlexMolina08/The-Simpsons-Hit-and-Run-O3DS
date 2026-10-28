#ifndef PDDI_PICA_DISPLAY_HPP
#define PDDI_PICA_DISPLAY_HPP

#include <pddi/pica/pica.hpp>

const float PICA_STEREO_CONVERGENCE = 7.0f;

struct SDL_Window;

class picaDisplay : public pddiDisplay
{
public:
    picaDisplay(pddiDisplayInfo* info);
    ~picaDisplay();

    long ProcessWindowMessage(SDL_Window* wnd, const SDL_WindowEvent* event);
    void SetWindow(SDL_Window* wnd);

    pddiDisplayInfo* GetDisplayInfo(void);

    bool InitDisplay(const pddiDisplayInit*);
    bool InitDisplay(int x, int y, int bpp);

    int GetHeight(void);
    int GetWidth(void);
    int GetDepth(void);
    pddiDisplayMode GetDisplayMode(void);
    int GetNumColourBuffer(void);
    unsigned GetBufferMask(void);

    unsigned GetFreeTextureMem(void);

    void SwapBuffers(void);

    unsigned Screenshot(pddiColour* buffer, int nBytes);

    void  SetContext(picaContext* c) { context = c; }
    bool  HasReset(void) { return reset; }

    void  BeginTiming(void);
    float EndTiming(void);

    C3D_RenderTarget* GetTarget(void) { return target; }

    void BeginFrame(void);
#ifdef SRR2_OLD_3DS
    bool BeginWorldTarget(void);
    bool EndWorldTarget(void);
    bool StereoActive(void) const { return stereoThisFrame; }
    void SubmitWorldEarly(void);
    float StereoEyeShift(void) const { return eyeShift; }
    bool IsWorldTarget(void) const { return worldActive; }
    int WorldWidth(void) const { return worldWidth; }
    int WorldHeight(void) const { return worldHeight; }
    int RenderWidth(void) const { return renderWidth; }
    C3D_Tex* WorldTexture(void) { return &worldTexture; }
#endif

    bool BeginBottomTarget(void);
    void EndBottomTarget(void);
    void EndFrame(void);

private:
    pddiDisplayMode  mode;
    pddiDisplayInfo* displayInfo;
    pddiDisplayInit  displayInit;

    picaContext* context;

    int winWidth;
    int winHeight;
    int winBitDepth;

    C3D_RenderTarget* target;
    C3D_RenderTarget* targetRight;
    C3D_RenderTarget* targetBottom;
#ifdef SRR2_OLD_3DS
    C3D_Tex worldTexture;
    C3D_RenderTarget* worldTarget;
    bool worldActive;
    int worldWidth, worldHeight;
    int renderWidth;
    bool ApplyGraphicsMode(void);
#endif
    bool  stereoThisFrame;
    u32* earlyWorldCommands;
    u32 earlyWorldWords;

    void StartTopSegment(void);
    void FinishTopSegment(void);
    float eyeShift;
    bool  c3dStarted;
    bool  inFrame;
    bool  reset;

    float beginTime;
};
#endif

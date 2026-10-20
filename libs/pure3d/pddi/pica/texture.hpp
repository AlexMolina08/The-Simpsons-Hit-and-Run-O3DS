#ifndef PDDI_PICA_TEXTURE_HPP
#define PDDI_PICA_TEXTURE_HPP

#include <pddi/pica/pica.hpp>
#include <pddi/pdditype.hpp>

struct picaVramAllocation;
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
struct picaNativeTextureStats { unsigned hits,misses,invalid,bytes,cost[3][5]; };
void picaSnapshotNativeTextures(picaNativeTextureStats&);
#ifdef SRR2_OLD3DS_NATIVE_ETC1
extern bool g_picaNativeEtcEnabled;
#endif
#endif


class picaTexture : public pddiTexture
{
public:
    picaTexture(picaContext*);
    ~picaTexture();

    bool Create(int xSize, int ySize, int bpp, int alphaDepth, int nMip,
                pddiTextureType type = PDDI_TEXTYPE_RGB,
                pddiTextureUsageHint usageHint = PDDI_USAGE_STATIC);

    pddiPixelFormat GetPixelFormat();
    int GetWidth();
    int GetHeight();
    int GetDepth();
    int GetNumMipMaps();
    int GetAlphaDepth();

    int GetNumPaletteEntries(void);
    void SetPalette(int nEntries, pddiColour* palette);
    int GetPalette(pddiColour* palette);

    pddiLockInfo* Lock(int mipLevel, pddiRect* rect = 0);
    void Unlock(int mipLevel);
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
    // Newly allocated static textures only. Failed reads remain unpublished;
    // the ordinary image decoder may fill this same allocation afterward.
    bool UploadPreparedPNG(const void* encoded,unsigned bytes,bool world=false);
#endif
#ifdef SRR2_OLD_3DS
    bool UploadMovieYUV(const unsigned char* y, const unsigned char* cr, const unsigned char* cb,
                        int width, int height, int yPitch, int chromaPitch);
#endif

    void Prefetch(void);
    void Discard(void);
    void SetPriority(int priority);
    int GetPriority();

    C3D_Tex* GetC3DTexture(void) {
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
        if(valid)renderExposed=true;
#endif
        return valid ? &tex : NULL;
    }
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    unsigned NativeRevision() const {return nativeRevision;}
#endif

protected:
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    unsigned nativeRevision=1;
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
    bool nativeEligible=false;
    bool UploadPrepared();
    bool ReadPrepared(const void* source,unsigned sourceBytes,unsigned sourceKind,bool world=false);
#endif
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    // Preserve the original mip-allocation and VRAM-budget policy while testing
    // compressed transfer/sampling. The backing allocation retains this size.
    unsigned nativeAllocationBytes=0;
#endif
    void FreeStaging(void);
    void UploadLevel(int mipLevel);
    void ConvertAndTile(const unsigned int* rgba, int w, int h, void* dst);

#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    void TryPromoteVram();
    picaVramAllocation* vramAllocation;
    void* reuploadLinear;
    bool vramEligible, vramUploaded;
#endif
    picaContext* context;

    int log2X, log2Y;
    int xSize, ySize;
    pddiTextureType type;
    int nMipMap;
    int alphaDepth;
    int priority;

#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // Monotonic: any descriptor handed to rendering keeps all future mutation
    // fences. Newly decoded textures have never been read by an earlier frame.
    bool renderExposed=false;
#endif
    C3D_Tex  tex;
    bool    created;
    bool    valid;
    GPU_TEXCOLOR hwFormat;

    pddiLockInfo lock;

    unsigned char* staging;
    unsigned       stagingBytes;
    int            stagingLevel;
};
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
bool picaNativeTexturesAvailable();
#endif
#endif

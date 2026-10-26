#ifndef PDDI_PICA_MATERIAL_HPP
#define PDDI_PICA_MATERIAL_HPP

#include <pddi/pica/pica.hpp>
#include <pddi/base/baseshader.hpp>


const int picaMaxPasses = 1;

struct picaTextureEnv
{
    bool enabled;
    picaTexture* texture;

    int uvSet;
    pddiTextureGen texGen;
    pddiUVMode uvMode;
    pddiFilterMode filterMode;

    bool alphaTest;
    pddiBlendMode alphaBlendMode;
    pddiCompareMode alphaCompareMode;
    float alphaRef;
    int writeMask;

    bool lit;
    bool twoSided;
    pddiShadeMode shadeMode;
    pddiColour diffuse;
    pddiColour specular;
    pddiColour ambient;
    pddiColour emissive;
    float shininess;
};

class picaMat : public pddiBaseShader
{
public:
    picaMat(picaContext*);
    ~picaMat();

    static pddiShadeColourTable colourTable[];
    static pddiShadeTextureTable textureTable[];
    static pddiShadeIntTable intTable[];
    static pddiShadeFloatTable floatTable[];

#ifdef SRR2_OLD3DS_MATERIAL_WRITES
    bool SetInt(unsigned parameter,int value) override;
    bool SetColour(unsigned parameter,pddiColour value) override;
#endif
    const char* GetType(void);
    int         GetPasses(void);
    void        SetPass(int pass);

    pddiShadeTextureTable* GetTextureTable(void) { return textureTable; }
    pddiShadeIntTable*     GetIntTable(void)     { return intTable; }
    pddiShadeFloatTable*   GetFloatTable(void)   { return floatTable; }
    pddiShadeColourTable*  GetColourTable(void)  { return colourTable; }

    void SetTexture(pddiTexture* texture);
    void SetUVMode(int mode);
    void SetFilterMode(int mode);

    void SetShadeMode(int shade);
    void SetTwoSided(int);

    void EnableLighting(int);

    void SetDiffuse(pddiColour colour);
    void SetAmbient(pddiColour colour);
    void SetEmissive(pddiColour);
    void SetEmissiveAlpha(int);
    void SetSpecular(pddiColour);
    void SetShininess(float power);

    void SetBlendMode(int mode);
    void EnableAlphaTest(int);
    void SetAlphaCompare(int compare);
    void SetAlphaRef(float ref);

    void SetColourWrite(int mask);

    int  CountDevPasses(void);
    void SetDevPass(unsigned);
    void PreRender(void);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    void SetVisualType(const char* name);
    bool VehicleSurface() const { return vehicleSurface; }
    bool WorldSurface() const { return worldSurface; }
    void SetReflectionMap(pddiTexture* texture);
    void SetEnvironmentBlend(pddiColour colour);
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    bool WheelLodOpaque() const { return texEnv[0].alphaBlendMode==PDDI_BLEND_NONE && texEnv[0].diffuse.Alpha()==255; }
#endif

private:
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    bool ReflectionEnabled() const;
    bool WorldLightmapEnabled() const;
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
    void PrepareNative();
    void PrepareColours();
    bool nativeDirty;
    GPU_TEXTURE_FILTER_PARAM nativeFilter;
    GPU_TEXTURE_WRAP_PARAM nativeWrap;
    GPU_TESTFUNC nativeCompare;
    GPU_BLENDEQUATION nativeEquation;
    GPU_BLENDFACTOR nativeSource, nativeDestination;
    int nativeAlphaRef;
    bool coloursValid;
    float nativeColours[3][4];
#endif
    picaContext* context;
    int pass;
    bool dirty;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    // Use the existing padding before texEnv so ordinary material fields keep
    // their original offsets. Reflection-only data stays after the hot prefix.
    bool vehicleSurface = false;
    bool worldSurface = false;
    bool glassSurface = false;
#endif
    picaTextureEnv texEnv[picaMaxPasses];
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    picaTexture* reflectionMap = nullptr;
    pddiColour environmentBlend = pddiColour(32,32,32);
#endif
};
#endif

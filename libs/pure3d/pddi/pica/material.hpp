#ifndef PDDI_PICA_MATERIAL_HPP
#define PDDI_PICA_MATERIAL_HPP

#include <pddi/pica/pica.hpp>
#include <pddi/base/baseshader.hpp>

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
struct picaPreparedMaterialPass;
#endif
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
struct picaRetainedMaterial;
#endif

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
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    bool CapturePreparedPass(picaPreparedMaterialPass&);
    static bool PreparedPreviousShader();
#endif
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    picaRetainedMaterial* PrepareRetainedMaterial();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    bool WheelLodOpaque() const { return texEnv[0].alphaBlendMode==PDDI_BLEND_NONE && texEnv[0].diffuse.Alpha()==255; }
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    bool QueueOpaque() const { return texEnv[0].alphaBlendMode == PDDI_BLEND_NONE && texEnv[0].writeMask == PDDI_WRITE_ALL; }
    bool CanApplyQueued(picaMat* previous) const;
    void ApplyQueued(picaMat* previous);
#endif

#ifdef SRR2_OLD3DS_STATIC_BATCH
    bool StaticBatchOpaque() const { return texEnv[0].alphaBlendMode==PDDI_BLEND_NONE && texEnv[0].writeMask==PDDI_WRITE_ALL; }
#endif
private:
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    picaRetainedMaterial* retainedMaterial=nullptr;
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
    picaTextureEnv texEnv[picaMaxPasses];
};
#endif

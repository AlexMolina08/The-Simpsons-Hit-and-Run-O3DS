#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <main/old3dsmainprofile.h>
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
#include <pddi/pica/pica.hpp>
#include <pddi/pica/material.hpp>
#include <pddi/pica/texture.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/pica/statecache.hpp>
#include <pddi/pica/display.hpp>
#include <pddi/pica/visualstyle.hpp>
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
#include <pddi/pica/nativecontext.h>
#endif

#include <pddi/base/debug.hpp>
#include <stdio.h>
#include <string.h>
#include <main/old3dsdiag.h>


pddiShadeColourTable picaMat::colourTable[] =
{
    {PDDI_SP_AMBIENT  , SHADE_COLOUR(&picaMat::SetAmbient)},
    {PDDI_SP_DIFFUSE  , SHADE_COLOUR(&picaMat::SetDiffuse)},
    {PDDI_SP_EMISSIVE , SHADE_COLOUR(&picaMat::SetEmissive)},
    {PDDI_SP_SPECULAR , SHADE_COLOUR(&picaMat::SetSpecular)},
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    {PDDI_SP_ENVBLEND , SHADE_COLOUR(&picaMat::SetEnvironmentBlend)},
#endif
    {PDDI_SP_NULL , NULL}
};

pddiShadeTextureTable picaMat::textureTable[] =
{
    {PDDI_SP_BASETEX , SHADE_TEXTURE(&picaMat::SetTexture)},
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    {PDDI_SP_REFLMAP , SHADE_TEXTURE(&picaMat::SetReflectionMap)},
#endif
    {PDDI_SP_NULL , NULL}
};

pddiShadeIntTable picaMat::intTable[] =
{
    {PDDI_SP_UVMODE , SHADE_INT(&picaMat::SetUVMode)},
    {PDDI_SP_FILTER , SHADE_INT(&picaMat::SetFilterMode)},
    {PDDI_SP_SHADEMODE , SHADE_INT(&picaMat::SetShadeMode)},
    {PDDI_SP_ISLIT , SHADE_INT(&picaMat::EnableLighting)},
    {PDDI_SP_BLENDMODE , SHADE_INT(&picaMat::SetBlendMode)},
    {PDDI_SP_ALPHATEST , SHADE_INT(&picaMat::EnableAlphaTest)},
    {PDDI_SP_ALPHACOMPARE , SHADE_INT(&picaMat::SetAlphaCompare)},
    {PDDI_SP_TWOSIDED , SHADE_INT(&picaMat::SetTwoSided)},
    {PDDI_SP_EMISSIVEALPHA , SHADE_INT(&picaMat::SetEmissiveAlpha)},
    {PDDI_SP_COLOURWRITE , SHADE_INT(&picaMat::SetColourWrite)},
    {PDDI_SP_NULL , NULL}
};

pddiShadeFloatTable picaMat::floatTable[] =
{
    {PDDI_SP_SHININESS , SHADE_FLOAT(&picaMat::SetShininess)},
    {PDDI_SP_ALPHACOMPARE_THRESHOLD , SHADE_FLOAT(&picaMat::SetAlphaRef)},
    {PDDI_SP_NULL , NULL}
};

static const GPU_TEXTURE_FILTER_PARAM filterTable[5] =
{
    GPU_NEAREST,
    GPU_LINEAR,
    GPU_NEAREST,
    GPU_LINEAR,
    GPU_LINEAR
};

static const GPU_TEXTURE_WRAP_PARAM uvTable[3] =
{
    GPU_REPEAT,
    GPU_CLAMP_TO_EDGE,
    GPU_CLAMP_TO_EDGE
};

static const GPU_TESTFUNC alphaCompareTable[8] =
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

static const GPU_BLENDEQUATION blendEquation[8] =
{
    GPU_BLEND_ADD,
    GPU_BLEND_ADD,
    GPU_BLEND_ADD,
    GPU_BLEND_REVERSE_SUBTRACT,
    GPU_BLEND_ADD,
    GPU_BLEND_ADD,
    GPU_BLEND_ADD,
    GPU_BLEND_REVERSE_SUBTRACT
};

static const GPU_BLENDFACTOR blendFactor[8][2] =
{
    { GPU_ONE,       GPU_ZERO },
    { GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA },
    { GPU_ONE,       GPU_ONE },
    { GPU_ONE,       GPU_ONE },
    { GPU_DST_COLOR, GPU_ZERO },
    { GPU_DST_COLOR, GPU_SRC_COLOR },
    { GPU_ONE,       GPU_SRC_ALPHA },
    { GPU_SRC_ALPHA, GPU_SRC_ALPHA }
};

#ifdef SRR2_OLD3DS_MATERIAL_WRITES
static unsigned picaMaterialWriteCalls=0,picaMaterialWriteReuses=0;
extern "C" void picaDumpMaterialWrites()
{
    printf("MATERIAL_WRITES calls=%u unchanged=%u\n",picaMaterialWriteCalls,picaMaterialWriteReuses);
}
bool picaMat::SetInt(unsigned parameter,int value)
{
    ++picaMaterialWriteCalls;
    const picaTextureEnv& e=texEnv[pass];
    bool unchanged=false;
    switch(parameter)
    {
        case PDDI_SP_BLENDMODE: unchanged=e.alphaBlendMode==value;break;
        case PDDI_SP_EMISSIVEALPHA:
        {
            const int alpha=value<255 ? 0 : 255;
            unchanged=e.diffuse.Alpha()==value && e.ambient.Alpha()==alpha &&
                e.specular.Alpha()==alpha && e.emissive.Alpha()==alpha;
            break;
        }
        default: break;
    }
    // An identical scalar write changes neither native state nor uniforms.
    // Texture writes, unsupported parameters and all real changes retain the
    // original dispatch/UID protocol. PreRender still runs on every draw.
    if(unchanged) {++picaMaterialWriteReuses;return true;}
    return pddiBaseShader::SetInt(parameter,value);
}
bool picaMat::SetColour(unsigned parameter,pddiColour value)
{
    ++picaMaterialWriteCalls;
    const picaTextureEnv& e=texEnv[pass];
    bool unchanged=false;
    switch(parameter)
    {
        case PDDI_SP_AMBIENT: unchanged=e.ambient.c==value.c;break;
        case PDDI_SP_DIFFUSE: unchanged=e.diffuse.c==value.c;break;
        case PDDI_SP_SPECULAR: unchanged=e.specular.c==value.c;break;
        case PDDI_SP_EMISSIVE:
        {
            const int alpha=value.Alpha()<255 ? 0 : 255;
            unchanged=e.emissive.c==value.c && e.diffuse.Alpha()==value.Alpha() &&
                e.ambient.Alpha()==alpha && e.specular.Alpha()==alpha;
            break;
        }
        default: break;
    }
    if(unchanged) {++picaMaterialWriteReuses;return true;}
    return pddiBaseShader::SetColour(parameter,value);
}
#endif

picaMat::picaMat(picaContext* c)
{
    context = c;

    for(int i = 0; i < picaMaxPasses; i++)
    {
        texEnv[i].enabled    = false;
        texEnv[i].texture    = NULL;
        texEnv[i].uvSet      = i;
        texEnv[i].texGen     = PDDI_TEXGEN_NONE;
        texEnv[i].uvMode     = PDDI_UV_CLAMP;
        texEnv[i].filterMode = PDDI_FILTER_BILINEAR;

        texEnv[i].lit       = false;
        texEnv[i].twoSided  = false;
        texEnv[i].shadeMode = PDDI_SHADE_GOURAUD;
        texEnv[i].ambient.Set(255, 255, 255);
        texEnv[i].diffuse.Set(255, 255, 255);
        texEnv[i].specular.Set(0, 0, 0);
        texEnv[i].emissive.Set(0, 0, 0);
        texEnv[i].shininess = 0.0f;

        texEnv[i].alphaTest        = false;
        texEnv[i].alphaCompareMode = PDDI_COMPARE_GREATEREQUAL;
        texEnv[i].alphaBlendMode   = PDDI_BLEND_NONE;
        texEnv[i].alphaRef         = 0.5f;

        texEnv[i].writeMask = PDDI_WRITE_ALL;
    }
    texEnv[0].enabled = true;
    pass = 0;
    dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif

#ifdef SRR2_OLD3DS_DRAW_PREP
    coloursValid = false;
#endif
    lastError = PDDI_OK;
}

picaMat::~picaMat()
{
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(reflectionMap) reflectionMap->Release();
#endif
    for(int i = 0; i < picaMaxPasses; i++)
        if(texEnv[i].texture)
            texEnv[i].texture->Release();
}

#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
void picaMat::SetVisualType(const char* name)
{
    glassSurface = name && !strcmp(name, "vehicle_glass");
    worldSurface = name && !strcmp(name, "worldsun");
    vehicleSurface = glassSurface || (name && (!strcmp(name, "spheremap") || !strcmp(name, "environment")));
}
void picaMat::SetReflectionMap(pddiTexture* texture)
{
    if(texture == reflectionMap) return;
    if(texture) texture->AddRef();
    if(reflectionMap) reflectionMap->Release();
    reflectionMap = static_cast<picaTexture*>(texture);
    dirty = true;
}
void picaMat::SetEnvironmentBlend(pddiColour colour)
{
    if(vehicleSurface && !glassSurface && picaVisual::Enabled(picaVisual::KeyLighting) &&
       colour.Red()==colour.Green() && colour.Red()==colour.Blue())
    {
        const unsigned strength=picaVisual::CoatReflectivity(colour.Red());
        colour=pddiColour(strength,strength,strength,colour.Alpha());
    }
    if(environmentBlend.c == colour.c) return;
    environmentBlend = colour;
    dirty = true;
}
bool picaMat::WorldLightmapEnabled() const
{
    return worldSurface && reflectionMap && reflectionMap->WorldLightmapParameters() &&
        picaVisual::Enabled(picaVisual::SceneryLighting) && !texEnv[0].lit &&
        texEnv[0].alphaBlendMode==PDDI_BLEND_NONE && !texEnv[0].alphaTest &&
        !context->skinProgramBound && context->ReflectionProgram();
}
bool picaMat::ReflectionEnabled() const
{
    return vehicleSurface && reflectionMap && picaVisual::Enabled(picaVisual::Reflections) &&
        (texEnv[0].alphaBlendMode == PDDI_BLEND_NONE ||
         (glassSurface && texEnv[0].alphaBlendMode == PDDI_BLEND_ALPHA)) && !context->skinProgramBound &&
        context->ReflectionProgram();
}
#endif

const char* picaMat::GetType(void)
{
    static char simple[] = "simple";
    return simple;
}

int picaMat::GetPasses(void)
{
    return 1;
}

void picaMat::SetPass(int p)
{
    SetDevPass(p);
}

void picaMat::SetTexture(pddiTexture* t)
{
    if(t == texEnv[pass].texture)
        return;

    if(texEnv[pass].texture)
        texEnv[pass].texture->Release();

    texEnv[pass].texture = (picaTexture*)t;

    if(texEnv[pass].texture)
        texEnv[pass].texture->AddRef();

    dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}

void picaMat::SetUVMode(int mode)     {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].uvMode == (pddiUVMode)mode) return;
#endif
    texEnv[pass].uvMode = (pddiUVMode)mode; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::SetFilterMode(int mode) {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].filterMode == (pddiFilterMode)mode) return;
#endif
    texEnv[pass].filterMode = (pddiFilterMode)mode; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::SetShadeMode(int shade) { texEnv[pass].shadeMode = (pddiShadeMode)shade; }
void picaMat::SetTwoSided(int b)      {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].twoSided == (b != 0)) return;
#endif
    texEnv[pass].twoSided = b != 0; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::EnableLighting(int b)   { texEnv[pass].lit = b != 0; }

void picaMat::SetAmbient(pddiColour a)
{
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].ambient.c != a.c) coloursValid = false;
#endif
    texEnv[pass].ambient = a;
}
void picaMat::SetDiffuse(pddiColour colour)
{
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].diffuse.c != colour.c) coloursValid = false;
#endif
    texEnv[pass].diffuse = colour;
}
void picaMat::SetSpecular(pddiColour c)      { texEnv[pass].specular = c; }

void picaMat::SetEmissive(pddiColour c)
{
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].emissive.c != c.c) coloursValid = false;
#endif
    texEnv[pass].emissive = c;
    SetEmissiveAlpha(c.Alpha());
}

void picaMat::SetEmissiveAlpha(int alpha)
{
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].diffuse.Alpha() != alpha) coloursValid = false;
#endif
    texEnv[pass].diffuse.SetAlpha(alpha);
    if(alpha < 255)
    {
        texEnv[pass].specular.SetAlpha(0);
        texEnv[pass].ambient.SetAlpha(0);
        texEnv[pass].emissive.SetAlpha(0);
    }
    else
    {
        texEnv[pass].specular.SetAlpha(255);
        texEnv[pass].ambient.SetAlpha(255);
        texEnv[pass].emissive.SetAlpha(255);
    }
}

void picaMat::SetShininess(float power)  { texEnv[pass].shininess = power; }
void picaMat::SetBlendMode(int mode)     {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].alphaBlendMode == (pddiBlendMode)mode) return;
#endif
    texEnv[pass].alphaBlendMode = (pddiBlendMode)mode; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::EnableAlphaTest(int b)     {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].alphaTest == (b != 0)) return;
#endif
    texEnv[pass].alphaTest = b != 0; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::SetAlphaCompare(int c)     {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].alphaCompareMode == pddiCompareMode(c)) return;
#endif
    texEnv[pass].alphaCompareMode = pddiCompareMode(c); dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::SetAlphaRef(float ref)     {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].alphaRef == ref) return;
#endif
    texEnv[pass].alphaRef = ref; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}
void picaMat::SetColourWrite(int mask)   {
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(texEnv[pass].writeMask == mask) return;
#endif
    texEnv[pass].writeMask = mask; dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
}

int picaMat::CountDevPasses(void)
{
    return 1;
}

#ifdef SRR2_OLD3DS_DRAW_PREP
void picaMat::PrepareNative()
{
    if(!nativeDirty) return;
    const picaTextureEnv& e = texEnv[0];
    nativeFilter = filterTable[e.filterMode];
    nativeWrap = uvTable[e.uvMode];
    nativeCompare = e.alphaTest ? alphaCompareTable[e.alphaCompareMode] : GPU_ALWAYS;
    nativeAlphaRef = 0;
    if(e.alphaTest)
    {
        nativeAlphaRef = (int)(e.alphaRef * 255.0f);
        if(nativeAlphaRef < 0) nativeAlphaRef = 0;
        if(nativeAlphaRef > 255) nativeAlphaRef = 255;
    }
    nativeEquation = blendEquation[e.alphaBlendMode];
    nativeSource = blendFactor[e.alphaBlendMode][0];
    nativeDestination = blendFactor[e.alphaBlendMode][1];
    nativeDirty = false;
}
void picaMat::PrepareColours()
{
    const int i=0;
    // Every colour writer invalidates this data on an actual change, including
    // shared-material fades and swatches between draws in the same frame.
    if(texEnv[i].lit && !coloursValid)
    {
        const pddiColour colours[3] = {texEnv[i].ambient, texEnv[i].diffuse, texEnv[i].emissive};
        for(unsigned c = 0; c < 3; ++c)
        {
            nativeColours[c][0] = (float)colours[c].Red() / 255.0f;
            nativeColours[c][1] = (float)colours[c].Green() / 255.0f;
            nativeColours[c][2] = (float)colours[c].Blue() / 255.0f;
        }
        nativeColours[0][3] = 1.0f;
        nativeColours[1][3] = (float)texEnv[i].diffuse.Alpha() / 255.0f;
        nativeColours[2][3] = 0.0f;
        coloursValid = true;
    }
}

#endif


void picaMat::SetDevPass(unsigned)
{
    O3_MAIN_SCOPE(MaterialPass);
#ifdef SRR2_OLD_3DS
    Old3dsCount(O3_MATERIAL_PASS);
#endif
    #ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_MATERIAL);
#endif
    const int i = 0;
#ifdef SRR2_OLD3DS_DRAW_PREP
    PrepareNative();
#endif

    dirty = false;

    if(!g_picaInFrame)
    {
        dirty = true;
#ifdef SRR2_OLD3DS_DRAW_PREP
    nativeDirty = true;
#endif
        return;
    }

    C3D_Tex* hwTex = texEnv[i].texture ? texEnv[i].texture->GetC3DTexture() : NULL;

    if(hwTex)
    {
#ifdef SRR2_OLD3DS_DRAW_PREP
        C3D_TexSetFilter(hwTex, nativeFilter, nativeFilter);
        C3D_TexSetWrap(hwTex, nativeWrap, nativeWrap);
#else
        C3D_TexSetFilter(hwTex, filterTable[texEnv[i].filterMode],
                                filterTable[texEnv[i].filterMode]);
        C3D_TexSetWrap(hwTex, uvTable[texEnv[i].uvMode], uvTable[texEnv[i].uvMode]);
#endif
        picaTexBind0(hwTex);
    }

    const int env0Mode = hwTex ? 1 : 2;
    if(g_picaState.env0Mode != env0Mode)
    {
        C3D_TexEnv* env0 = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env0);
        if(hwTex)
        {
            C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env0, C3D_Both, GPU_MODULATE);
        }
        else
        {
            C3D_TexEnvSrc(env0, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
        }
        g_picaState.env0Mode = env0Mode;
    }

    if(!g_picaState.passStagesSet)
    {
        for(int stage = 1; stage < 6; stage++)
        {
            C3D_TexEnv* env = C3D_GetTexEnv(stage);
            C3D_TexEnvInit(env);
            C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_PREVIOUS, GPU_PREVIOUS);
            C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        }
        g_picaState.passStagesSet = true;
    }

#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    const bool worldLightmap=worldSurface && WorldLightmapEnabled();
    C3D_Tex* reflection = (worldLightmap || (vehicleSurface && ReflectionEnabled())) ? reflectionMap->GetC3DTexture() : nullptr;
    const unsigned colour = environmentBlend.Red() | (environmentBlend.Green()<<8) |
        (environmentBlend.Blue()<<16) | 0xff000000u;
    if(!g_picaState.reflectionValid || g_picaState.reflectionTexture != reflection ||
       (reflection && (g_picaState.reflectionColour != colour || g_picaState.worldLightmapStage!=worldLightmap)))
    {
        picaNativeBindTexture1(reflection);
        C3D_TexEnv* env = C3D_GetTexEnv(1);
        C3D_TexEnvInit(env);
        if(reflection)
        {
            if(worldLightmap) {
                C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_TEXTURE1);
                C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
            } else {
            C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE1, GPU_CONSTANT, GPU_PREVIOUS);
            C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
            C3D_TexEnvColor(env, colour);
            }
        }
        // Preserve stage-zero alpha, including texture cutouts and vehicle fades.
        C3D_TexEnvSrc(env, C3D_Alpha, GPU_PREVIOUS);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
        g_picaState.reflectionTexture = reflection;
        g_picaState.reflectionColour = colour;
        g_picaState.reflectionValid = true;
        g_picaState.worldLightmapStage=worldLightmap;
    }
#endif

#ifdef SRR2_OLD3DS_DRAW_PREP
    picaAlphaTest(texEnv[i].alphaTest, nativeCompare, nativeAlphaRef);
    picaAlphaBlend(nativeEquation, nativeEquation, nativeSource,
                   nativeDestination, nativeSource, nativeDestination);
#else
    if(texEnv[i].alphaTest)
    {
        int ref = (int)(texEnv[i].alphaRef * 255.0f);
        if(ref < 0)   ref = 0;
        if(ref > 255) ref = 255;
        picaAlphaTest(true, alphaCompareTable[texEnv[i].alphaCompareMode], ref);
    }
    else
    {
        picaAlphaTest(false, GPU_ALWAYS, 0);
    }

    picaAlphaBlend(blendEquation[texEnv[i].alphaBlendMode],
                   blendEquation[texEnv[i].alphaBlendMode],
                   blendFactor[texEnv[i].alphaBlendMode][0],
                   blendFactor[texEnv[i].alphaBlendMode][1],
                   blendFactor[texEnv[i].alphaBlendMode][0],
                   blendFactor[texEnv[i].alphaBlendMode][1]);

#endif
    context->EnableCulling(!texEnv[i].twoSided);
    context->SetMaterialWriteMask(texEnv[i].writeMask);
}

void picaMat::PreRender(void)
{
    O3_MAIN_SCOPE(MaterialPreRender);
    const int i = 0;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    const bool worldLightmap = worldSurface && WorldLightmapEnabled();
    const bool reflective = vehicleSurface && ReflectionEnabled();
    const unsigned char desired=worldLightmap?picaContext::SurfaceWorld:
        reflective?picaContext::SurfaceReflection:picaContext::SurfaceOrdinary;
    if(context->reflectionState!=picaContext::SurfaceSkin &&
       context->reflectionState!=desired)
        context->BindReflectionProgram(reflective || worldLightmap,worldLightmap);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    if(reflective) ++picaVisual::reflectionDraws;
    if(worldLightmap) ++picaVisual::sceneryDraws;
#else
    (void)reflective;
#endif
#endif
#ifdef SRR2_OLD3DS_RUNTIME_PROFILE
    g_old3dsDrawBlended = texEnv[i].alphaBlendMode != PDDI_BLEND_NONE;
    g_old3dsDrawAlphaTested = texEnv[i].alphaTest;
#endif

    if(dirty)
    {
        SetDevPass(0);
    }

#ifdef SRR2_OLD3DS_DRAW_PREP
    PrepareColours();
    context->SetVertexUniforms(texEnv[i].lit, texEnv[i].ambient,
                               texEnv[i].diffuse, texEnv[i].emissive, nativeColours);
#else
    context->SetVertexUniforms(texEnv[i].lit,
                               texEnv[i].ambient,
                               texEnv[i].diffuse,
                               texEnv[i].emissive);
#endif
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(worldLightmap) context->SetWorldLightmapParameters(reflectionMap);
#endif
}

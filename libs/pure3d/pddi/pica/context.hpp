#ifndef PDDI_PICA_CONTEXT_HPP
#define PDDI_PICA_CONTEXT_HPP

#include <pddi/pica/pica.hpp>
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
#include <pddi/pica/clipbounds.hpp>
#endif
#include <pddi/pica/skinning.hpp>
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
#include <pddi/pica/contactshadow.hpp>
#endif
#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
#include <pddi/pica/lightcapture.hpp>
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_CACHE
#include <pddi/pica/skineyecache.hpp>
#endif

const unsigned PICA_IMM_VERTEX_BYTES = 512 * 1024;
const unsigned PICA_IMM_INDEX_BYTES  = 96 * 1024;
const unsigned PICA_IMM_BUFFERS      = 2;

const int PICA_IMM_DEFAULT_VERTICES = 256;

class picaExtGamma;
class picaPrimBuffer;

namespace picaOcclusion {struct Geometry;}
struct picaPreparedUniformInput;
struct picaPreparedDrawInputs;
struct picaPreparedGeometry;
struct picaPreparedMaterialPass;
struct picaPreparedMaterialUniforms;
struct picaPreparedMaterialRaster;
struct picaPreparedState;
struct picaPreparedSkinPalette;
struct picaPreparedUniformView;
struct picaPreparedUniformRecord;
class picaPreparedInputPool;
class picaPreparedExecutor;
class picaPreparedWorker;
class picaFramePipeline;
class picaContext : public pddiBaseContext
{
public:
    picaContext(picaDevice* dev, picaDisplay* disp);
    ~picaContext();

    void BeginFrame();
    void EndFrame();
#ifdef SRR2_OLD_3DS
    void BeginWorld(void);
    void EndWorld(void);
    bool BeginStereoObject(float screenDistance);
    void EndStereoObject(void);
#endif

    void Clear(unsigned bufferMask);

    void SetScissor(pddiRect* rect);

    pddiPrimStream* BeginPrims(pddiShader* material, pddiPrimType primType, unsigned vertexType, int vertexCount, unsigned pass = 0);
    void EndPrims(pddiPrimStream* stream);

    void DrawPrimBuffer(pddiShader* material, pddiPrimBuffer* buffer);
#ifdef SRR2_OLD3DS_WHEEL_LOD
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    bool DrawWheelSkin(pddiShader*,picaPrimBuffer*,const pddiMatrix* wheels);
#endif
    float WheelLodPixelScale() const { return bottomPass ? 0.0f : wheelLodPixelScale; }
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
    bool BindSkinProgram(bool enabled);
    bool skinProgramBound = false;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    enum SurfaceState { SurfaceUnknown,SurfaceOrdinary,SurfaceReflection,SurfaceSkin,SurfaceWorld };
    // The legacy skin flag leaves one padding byte before the eye cache.
    unsigned char reflectionState=SurfaceUnknown;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_CACHE
    picaSkin::EyeCache skinEyeCache;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    bool skinRigidLayout = false;
    C3D_AttrInfo skinRigidAttrInfo;
    void SetSkinLayout(bool rigid);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
    unsigned skinBatchDepth = 0;
    void BeginSkinBatch();
    void EndSkinBatch();
    void SkinBatchBarrier();
#endif
    DVLB_s* skinBinary;
    shaderProgram_s skinProgram;
    C3D_AttrInfo skinAttrInfo;
#endif

    int  GetMaxLights();
    void SetAmbientLight(pddiColour col);

    void EnableCulling(bool enable);
    void SetCullMode(pddiCullMode mode);

    void SetColourWrite(bool red, bool green, bool blue, bool alpha);

    void EnableZBuffer(bool enable);
    void SetZCompare(pddiCompareMode compareMode);
    void SetZWrite(bool);
    void SetZBias(float bias);
    void SetZRange(float n, float f);

    void EnableStencilBuffer(bool enable);
    void SetStencilCompare(pddiCompareMode compare);
    void SetStencilRef(int ref);
    void SetStencilMask(unsigned mask);
    void SetStencilWriteMask(unsigned mask);
    void SetStencilOp(pddiStencilOp failOp, pddiStencilOp zFailOp, pddiStencilOp zPassOp);

    void SetFillMode(pddiFillMode mode);

    void EnableFog(bool enable);
    void SetFog(pddiColour colour, float start, float end);

    int GetMaxTextureDimension(void);

    pddiExtension* GetExtension(unsigned extID);
    bool VerifyExtension(unsigned extID);

    picaDisplay* GetDisplay(void) { return display; }
    bool ExtBGRA(void)            { return false; }

    void SetVertexUniforms(bool lit, pddiColour ambient, pddiColour diffuse, pddiColour emissive, const float (*prepared)[4] = NULL);
    void SetMaterialWriteMask(int mask);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    shaderProgram_s* ReflectionProgram();
    bool BindReflectionProgram(bool enabled, bool world=false);
    void SetWorldLightmapParameters(picaTexture* texture);
#endif

#ifdef SRR2_OLD3DS_SHADOW_MESH
    bool DrawBlobShadow(pddiShader* shader, const pddiVector* vertices,
                        pddiColour inside, pddiColour outside);
#endif
#ifdef SRR2_OLD3DS_CHARACTER_SHADOW
    bool DrawCharacterShadow(pddiShader* shader, const pddiVector* vertices,
                             pddiColour inside, pddiColour outside);
#endif
    unsigned char*  AllocImmVertices(unsigned count);
#ifdef SRR2_OLD3DS_SKIN_RING
    unsigned char* AllocSkinVertices(unsigned count);
#endif
    void FlushStreams();
    unsigned short* AllocImmIndices(unsigned count);
#ifdef SRR2_OLD3DS_QUAD_STREAM
    pddiPrimStream* BeginQuadStream(pddiShader* shader,unsigned count);
    bool EmitQuad(pddiPrimStream* stream,const pddiVector* positions,const pddiVector2* coordinates,
                  pddiColour colour,unsigned topology);
    bool EmitColourQuad(pddiPrimStream* stream,const pddiVector* positions,const pddiVector2* coordinates,
                        const pddiColour* colours,unsigned topology);
    unsigned short* quadIndices=nullptr;
    unsigned quadCapacity=0,quadWritten=0,quadTotal=0,quadFallbacks=0,quadColourTotal=0;
#endif

    void ApplyFogState(void);
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
    void BeginDistanceHaze(unsigned level, unsigned colour);
    void EndDistanceHaze();
#endif
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    void BeginContactShadows();
    void QueueContactShadow(const float*,const float*,const float*,float,float,float);
    void DrawContactShadows();
#endif

    void ApplyViewport(void);

    unsigned contextID;

protected:
    friend class picaPrimBuffer;

    void LoadHardwareMatrix(pddiMatrixType id);
    void SetupHardwareProjection(void);
    void SetupHardwareStencil(void);
    void SetupHardwareLight(int);
    void SetupViewport(void);

    int  wantViewport[4];
    int  hwViewport[4];
    void  BeginTiming(void);
    float EndTiming(void);

    void ApplyProjection(pddiMatrix* m);

#ifdef SRR2_OLD_3DS
    unsigned char* worldQuad;
#endif
    picaDevice*  device;
    picaDisplay* display;

    picaExtGamma* extGamma;

    pddiShader* defaultShader;
    pddiShader* clearShader;
    pddiShader* bottomStaticShader=nullptr;
    bool bottomStaticAttempted=false;

    int         matWriteMask;

    DVLB_s*         shaderBinary;
    shaderProgram_s shaderProgram;
    bool stereoOn;
#ifdef SRR2_3DS_STEREO_GPU_SKIN
    DVLB_s* stereoShaderBinary = nullptr;
    DVLB_s* stereoSkinBinary = nullptr;
    shaderProgram_s stereoShaderProgram, stereoSkinProgram;
#endif
    shaderProgram_s* MainProgram();
    shaderProgram_s* SkinProgram();
#ifdef SRR2_OLD_3DS
    bool stereoWorldPass;
    bool stereoObjectPass;
    float stereoObjectDistance;
#endif

public:
    bool StereoShadersReady() const;
    void PrepareFramePrograms();
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    void DumpUnlitProjectionStats();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    void DumpClipBoundsStats();
    bool CullBounds(const float low[3], const float high[3]);
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
    bool PrepareParticleClip(picaClipBounds::Transform& output) const;
#endif
#endif
    bool TextLineVisible(float left, float bottom, float right, float top) const;
    bool BeginBottomScreen(float u0, float v0, float u1, float v1, bool fitFrame=true);
    void DrawBottomFrame(bool staticPicture=false);
    void SetBottomMapping(const float* source,float l,float t,float r,float b);
    void DrawBottomPanel(int selection,int count);
    void EndBottomScreen(void);

private:
    bool  bottomPass;
#ifdef SRR2_OLD3DS_COARSE_POOLS
    int bottomPoolPrevious=-1;
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    float wheelLodPixelScale = 0.0f;
#endif
    float bottomSrc[4];
    float bottomDst[4];
    void  BottomMapPoint(float u, float v, float* bu, float* bv) const;

    C3D_AttrInfo attrInfo;
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    C3D_AttrInfo noNormalAttrInfo;
    bool noNormalLayout;
public:
    void SetNoNormalLayout(bool enabled);
private:
#endif

    C3D_Mtx hwProjection;
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    bool clipPrepared;
    pddiMatrix clipModelView;
    C3D_Mtx clipProjection;
    picaClipBounds::Transform clipTransform;
#ifdef SRR2_OLD3DS_CLIP_PLANE_PREP
    picaClipBounds::ProjectionPlanes clipProjectionPlanes;
#endif
    unsigned clipTested, clipRejected;
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    bool sparseUnlitProjection, lastFastUnlit;
    unsigned fastUnlitCalls, otherProjectionCalls;
#endif

#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    C3D_FVec directionalPos[PDDI_MAX_LIGHTS];
    bool directionalOnly = false;
    unsigned directionalDraws = 0, generalLightDraws = 0;
#endif
    C3D_FVec lightPos[PDDI_MAX_LIGHTS];
    C3D_FVec lightColour[PDDI_MAX_LIGHTS];
    C3D_FVec lightAtten[PDDI_MAX_LIGHTS];
    int      nActiveLights;
    bool     lightsDirty;

#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
    picaLightCapture::Cache lightCaptureCache[PDDI_MAX_LIGHTS];
    unsigned lightSetupCalls=0,lightDisabledSkips=0,lightCaptureHits=0,lightCaptureComputations=0;
#endif
    C3D_FVec eyeLight[PDDI_MAX_LIGHTS];

    void CaptureEyeLight(int handle);
    void RebuildLightArray(void);

#ifdef SRR2_OLD_3DS
    bool       modelViewUploaded;
    pddiMatrix lastModelView;
#endif
    bool       projUploaded;
    bool       lightsUploaded;
    bool       matUploaded;
    pddiColour lastMatAmbient;
    pddiColour lastMatDiffuse;
    pddiColour lastMatEmissive;
    pddiColour lastSceneAmbient;
    bool       lastLit;
    bool       miscUploaded;

    void InvalidateUniformCache(void);

    C3D_FogLut fogLut;
    float      fogLutStart;
    float      fogLutEnd;
    bool       fogLutValid;
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
    C3D_FogLut hazeLut;
    float hazeNear=0, hazeFar=0;
    unsigned hazeColour=0;
    bool hazeValid=false, hazeActive=false;
#endif

    unsigned char*  immVertices;
    unsigned char*  immVertexRing[PICA_IMM_BUFFERS];
    unsigned short* immIndexRing[PICA_IMM_BUFFERS];
    unsigned        immRing;
    unsigned        immVertexUsed;
#ifdef SRR2_OLD3DS_SKIN_RING
    unsigned skinVertexUsed;
#endif
    unsigned short* immIndices;
    unsigned        immIndexUsed;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    // Keep the complete legacy context prefix at its original offsets. The
    // additional visual resources do not belong between hot renderer fields.
protected:
    DVLB_s* reflectionBinary = nullptr;
    DVLB_s* reflectionStereoBinary = nullptr;
    shaderProgram_s reflectionProgram, reflectionStereoProgram;
    bool reflectionEntriesReady=false;
    DVLB_s* contactBinary=nullptr;
    shaderProgram_s contactProgram;
    picaMat* contactMaterial=nullptr;
    picaContact::Vertex* contactVertices=nullptr;
    unsigned short* contactIndices=nullptr;
    C3D_AttrInfo contactAttributes;
    picaContact::Instance contactInstances[picaContact::Capacity];
    unsigned contactCount=0;
    bool contactActive=false;
#endif
};

class picaPrimBufferStream;

class picaPrimBuffer : public pddiPrimBuffer
{
public:
    picaPrimBuffer(picaContext* context, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
        , unsigned stride = 36
#endif
    );
    ~picaPrimBuffer();

    pddiPrimBufferStream* Lock();
    void Unlock(pddiPrimBufferStream* stream);
#ifdef SRR2_OLD3DS_SKIN_DIRECT
    unsigned char* LockSkin(unsigned count);
#endif

    unsigned char* LockIndexBuffer();
    void UnlockIndexBuffer(int count);

    void SetIndices(unsigned short* indices, int count);

#ifdef SRR2_OLD3DS_LOAD_UPLOAD
    void BeginUpload() { uploadPending = true; }
    void EndUpload();
#endif
    bool CheckMemImageVersion(int version) { return false; }
    void* LockMemImage(unsigned)           { return NULL; }
    void UnlockMemImage()                  { }
    unsigned GetMemImageLength()           { return 0; }
    void SetMemImageParam(unsigned param, unsigned value) {  }

#ifdef SRR2_OLD3DS_WHEEL_LOD
    picaPrimBuffer* BakeWheelCopies(const pddiMatrix* rest,unsigned copies);
    unsigned WheelSourceVersion() const {return wheelSourceVersion;}
    unsigned WheelVertexCount() const {return allocated;}
    void TouchWheelSource(){if(!transient && wheelSourceVersion!=~0u) ++wheelSourceVersion;}
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    bool wheelSkinBuffer=false;
#endif
    unsigned wheelSourceVersion=1,wheelLodBytes=0;
#endif
    unsigned GetVertexFormat() { return vertexType; }
    void Display(void);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    void PrepareVehicleForm();
#endif
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    unsigned DiagnosticVertexCount() const { return indices?indexCount:allocated; }
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    void UpdateClipBounds();
    bool CullNative();
    bool clipBoundsValid;
    float clipLow[3], clipHigh[3];
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    void PrepareGpuSkin(const void* source, unsigned count, unsigned bones, bool selected, bool pedestrian = false, bool named = false,
                        const char* name = nullptr, unsigned group = 0);
    bool TryGpuSkin(pddiShader* material, const pddiMatrix* const* matrices, unsigned count);
#ifdef SRR2_OLD3DS_GPU_SKIN
    bool SubmitGpuSkin(pddiShader*);
#endif
    void MutateGpuSkin();
    picaSkinAllocation* skinAllocation;
    picaSkin::Reason skinReason;
    unsigned skinBoneCount;
    bool skinDrawing;
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    bool skinRigid;
#endif
#endif

#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    void AllowNoNormalPacking() { noNormalAllowed = true; }
    void ExpandNoNormal();
    bool noNormalAllowed, noNormalPacked;
#endif
    void SetFormat(pddiPrimType type, unsigned vertexFormat);

protected:
    bool BuildExpanded(void);
#ifdef SRR2_OLD3DS_DRAW_PREP
    void PrepareDraw();
    bool drawPrepared;
    GPU_Primitive_t nativePrimitive;
    u32 nativeOffset;
    unsigned nativeCount;
#endif

public:

    void SetTransient(unsigned char* verts, unsigned short* idx, unsigned count);
#ifdef SRR2_OLD3DS_QUAD_STREAM
    void AppendQuad(const pddiVector* positions,const pddiVector2* coordinates,pddiColour colour);
    void AppendColourQuad(const pddiVector* positions,const pddiVector2* coordinates,const pddiColour* colours);
    // Only the generated four-vertex topology calls this after range checks.
    void SetQuadIndices(unsigned short* idx,unsigned count)
    {
        indices=idx;indexCount=count;elemState=1;
    }
#endif
#ifdef SRR2_OLD3DS_SHADOW_MESH
    void SetTransientIndices(unsigned short* idx, unsigned count)
    {
        indices = idx;
        indexCount = count;
        elemState = -1;
    }
#endif

protected:
    friend class picaPrimBufferStream;
    picaPrimBufferStream* stream;
    picaContext* context;

    pddiPrimType primType;
    unsigned vertexType;

    unsigned char* buffer;
#ifdef SRR2_OLD3DS_SKIN_RING
    unsigned char* skinVertices;
#endif
    float* coord;
    float* normal;
    float* uv;
    unsigned char* colour;

    unsigned allocated;
    unsigned total;

    unsigned short* indices;
    unsigned indexCount;

    unsigned char* expanded;
    unsigned       expandedBytes;
    bool           expandDirty;

    int            elemState;
    bool           IndicesInRange(void) const;

#ifdef SRR2_OLD3DS_LOAD_UPLOAD
    bool uploadPending;
#endif
    bool defaultsValid;
    bool transient;
    unsigned mem;
};
#endif

#ifndef PDDI_PICA_CONTEXT_HPP
#define PDDI_PICA_CONTEXT_HPP

#include <pddi/pica/pica.hpp>
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
#include <pddi/pica/clipbounds.hpp>
#endif
#include <pddi/pica/skinning.hpp>
#ifdef SRR2_OLD3DS_LIGHT_CAPTURE
#include <pddi/pica/lightcapture.hpp>
#endif
#ifdef SRR2_OLD3DS_SKIN_CLIP
namespace picaSkinClip { struct Bounds; }
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_CACHE
#include <pddi/pica/skineyecache.hpp>
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
#include <pddi/pica/materialqueue.hpp>
#endif

const unsigned PICA_IMM_VERTEX_BYTES = 512 * 1024;
const unsigned PICA_IMM_INDEX_BYTES  = 96 * 1024;
const unsigned PICA_IMM_BUFFERS      = 2;

const int PICA_IMM_DEFAULT_VERTICES = 256;

class picaExtGamma;
class picaPrimBuffer;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
#include <cstdio>
namespace picaDeferredSkin {struct Rest;struct Pool;struct Snapshot;}
#endif

#ifdef SRR2_OLD3DS_STATIC_BATCH
#include <pddi/pica/staticbatch.hpp>
#endif
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
#endif

    void Clear(unsigned bufferMask);
#ifdef SRR2_OLD3DS_OCCLUSION
    bool VisibilityPrepare();
    bool VisibilityQuery(picaPrimBuffer* buffer);
    void VisibilityRaster(picaPrimBuffer* buffer);
    void VisibilityBarrier();
#endif

    void SetScissor(pddiRect* rect);

    pddiPrimStream* BeginPrims(pddiShader* material, pddiPrimType primType, unsigned vertexType, int vertexCount, unsigned pass = 0);
    void EndPrims(pddiPrimStream* stream);

    void DrawPrimBuffer(pddiShader* material, pddiPrimBuffer* buffer);
#ifdef SRR2_OLD3DS_WHEEL_LOD
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    bool DrawWheelPalette(pddiShader*,picaPrimBuffer*,const pddiMatrix* wheels);
    void SetWheelPaletteLayout(bool enabled);
#endif
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    bool DrawWheelSkin(pddiShader*,picaPrimBuffer*,const pddiMatrix* wheels);
#endif
    float WheelLodPixelScale() const { return bottomPass ? 0.0f : wheelLodPixelScale; }
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN
    bool BindSkinProgram(bool enabled);
    bool skinProgramBound = false;
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
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    struct Submission
    {
        pddiShader* material;
        picaPrimBuffer* buffer;
        picaQueue::Rect rect;
    };
    bool CanQueue(pddiShader* material, pddiPrimBuffer* buffer);
    void SubmitMaterialQueue(Submission* entries, unsigned count);
    void SubmitQueuedPrim(pddiShader* material, picaPrimBuffer* buffer);
    void SubmitQueuedOne(pddiShader* material, picaPrimBuffer* buffer, pddiShader*& previous);
    // WorldScene owns this scope across consecutive exact StaticEntityDSGs.
    // Their Display methods do not change transforms/lights or mutate assets.
    void BeginStaticQueue() { staticQueueActive = true; staticQueueCount = 0; staticQueuePrevious = NULL; }
    void EndStaticQueue() { SubmitMaterialQueue(staticQueue, staticQueueCount); staticQueueCount = 0; staticQueuePrevious = NULL; staticQueueActive = false; }
    bool staticQueueActive;
    unsigned staticQueueCount;
    pddiShader* staticQueuePrevious;
    Submission staticQueue[picaQueue::Capacity];
#endif

#ifdef SRR2_OLD3DS_STATIC_BATCH
    bool DrawStaticBatch(pddiShader* material,picaPrimBuffer* const* sources,unsigned count);
    void ClearStaticBatches();
    picaStaticBatch::Entry<picaPrimBuffer> staticBatches[picaStaticBatch::Slots];
    unsigned staticBatchFrame=0,staticBatchBuildFrame=~0u;
    unsigned staticBatchBuilt=0,staticBatchHits=0,staticBatchAvoided=0,staticBatchMisses=0,staticBatchFailures=0;
    unsigned staticBatchBytes=0,staticBatchPeak=0;
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

#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    picaDeferredSkin::Pool* deferredSkinPool=nullptr;
    picaDeferredSkin::Snapshot* deferredSkin=nullptr;
    pddiMatrix* deferredSkinScratch=nullptr;
    bool BeginDeferredSkin(picaDeferredSkin::Rest*,pddiMatrix*,const pddiMatrix* const*,const bool*);
    void EndDeferredSkin();
    void MaterializeDeferredSkin();
    bool QueueDeferredSkinPalette(const pddiMatrix* const*,unsigned);
    void DumpDeferredSkin(FILE*);
#endif
    picaPreparedDrawInputs* preparedDrawCapture=nullptr;
    void ApplyDrawMaterial(pddiBaseShader*);
    picaPreparedInputPool* preparedInputPool=nullptr;
    picaPreparedExecutor* preparedExecutor=nullptr;
#ifdef SRR2_OLD3DS_PREPARED_THREAD
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    picaFramePipeline* preparedFrame=nullptr;
    bool preparedFrameAttempted=false,preparedGpuPending=false;
    void RefreshPreparedOwnership();
    bool PreparedFrameInFlight() const;
    bool SubmitPreparedFrame();
    bool QueuePreparedDriver(void(*)(const void*),const void*,unsigned);
#endif
    picaPreparedWorker* preparedWorker=nullptr;
    bool preparedWorkerAttempted=false;
    void PreparedCommandBuffer(u32**,u32*,u32*);
#endif
    bool preparedBatchFailed=false,preparedBatchReady=false;
    bool PreparedBatching() const;
    void BeginPreparedExecution();
    void FlushPreparedExecution(bool release);
    bool PreparedGeometryRoom();
    void QueuePreparedGeometry(const picaPreparedGeometry&);
    bool QueuePreparedState(const picaPreparedState&);
    bool QueuePreparedPalette(const picaPreparedSkinPalette&,unsigned);
    bool QueuePreparedProgram(shaderProgram_s*,C3D_AttrInfo*,bool,unsigned,float);
    bool QueuePreparedLayout(C3D_AttrInfo*,bool);
    void SubmitPreparedScissor(GPU_SCISSORMODE,u32,u32,u32,u32);
    void ExecutePreparedUniforms(const picaPreparedUniformView&);
    bool CapturePreparedUniformRecord(picaPreparedUniformRecord&,const void*,unsigned,bool,pddiColour,pddiColour,pddiColour,const float (*)[4],const picaPreparedMaterialUniforms* = nullptr);
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    bool CanRetainMaterial() const;
#endif
    void CapturePreparedMaterialRaster(const picaPreparedMaterialPass&,picaPreparedMaterialRaster&);
    bool CapturePreparedUniforms(picaPreparedUniformInput&,bool,pddiColour,pddiColour,pddiColour,const float (*)[4]);
#endif
    void SetVertexUniforms(bool lit, pddiColour ambient, pddiColour diffuse, pddiColour emissive, const float (*prepared)[4] = NULL);
    void SetMaterialWriteMask(int mask);

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

public:
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    void DumpUnlitProjectionStats();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    void DumpClipBoundsStats();
    bool CullBounds(const float low[3], const float high[3]);
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
    bool PrepareParticleClip(picaClipBounds::Transform& output) const;
#endif
#ifdef SRR2_OLD3DS_SKIN_CLIP
    bool CullSkinBounds(const picaSkinClip::Bounds* const* groups, unsigned groupCount,
                        const pddiMatrix* matrices, unsigned count);
#endif
#endif
    bool BeginBottomScreen(float u0, float v0, float u1, float v1);
    void DrawBottomFrame(bool staticPicture=false);
    void EndBottomScreen(void);

private:
    bool  bottomPass;
#ifdef SRR2_OLD3DS_OCCLUSION
    bool visibilityWorld = false;
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    float wheelLodPixelScale = 0.0f;
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    bool wheelPaletteLayout = false;
    C3D_AttrInfo wheelPaletteAttrInfo;
#endif
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
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    unsigned preparedUniformEpoch=0,preparedLightEpoch=0,preparedProjectionEpoch=0;
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
};

class picaPrimBufferStream;

class picaPrimBuffer : public pddiPrimBuffer
{
public:
    picaPrimBuffer(picaContext* context, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex
#if defined(SRR2_OLD3DS_WHEEL_GPU_BATCH) || defined(SRR2_OLD3DS_WHEEL_SKIN_BATCH)
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
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    bool wheelPaletteBuffer=false;
#endif
#endif
#ifdef SRR2_OLD3DS_STATIC_BATCH
    bool StaticBatchSource() const;
    static unsigned StaticBatchSize(picaPrimBuffer* const* sources,unsigned count,unsigned& vertices,unsigned& indexCount);
    static picaPrimBuffer* BakeStaticBatch(picaPrimBuffer* const* sources,unsigned count);
#endif
    unsigned GetVertexFormat() { return vertexType; }
    void Display(void);
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
    bool CapturePreparedGeometry(picaPreparedGeometry&);
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
    void VisibilityBuild();
    void VisibilityInvalidate();
    const picaOcclusion::Geometry* VisibilityGeometry() const;
    unsigned VisibilityVertexCount() const {return indices?indexCount:allocated;}
    picaOcclusion::Geometry* visibilityGeometry=nullptr;
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
    void PrepareGpuSkin(const void* source, unsigned count, unsigned bones, bool selected, bool pedestrian = false, bool named = false);
    bool TryGpuSkin(pddiShader* material, const pddiMatrix* const* matrices, unsigned count);
#ifdef SRR2_OLD3DS_GPU_SKIN
    bool SubmitGpuSkin(pddiShader*);
#endif
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    bool CanDeferGpuSkin(pddiShader*,const pddiMatrix* const*,unsigned,const pddiMatrix*,unsigned) const;
#endif
    void MutateGpuSkin();
    picaSkinAllocation* skinAllocation;
    picaSkin::Reason skinReason;
    unsigned skinBoneCount;
    bool skinDrawing;
#ifdef SRR2_OLD3DS_SKIN_CLIP
    picaSkinClip::Bounds* skinClipBounds;

#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
    bool skinRigid;
#endif
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    bool QueueBounds(float* low, float* high);
#endif
    bool QueuePersistent();
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
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    // Never clear this after the first draw. Old allocations and all later
    // writes retain conservative CPU/GPU fences for the rest of their lifetime.
    bool renderExposed=false;
#endif
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
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    bool PrimQueueSupported() const { return primType == PDDI_PRIM_TRIANGLES || primType == PDDI_PRIM_TRISTRIP; }
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    bool queueBoundsValid;
    float queueLow[3], queueHigh[3];
#endif
#endif
};
#endif

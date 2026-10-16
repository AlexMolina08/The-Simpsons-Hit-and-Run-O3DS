#ifndef PDDI_PICA_CONTEXT_HPP
#define PDDI_PICA_CONTEXT_HPP

#include <pddi/pica/pica.hpp>
#include <pddi/pica/skinning.hpp>
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
#include <pddi/pica/materialqueue.hpp>
#endif

const unsigned PICA_IMM_VERTEX_BYTES = 512 * 1024;
const unsigned PICA_IMM_INDEX_BYTES  = 96 * 1024;
const unsigned PICA_IMM_BUFFERS      = 2;

const int PICA_IMM_DEFAULT_VERTICES = 256;

class picaExtGamma;
class picaPrimBuffer;

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

    void SetScissor(pddiRect* rect);

    pddiPrimStream* BeginPrims(pddiShader* material, pddiPrimType primType, unsigned vertexType, int vertexCount, unsigned pass = 0);
    void EndPrims(pddiPrimStream* stream);

    void DrawPrimBuffer(pddiShader* material, pddiPrimBuffer* buffer);
#ifdef SRR2_OLD3DS_GPU_SKIN
    bool BindSkinProgram(bool enabled);
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

    void SetVertexUniforms(bool lit, pddiColour ambient, pddiColour diffuse, pddiColour emissive);
    void SetMaterialWriteMask(int mask);

#ifdef SRR2_OLD3DS_SHADOW_MESH
    bool DrawBlobShadow(pddiShader* shader, const pddiVector* vertices,
                        pddiColour inside, pddiColour outside);
#endif
    unsigned char*  AllocImmVertices(unsigned count);
    unsigned short* AllocImmIndices(unsigned count);

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

    int         matWriteMask;

    DVLB_s*         shaderBinary;
    shaderProgram_s shaderProgram;
    bool stereoOn;

public:
    bool BeginBottomScreen(float u0, float v0, float u1, float v1);
    void EndBottomScreen(void);

private:
    bool  bottomPass;
    float bottomSrc[4];
    float bottomDst[4];
    void  BottomMapPoint(float u, float v, float* bu, float* bv) const;

    C3D_AttrInfo attrInfo;

    C3D_Mtx hwProjection;

    C3D_FVec lightPos[PDDI_MAX_LIGHTS];
    C3D_FVec lightColour[PDDI_MAX_LIGHTS];
    C3D_FVec lightAtten[PDDI_MAX_LIGHTS];
    int      nActiveLights;
    bool     lightsDirty;

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

    unsigned char*  immVertices;
    unsigned char*  immVertexRing[PICA_IMM_BUFFERS];
    unsigned short* immIndexRing[PICA_IMM_BUFFERS];
    unsigned        immRing;
    unsigned        immVertexUsed;
    unsigned short* immIndices;
    unsigned        immIndexUsed;
};

class picaPrimBufferStream;

class picaPrimBuffer : public pddiPrimBuffer
{
public:
    picaPrimBuffer(picaContext* context, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex);
    ~picaPrimBuffer();

    pddiPrimBufferStream* Lock();
    void Unlock(pddiPrimBufferStream* stream);
#ifdef SRR2_OLD3DS_SKIN_DIRECT
    unsigned char* LockSkin(unsigned count);
#endif

    unsigned char* LockIndexBuffer();
    void UnlockIndexBuffer(int count);

    void SetIndices(unsigned short* indices, int count);

    bool CheckMemImageVersion(int version) { return false; }
    void* LockMemImage(unsigned)           { return NULL; }
    void UnlockMemImage()                  { }
    unsigned GetMemImageLength()           { return 0; }
    void SetMemImageParam(unsigned param, unsigned value) {  }

    unsigned GetVertexFormat() { return vertexType; }
    void Display(void);
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    void PrepareGpuSkin(const void* source, unsigned count, unsigned bones, bool selected);
    bool TryGpuSkin(pddiShader* material, const pddiMatrix* const* matrices, unsigned count);
    void MutateGpuSkin();
    picaSkinAllocation* skinAllocation;
    picaSkin::Reason skinReason;
    unsigned skinBoneCount;
    bool skinDrawing;
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    bool QueueBounds(float* low, float* high);
#endif
    bool QueuePersistent();
#endif

    void SetFormat(pddiPrimType type, unsigned vertexFormat);

protected:
    bool BuildExpanded(void);

public:

    void SetTransient(unsigned char* verts, unsigned short* idx, unsigned count);
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

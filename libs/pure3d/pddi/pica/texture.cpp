#include <pddi/pica/pica.hpp>
#include <main/old3dsexperiment.h>
#include <pddi/pica/texture.hpp>
#include <pddi/pica/visualstyle.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/pica/gpuuploadguard.hpp>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <radloadtiming.hpp>
#endif
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_VRAM_TEXTURES)
#include <main/nativebench.h>
#endif
#include <pddi/pica/swizzle.hpp>
#ifdef SRR2_OLD3DS_TEXTURE_TILES
#include <pddi/pica/texturetile.hpp>
#endif

#include <pddi/base/debug.hpp>
#include <pddi/gl/decompress.h>

#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
#include <pddi/pica/nativetexture.hpp>
#include <sys/stat.h>
extern "C" {
#include <libavutil/sha.h>
#include <libavutil/mem.h>
}
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
#include <pddi/pica/nativebundle.hpp>
#include <bundle_index.hpp>
bool g_picaNativeBundleEnabled=true;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
bool g_picaNativeExactWorldFallback=false;
bool g_picaNativeBundleUnbuffered=false;
static unsigned nativeBundleOpens=0,nativeBundleUnbufferedOpens=0;
static unsigned nativeExactWorldHits=0;
#endif

namespace {
FILE* OpenNativeBundle()
{
    char bundled[128];
    snprintf(bundled,sizeof(bundled),"romfs:/%s",picaNativeBundle::BundlePath);
    FILE* file=fopen(bundled,"rb");
    if(!file)file=fopen(picaNativeBundle::BundlePath,"rb");
    // Configure each owned cursor once, matching the bounded body-read size.
    // Descriptor validation, payload hashing and cooperative yields stay intact.
    if(file){
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        __atomic_fetch_add(&nativeBundleOpens,1u,__ATOMIC_RELAXED);
        if(g_picaNativeBundleUnbuffered){
            (void)setvbuf(file,nullptr,_IONBF,0);
            __atomic_fetch_add(&nativeBundleUnbufferedOpens,1u,__ATOMIC_RELAXED);
        }
        else
#endif
        (void)setvbuf(file,nullptr,_IOFBF,32768);
    }
    return file;
}
struct NativeBundleSlot {void* owner=nullptr;FILE* file=nullptr;bool active=false;};
NativeBundleSlot bundleSlots[2];
struct NativeBundleFile {
    FILE* file=nullptr;NativeBundleSlot* slot=nullptr;
    NativeBundleFile(){
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        radLoadTiming::StageRegion open(radLoadTiming::NativeTextureOpen);
#endif
        void* tls=getThreadLocalStorage();
        for(auto& candidate:bundleSlots){
            void* owner=__atomic_load_n(&candidate.owner,__ATOMIC_ACQUIRE);
            if(!owner){void* empty=nullptr;
                if(__atomic_compare_exchange_n(&candidate.owner,&empty,tls,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))owner=tls;
            }
            if(owner!=tls || candidate.active)continue;
            candidate.active=true;slot=&candidate;
            if(!candidate.file)candidate.file=OpenNativeBundle();
            file=candidate.file;return;
        }
        // Reentrancy or additional resource threads use an independent cursor.
        // Never hold a shared FILE lock across a cooperative loader yield.
        file=OpenNativeBundle();
    }
    ~NativeBundleFile(){if(slot)slot->active=false;else if(file)fclose(file);}
};
}
#endif
bool g_picaNativeTexturesEnabled=true;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
bool g_picaNativeEtcEnabled=true;
#endif
bool g_picaNativeTextureDiagnostics=false;
static unsigned nativeTextureHits=0,nativeTextureMisses=0,nativeTextureInvalid=0,nativeTextureBytes=0;
static unsigned nativeTextureCost[3][5]={};
// Bounded diagnostic evidence: one descriptor per 64 misses, no logging or
// blocking while the loader owns a texture. Atomic ordinals give unique slots.
static unsigned char nativeMissSamples[16][76];
static unsigned nativeMissSampleReady[16]={};
static void NativeTextureMiss(const unsigned char* header)
{
    const unsigned ordinal=__atomic_fetch_add(&nativeTextureMisses,1u,__ATOMIC_RELAXED);
    if(g_picaNativeTextureDiagnostics && !(ordinal%64) && ordinal/64<16){
        const unsigned slot=ordinal/64;
        memcpy(nativeMissSamples[slot],header,76);
        __atomic_store_n(&nativeMissSampleReady[slot],1u,__ATOMIC_RELEASE);
    }
}

struct NativeTextureCost {
    enum Bucket { Other, HashWork, Open, ReadCheck, BucketCount };
    bool enabled;
    unsigned outcome=2,active=Other;
    u64 last=0,ticks[BucketCount]={};
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    radLoadTiming::StageRegion worker{radLoadTiming::NativeTexture};
#endif
    explicit NativeTextureCost(bool selected):enabled(selected){if(enabled)last=svcGetSystemTick();}
    unsigned Select(unsigned next){
        const unsigned previous=active;
        if(enabled){const u64 now=svcGetSystemTick();ticks[active]+=now-last;last=now;active=next;}
        return previous;
    }
    struct Phase {
        NativeTextureCost& cost;unsigned previous;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        radLoadTiming::StageRegion worker;
#endif
        Phase(NativeTextureCost& selected,unsigned next):cost(selected),previous(cost.Select(next))
#ifdef SRR2_OLD3DS_NATIVE_BENCH
            ,worker(radLoadTiming::NativeTexture+next)
#endif
        {}
        ~Phase(){cost.Select(previous);}
    };
    ~NativeTextureCost(){
        if(!enabled)return;
        Select(Other);
        // One active clock bucket per attempt. Payload hashing pauses ReadCheck;
        // total is derived, never an enclosing timer. Wall phases include yields
        // and must not be interpreted as exclusive CPU or summed into frames.
        const u64 total=ticks[Other]+ticks[HashWork]+ticks[Open]+ticks[ReadCheck];
        const u64 values[]={total,ticks[HashWork],ticks[Open],ticks[ReadCheck]};
        __atomic_fetch_add(&nativeTextureCost[outcome][0],1u,__ATOMIC_RELAXED);
        for(unsigned i=0;i<4;++i)__atomic_fetch_add(&nativeTextureCost[outcome][i+1],
            unsigned(values[i]*1000000/SYSCLOCK_ARM11),__ATOMIC_RELAXED);
    }
};
void picaSnapshotNativeTextures(picaNativeTextureStats& stats)
{
    stats.hits=__atomic_load_n(&nativeTextureHits,__ATOMIC_RELAXED);
    stats.misses=__atomic_load_n(&nativeTextureMisses,__ATOMIC_RELAXED);
    stats.invalid=__atomic_load_n(&nativeTextureInvalid,__ATOMIC_RELAXED);
    stats.bytes=__atomic_load_n(&nativeTextureBytes,__ATOMIC_RELAXED);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<5;++j)
        stats.cost[i][j]=__atomic_load_n(&nativeTextureCost[i][j],__ATOMIC_RELAXED);
}
extern "C" void picaDumpNativeTextures(FILE* f)
{
    unsigned preparedVersion=picaNativeTexture::Version;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    if(g_picaNativeEtcEnabled)preparedVersion=2;
    fprintf(f,"NATIVE_ETC1 enabled=%u source_scope=opaque_world_png version=2 original_allocation_budgets=1 mip_policy_unchanged=1\n",g_picaNativeEtcEnabled);
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
    fprintf(f,"NATIVE_TEXTURE_BUNDLE enabled=%u entries=%u index_bytes=%u bundle_bytes=%u sha256=%s source_bound_payload_sha=1 whole_bundle_hash_at_runtime=0\n",g_picaNativeBundleEnabled,picaNativeBundle::EntryCount,unsigned(sizeof(picaNativeBundle::Entries)),picaNativeBundle::BundleBytes,picaNativeBundle::BundleSHA256);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    fprintf(f,"NATIVE_TEXTURE_EXACT_FALLBACK enabled=%u hits=%u accepted_etc_hits_preferred=1 original_format_and_mips=1 scope=whole_session\n",
        g_picaNativeExactWorldFallback,__atomic_load_n(&nativeExactWorldHits,__ATOMIC_RELAXED));
    fprintf(f,"NATIVE_TEXTURE_IO unbuffered=%u opens=%u unbuffered_opens=%u\n",
        g_picaNativeBundleUnbuffered,
        __atomic_load_n(&nativeBundleOpens,__ATOMIC_RELAXED),__atomic_load_n(&nativeBundleUnbufferedOpens,__ATOMIC_RELAXED));
#endif

#endif
    fprintf(f,"NATIVE_TEXTURES enabled=%u hits=%u misses=%u invalid=%u bytes=%u version=%u\n",
        g_picaNativeTexturesEnabled,__atomic_load_n(&nativeTextureHits,__ATOMIC_RELAXED),__atomic_load_n(&nativeTextureMisses,__ATOMIC_RELAXED),__atomic_load_n(&nativeTextureInvalid,__ATOMIC_RELAXED),__atomic_load_n(&nativeTextureBytes,__ATOMIC_RELAXED),preparedVersion);
    if(g_picaNativeTextureDiagnostics)for(unsigned i=0;i<16;++i){
        if(!__atomic_load_n(&nativeMissSampleReady[i],__ATOMIC_ACQUIRE))continue;
        fprintf(f,"NATIVE_TEXTURE_MISS_SAMPLE ordinal=%u descriptor_hex=",i*64);
        for(unsigned j=0;j<76;++j)fprintf(f,"%02x",nativeMissSamples[i][j]);
        fprintf(f,"\n");
    }
    for(unsigned i=0;i<3;++i)fprintf(f,"NATIVE_TEXTURE_COST enabled=%u outcome=%s calls=%u total_us=%u hash_us=%u open_us=%u read_verify_us=%u overlapping_fields=0 total_is_derived_sum=1 phase_fields_disjoint_within_attempt=1 includes_yields=1 cpu_time=0\n",
        g_picaNativeTextureDiagnostics,i==0?"hit":i==1?"miss":"invalid",
        __atomic_load_n(&nativeTextureCost[i][0],__ATOMIC_RELAXED),
        __atomic_load_n(&nativeTextureCost[i][1],__ATOMIC_RELAXED),
        __atomic_load_n(&nativeTextureCost[i][2],__ATOMIC_RELAXED),
        __atomic_load_n(&nativeTextureCost[i][3],__ATOMIC_RELAXED),
        __atomic_load_n(&nativeTextureCost[i][4],__ATOMIC_RELAXED));
}
#endif
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef SRR2_OLD3DS_DRAW_PREP
// Reserve the ownership node before creating GPU storage. Destruction can then
// defer the SDK free without allocating or changing any queued texture bytes.
struct picaLinearTextureRetirement {
    picaLinearTextureRetirement* next;
    C3D_Tex texture;
};
static picaLinearTextureRetirement* retiredLinearTextures;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned linearTextureRetired,linearTextureReclaimed,linearTexturePending,linearTexturePeak;
#endif
static picaLinearTextureRetirement* PrepareLinearTextureRetirement()
{
    return static_cast<picaLinearTextureRetirement*>(malloc(sizeof(picaLinearTextureRetirement)));
}
static void RetireLinearTexture(picaLinearTextureRetirement* node,const C3D_Tex& texture)
{
    node->texture=texture;
    node->next=retiredLinearTextures;
    retiredLinearTextures=node;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    ++linearTextureRetired;
    if(++linearTexturePending>linearTexturePeak)linearTexturePeak=linearTexturePending;
#endif
}
extern "C" void picaReclaimLinearTextures()
{
    // The caller has completed the same GPU fence used for retired geometry.
    while(retiredLinearTextures) {
        picaLinearTextureRetirement* node=retiredLinearTextures;
        retiredLinearTextures=node->next;
        C3D_TexDelete(&node->texture);
        free(node);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        ++linearTextureReclaimed;--linearTexturePending;
#endif
    }
}
extern "C" void C3Di_RenderQueueWaitDone(void);
#endif

#ifdef SRR2_OLD3DS_VRAM_TEXTURES
// Ownership nodes are allocated before GPU submission. Retirement never allocates
// or overwrites GPU-visible bytes; context's completed-frame fence reclaims them.
struct picaVramAllocation { picaVramAllocation* next; void* data; unsigned bytes; };
static picaVramAllocation* retiredVram;
static unsigned vramTextureBytes, vramTexturePeak, vramTextureCopies;
static unsigned vramTextureEligible, vramTextureInFrame, vramTextureFailures;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
static unsigned vramTextureHandoffSkipped;
bool g_picaVramCapturePromote=true,g_picaVramCopyDiagnostics=false;
bool g_picaVramCompact=false;
static unsigned vramCompactCopies=0,vramCompactSaved=0;
static unsigned vramCaptureSkipped,vramCaptureCopies,vramCaptureAllocUs,vramCaptureCopyUs,vramCaptureReleaseUs,vramCaptureMaxUs,vramCaptureMaxCopyUs;
#endif
static void RetireVram(picaVramAllocation* allocation)
{
    allocation->next=retiredVram;retiredVram=allocation;
}
extern "C" void picaReclaimVramTextures()
{
    while(retiredVram)
    {
        picaVramAllocation* node=retiredVram;retiredVram=node->next;
        vramTextureBytes-=node->bytes;vramFree(node->data);free(node);
    }
}
extern "C" void picaDumpVramTextures()
{
#if defined(SRR2_OLD3DS_DRAW_PREP) && defined(SRR2_OLD3DS_NATIVE_BENCH)
    printf("OLD3DS_LINEAR_TEXTURE_RETIRE retired=%u reclaimed=%u pending=%u peak_pending=%u fenced=1\n",
        linearTextureRetired,linearTextureReclaimed,linearTexturePending,linearTexturePeak);
#endif
    printf("OLD3DS_VRAM_TEXTURES used=%u peak=%u copies=%u eligible=%u in_frame=%u failures=%u free=%lu\n",
        vramTextureBytes,vramTexturePeak,vramTextureCopies,vramTextureEligible,
        vramTextureInFrame,vramTextureFailures,(unsigned long)vramSpaceFree());
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    printf("OLD3DS_VRAM_HANDOFF upload_guard=1 skipped=%u scope=whole_session\n",vramTextureHandoffSkipped);
    printf("OLD3DS_VRAM_CAPTURE promote=%u diagnostics=%u skipped=%u copies=%u alloc_us=%u copy_us=%u release_us=%u maximum_us=%u maximum_copy_us=%u measured_route_only=1 wall_not_cpu=1\n",g_picaVramCapturePromote,g_picaVramCopyDiagnostics,vramCaptureSkipped,vramCaptureCopies,vramCaptureAllocUs,vramCaptureCopyUs,vramCaptureReleaseUs,vramCaptureMaxUs,vramCaptureMaxCopyUs);
    printf("OLD3DS_VRAM_COMPACT enabled=%u copies=%u cumulative_saved_bytes=%u live_accounting_uses_actual_bytes=1 formats_and_mips_unchanged=1\n",g_picaVramCompact,vramCompactCopies,vramCompactSaved);
#endif
}
class picaVramUpload
{
    C3D_Tex& tex;picaVramAllocation*& owner;void*& pending;bool& valid;
    void* previous;bool previousValid,complete;
public:
    picaVramUpload(C3D_Tex& t,picaVramAllocation*& o,void*& p,bool& v):
        tex(t),owner(o),pending(p),valid(v),previous(t.data),previousValid(v),complete(false)
        {if(pending)tex.data=pending;}
    void Complete(){complete=true;}
    ~picaVramUpload()
    {
        if(!pending)return;
        if(complete){RetireVram(owner);owner=NULL;}
        else {linearFree(pending);tex.data=previous;valid=previousValid;}
        pending=NULL;
    }
};
void picaTexture::TryPromoteVram()
{
    if(vramUploaded)return;
    vramUploaded=true;
    if(!vramEligible)return;
    ++vramTextureEligible;
    // This API waits for copy completion outside a Citro3D frame. In-frame
    // copies are asynchronous, so retain the original allocation in that case.
    // The renderer also blocks uploads while lending the loader mutex across
    // FrameBegin: Citro3D can be in-frame before g_picaInFrame becomes true.
    if(g_picaInFrame){++vramTextureInFrame;return;}
    if(picaUpload::Blocked()){
        ++vramTextureInFrame;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        ++vramTextureHandoffSkipped;
#endif
        return;
    }
    const unsigned copyBytes=C3D_TexCalcTotalSize(tex.size,tex.maxLevel);
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    const unsigned eligibleBytes=nativeAllocationBytes?nativeAllocationBytes:copyBytes;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    const unsigned bytes=g_picaVramCompact && hwFormat==GPU_ETC1?copyBytes:eligibleBytes;
#else
    const unsigned bytes=eligibleBytes;
#endif
#else
    const unsigned bytes=copyBytes;
    const unsigned eligibleBytes=bytes;
#endif
    const unsigned budget=2*1024*1024,reserve=2*1024*1024;
    const unsigned maximum=131072;
    if(eligibleBytes<8192 || eligibleBytes>maximum || bytes>budget-vramTextureBytes ||
       vramSpaceFree()<bytes+reserve)return;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    const bool capture=NativeBench::collecting;
    if(capture && !g_picaVramCapturePromote){++vramCaptureSkipped;return;}
    const bool timed=capture && g_picaVramCopyDiagnostics;
    const u64 allocationStart=timed?svcGetSystemTick():0;
#endif
    picaVramAllocation* node=(picaVramAllocation*)malloc(sizeof(picaVramAllocation));
    if(!node){++vramTextureFailures;return;}
    node->data=vramAlloc(bytes);
    if(!node->data){free(node);++vramTextureFailures;return;}
    node->bytes=bytes;node->next=NULL;
    // UploadLevel already flushed every mip. This is a raw byte copy, with no
    // display-transfer format conversion, resampling or color quantization.
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    const u64 copyStart=timed?svcGetSystemTick():0;
#endif
    C3D_SyncTextureCopy((u32*)tex.data,0,(u32*)node->data,0,copyBytes,8);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    const u64 releaseStart=timed?svcGetSystemTick():0;
#endif
    linearFree(tex.data);tex.data=node->data;vramAllocation=node;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    if(timed){
        const u64 end=svcGetSystemTick();
        const unsigned allocUs=unsigned((copyStart-allocationStart)*1000000/SYSCLOCK_ARM11);
        const unsigned copyUs=unsigned((releaseStart-copyStart)*1000000/SYSCLOCK_ARM11);
        const unsigned releaseUs=unsigned((end-releaseStart)*1000000/SYSCLOCK_ARM11);
        const unsigned totalUs=allocUs+copyUs+releaseUs;
        ++vramCaptureCopies;vramCaptureAllocUs+=allocUs;vramCaptureCopyUs+=copyUs;vramCaptureReleaseUs+=releaseUs;
        if(totalUs>vramCaptureMaxUs)vramCaptureMaxUs=totalUs;
        if(copyUs>vramCaptureMaxCopyUs)vramCaptureMaxCopyUs=copyUs;
    }
#endif
    vramTextureBytes+=bytes;++vramTextureCopies;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    if(bytes<eligibleBytes){++vramCompactCopies;vramCompactSaved+=eligibleBytes-bytes;}
#endif
    if(vramTextureBytes>vramTexturePeak)vramTexturePeak=vramTextureBytes;
}
#endif

static const int PICA_STAGING_BPP = 4;

static inline int NextPowerOfTwo(int v)
{
    int p = 1;
    while(p < v)
        p <<= 1;
    return p;
}

static inline int FastLog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp >>= 1;
        if((tmp << r) != x)
            return -1;
    }
    return r;
}

static inline pddiPixelFormat PickPixelFormat(pddiTextureType type, int bitDepth, int alphaDepth)
{
    switch(type)
    {
    case PDDI_TEXTYPE_RGB:
        switch(alphaDepth)
        {
        case 0:  return (bitDepth <= 16) ? PDDI_PIXEL_RGB565   : PDDI_PIXEL_RGB888;
        case 1:  return (bitDepth <= 16) ? PDDI_PIXEL_ARGB1555 : PDDI_PIXEL_ARGB8888;
        default: return (bitDepth <= 16) ? PDDI_PIXEL_ARGB4444 : PDDI_PIXEL_ARGB8888;
        }

    case PDDI_TEXTYPE_PALETTIZED: return PDDI_PIXEL_PAL8;
    case PDDI_TEXTYPE_LUMINANCE:  return PDDI_PIXEL_LUM8;
    case PDDI_TEXTYPE_BUMPMAP:    return PDDI_PIXEL_DUDV88;
    case PDDI_TEXTYPE_DXT1:       return PDDI_PIXEL_DXT1;
    case PDDI_TEXTYPE_DXT2:       return PDDI_PIXEL_DXT2;
    case PDDI_TEXTYPE_DXT3:       return PDDI_PIXEL_DXT3;
    case PDDI_TEXTYPE_DXT4:       return PDDI_PIXEL_DXT4;
    case PDDI_TEXTYPE_DXT5:       return PDDI_PIXEL_DXT5;
    case PDDI_TEXTYPE_YUV:        return PDDI_PIXEL_YUV;
    }

    return PDDI_PIXEL_UNKNOWN;
}

static inline bool IsCompressed(pddiTextureType type)
{
    return (type == PDDI_TEXTYPE_DXT1) ||
           (type == PDDI_TEXTYPE_DXT2) ||
           (type == PDDI_TEXTYPE_DXT3) ||
           (type == PDDI_TEXTYPE_DXT4) ||
           (type == PDDI_TEXTYPE_DXT5);
}

static inline GPU_TEXCOLOR PickHardwareFormat(int alphaDepth, int* bytesPerPixel)
{
    *bytesPerPixel = 2;

    if(alphaDepth <= 0)
        return GPU_RGB565;
    if(alphaDepth == 1)
        return GPU_RGBA5551;

    return GPU_RGBA4;
}

picaTexture::picaTexture(picaContext* c)
{
    context      = c;
    xSize        = 0;
    ySize        = 0;
    log2X        = 0;
    log2Y        = 0;
    type         = PDDI_TEXTYPE_RGB;
    nMipMap      = 0;
    alphaDepth   = 0;
    priority     = 15;
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    vramAllocation=NULL;reuploadLinear=NULL;vramEligible=vramUploaded=false;
#endif
    created      = false;
#ifdef SRR2_OLD3DS_DRAW_PREP
    linearRetirement = NULL;
#endif
    valid        = false;
    hwFormat     = GPU_RGBA8;
    staging      = NULL;
    stagingBytes = 0;
    stagingLevel = -1;
    memset(&tex, 0, sizeof(tex));
}

picaTexture::~picaTexture()
{
    FreeStaging();
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    free(worldLightmapParameters);
#endif

    if(created)
    {
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
        if(vramAllocation){RetireVram(vramAllocation);vramAllocation=NULL;}
        else
#endif
#ifdef SRR2_OLD3DS_DRAW_PREP
        {
            RetireLinearTexture(linearRetirement,tex);
            linearRetirement=NULL;
        }
#else
        C3D_TexDelete(&tex);
#endif
        created = false;

        context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
    }
#ifdef SRR2_OLD3DS_DRAW_PREP
    free(linearRetirement);
#endif
}

#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
void picaTexture::SetWorldLightmapParameters(const float* parameters)
{
    if(!created || hwFormat!=GPU_L8) return;
    for(unsigned i=0;i<4;++i) if(!std::isfinite(parameters[i])) return;
    if(!worldLightmapParameters) worldLightmapParameters=static_cast<float*>(malloc(4*sizeof(float)));
    if(worldLightmapParameters) memcpy(worldLightmapParameters,parameters,4*sizeof(float));
}
#endif

void picaTexture::FreeStaging(void)
{
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    if(reuploadLinear){linearFree(reuploadLinear);reuploadLinear=NULL;}
#endif
    if(staging)
    {
        free(staging);
        staging = NULL;
    }
    stagingBytes = 0;
    stagingLevel = -1;
}

bool picaTexture::Create(int x, int y, int bpp, int alpha, int nMip,
                         pddiTextureType textureType, pddiTextureUsageHint usageHint)
{
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    const bool worldLightmap=usageHint==PDDI_USAGE_WORLD_LIGHTMAP;
#else
    const bool worldLightmap=false;
#endif
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    // Movie construction explicitly sets KeepFullSize, and font pages set
    // FontTexture. Keep both CPU-writable; later ordinary reuploads can demote.
    vramEligible=usageHint==PDDI_USAGE_STATIC && !g_picaKeepFullSize &&
                 !g_picaFontTexture && textureType!=PDDI_TEXTYPE_YUV;
    vramUploaded=false;
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
    nativeEligible=usageHint==PDDI_USAGE_STATIC && !g_picaKeepFullSize &&
        !g_picaFontTexture && (IsCompressed(textureType) || textureType==PDDI_TEXTYPE_RGB || textureType==PDDI_TEXTYPE_PALETTIZED);
#endif
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(worldLightmap && (textureType!=PDDI_TEXTYPE_RGB || bpp!=32 || alpha!=0 || nMip!=0))
    { lastError=PDDI_TEX_BADFORMAT;return false; }
#endif
    xSize      = x;
    ySize      = y;
    nMipMap    = nMip;
    type       = textureType;
    alphaDepth = alpha;

    log2X = FastLog2(xSize);
    log2Y = FastLog2(ySize);

    if((xSize > PICA_MAX_TEXTURE_DIMENSION) || (ySize > PICA_MAX_TEXTURE_DIMENSION))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    if(textureType == PDDI_TEXTYPE_YUV)
    {
        lastError = PDDI_TEX_BADFORMAT;
        return false;
    }

    if(textureType == PDDI_TEXTYPE_PALETTIZED)
    {
        textureType = PDDI_TEXTYPE_RGB;
        type        = PDDI_TEXTYPE_RGB;
        bpp         = 32;
    }

    if(IsCompressed(textureType))
    {
        alpha = (textureType == PDDI_TEXTYPE_DXT1) ? 1 : 8;
    }

    int texW = NextPowerOfTwo(xSize);
    int texH = NextPowerOfTwo(ySize);
    if(texW < PICA_MIN_TEXTURE_DIMENSION) texW = PICA_MIN_TEXTURE_DIMENSION;
    if(texH < PICA_MIN_TEXTURE_DIMENSION) texH = PICA_MIN_TEXTURE_DIMENSION;

    if(!g_picaMipmapsAllowed && !g_picaKeepFullSize && !worldLightmap)
    {
        while((texW > 256 || texH > 256) &&
              texW > PICA_MIN_TEXTURE_DIMENSION && texH > PICA_MIN_TEXTURE_DIMENSION)
        {
            texW >>= 1;
            texH >>= 1;
        }
    }

    if((texW > PICA_MAX_TEXTURE_DIMENSION) || (texH > PICA_MAX_TEXTURE_DIMENSION))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    int bytesPerPixel = 4;
    // Font pages use grayscale coverage. LA8 retains all eight alpha bits
    // without increasing the two-byte texel budget used by RGBA4.
    hwFormat = g_picaFontTexture ? GPU_LA8 : PickHardwareFormat(alpha, &bytesPerPixel);
    if(g_picaFontTexture) bytesPerPixel = 2;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(worldLightmap) { hwFormat=GPU_L8;bytesPerPixel=1; }
#endif

    const unsigned mipExtra = (unsigned)(texW * texH * bytesPerPixel) / 3;
    const bool roomForMips = linearSpaceFree() > (PICA_MIPMAP_LINEAR_RESERVE + mipExtra);
    const bool wantMips = !worldLightmap && (g_picaMipmapsAllowed || g_picaFontTexture) && roomForMips &&
                          (texW >= 2 * PICA_MIN_TEXTURE_DIMENSION) &&
                          (texH >= 2 * PICA_MIN_TEXTURE_DIMENSION);

    const C3D_TexInitParams fontParams={(u16)texW,(u16)texH,1,hwFormat,GPU_TEX_2D,false};
#ifdef SRR2_OLD3DS_DRAW_PREP
    linearRetirement=PrepareLinearTextureRetirement();
    if(!linearRetirement) {lastError=PDDI_ERR;return false;}
#endif
    const auto initialize=[&]() {
        return g_picaFontTexture && wantMips ? C3D_TexInitWithParams(&tex,NULL,fontParams) :
            wantMips ? C3D_TexInitMipmap(&tex,(u16)texW,(u16)texH,hwFormat) :
            C3D_TexInit(&tex,(u16)texW,(u16)texH,hwFormat);
    };
    bool initialized=initialize();
#ifdef SRR2_OLD3DS_DRAW_PREP
    if(!initialized && retiredLinearTextures && !g_picaInFrame && !picaUpload::Blocked()) {
        // Reclaim completed storage under pressure without ending an active frame.
        C3Di_RenderQueueWaitDone();
        picaReclaimLinearTextures();
        initialized=initialize();
    }
#endif
    if(!initialized)
    {
#ifdef SRR2_OLD3DS_DRAW_PREP
        free(linearRetirement);linearRetirement=NULL;
#endif
        lastError = PDDI_ERR;
        return false;
    }
    created = true;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    nativeAllocationBytes=C3D_TexCalcTotalSize(tex.size,tex.maxLevel);
#endif

    C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&tex, worldLightmap?GPU_CLAMP_TO_EDGE:GPU_REPEAT, worldLightmap?GPU_CLAMP_TO_EDGE:GPU_REPEAT);
    C3D_TexSetFilterMipmap(&tex, GPU_NEAREST);

    lock.depth  = bpp;
    lock.format = PickPixelFormat(textureType, bpp, alphaDepth);

    lock.native = false;
    lock.rgbaRShift[0] = 16;
    lock.rgbaLShift[2] = 16;
    lock.rgbaLShift[0] =
    lock.rgbaLShift[1] = lock.rgbaRShift[1] =
    lock.rgbaRShift[2] =
    lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;
    lock.rgbaMask[0] = 0x000000ff;
    lock.rgbaMask[1] = 0x0000ff00;
    lock.rgbaMask[2] = 0x00ff0000;
    lock.rgbaMask[3] = 0xff000000;

    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);
    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT,
                      (float)((texW * texH * bytesPerPixel * 8) / 8192));

    return true;
}

pddiPixelFormat picaTexture::GetPixelFormat() { return lock.format; }
int picaTexture::GetWidth()                   { return xSize; }
int picaTexture::GetHeight()                  { return ySize; }
int picaTexture::GetDepth()                   { return 32; }
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
#include <p3d/spriteatlas.hpp>
bool picaTexture::PrepareAtlas(picaTexture* atlas,picaTexture* const* originals,unsigned count)
{
    if(!atlas || !count || !originals[0])return false;
    const unsigned level=originals[0]->tex.maxLevel;
    if(level && !p3dSpriteAtlas::allowMipmaps)return false;
    if(!atlas->created || !atlas->valid || atlas->tex.maxLevel<level)return false;
    for(unsigned i=0;i<count;++i){const picaTexture* original=originals[i];
        if(!original || !original->created || !original->valid || original->tex.maxLevel!=level ||
           original->hwFormat!=atlas->hwFormat)return false;
    }
    // A private mipmapped experiment clamps to the original tiles' common LOD.
    // Atlas mip border filtering still requires physical pixel comparison.
    atlas->tex.maxLevel=level;return true;
}
extern "C" bool picaSpriteAtlasPrepare(pddiTexture* atlas,pddiTexture* const* originals,unsigned count)
{
    if(!count || count>p3dSpriteAtlas::MaxTiles)return false;
    picaTexture* native[p3dSpriteAtlas::MaxTiles];
    for(unsigned i=0;i<count;++i)native[i]=dynamic_cast<picaTexture*>(originals[i]);
    return picaTexture::PrepareAtlas(dynamic_cast<picaTexture*>(atlas),native,count);
}
#endif
int picaTexture::GetNumMipMaps()              { return nMipMap; }
int picaTexture::GetAlphaDepth()              { return alphaDepth; }

pddiLockInfo* picaTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    if(!created)
        return NULL;

    const int w = (xSize >> mipMap) > 0 ? (xSize >> mipMap) : 1;
    const int h = (ySize >> mipMap) > 0 ? (ySize >> mipMap) : 1;

    unsigned needed;
    if(IsCompressed(type))
    {
        const unsigned blockSize = (type == PDDI_TEXTYPE_DXT1) ? 8u : 16u;
        const unsigned blocksW = (unsigned)((w + 3) / 4);
        const unsigned blocksH = (unsigned)((h + 3) / 4);
        lock.pitch = (int)(blocksW * blockSize);
        needed = blocksW * blocksH * blockSize;
    }
    else
    {
        lock.pitch = w * PICA_STAGING_BPP;
        needed = (unsigned)(w * h * PICA_STAGING_BPP);
    }

    if(staging && (stagingBytes < needed))
        FreeStaging();

    if(staging == NULL)
    {
        staging = (unsigned char*)calloc(1, needed);
        if(staging == NULL)
            return NULL;
        stagingBytes = needed;
    }

#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    // A later full upload demotes to CPU-writable linear storage. Allocate before
    // granting the lock; failure leaves the previous texture and ownership intact.
    if(mipMap==0 && vramAllocation && !reuploadLinear)
    {
        unsigned bytes=C3D_TexCalcTotalSize(tex.size,tex.maxLevel);
#ifdef SRR2_OLD3DS_NATIVE_ETC1
        // A later CPU upload restores the original uncompressed layout.
        if(hwFormat==GPU_ETC1)bytes=nativeAllocationBytes;
#endif
        reuploadLinear=linearAlloc(bytes);
        if(!reuploadLinear)return NULL;
    }
#endif
    stagingLevel = mipMap;

    lock.width  = w;
    lock.height = h;
    lock.bits   = staging;

    return &lock;
}

void picaTexture::Unlock(int mipLevel)
{
    if((staging == NULL) || !created)
        return;

    if(mipLevel == 0)
        UploadLevel(0);

    FreeStaging();
}

extern "C" int g_picaMipmapsAllowed = 0;
extern "C" int g_picaKeepFullSize = 0;
extern "C" int g_picaFontTexture = 0;

void picaTexture::UploadLevel(int mipLevel)
{
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_TEXTURE);
#endif
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    picaVramUpload vramUpload(tex,vramAllocation,reuploadLinear,valid);
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
    if(mipLevel==0 && !valid && nativeEligible && UploadPrepared())
    {
        C3D_TexFlush(&tex);
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
        vramUpload.Complete();TryPromoteVram();
#endif
        valid=true;return;
    }
#endif
    const int srcW = (xSize >> mipLevel) > 0 ? (xSize >> mipLevel) : 1;
    const int srcH = (ySize >> mipLevel) > 0 ? (ySize >> mipLevel) : 1;

    const int dstW = tex.width;
    const int dstH = tex.height;

#ifdef SRR2_OLD3DS_TEXTURE_TILES
    if(IsCompressed(type) && tex.maxLevel == 0 && srcW == dstW && srcH == dstH &&
       !(srcW & 7) && !(srcH & 7))
    {
        // Common streamed-world path: no full RGBA image, allocation or row flip.
        // Scaled/mipmapped textures retain the exact original filtering path.
        const unsigned kind = type == PDDI_TEXTYPE_DXT1 ? 1 :
            (type == PDDI_TEXTYPE_DXT2 || type == PDDI_TEXTYPE_DXT3) ? 2 : 3;
        const unsigned blockBytes = kind == 1 ? 8 : 16;
        const unsigned char* block = staging;
        unsigned int rgba[16];
        for(int y = 0; y < srcH; y += 4)
        {
            if(g_picaYieldHook) g_picaYieldHook();
            for(int x = 0; x < srcW; x += 4, block += blockBytes)
            {
                BlockDecompressBC(kind, block, rgba);
                const unsigned dy = srcH - 4 - y;
                switch(hwFormat)
                {
                case GPU_RGBA8: picaTile::Block<picaTile::RGBA8>(rgba,x,dy,dstW,tex.data); break;
                case GPU_RGBA5551: picaTile::Block<picaTile::RGBA5551>(rgba,x,dy,dstW,tex.data); break;
                case GPU_LA8: picaTile::Block<picaTile::LA8>(rgba,x,dy,dstW,tex.data); break;
                case GPU_RGBA4: picaTile::Block<picaTile::RGBA4>(rgba,x,dy,dstW,tex.data); break;
                default: picaTile::Block<picaTile::RGB565>(rgba,x,dy,dstW,tex.data); break;
                }
            }
        }
        C3D_TexFlush(&tex);
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
        vramUpload.Complete();TryPromoteVram();
#endif
        valid = true;
        return;
    }
#endif
    unsigned char* decompressed = NULL;
    const unsigned char* src = staging;

    if(IsCompressed(type))
    {
        decompressed = (unsigned char*)malloc((size_t)srcW * srcH * 4);
        if(decompressed == NULL)
            return;

        switch(type)
        {
            case PDDI_TEXTYPE_DXT1:
                BlockDecompressImageBC1(srcW, srcH, staging, decompressed);
                break;
            case PDDI_TEXTYPE_DXT2:
            case PDDI_TEXTYPE_DXT3:
                BlockDecompressImageBC2(srcW, srcH, staging, decompressed);
                break;
            default:
                BlockDecompressImageBC3(srcW, srcH, staging, decompressed);
                break;
        }
        src = decompressed;
    }

    unsigned int* level = NULL;
    bool levelOwned = false;
    if(decompressed != NULL && dstW == srcW && dstH == srcH)
    {
        level = (unsigned int*)decompressed;
        for(int y = 0; y < srcH / 2; y++)
        {
            unsigned int* a = level + (size_t)y * srcW;
            unsigned int* b = level + (size_t)(srcH - 1 - y) * srcW;
            for(int x = 0; x < srcW; x++)
            {
                const unsigned int t = a[x]; a[x] = b[x]; b[x] = t;
            }
        }
    }
    else if(decompressed == NULL && dstW == srcW && dstH == srcH)
    {
        level = (unsigned int*)staging;
    }
    else
    {
        level = (unsigned int*)malloc((size_t)dstW * dstH * 4);
        if(level == NULL)
        {
            if(decompressed)
                free(decompressed);
            return;
        }
        levelOwned = true;

        const bool boxShrink = (srcW >= dstW) && (srcH >= dstH) &&
                               (srcW > dstW || srcH > dstH) &&
                               (srcW % dstW == 0) && (srcH % dstH == 0);
        if(boxShrink)
        {
            const int rx = srcW / dstW;
            const int ry = srcH / dstH;
            const unsigned n = (unsigned)(rx * ry);
            for(int y = 0; y < dstH; y++)
            {
                unsigned int* row = level + (size_t)y * dstW;
                for(int x = 0; x < dstW; x++)
                {
                    unsigned r = 0, g = 0, b = 0, a = 0;
                    for(int j = 0; j < ry; j++)
                    {
                        const int sy0 = y * ry + j;
                        const int sy  = IsCompressed(type) ? (srcH - 1 - sy0) : sy0;
                        const unsigned int* srcRow =
                            (const unsigned int*)(src + ((size_t)sy * srcW * 4)) + x * rx;
                        for(int i = 0; i < rx; i++)
                        {
                            const unsigned p = srcRow[i];
                            r += p & 0xff;
                            g += (p >> 8) & 0xff;
                            b += (p >> 16) & 0xff;
                            a += p >> 24;
                        }
                    }
                    row[x] = (r / n) | ((g / n) << 8) | ((b / n) << 16) | ((a / n) << 24);
                }
            }
        }
        else
        for(int y = 0; y < dstH; y++)
        {
            const int sy0 = (dstH == srcH) ? y : ((y * srcH) / dstH);
            const int sy  = IsCompressed(type) ? (srcH - 1 - sy0) : sy0;
            const unsigned int* srcRow = (const unsigned int*)(src + ((size_t)sy * srcW * 4));
            unsigned int* row = level + (size_t)y * dstW;
            for(int x = 0; x < dstW; x++)
                row[x] = srcRow[(dstW == srcW) ? x : ((x * srcW) / dstW)];
        }
    }

#ifdef SRR2_OLD3DS_NATIVE_ETC1
    if(hwFormat==GPU_ETC1)
    {
        // All fallible preparation is complete. Preserve the ETC1 descriptor
        // until this point so allocation failure keeps the previous image.
        int bytesPerPixel;
        hwFormat=PickHardwareFormat(alphaDepth,&bytesPerPixel);
        tex.fmt=hwFormat;tex.size*=4;
        tex.param&=~GPU_TEXTURE_ETC1_PARAM;
    }
#endif
    ConvertAndTile(level, dstW, dstH, tex.data);

    int lw = dstW, lh = dstH;
    for(int mip = 1; mip <= tex.maxLevel; mip++)
    {
        const int nw = lw / 2, nh = lh / 2;
        for(int y = 0; y < nh; y++)
        {
            if(g_picaYieldHook)
                g_picaYieldHook();

            const unsigned int* r0 = level + (size_t)(2 * y) * lw;
            const unsigned int* r1 = r0 + lw;
            unsigned int* out = level + (size_t)y * nw;
            for(int x = 0; x < nw; x++)
            {
                const unsigned int p0 = r0[2 * x], p1 = r0[2 * x + 1];
                const unsigned int p2 = r1[2 * x], p3 = r1[2 * x + 1];
                const unsigned int lo = (((p0 & 0x00ff00ffu) + (p1 & 0x00ff00ffu) +
                                          (p2 & 0x00ff00ffu) + (p3 & 0x00ff00ffu) +
                                          0x00020002u) >> 2) & 0x00ff00ffu;
                const unsigned int hi = ((((p0 >> 8) & 0x00ff00ffu) + ((p1 >> 8) & 0x00ff00ffu) +
                                          ((p2 >> 8) & 0x00ff00ffu) + ((p3 >> 8) & 0x00ff00ffu) +
                                          0x00020002u) >> 2) & 0x00ff00ffu;
                out[x] = lo | (hi << 8);
            }
        }
        lw = nw; lh = nh;
        ConvertAndTile(level, lw, lh, C3D_TexGetImagePtr(&tex, tex.data, mip, NULL));
    }

    if(levelOwned)
        free(level);
    if(decompressed)
        free(decompressed);

    C3D_TexFlush(&tex);
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    vramUpload.Complete();TryPromoteVram();
#endif
    valid = true;
}

void picaTexture::ConvertAndTile(const unsigned int* rgba, int w, int h, void* dstData)
{
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
    O3ExperimentTimer causalTimer(O3E_CONVERT);
#endif
#ifdef SRR2_OLD3DS_TEXTURE_TILES
    switch(hwFormat)
    {
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    case GPU_L8: picaTile::Convert<picaTile::L8>(rgba,w,h,dstData,g_picaYieldHook); break;
#endif
    case GPU_RGBA8: picaTile::Convert<picaTile::RGBA8>(rgba,w,h,dstData,g_picaYieldHook); break;
    case GPU_RGBA5551: picaTile::Convert<picaTile::RGBA5551>(rgba,w,h,dstData,g_picaYieldHook); break;
    case GPU_LA8: picaTile::Convert<picaTile::LA8>(rgba,w,h,dstData,g_picaYieldHook); break;
    case GPU_RGBA4: picaTile::Convert<picaTile::RGBA4>(rgba,w,h,dstData,g_picaYieldHook); break;
    default: picaTile::Convert<picaTile::RGB565>(rgba,w,h,dstData,g_picaYieldHook); break;
    }
#else
    unsigned char* dst = (unsigned char*)dstData;

    for(int y = 0; y < h; y++)
    {
        if(g_picaYieldHook)

        {
            g_picaYieldHook();
        }

        const unsigned int* srcRow = rgba + (size_t)y * w;

        for(int x = 0; x < w; x++)
        {
            const unsigned int p = srcRow[x];

            const unsigned r = (p      ) & 0xff;
            const unsigned g = (p >>  8) & 0xff;
            const unsigned b = (p >> 16) & 0xff;
            const unsigned a = (p >> 24) & 0xff;

            const unsigned offset = picaSwizzleOffset(x, y, w);

            switch(hwFormat)
            {
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
                case GPU_L8: dst[offset]=r;break;
#endif
                case GPU_RGBA8:
                    ((unsigned int*)dst)[offset] =
                        (r << 24) | (g << 16) | (b << 8) | a;
                    break;

                case GPU_RGBA5551:
                    ((unsigned short*)dst)[offset] = (unsigned short)
                        (((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >> 7));
                    break;

                case GPU_LA8:
                    ((unsigned short*)dst)[offset] = (unsigned short)
                        ((((77 * r + 150 * g + 29 * b) >> 8) << 8) | a);
                    break;

                case GPU_RGBA4:
                    ((unsigned short*)dst)[offset] = (unsigned short)
                        (((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4));
                    break;

                default:
                    ((unsigned short*)dst)[offset] = (unsigned short)
                        (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                    break;
            }
        }
    }
#endif
}

void picaTexture::SetPriority(int p) { priority = p; }
int  picaTexture::GetPriority(void)  { return priority; }

void picaTexture::Prefetch(void) { }
void picaTexture::Discard(void)  {
}

int  picaTexture::GetNumPaletteEntries(void)                   { return 0; }
void picaTexture::SetPalette(int nEntries, pddiColour* palette) { }
int  picaTexture::GetPalette(pddiColour* palette)              { return 0; }

#ifdef SRR2_OLD_3DS
bool picaTexture::UploadMovieYUV(const unsigned char* y, const unsigned char* cr, const unsigned char* cb,
                               int width, int height, int yPitch, int chromaPitch)
{
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    // Movie construction is excluded from promotion. Keep this guard so a future
    // caller can use its ordinary writable-texture fallback instead of writing VRAM.
    if(vramAllocation)return false;
#endif
    if(!created || !y || !cr || !cb || hwFormat != GPU_RGB565 || tex.maxLevel != 0 ||
       width <= 0 || height <= 0 || width > tex.width || height > tex.height ||
       (width & 7) || (height & 7) || yPitch < width || chromaPitch < width / 2) return false;
    if(!valid) memset(tex.data, 0, (size_t)tex.width * tex.height * 2);
    picaSwizzleYUV420toRGB565(tex.data, y, cr, cb, width, height, tex.width, yPitch, chromaPitch);
    C3D_TexFlush(&tex);
    valid = true;
    return true;
}
#endif

#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
bool picaNativeTexturesAvailable()
{
    if(!g_picaNativeTexturesEnabled)return false;
    static int available=-1;
    int selected=__atomic_load_n(&available,__ATOMIC_RELAXED);
    if(selected<0){
        struct stat st;
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
        if(g_picaNativeBundleEnabled){
            char bundled[128];snprintf(bundled,sizeof(bundled),"romfs:/%s",picaNativeBundle::BundlePath);
            selected=((stat(bundled,&st)==0 || stat(picaNativeBundle::BundlePath,&st)==0) && S_ISREG(st.st_mode) && st.st_size==picaNativeBundle::BundleBytes);
        }
        else
#endif
        selected=stat("native-textures/v1",&st)==0 && S_ISDIR(st.st_mode);
        __atomic_store_n(&available,selected,__ATOMIC_RELAXED);
    }
    return selected!=0;
}
bool picaTexture::UploadPreparedPNG(const void* encoded,unsigned bytes,bool world)
{
    if(!created || valid || !nativeEligible || type!=PDDI_TEXTYPE_RGB ||
       !ReadPrepared(encoded,bytes,picaNativeTexture::EncodedPNG,world))return false;
    {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    radLoadTiming::StageRegion flush(radLoadTiming::TextureFlush);
#endif
    C3D_TexFlush(&tex);
    }
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    TryPromoteVram();
#endif
    valid=true;return true;
}
bool picaTexture::UploadPrepared()
{
    // Encoded PNGs are handled before decode. This legacy-upload seam covers
    // DXT assets only, avoiding a second content hash on PNG cache misses.
    if(!IsCompressed(type))return false;
    const unsigned blockBytes=type==PDDI_TEXTYPE_DXT1?8:16;
    const unsigned bytes=((xSize+3)/4)*((ySize+3)/4)*blockBytes;
    return bytes<=stagingBytes && ReadPrepared(staging,bytes,unsigned(type-PDDI_TEXTYPE_DXT1+1));
}
bool picaTexture::ReadPrepared(const void* source,unsigned sourceBytes,unsigned sourceKind,bool world)
{
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    bool etc=g_picaNativeEtcEnabled;
    if(etc && (!world || sourceKind!=picaNativeTexture::EncodedPNG))return false;
#endif
    if(!picaNativeTexturesAvailable())return false;
    NativeTextureCost cost(g_picaNativeTextureDiagnostics);
    struct Hash {
        NativeTextureCost& cost;
        AVSHA* context=av_sha_alloc();
        explicit Hash(NativeTextureCost& selected):cost(selected){}
        ~Hash(){av_free(context);}

        bool operator()(const void* bytes,size_t size,unsigned char* result)
        {
            if(!context || av_sha_init(context,256))return false;
            NativeTextureCost::Phase phase(cost,NativeTextureCost::HashWork);
            const unsigned char* cursor=static_cast<const unsigned char*>(bytes);
            while(size){const size_t count=size<32768?size:32768;
                av_sha_update(context,cursor,count);cursor+=count;size-=count;
                if(g_picaYieldHook)g_picaYieldHook();
            }
            av_sha_final(context,result);
            return true;
        }
    } hash(cost);
    unsigned format=hwFormat==GPU_RGB565?picaNativeTexture::RGB565:
        hwFormat==GPU_RGBA5551?picaNativeTexture::RGBA5551:
        hwFormat==GPU_RGBA4?picaNativeTexture::RGBA4:0;
    if(!format)return false;
    unsigned size=tex.size;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
    if(etc){format=picaNativeTexture::ETC1;size/=4;}
#endif
    unsigned bytes=C3D_TexCalcTotalSize(size,tex.maxLevel);
    const auto publish=[&](bool ok){
#ifdef SRR2_OLD3DS_NATIVE_ETC1
        // Change the unpublished descriptor only after full identity and body
        // verification. Failure leaves the original allocation/format intact.
        if(ok && etc){
            hwFormat=GPU_ETC1;tex.fmt=GPU_ETC1;tex.size=size;
            // Match C3D_TexInitWithParams: ETC1 also requires the sampler's
            // dedicated hardware mode bit. Keep the existing wrap/filter bits.
            tex.param|=GPU_TEXTURE_ETC1_PARAM;
        }
#endif
        return ok;
    };
    const picaNativeTexture::Description description={unsigned(xSize),unsigned(ySize),
        sourceKind,tex.width,tex.height,format,tex.maxLevel,sourceBytes,bytes};
    unsigned char header[picaNativeTexture::HeaderBytes];char key[65],path[96];
    if(!picaNativeTexture::Key(description,source,hash,header,key))return false;
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
    if(g_picaNativeBundleEnabled){
        const auto* entry=picaNativeBundle::Find(picaNativeBundle::Entries,picaNativeBundle::EntryCount,key,bytes+picaNativeTexture::HeaderBytes);
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_NATIVE_ETC1)
        if(!entry && etc && g_picaNativeExactWorldFallback){
            // Preserve every accepted ETC1 hit. Only its cache misses may use
            // byte-exact legacy-format payloads, including transparent scenery.
            // Reuse the already checked source digest instead of hashing twice.
            size=tex.size;bytes=C3D_TexCalcTotalSize(size,tex.maxLevel);
            const unsigned legacyFormat=hwFormat==GPU_RGB565?picaNativeTexture::RGB565:
                hwFormat==GPU_RGBA5551?picaNativeTexture::RGBA5551:picaNativeTexture::RGBA4;
            const picaNativeTexture::Description legacy={unsigned(xSize),unsigned(ySize),
                sourceKind,tex.width,tex.height,legacyFormat,tex.maxLevel,sourceBytes,bytes};
            picaNativeTexture::Encode(legacy,header);
            if(!picaNativeTexture::HeaderKey(hash,header,key))return false;
            entry=picaNativeBundle::Find(picaNativeBundle::Entries,picaNativeBundle::EntryCount,key,bytes+picaNativeTexture::HeaderBytes);
            if(entry){etc=false;__atomic_fetch_add(&nativeExactWorldHits,1u,__ATOMIC_RELAXED);}
        }
#endif
        if(!entry){cost.outcome=1;NativeTextureMiss(header);return false;}
        const unsigned previous=cost.Select(NativeTextureCost::Open);
        NativeBundleFile input;
        bool positioned;
        {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
            radLoadTiming::StageRegion seek(radLoadTiming::NativeTextureSeek);
#endif
            positioned=input.file && entry->bytes<=picaNativeBundle::BundleBytes &&
                entry->offset<=picaNativeBundle::BundleBytes-entry->bytes &&
                !fseek(input.file,entry->offset,SEEK_SET);
        }
        cost.Select(previous);
        bool ok=false;
        {NativeTextureCost::Phase phase(cost,NativeTextureCost::ReadCheck);
         if(positioned)ok=picaNativeTexture::Read(input.file,header,bytes,tex.data,hash,g_picaYieldHook,false);}
        cost.outcome=ok?0:2;
        if(ok){__atomic_fetch_add(&nativeTextureHits,1u,__ATOMIC_RELAXED);__atomic_fetch_add(&nativeTextureBytes,bytes,__ATOMIC_RELAXED);}
        else __atomic_fetch_add(&nativeTextureInvalid,1u,__ATOMIC_RELAXED);
        return publish(ok);
    }
#endif
    snprintf(path,sizeof(path),"native-textures/v%u/%s.ntx",format==picaNativeTexture::ETC1?2u:1u,key);
    FILE* f=nullptr;
    {NativeTextureCost::Phase phase(cost,NativeTextureCost::Open);f=fopen(path,"rb");}
    if(!f){cost.outcome=1;NativeTextureMiss(header);return false;}
    bool ok=false;
    {NativeTextureCost::Phase phase(cost,NativeTextureCost::ReadCheck);
     ok=picaNativeTexture::Read(f,header,bytes,tex.data,hash,g_picaYieldHook);}
    fclose(f);
    cost.outcome=ok?0:2;
    if(ok){__atomic_fetch_add(&nativeTextureHits,1u,__ATOMIC_RELAXED);__atomic_fetch_add(&nativeTextureBytes,bytes,__ATOMIC_RELAXED);}
    else __atomic_fetch_add(&nativeTextureInvalid,1u,__ATOMIC_RELAXED);
    return publish(ok);
}
#endif

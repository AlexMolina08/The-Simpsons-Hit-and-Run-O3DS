#include <main/worldstreaming.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/streamingpolicy.h>
#include <main/nativebench.h>
#include <main/graphics3ds.h>
#include <main/old3dsperformance.h>
#include <main/old3dsdiag.h>
#include <mission/gameplaymanager.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/avatar.h>
#include <worldsim/ped/pedestrianmanager.h>
#include <camera/supercammanager.h>
#include <camera/supercamcentral.h>
#include <camera/supercam.h>
#include <render/DSG/StaticEntityDSG.h>
#include <render/RenderManager/RenderManager.h>
#include <render/Culling/WorldScene.h>
#include <render/IntersectManager/IntersectManager.h>
#include <loading/loadingmanager.h>
#include <cstdlib>
#include <initializer_list>
#include <cstring>
#include <cstdint>
#include <malloc.h>
#include <3ds.h>
extern "C" u32 __ctru_heap_size;
extern "C" unsigned g_picaPresentedFrames;

namespace WorldStreaming {
bool predictive=false, audit=false, manual=false, capture=false, passive=false, passiveActive=false;
bool fly=false,flightActive=false;
unsigned incidentRoute=0,flightCpuUs=0;
float flightFov=1.15f;
bool idleLoader=true;
bool worldPriority=false,worldRetention=false,pedestrianReuse=false;
Avatar* flightAvatar=nullptr;
float flightPosition[3]={},flightVelocity[3]={},flightSpeed=32.f;
static float progress=0.f,captureDistance=0.f;
unsigned pedestrianLimit=0, traversal=0, cameraSweep=0;
float cruiseSpeed=18.f;
static unsigned nextScan=0, route=0;
static float observedLoadMs=1000.f;
static unsigned admissionChecks=0,memoryRejects=0,slotRejects=0,probeBindings=0;
static unsigned pedSpawnCounts[9]={};
static unsigned pedSlotSamples=0,pedReservedTotal=0,pedAwaitingTotal=0,pedReservedMaximum=0;
void PedSpawn(PedSpawnStage stage)
{
    if(NativeBench::collecting || passiveActive)++pedSpawnCounts[unsigned(stage)];
}
void PedSlots(unsigned reserved,unsigned awaitingActivation)
{
    // End-of-frame population sampling runs after the timed capture flag clears.
    // Its caller already limits calls to measured frames or passive audit scans.
    ++pedSlotSamples;pedReservedTotal+=reserved;pedAwaitingTotal+=awaitingActivation;
    if(reserved>pedReservedMaximum)pedReservedMaximum=reserved;
}
unsigned Key(const char* text)
{
    unsigned h=2166136261u;
    for(;*text;++text) h=(h^static_cast<unsigned char>(*text))*16777619u;
    return h;
}
bool ParseArgument(const char* text)
{
    if(!std::strcmp(text,"bench_stream_fly=1")){fly=true;return true;}
    if(!std::strcmp(text,"bench_incident_route=1")){incidentRoute=1;return true;}
    if(!std::strcmp(text,"bench_incident_route=2")){incidentRoute=2;return true;}
    if(!std::strcmp(text,"bench_incident_route=3")){incidentRoute=3;return true;}
    if(!std::strcmp(text,"bench_idle_loader=1")){idleLoader=true;return true;}
    if(!std::strcmp(text,"bench_idle_loader=0")){idleLoader=false;return true;}
    if(!std::strcmp(text,"bench_pedestrian_reuse=1")){pedestrianReuse=true;return true;}
    if(!std::strcmp(text,"bench_pedestrian_reuse=0")){pedestrianReuse=false;return true;}
    if(!std::strcmp(text,"bench_world_priority=1")){worldPriority=true;return true;}
    if(!std::strcmp(text,"bench_world_priority=0")){worldPriority=false;return true;}
    if(!std::strcmp(text,"bench_world_retention=1")){worldRetention=true;return true;}
    if(!std::strcmp(text,"bench_world_retention=0")){worldRetention=false;return true;}
    static const char cpuOption[]="bench_fly_cpu_us=";
    if(!std::strncmp(text,cpuOption,sizeof(cpuOption)-1)) {
        const char* value=text+sizeof(cpuOption)-1;char* end=nullptr;const unsigned long us=std::strtoul(value,&end,10);
        if(end==value || *end || us>20000)return false;
        flightCpuUs=unsigned(us);return true;
    }
    static const char fovOption[]="bench_fly_fov=";
    if(!std::strncmp(text,fovOption,sizeof(fovOption)-1)) {
        const char* value=text+sizeof(fovOption)-1;char* end=nullptr;const float fov=std::strtof(value,&end);
        if(end==value || *end || !std::isfinite(fov) || fov<.9f || fov>1.7f)return false;
        flightFov=fov;return true;
    }
    if(!std::strncmp(text,"bench_fly_speed=",16)) {
        const char* value=text+16;char* end=nullptr;const float speed=std::strtof(value,&end);
        if(end==value || *end || !std::isfinite(speed) || speed<18.f || speed>80.f)return false;
        flightSpeed=speed;return true;
    }
    struct Option { const char* name; unsigned* target; unsigned low,high; };
    Option options[]={{"bench_pedestrians=",&pedestrianLimit,4,7},
        {"bench_traversal=",&traversal,0,3},{"bench_camera_sweep=",&cameraSweep,0,1}};
    for(const auto& o:options) if(!std::strncmp(text,o.name,std::strlen(o.name))) {
        char* end=nullptr; const unsigned long v=std::strtoul(text+std::strlen(o.name),&end,10);
        if(*end || v<o.low || v>o.high) return false;
        *o.target=unsigned(v); return true;
    }
    for(const char* name:{"bench_predictive_streaming=","bench_stream_audit=","bench_manual=","bench_stream_capture="}) {
        if(!std::strncmp(text,name,std::strlen(name))) {
            const char* v=text+std::strlen(name);
            if(std::strcmp(v,"0") && std::strcmp(v,"1")) return false;
            bool& target=!std::strcmp(name,"bench_stream_audit=")?audit:!std::strcmp(name,"bench_manual=")?manual:!std::strcmp(name,"bench_stream_capture=")?capture:predictive;
            target=*v=='1'; return true;
        }
    }
    const char* captureOption="bench_capture_distance=";
    if(!std::strncmp(text,captureOption,std::strlen(captureOption))) {
        char* end=nullptr;const float v=std::strtof(text+std::strlen(captureOption),&end);
        if(*end || !std::isfinite(v) || v<=0.f || v>5000.f)return false;
        captureDistance=v;return true;
    }
    const char* name="bench_cruise_speed=";
    if(!std::strncmp(text,name,std::strlen(name))) {
        char* end=nullptr; const float v=std::strtof(text+std::strlen(name),&end);
        if(*end || !std::isfinite(v) || v<18.f || v>32.f) return false;
        cruiseSpeed=v; return true;
    }
    return false;
}
bool ScanDue(unsigned now)
{
    if((!predictive && !worldRetention) || int(now-nextScan)<0) return false;
    nextScan=now+100; return true;
}
float LeadSeconds(float speed) { return StreamingPolicy::LeadSeconds(speed,observedLoadMs); }
void ObserveLoad(unsigned ms) { observedLoadMs=.75f*observedLoadMs+.25f*float(ms); }
bool Admit(unsigned n,unsigned cap,unsigned normal,unsigned linear)
{
    ++admissionChecks;
    if(n+2>cap)++slotRejects;
    if(normal<8u*1024*1024 || linear<3u*1024*1024)++memoryRejects;
    return StreamingPolicy::Admit(n,cap,normal,linear);
}

struct ZoneEvent { std::uint64_t tick; unsigned route,zone; float speed,x,z;char stage[20]; };
struct QueueFile {unsigned route,position,total,active,promoted;char filename[96];};
static QueueFile queueFiles[128];static unsigned queueFileCount=0;
void QueuedFile(const char* filename,unsigned position,unsigned total,bool active,bool promoted)
{
    if(!audit)return;
    auto& entry=queueFiles[queueFileCount++%128];
    entry.route=route;entry.position=position;entry.total=total;entry.active=active;entry.promoted=promoted;
    std::snprintf(entry.filename,sizeof(entry.filename),"%s",filename && *filename?filename:"callback");
}
static ZoneEvent events[512];
static unsigned eventCount=0,eventLost=0;
void EventUID(const char* stage,unsigned long long zone)
{
    if(!audit) return;
    if(eventCount>=512 && !passive) { ++eventLost; return; }
    ZoneEvent& e=events[eventCount++%512]; e.tick=svcGetSystemTick(); e.route=route;
    e.zone=2166136261u;
    for(unsigned n=0;n<8;++n)e.zone=(e.zone^unsigned((zone>>(n*8))&255))*16777619u;
    e.speed=e.x=e.z=0.f;
    if(NativeBench::collecting || passiveActive) { rmt::Vector p;Avatar* a=GetAvatarManager()->GetAvatarForPlayer(0);
        if(a && a->GetCharacter()){a->GetPosition(p);e.speed=a->GetSpeedMps();e.x=p.x;e.z=p.z;} }
    std::snprintf(e.stage,sizeof(e.stage),"%s",stage);
}
void Event(const char* stage,const char* zone)
{
    EventUID(stage,static_cast<radInt64>(tName::MakeUID(zone)));
}
void RequestEvent(const char* stage,const char* filename,const char* section)
{
    if(!audit || !filename)return;
    const char* base=filename;for(const char* p=filename;*p;++p)if(*p=='/' || *p=='\\')base=p+1;
    if(std::strlen(base)<4)return;
    if(base[0]=='l' && base[1]>='1' && base[1]<='7' && (base[2]=='r' || base[2]=='z'))Event(stage,section?section:base);
}
// On-disk records come from original static mesh bounds, never from a previous
// run's resident scene. Thus an unloaded section still has observable probes.
struct ProbeDisk { unsigned key,zone,level; float sphere[4]; };
struct Probe { ProbeDisk disk; StaticEntityDSG* entity; unsigned drawFrame,lastFlags,lastRoute,loadedAt,firstDispatch; };
struct Observation { unsigned route,probe,flags,presented; };
static constexpr unsigned ObservationCapacity=65536;
struct Snapshot { unsigned route,linear,normal,peds,visible,queued,ground,groundSample; float groundY,player[3],camera[3],heading[3],fov; };
static unsigned groundSample=0,normalSample=0,normalFree=0;static bool groundFound=false;static float groundHeight=0.f;
static Probe* probes=nullptr;
static Observation* observations=nullptr;
static Snapshot* snapshots=nullptr;
static unsigned probeCount=0,observationCount=0,snapshotCount=0,observationLost=0,frameTag=0;
static unsigned pointers[4096]={},nextAudit=0;
static unsigned lastGameplayRoute=0;
static bool catalogValid=false;
struct Image { std::uint64_t tick;unsigned route,sequence,reason;float distance;unsigned char pixels[400*240*3]; };
static Image* images=nullptr;
static unsigned imageCapacity=8;
static unsigned imageSequence=0,nextImage=0,imageFailures=0,triggerSequence=0,triggerRoute=0;
static unsigned imageImportFailures=0,imageLayoutFailures=0,imageCacheFailures=0,imageChangedFailures=0;
static std::uint64_t imageCopyMaxTicks=0;
static void CaptureImage(unsigned reason)
{
    if(!images || route<nextImage || (triggerSequence && imageSequence>=triggerSequence+4))return;
    nextImage=route+250;
    GSPGPU_CaptureInfo before={},after={};
    if(R_FAILED(GSPGPU_ImportDisplayCaptureInfo(&before))) { ++imageFailures;++imageImportFailures;return; }
    const auto& top=before.screencapture[0];
    if(!top.framebuf0_vaddr || (top.format&7)!=GSP_BGR8_OES || top.framebuf_widthbytesize!=240*3) { ++imageFailures;++imageLayoutFailures;return; }
    Image& image=images[imageSequence%imageCapacity];image.sequence=0;
    const unsigned sourceWidth=Graphics3DS::RenderWidth();
    if(sourceWidth!=400 && sourceWidth!=800){++imageFailures;++imageLayoutFailures;return;}
    const unsigned scale=sourceWidth/400;
    if(R_FAILED(GSPGPU_InvalidateDataCache(top.framebuf0_vaddr,sizeof(image.pixels)*scale))){++imageFailures;++imageCacheFailures;return;}
    // Wide mode has 800 columns. Preserve the full view in the physical
    // 400-column output instead of copying only its left half.
    const auto* pixels=reinterpret_cast<const unsigned char*>(top.framebuf0_vaddr);
    const auto copyStart=svcGetSystemTick();
    if(scale==1)std::memcpy(image.pixels,pixels,sizeof(image.pixels));
    else for(unsigned x=0;x<400;++x)
        std::memcpy(image.pixels+x*240*3,pixels+x*scale*240*3,240*3);
    const auto copyTicks=svcGetSystemTick()-copyStart;
    if(copyTicks>imageCopyMaxTicks)imageCopyMaxTicks=copyTicks;
    if(R_FAILED(GSPGPU_ImportDisplayCaptureInfo(&after)) ||
       after.screencapture[0].framebuf0_vaddr!=top.framebuf0_vaddr ||
       after.screencapture[0].format!=top.format || after.screencapture[0].framebuf_widthbytesize!=top.framebuf_widthbytesize) { ++imageFailures;++imageChangedFailures;return; }
    image.tick=svcGetSystemTick();image.route=route;image.sequence=++imageSequence;image.reason=reason;image.distance=progress;
}

void MarkerImage()
{
    if(!images){imageCapacity=1;images=static_cast<Image*>(std::calloc(1,sizeof(Image)));}
    nextImage=0;CaptureImage(3);
}
static void RecordObservation(unsigned i,unsigned flags)
{
    if(observationCount>=ObservationCapacity && !passive){++observationLost;return;}
    observations[observationCount++%ObservationCapacity]={route,i,flags,g_picaPresentedFrames};
}
static unsigned PointerSlot(StaticEntityDSG* p) { return (reinterpret_cast<std::uintptr_t>(p)>>4)%4096; }
static void Index(unsigned i)
{
    unsigned slot=PointerSlot(probes[i].entity);
    for(unsigned n=0;n<4096;++n,slot=(slot+1)%4096) if(!pointers[slot]) { pointers[slot]=i+1;return; }
    ++observationLost;
}
void Init()
{
    if(!audit) return;
    const unsigned residency=NativeBench::WorldResidencyChoice();
    const char* catalog=residency==4 ? "sdmc:/3ds/hitrun-dev/world-residency-05/all/stream-probes.bin" :
        residency==3 ? "sdmc:/3ds/hitrun-dev/world-residency-04/opaque/stream-probes.bin" :
        residency==2 ? "sdmc:/3ds/hitrun-dev/world-residency-01/resident/stream-probes.bin" :
        "sdmc:/3ds/hitrun-dev/stream-probes.bin";
    FILE* f=std::fopen(catalog,"rb");
    unsigned header[4]={};
    if(!f) return;
    bool valid=std::fread(header,sizeof(header),1,f)==1 && header[0]==0x32504753u &&
        header[1]==2 && header[2]>0 && header[2]<=8192 && header[3]==sizeof(ProbeDisk);
    const unsigned count=valid?header[2]:0;
    probes=static_cast<Probe*>(std::calloc(count,sizeof(Probe)));
    valid=valid && probes;
    for(unsigned i=0;valid && i<count;++i) {
        valid=std::fread(&probes[i].disk,sizeof(ProbeDisk),1,f)==1;
        probes[i].lastFlags=~0u;
        const ProbeDisk& p=probes[i].disk;
        valid=valid && p.level>=1 && p.level<=7 && p.sphere[3]>0.f;
        for(float v:p.sphere) valid=valid && std::isfinite(v);
        if(i) valid=valid && probes[i-1].disk.key<p.key;
    }
    valid=valid && std::fgetc(f)==EOF && !std::ferror(f); std::fclose(f);
    observations=static_cast<Observation*>(std::calloc(ObservationCapacity,sizeof(Observation)));
    snapshots=static_cast<Snapshot*>(std::calloc(1201,sizeof(Snapshot)));
    catalogValid=valid && observations && snapshots;
    if(!catalogValid) {
        std::free(probes);std::free(observations);std::free(snapshots);
        probes=nullptr;observations=nullptr;snapshots=nullptr;return;
    }
    if(catalogValid) {
        probeCount=count;
        if(capture)images=static_cast<Image*>(std::calloc(8,sizeof(Image)));
    }
}
void Loaded(unsigned long long zone,StaticEntityDSG* entity)
{
    if(!catalogValid) return;
    unsigned key=2166136261u;
    const unsigned long long entityUID=static_cast<radInt64>(entity->GetUID());
    for(unsigned long long id:{zone,entityUID})
        for(unsigned n=0;n<8;++n)key=(key^unsigned((id>>(n*8))&255))*16777619u;
    rmt::Sphere sphere;entity->GetBoundingSphere(&sphere);
    const float bounds[4]={sphere.centre.x,sphere.centre.y,sphere.centre.z,sphere.radius};
    const unsigned char* bytes=reinterpret_cast<const unsigned char*>(bounds);
    for(unsigned i=0;i<sizeof(bounds);++i) key=(key^bytes[i])*16777619u;
    unsigned low=0,high=probeCount;
    while(low<high) { unsigned m=(low+high)/2; if(probes[m].disk.key<key) low=m+1;else high=m; }
    if(low<probeCount && probes[low].disk.key==key) {
        ++probeBindings;probes[low].entity=entity;probes[low].loadedAt=route;probes[low].firstDispatch=0;Index(low);
    }
}
void Unloaded(unsigned long long zone)
{
    if(!catalogValid) return;
    unsigned key=2166136261u;
    for(unsigned n=0;n<8;++n)key=(key^unsigned((zone>>(n*8))&255))*16777619u;
    std::memset(pointers,0,sizeof(pointers));
    for(unsigned i=0;i<probeCount;++i) {
        if(probes[i].disk.zone==key) { probes[i].entity=nullptr; probes[i].drawFrame=0; }
        if(probes[i].entity) Index(i);
    }
}
void ResetResidents()
{
    std::memset(pointers,0,sizeof(pointers));
    for(unsigned i=0;i<probeCount;++i) {probes[i].entity=nullptr;probes[i].drawFrame=0;probes[i].lastFlags=~0u;}
}
void Displayed(StaticEntityDSG* entity,unsigned draws)
{
    if(!catalogValid || (!NativeBench::collecting && !passiveActive) || !draws) return;
    unsigned slot=PointerSlot(entity);
    for(unsigned n=0;n<4096 && pointers[slot];++n,slot=(slot+1)%4096) {
        Probe& p=probes[pointers[slot]-1];
        if(p.entity==entity) { p.drawFrame=frameTag;if(!p.firstDispatch)p.firstDispatch=route;return; }
    }
}
void BeginFrame(unsigned ms,float distance)
{
    route=ms;progress=distance;++frameTag;
}
void EndFrame()
{
    lastGameplayRoute=route;
    if(!catalogValid)return;
    if(images && captureDistance>0.f && !triggerSequence && progress>=captureDistance) {triggerSequence=imageSequence+1;triggerRoute=route;}
    CaptureImage(triggerSequence?2:0);
    if(route<nextAudit)return;
    nextAudit=route+(passive?250u:100u);
    if(snapshotCount>=1201 && !passive) { ++observationLost; return; }
    Snapshot& s=snapshots[snapshotCount++%1201]; s.route=route;
    const unsigned level=unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1;
    rmt::Vector player,camera,heading;
    GetAvatarManager()->GetAvatarForPlayer(0)->GetPosition(player);
    SuperCam* cam=GetSuperCamManager()->GetSCC(0)->GetActiveSuperCam();
    cam->GetPosition(&camera); cam->GetHeadingNormalized(&heading);
    s.fov=cam->GetFOV();
    for(unsigned i=0;i<3;++i) { s.player[i]=player[i];s.camera[i]=camera[i];s.heading[i]=heading[i]; }
    s.queued=GetLoadingManager()->GetNumCurrentRequests();
    if(!groundSample || route-groundSample>=250) {
        rmt::Vector probe=player,normal,ground;probe.y+=2.f;
        GetIntersectManager()->FindIntersection(probe,groundFound,normal,ground);
        groundHeight=groundFound?ground.y:0.f;groundSample=route;
    }
    s.ground=groundFound;s.groundSample=groundSample;s.groundY=groundHeight;
    s.linear=linearSpaceFree();
    if(!normalSample || route-normalSample>=1000) {
        const unsigned used=mallinfo().uordblks;normalFree=used<__ctru_heap_size?__ctru_heap_size-used:0;normalSample=route;
    }
    s.normal=normalFree;
    PedestrianManager::GetInstance()->GetPopulationCounts(s.peds,s.visible);
    unsigned critical=0;
    for(unsigned i=0;i<probeCount;++i) {
        Probe& p=probes[i]; if(p.disk.level!=level) continue;
        rmt::Vector center(p.disk.sphere[0],p.disk.sphere[1],p.disk.sphere[2]);
        const rmt::Vector delta=center-camera;const float d2=delta.MagnitudeSqr(),radius=p.disk.sphere[3];
        if(d2>(160.f+radius)*(160.f+radius)) {
            if(p.lastFlags!=~0u && (p.lastFlags&64) && (observationCount<ObservationCapacity || passive)) {
                RecordObservation(i,0);p.lastFlags=0;p.lastRoute=route;
            }
            continue;
        }
        const float limit=Old3dsPerformance::DrawDistance()+radius;
        const bool distance=d2<=limit*limit;
        const bool small=radius>=.85f || d2<=NativeBench::SmallObjectCullSquared();
        const bool cullAccepted=GetRenderManager()->pWorldScene()->AuditSphereVisibility(center,radius);
        if(!p.entity && cullAccepted && radius>=10.f && (center-player).MagnitudeSqr()<=(radius+35.f)*(radius+35.f))++critical;
        unsigned flags=unsigned(p.entity!=nullptr) | (unsigned(p.entity && frameTag && p.drawFrame==frameTag)<<1) |
            (unsigned(distance)<<3) | (unsigned(small)<<4) | (unsigned(cullAccepted)<<5) | 64u;
        if(flags==p.lastFlags && route-p.lastRoute<1000)continue;
        RecordObservation(i,flags);
        p.lastFlags=flags;p.lastRoute=route;
    }
    if(images && !captureDistance && !triggerSequence && critical>=2 && GetAvatarManager()->GetAvatarForPlayer(0)->GetSpeedMps()>=22.f) {
        triggerSequence=imageSequence+1;triggerRoute=route;CaptureImage(1);
    }
}
void Dump(FILE* f,const char* trial)
{
    const unsigned oldestObservation=observationCount>ObservationCapacity?observationCount-ObservationCapacity:0;
    const unsigned reference=passive?lastGameplayRoute:route;
    const unsigned retainedObservationMs=observationCount?reference-observations[oldestObservation%ObservationCapacity].route:0;
    std::fprintf(f,"STREAM_REFERENCE reference_ms=%u marker_ms=%u pause_gap_ms=%u fly=%u fly_speed=%.3f performance_comparison=0\n",reference,route,route-reference,fly,flightSpeed);
    std::fprintf(f,"STREAM_WINDOW requested_ms=20000 retained_observation_span_ms=%u truncated=%u observations_ring_wrapped=%u\n",retainedObservationMs,passive && observationCount>ObservationCapacity && retainedObservationMs<20000,observationCount>ObservationCapacity);
    std::fprintf(f,"STREAM_CONFIG predictive=%u audit=%u catalog_valid=%u probes=%u observations=%u snapshots=%u lost=%u events_lost=%u performance_capture=%u pixels_proven=0 pedestrians=%u traversal=%u cruise=%.3f camera_sweep=%u manual=%u passive=%u window_ms=20000\n",
        predictive,audit,catalogValid,probeCount,observationCount,snapshotCount,observationLost,eventLost,!audit,pedestrianLimit,traversal,cruiseSpeed,cameraSweep,manual,passive);
    std::fprintf(f,"STREAM_ADMISSION checks=%u memory_rejects=%u slot_rejects=%u probe_bindings=%u\n",admissionChecks,memoryRejects,slotRejects,probeBindings);
    std::fprintf(f,"PED_SPAWN_AUDIT calls=%u cap=%u disabled=%u no_free=%u no_paths=%u no_crossing=%u rejected=%u spawned=%u trials=1 scope=measured_or_whole_passive reuse_enabled=%u reused=%u\n",
        pedSpawnCounts[0],pedSpawnCounts[1],pedSpawnCounts[2],pedSpawnCounts[3],pedSpawnCounts[4],pedSpawnCounts[5],pedSpawnCounts[6],pedSpawnCounts[7],pedestrianReuse,pedSpawnCounts[8]);
    std::fprintf(f,"PED_SLOT_AUDIT samples=%u reserved_total=%u awaiting_activation_total=%u reserved_maximum=%u scope=population_sample_calls\n",pedSlotSamples,pedReservedTotal,pedAwaitingTotal,pedReservedMaximum);
    std::fprintf(f,"STREAM_PREFETCH_POLICY nearby_radius=64 forward_limit=80 extra_sections=1 reserved_slots=1 normal_minimum=8388608 linear_minimum=3145728 interior_loads=0 early_dump_events=0\n");
    std::fprintf(f,"STREAM_RETENTION_POLICY enabled=%u shared_extra_sections=1 incoming_must_be_resident=1 maximum_ms=20000 normal_retire_below=6291456 linear_retire_below=2097152\n",worldRetention);
    std::fprintf(f,"STREAM_CAPTURE requested=%u allocated=%u frames=%u failures=%u trigger_ms=%u trigger_distance=%.3f performance_capture=0 display_capture_info=1 exact_cpu_frame_identity=0\n",capture,images!=nullptr,imageSequence<imageCapacity?imageSequence:imageCapacity,imageFailures,triggerRoute,captureDistance);
    std::fprintf(f,"STREAM_CAPTURE_COST import_failures=%u layout_failures=%u cache_failures=%u buffer_changed=%u maximum_copy_us=%.3f wide_column_sampling=2\n",imageImportFailures,imageLayoutFailures,imageCacheFailures,imageChangedFailures,imageCopyMaxTicks*1000000.0/SYSCLOCK_ARM11);
    if(images)for(unsigned n=0;n<imageCapacity;++n) {
        const Image& image=images[n];if(!image.sequence)continue;
        char path[256];std::snprintf(path,sizeof(path),"sdmc:/3ds/hitrun-dev/%s-frame-%02u.ppm",trial,n);
        FILE* pixelFile=std::fopen(path,"wb");if(!pixelFile)continue;
        std::fprintf(pixelFile,"P6\n400 240\n255\n");unsigned char row[400*3];
        bool valid=true;
        for(unsigned y=0;y<240;++y) {for(unsigned x=0;x<400;++x) {
            const unsigned offset=(x*240+(239-y))*3;
            for(unsigned c=0;c<3;++c)row[x*3+c]=image.pixels[offset+2-c];
        }valid=valid && std::fwrite(row,1,sizeof(row),pixelFile)==sizeof(row);}
        if(std::fclose(pixelFile))valid=false;
        std::fprintf(f,"STREAM_IMAGE file=%s tick=%llu route_ms=%u distance=%.3f sequence=%u reason=%u valid=%u\n",path+5,image.tick,image.route,image.distance,image.sequence,image.reason,valid);
    }
    std::fprintf(f,"STREAM_QUEUE_CONFIG priority=%u records=%u retained=%u snapshot_limit=32\n",worldPriority,queueFileCount,queueFileCount<128?queueFileCount:128);
    for(unsigned i=queueFileCount>128?queueFileCount-128:0;i<queueFileCount;++i) {
        const auto& item=queueFiles[i%128];
        std::fprintf(f,"STREAM_QUEUED route_ms=%u position=%u total=%u active=%u promoted=%u file=%s\n",item.route,item.position,item.total,item.active,item.promoted,item.filename);
    }
    for(unsigned i=eventCount>512?eventCount-512:0;i<eventCount;++i) { const auto& e=events[i%512]; std::fprintf(f,"STREAM_EVENT tick=%llu route_ms=%u zone=%08x stage=%s speed=%.5f x=%.5f z=%.5f\n",e.tick,e.route,e.zone,e.stage,e.speed,e.x,e.z); }
    for(unsigned i=0;i<probeCount;++i) { const auto& p=probes[i].disk; std::fprintf(f,"STREAM_PROBE id=%u key=%08x zone=%08x level=%u x=%.5f y=%.5f z=%.5f radius=%.5f resident=%u loaded_ms=%u first_draw_dispatch_ms=%u\n",i,p.key,p.zone,p.level,p.sphere[0],p.sphere[1],p.sphere[2],p.sphere[3],probes[i].entity!=nullptr,probes[i].loadedAt,probes[i].firstDispatch); }
    for(unsigned i=snapshotCount>1201?snapshotCount-1201:0;i<snapshotCount;++i) { const auto& s=snapshots[i%1201];if(passive && reference-s.route>20000)continue; std::fprintf(f,"STREAM_SNAPSHOT route_ms=%u linear=%u normal=%u peds=%u visible_peds=%u queue_slots=%u ground_query_hit=%u ground_sample_ms=%u ground_y=%.5f x=%.5f y=%.5f z=%.5f camera_x=%.5f camera_y=%.5f camera_z=%.5f heading_x=%.5f heading_y=%.5f heading_z=%.5f fov=%.6f\n",s.route,s.linear,s.normal,s.peds,s.visible,s.queued,s.ground,s.groundSample,s.groundY,s.player[0],s.player[1],s.player[2],s.camera[0],s.camera[1],s.camera[2],s.heading[0],s.heading[1],s.heading[2],s.fov); }
    for(unsigned i=observationCount>ObservationCapacity?observationCount-ObservationCapacity:0;i<observationCount;++i) { const auto& o=observations[i%ObservationCapacity];if(passive && reference-o.route>20000)continue; std::fprintf(f,"STREAM_OBSERVATION route_ms=%u probe=%u flags=%u presented=%u\n",o.route,o.probe,o.flags,o.presented); }
}
}
#endif

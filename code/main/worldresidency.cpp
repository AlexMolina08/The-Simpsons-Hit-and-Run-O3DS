#include <main/worldresidency.h>
#include <main/old3dsmainprofile.h>
#if defined(SRR2_OLD3DS_NATIVE_BENCH) || defined(SRR2_OLD3DS_WORLD_RESIDENCY)
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/nativebench.h>
#endif
#include <main/worldresidencyset.h>
#include <worldresidencyassets.hpp>
#include <meta/zoneeventlocator.h>
#include <render/DSG/StaticEntityDSG.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <initializer_list>
extern "C" {
#include <libavutil/sha.h>
#include <libavutil/mem.h>
extern unsigned g_old3dsWorldResidencyChoice;
}

namespace WorldResidency {
struct Disk { std::uint64_t alias, zone; float sphere[4]; };
static_assert(sizeof(Disk)==32,"resident visibility wire stride");
struct Entry { Disk disk; unsigned char level, zone; bool bound; };
struct Catalog {
    unsigned long long terrain, zones[32];
    unsigned count, zoneCount, wanted, bound, bad, loads, unloads;
    bool traversal, checked;
};
static Entry* entries=nullptr;
static Catalog catalogs[7]={};
static unsigned count=0, readyMask=0, activeLevel=0, skips=0, changes=0;
static unsigned staticPlacementFailures=0,groundPlacementFailures=0;
static bool attempted=false,production=false;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
static bool cacheLookup=true;
void CacheLookup(bool enabled){cacheLookup=enabled;}
unsigned CachePolicy(){return unsigned(cacheLookup);}
#endif

static unsigned long long UID(const char* text)
{
    unsigned long long value=0;
    for(;*text;++text)value=(value*65599)^static_cast<unsigned char>(*text);
    return value;
}
static unsigned long long ZoneUID(const char* filename)
{
    if(!filename)return 0;
    const char* base=filename;
    for(const char* p=filename;*p;++p)if(*p=='/' || *p=='\\')base=p+1;
    unsigned long long uid=0;
    for(;*base;++base){unsigned char c=*base;if(c>='A' && c<='Z')c+='a'-'A';uid=(uid*65599)^c;}
    return uid;
}
static Entry* Find(unsigned long long uid)
{
    unsigned low=0,high=count;
    while(low<high){unsigned middle=low+(high-low)/2;if(entries[middle].disk.alias<uid)low=middle+1;else high=middle;}
    return low<count && entries[low].disk.alias==uid ? entries+low : nullptr;
}
static void Demand(const char* filename,bool value)
{
    const auto uid=ZoneUID(filename);
    for(auto& catalog:catalogs)for(unsigned i=0;i<catalog.zoneCount;++i)if(catalog.zones[i]==uid){
        const unsigned mask=1u<<i,previous=catalog.wanted;
        if(value)catalog.wanted|=mask;else catalog.wanted&=~mask;
        changes+=previous!=catalog.wanted;return;
    }
}
void ClearDemand(){for(auto& c:catalogs)c.wanted=0;}
static void ResetLevel(unsigned map)
{
    Catalog& c=catalogs[map-1];
    if(c.bound)++c.unloads;
    c.wanted=c.bound=0;c.traversal=c.checked=false;
    for(unsigned i=0;i<count;++i)if(entries[i].level==map)entries[i].bound=false;
}
void Reset(){for(unsigned map=1;map<=7;++map)ResetLevel(map);}
static bool LoadCatalog(const char* path,unsigned map)
{
    FILE* file=std::fopen(path,"rb");
    unsigned char header[64]={};unsigned words[4]={};std::uint64_t section=0,level=0;
    bool valid=file && std::fread(header,1,64,file)==64;
    if(valid){std::memcpy(words,header,16);std::memcpy(&section,header+16,8);std::memcpy(&level,header+24,8);}
    valid=valid && !std::memcmp(header,"WRV1",4) && words[1]==1 && words[2]>0 && words[2]<=2048 && words[3]==32 && level==map && count+words[2]<=8192;
    Disk* disk=valid ? static_cast<Disk*>(std::malloc(words[2]*sizeof(Disk))) : nullptr;
    valid=valid && disk && std::fread(disk,sizeof(Disk),words[2],file)==words[2] && std::fgetc(file)==EOF && !std::ferror(file);
    if(file)std::fclose(file);
    unsigned char hash[32]={};AVSHA* sha=valid?av_sha_alloc():nullptr;
    valid=valid && sha && av_sha_init(sha,256)==0;
    if(valid){av_sha_update(sha,reinterpret_cast<unsigned char*>(disk),words[2]*sizeof(Disk));av_sha_final(sha,hash);valid=!std::memcmp(hash,header+32,32);}
    av_free(sha);
    char expected[32];const char prefix=(map==1 || map==4 || map==7)?'L':'l';
    std::snprintf(expected,sizeof(expected),"art\\%c%u_TERRA.p3d",prefix,map);
    valid=valid && section==UID(expected);
    Catalog catalog={};catalog.terrain=section;catalog.count=words[2];
    for(unsigned i=0;valid && i<words[2];++i){
        valid=disk[i].alias && WorldResidencyAssets::Sector(map,disk[i].zone) && (!i || disk[i-1].alias<disk[i].alias);
        for(float value:disk[i].sphere)valid=valid && std::isfinite(value);
        valid=valid && disk[i].sphere[3]>0;
        unsigned zone=0;while(zone<catalog.zoneCount && catalog.zones[zone]!=disk[i].zone)++zone;
        if(zone==catalog.zoneCount){if(catalog.zoneCount==32){valid=false;break;}catalog.zones[catalog.zoneCount++]=disk[i].zone;}
    }
    if(valid){
        Entry* expanded=static_cast<Entry*>(std::realloc(entries,(count+words[2])*sizeof(Entry)));
        if(!expanded)valid=false;
        else {
            entries=expanded;
            for(unsigned i=0;i<words[2];++i){
                Entry& entry=entries[count+i];entry.disk=disk[i];entry.level=map;entry.bound=false;
                unsigned zone=0;while(catalog.zones[zone]!=disk[i].zone)++zone;entry.zone=zone;
            }
            count+=words[2];catalogs[map-1]=catalog;readyMask|=1u<<(map-1);
        }
    }
    std::free(disk);return valid;
}
unsigned GroundCapacity()
{
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    const unsigned choice=NativeBench::WorldResidencyChoice();if(choice>=2 && choice<=4)return 1536;
#endif
    return readyMask?1536:0;
}
static bool ResidentLevel(unsigned map)
{
    if(map<1 || map>7 || !(readyMask&(1u<<(map-1))))return false;
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
    if(production && !WorldResidencySet::Allows(map))return false;
#endif
    return true;
}
void Init()
{
    if(attempted)return;
    unsigned choice=0;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    choice=NativeBench::WorldResidencyChoice();
    if(choice!=3 && choice!=4){
        if(choice || NativeBench::WorldResidencyOverridden())return;
#ifndef SRR2_OLD3DS_WORLD_RESIDENCY
        return;
#endif
    }
#endif
    attempted=true;bool valid=true;
    if(choice==3)valid=LoadCatalog("sdmc:/3ds/hitrun-dev/world-residency-04/opaque/resident-visibility.bin",2);
    else if(choice==4)for(unsigned map=1;valid && map<=7;++map){
        char path[128];std::snprintf(path,sizeof(path),"sdmc:/3ds/hitrun-dev/world-residency-05/all/resident-visibility%u.bin",map);
        valid=LoadCatalog(path,map);
    }
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
    else {
        production=true;valid=WorldResidencySet::Open();
        for(unsigned map=1;valid && map<=7;++map){
            char path[256];const int length=std::snprintf(path,sizeof(path),"%s/resident-visibility%u.bin",WorldResidencySet::Root(),map);
            valid=length>0 && unsigned(length)<sizeof(path) && LoadCatalog(path,map);
        }
    }
#endif
    if(valid){
        std::qsort(entries,count,sizeof(Entry),[](const void* a,const void* b){
            const auto left=static_cast<const Entry*>(a)->disk.alias,right=static_cast<const Entry*>(b)->disk.alias;
            return left<right?-1:left>right?1:0;
        });
        for(unsigned i=1;i<count;++i)if(entries[i-1].disk.alias>=entries[i].disk.alias)valid=false;
    }
    if(!valid){std::free(entries);entries=nullptr;count=readyMask=0;std::memset(catalogs,0,sizeof(catalogs));
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
        if(production)WorldResidencySet::Abort();
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(!production){__atomic_store_n(&g_old3dsWorldResidencyChoice,2u,__ATOMIC_RELEASE);std::printf("WORLD_RESIDENCY metadata_invalid fallback=2\n");}
#endif
        if(production)std::printf("WORLD_RESIDENCY metadata_invalid original_fallback=1\n");return;
    }
    Reset();
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
    if(production)WorldResidencySet::Activate();
#endif
    std::printf("WORLD_RESIDENCY ready=1 mask=%02x entries=%u bytes=%u production=%u\n",readyMask,count,unsigned(count*sizeof(Entry)),production);
}
void InitialDemand(const char* filename)
{
    const auto uid=ZoneUID(filename);
    for(auto& c:catalogs)if(!c.traversal)for(unsigned i=0;i<c.zoneCount;++i)if(c.zones[i]==uid){c.wanted|=1u<<i;return;}
}
void Gameplay(unsigned map)
{
    if(!ResidentLevel(map))return;
    activeLevel=map;Catalog& c=catalogs[map-1];c.traversal=true;
    if(!c.checked){c.checked=true;++c.loads;
        if(c.bound!=c.count)++c.bad;
        std::printf("WORLD_RESIDENCY gameplay_level=%u bound=%u expected=%u bad=%u wanted=%08x loads=%u\n",map,c.bound,c.count,c.bad,c.wanted,c.loads);
    }
}
void Trigger(ZoneEventLocator* locator,unsigned player)
{
    if(!readyMask || player!=0 || !locator->GetPlayerEntered())return;
    for(int i=0;i<locator->GetNumDumpZones();++i)Demand(locator->GetDumpZone(i),false);
    for(int i=0;i<locator->GetNumLoadZones();++i)Demand(locator->GetLoadZone(i),true);
}
void Bind(unsigned long long section,StaticEntityDSG* entity)
{
    if(!readyMask)return;
    for(unsigned map=1;map<=7;++map)if(ResidentLevel(map) && catalogs[map-1].terrain==section){
        Catalog& c=catalogs[map-1];Entry* entry=Find(static_cast<radInt64>(entity->GetUID()));if(!entry)return;
        rmt::Sphere sphere;entity->GetBoundingSphere(&sphere);
        const float actual[4]={sphere.centre.x,sphere.centre.y,sphere.centre.z,sphere.radius};
        if(entry->level!=map || std::memcmp(actual,entry->disk.sphere,sizeof(actual)) || entry->bound){++c.bad;return;}
        entry->bound=true;++c.bound;return;
    }
}
bool Visible(StaticEntityDSG* entity)
{
    O3_MAIN_SCOPE(ResidentLookup);
    if(!readyMask)return true;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(entity->mOld3dsLandmark)return true;
#endif
    const auto uid=static_cast<unsigned long long>(static_cast<radInt64>(entity->GetUID()));
    Entry* entry=nullptr;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    if(cacheLookup)
#endif
    {
        const unsigned index=entity->mWorldResidencyIndex;
        if(index && index<=count && entries[index-1].disk.alias==uid)entry=entries+index-1;
    }
    if(!entry){
        entry=Find(uid);
        // Only the renderer writes this hint. Catalog initialization finishes
        // before gameplay; no engine pointer or demand state is cached.
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(cacheLookup)
#endif
        entity->mWorldResidencyIndex=entry?unsigned(entry-entries)+1:0;
    }
    const bool visible=!entry || !ResidentLevel(entry->level) || (entry->bound && (catalogs[entry->level-1].wanted&(1u<<entry->zone)));
    if(!visible)++skips;return visible;
}
void Unloaded(unsigned long long section)
{
    for(unsigned map=1;map<=7;++map)if(catalogs[map-1].count && catalogs[map-1].terrain==section)ResetLevel(map);
}
void PlacementFailure(bool ground)
{
    unsigned& failures=ground?groundPlacementFailures:staticPlacementFailures;++failures;
    if(failures<=8)std::printf("WORLD_RESIDENCY placement_failed ground=%u count=%u\n",ground,failures);
}
bool Failed()
{
    if(staticPlacementFailures || groundPlacementFailures)return true;
    for(const auto& c:catalogs)if(c.bad)return true;
    return false;
}
LevelStatus Status(unsigned level)
{
    if(level<1 || level>7)return {};
    const auto& c=catalogs[level-1];return {c.count,c.bound,c.bad,c.wanted,c.loads,c.unloads};
}
void Dump(FILE* file)
{
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    std::fprintf(file,"WORLD_RESIDENCY_LOOKUP cache=%u memo_index_bytes=2 engine_pointers=0\n",cacheLookup);
#endif
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
    if(production)WorldResidencySet::Dump(file);
#endif
    const unsigned selected=activeLevel?activeLevel:2;const Catalog& c=catalogs[selected-1];
    std::fprintf(file,"WORLD_RESIDENCY ready=%u level=%u count=%u bound=%u bad=%u zones=%u wanted=%08x traversal=%u skips=%u changes=%u mask=%02x total_records=%u authored_visibility=1\n",readyMask!=0,selected,c.count,c.bound,c.bad,c.zoneCount,c.wanted,c.traversal,skips,changes,readyMask,count);
    for(unsigned map=1;map<=7;++map)if(catalogs[map-1].count){const Catalog& item=catalogs[map-1];
        std::fprintf(file,"WORLD_RESIDENCY_LEVEL level=%u count=%u bound=%u bad=%u loads=%u unloads=%u wanted=%08x\n",map,item.count,item.bound,item.bad,item.loads,item.unloads,item.wanted);
    }
    std::fprintf(file,"WORLD_RESIDENCY_PLACEMENT static_failures=%u ground_failures=%u\n",staticPlacementFailures,groundPlacementFailures);
}
}
#endif

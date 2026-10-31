#include <main/worldresidencyset.h>
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
#include <worldresidencymanifest.hpp>
#include <3ds.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
extern "C" {
#include <libavutil/sha.h>
#include <libavutil/mem.h>
const char* old3dsNativeBundleHash();
const char* old3dsNativeBundlePath();
unsigned old3dsNativeBundleBytes();
bool old3dsResidencyTexturePolicy();
const char* g_old3dsResidencyRoot=nullptr;
}
#ifndef OLD3DS_WORLD_RESIDENCY_ROOT
#define OLD3DS_WORLD_RESIDENCY_ROOT "romfs:/world-residency"
#endif
namespace WorldResidencySet {
namespace {
static WorldResidencyAssets::Manifest* manifest=nullptr;
static unsigned states[7]={},admitted=0,rejected=0;
static bool attempted=false,opened=false;
static const char* selectedRoot=OLD3DS_WORLD_RESIDENCY_ROOT;
struct Hash {
    AVSHA* context=av_sha_alloc();
    ~Hash(){av_free(context);}
    bool operator()(const void* data,unsigned bytes,unsigned char* result){
        if(!context || av_sha_init(context,256))return false;
        av_sha_update(context,static_cast<const unsigned char*>(data),bytes);av_sha_final(context,result);return true;
    }
};
static bool Path(const char* name,char (&path)[256]) {
    const int count=std::snprintf(path,sizeof(path),"%s/%s",selectedRoot,name);
    return count>0 && unsigned(count)<sizeof(path);
}
static bool HashFile(FILE* file,unsigned bytes,const unsigned char* expected) {
    if(!file)return false;
    AVSHA* context=av_sha_alloc();auto* buffer=static_cast<unsigned char*>(std::malloc(32768));
    bool valid=context && buffer && av_sha_init(context,256)==0;
    unsigned remaining=bytes;
    while(valid && remaining){
        const unsigned chunk=remaining<32768?remaining:32768;
        valid=std::fread(buffer,1,chunk,file)==chunk;
        if(valid){av_sha_update(context,buffer,chunk);remaining-=chunk;}
    }
    unsigned char actual[32]={};
    if(valid){valid=std::fgetc(file)==EOF && !std::ferror(file);av_sha_final(context,actual);valid=valid && !std::memcmp(actual,expected,32);}
    std::free(buffer);av_free(context);std::fclose(file);return valid;
}
static bool HashAsset(const WorldResidencyAssets::AssetRecord& asset) {
    char path[256];return Path(asset.path,path) && HashFile(std::fopen(path,"rb"),asset.bytes,asset.sha256);
}
static bool NativeBundle() {
    char path[256];const int count=std::snprintf(path,sizeof(path),"romfs:/%s",old3dsNativeBundlePath());
    if(count<=0 || unsigned(count)>=sizeof(path))return false;
    FILE* file=std::fopen(path,"rb");if(!file)file=std::fopen(old3dsNativeBundlePath(),"rb");
    return HashFile(file,old3dsNativeBundleBytes(),manifest->header+16);
}
struct AdmissionJob {
    unsigned busy=0,record=0,remaining=0;
    FILE* file=nullptr;
    AVSHA* hash=nullptr;
    unsigned char* buffer=nullptr;
};
static AdmissionJob jobs[7];
static constexpr unsigned AdmissionReadBytes=16384;
struct AdmissionStepLock {
    unsigned& busy;
    ~AdmissionStepLock(){__atomic_store_n(&busy,0u,__ATOMIC_RELEASE);}
};
static int FinishAdmission(unsigned level,bool valid,const char* failed) {
    AdmissionJob& job=jobs[level-1];
    if(job.file)std::fclose(job.file);
    std::free(job.buffer);av_free(job.hash);
    job.file=nullptr;job.buffer=nullptr;job.hash=nullptr;
    __atomic_fetch_or(valid?&admitted:&rejected,1u<<(level-1),__ATOMIC_RELEASE);
    __atomic_store_n(states+level-1,valid?2u:3u,__ATOMIC_RELEASE);
    std::printf("WORLD_RESIDENCY_SET level=%u admitted=%u failed=%s original_fallback=%u\n",level,valid,failed,!valid);
    return valid?1:-1;
}
static int AdmitStep(unsigned level) {
    if(level<1 || level>7 || !__atomic_load_n(&g_old3dsResidencyRoot,__ATOMIC_ACQUIRE))return -1;
    unsigned state=__atomic_load_n(states+level-1,__ATOMIC_ACQUIRE);
    if(state==2 || state==3)return state==2?1:-1;
    AdmissionJob& job=jobs[level-1];
    if(__atomic_exchange_n(&job.busy,1u,__ATOMIC_ACQUIRE))return 0;
    AdmissionStepLock lock{job.busy};
    state=__atomic_load_n(states+level-1,__ATOMIC_ACQUIRE);
    if(state==2 || state==3)return state==2?1:-1;
    __atomic_store_n(states+level-1,1u,__ATOMIC_RELEASE);
    if(!job.hash){
        job.hash=av_sha_alloc();job.buffer=static_cast<unsigned char*>(std::malloc(AdmissionReadBytes));
        if(!job.hash || !job.buffer)return FinishAdmission(level,false,"allocation");
    }
    // Requeue the open between bounded reads so high-priority music requests
    // can use the same drive worker. No prepared file opens before all hashes pass.
    while(job.record<WorldResidencyAssets::ManifestCount){
        const auto& asset=manifest->records[job.record];
        if(std::strncmp(asset.path,"art/",4) || WorldResidencyAssets::Level(asset.path+4)!=level){++job.record;continue;}
        if(!job.file){
            char path[256];
            if(!Path(asset.path,path) || av_sha_init(job.hash,256))return FinishAdmission(level,false,asset.path);
            job.file=std::fopen(path,"rb");job.remaining=asset.bytes;
            if(!job.file)return FinishAdmission(level,false,asset.path);
        }
        if(job.remaining){
            const unsigned chunk=job.remaining<AdmissionReadBytes?job.remaining:AdmissionReadBytes;
            if(std::fread(job.buffer,1,chunk,job.file)!=chunk)return FinishAdmission(level,false,asset.path);
            av_sha_update(job.hash,job.buffer,chunk);job.remaining-=chunk;
            if(job.remaining)return 0;
        }
        unsigned char actual[32];av_sha_final(job.hash,actual);
        const bool valid=std::fgetc(job.file)==EOF && !std::ferror(job.file) && !std::memcmp(actual,asset.sha256,32);
        std::fclose(job.file);job.file=nullptr;
        if(!valid)return FinishAdmission(level,false,asset.path);
        ++job.record;return 0;
    }
    return FinishAdmission(level,true,"none");
}
static bool Admit(unsigned level) {
    int result;
    while((result=AdmitStep(level))==0)svcSleepThread(1000000);
    return result>0;
}
}
bool Open() {
    if(attempted)return opened;
    attempted=true;
    char path[256];if(!Path("asset-admission.bin",path))return false;
    FILE* file=std::fopen(path,"rb");
    // Loose-data homebrew has no complete game RomFS. Select its entire SD
    // subtree only when the bundled admission marker is absent. A malformed
    // bundled set never mixes with a different SD set.
    if(!file && errno==ENOENT && !std::strcmp(selectedRoot,"romfs:/world-residency")){
        selectedRoot="sdmc:/3ds/simpsons/world-residency";
        if(Path("asset-admission.bin",path))file=std::fopen(path,"rb");
    }
    if(!file)return false;
    manifest=static_cast<WorldResidencyAssets::Manifest*>(std::malloc(sizeof(*manifest)));
    bool valid=manifest && std::fread(manifest,1,sizeof(*manifest),file)==sizeof(*manifest) && std::fgetc(file)==EOF && !std::ferror(file);
    std::fclose(file);Hash hash;
    valid=valid && old3dsResidencyTexturePolicy() && manifest->Validate(hash,old3dsNativeBundleHash());
    for(unsigned i=0;valid && i<WorldResidencyAssets::ManifestCount;++i){
        const auto& asset=manifest->records[i];struct stat status;
        valid=Path(asset.path,path) && !stat(path,&status) && S_ISREG(status.st_mode) && std::uint64_t(status.st_size)==asset.bytes;
        // Visibility and landmark profiles are small and validate at startup.
        if(valid && std::strncmp(asset.path,"art/",4))valid=HashAsset(asset);
    }
    if(valid)valid=NativeBundle();
    if(!valid){Abort();std::printf("WORLD_RESIDENCY_SET admitted=0 original_fallback=1\n");return false;}
    opened=true;return true;
}
void Activate(){if(opened)__atomic_store_n(&g_old3dsResidencyRoot,selectedRoot,__ATOMIC_RELEASE);}
void Abort(){if(__atomic_load_n(&g_old3dsResidencyRoot,__ATOMIC_ACQUIRE))return;std::free(manifest);manifest=nullptr;opened=false;}
const char* Root(){return selectedRoot;}
bool Allows(unsigned level){return level>=1 && level<=7 && (__atomic_load_n(&admitted,__ATOMIC_ACQUIRE)&(1u<<(level-1)));}
void Dump(FILE* file){std::fprintf(file,"WORLD_RESIDENCY_SET opened=%u published=%u admitted=%02x rejected=%02x whole_level_hash_before_open=1\n",opened,__atomic_load_n(&g_old3dsResidencyRoot,__ATOMIC_ACQUIRE)!=nullptr,__atomic_load_n(&admitted,__ATOMIC_ACQUIRE),__atomic_load_n(&rejected,__ATOMIC_ACQUIRE));}
}
extern "C" bool old3dsWorldResidencyAdmit(unsigned level){return WorldResidencySet::Admit(level);}
extern "C" int old3dsWorldResidencyAdmitStep(unsigned level){return WorldResidencySet::AdmitStep(level);}
#endif

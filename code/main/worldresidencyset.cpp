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
static bool Admit(unsigned level) {
    if(level<1 || level>7 || !__atomic_load_n(&g_old3dsResidencyRoot,__ATOMIC_ACQUIRE))return false;
    unsigned expected=0;
    if(!__atomic_compare_exchange_n(states+level-1,&expected,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        while(expected==1){svcSleepThread(1000000);expected=__atomic_load_n(states+level-1,__ATOMIC_ACQUIRE);}
        return expected==2;
    }
    bool valid=true;const char* failed="none";
    // Validate every exterior file for this map before opening even its first
    // prepared file. A failed level always resolves its entire original set.
    for(const auto& asset:manifest->records)if(!std::strncmp(asset.path,"art/",4) && WorldResidencyAssets::Level(asset.path+4)==level){
        if(!HashAsset(asset)){valid=false;failed=asset.path;break;}
    }
    __atomic_fetch_or(valid?&admitted:&rejected,1u<<(level-1),__ATOMIC_RELEASE);
    __atomic_store_n(states+level-1,valid?2u:3u,__ATOMIC_RELEASE);
    std::printf("WORLD_RESIDENCY_SET level=%u admitted=%u failed=%s original_fallback=%u\n",level,valid,failed,!valid);
    return valid;
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
#endif

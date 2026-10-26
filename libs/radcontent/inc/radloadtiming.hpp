#ifndef RAD_LOAD_TIMING_PUBLIC_HPP
#define RAD_LOAD_TIMING_PUBLIC_HPP
namespace radLoadTiming {
enum WorkerStage {
    NativeTexture=0x7f4c0000, NativeTextureHash, NativeTextureOpen, NativeTextureRead,
    TextureAllocate, TextureDecode, TextureFlush, SkinInput, SkinPrepare, NativeTextureSeek
};
// Tags the existing worker interval; it never changes scheduling or file reads.
class StageRegion {
    unsigned previous=0;
    bool active=false;
public:
    explicit StageRegion(unsigned id);
    ~StageRegion();
    StageRegion(const StageRegion&)=delete;
    StageRegion& operator=(const StageRegion&)=delete;
};
}
#endif

// Emulator-only GCC profile export. The optimized game contains none of this.
extern "C" {
#include <gcov.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstdint>

extern const struct gcov_info* const __gcov_info_start[];
extern const struct gcov_info* const __gcov_info_end[];
namespace {
struct alignas(8) Allocation { Allocation* next; };
struct Output {
    FILE* file;
    Allocation* allocations;
    unsigned bytes;
    bool failed;
};
void Dump(const void* data, unsigned bytes, void* arg)
{
    Output& out = *static_cast<Output*>(arg);
    if(out.failed) return;
    if(std::fwrite(data, 1, bytes, out.file) != bytes) out.failed = true;
    else out.bytes += bytes;
}
void Filename(const char* filename, void* arg)
{
    if(!filename) { static_cast<Output*>(arg)->failed = true; return; }
    __gcov_filename_to_gcfn(filename, Dump, arg);
}
void* Allocate(unsigned bytes, void* arg)
{
    Output& out = *static_cast<Output*>(arg);
    if(bytes > UINT32_MAX - sizeof(Allocation)) std::abort();
    Allocation* p = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
    if(!p) std::abort();
    p->next = out.allocations;
    out.allocations = p;
    return p + 1;
}
}
extern "C" void old3dsDumpProfile()
{
    Output out = {std::fopen("old3ds-profile.gcfn", "wb"), NULL, 0, false};
    if(!out.file) { std::printf("OLD3DS_PGO export_open_failed=1\n"); return; }
    unsigned files = 0;
    const gcov_info* const* info = __gcov_info_start;
    __asm__ ("" : "+r" (info));
    for(; info != __gcov_info_end && !out.failed; ++info)
    {
        __gcov_info_to_gcda(*info, Filename, Dump, Allocate, &out);
        while(out.allocations) {
            Allocation* p = out.allocations;
            out.allocations = p->next;
            std::free(p);
        }
        ++files;
    }
    if(std::fclose(out.file)) out.failed = true;
    std::printf("OLD3DS_PGO files=%u bytes=%u complete=%u\n", files, out.bytes, !out.failed);
}

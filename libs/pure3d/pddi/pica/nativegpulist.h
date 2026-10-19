#ifndef PICA_NATIVE_GPU_LIST_H
#define PICA_NATIVE_GPU_LIST_H
#include <3ds.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { PICA_GPU_LIST_WORDS=384, PICA_GPU_LIST_KEY_WORDS=192 };
typedef struct {
    unsigned hits,misses,fallbacks,calls,retainedWords,comparisons,mismatches;
} picaGpuListStats;
extern bool g_picaGpuListsEnabled;
extern bool g_picaGpuListsValidate;
void picaGpuListsBeginFrame(void);
// The caller checks room before consuming SDK state. Lists and return records
// remain alive until the previous frame's render queue has completed.
bool picaGpuListRoom(void);
u32* picaGpuListFind(const u32* key,unsigned words,bool* hit);
void picaGpuListCommit(u32* list,unsigned words);
bool picaGpuListCompare(const u32* list,const u32* reference,unsigned words);
void picaGpuListCall(u32* list);
void picaGpuListsGetStats(picaGpuListStats* output);
#ifdef __cplusplus
}
#endif
#endif

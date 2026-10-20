#ifndef OLD3DS_MEMORY_H
#define OLD3DS_MEMORY_H
#include <cstdio>
enum Old3dsMemoryPhase { O3MemoryStartup, O3MemoryLoading, O3MemoryWarmup, O3MemoryRoute, O3MemoryStreaming, O3MemoryReload, O3MemoryPhaseCount };
#ifdef SRR2_OLD3DS_MEMORY_DIAG
void Old3dsMemorySetStride(unsigned);
void Old3dsMemorySetPhase(unsigned);
void Old3dsMemoryLinear(unsigned freeBytes,bool failed);
void Old3dsMemoryDump(FILE*);
#else
inline void Old3dsMemorySetPhase(unsigned){}
inline void Old3dsMemoryLinear(unsigned,bool){}
inline void Old3dsMemoryDump(FILE*){}
#endif
#endif

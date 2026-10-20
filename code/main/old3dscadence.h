#ifndef OLD3DS_CADENCE_H
#define OLD3DS_CADENCE_H
#include <cstdio>
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && !defined(SRR2_OLD3DS_CAUSAL_DIAG)
namespace Old3dsCadence {
extern bool enabled;
void Submit(unsigned identity);
void Dump(FILE*);
}
#else
namespace Old3dsCadence {inline void Submit(unsigned){} inline void Dump(FILE*){}}
#endif
#endif

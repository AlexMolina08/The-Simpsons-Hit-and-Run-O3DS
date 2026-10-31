#ifndef SRR_WORLD_RESIDENCY_SET_H
#define SRR_WORLD_RESIDENCY_SET_H
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
#include <cstdio>
namespace WorldResidencySet {
// Publication happens only after the visibility catalogs also validate.
// Original art remains available when a complete set or level is rejected.
bool Open();
void Activate();
void Abort();
const char* Root();
bool Allows(unsigned level);
void Dump(FILE*);
}
#endif
#endif

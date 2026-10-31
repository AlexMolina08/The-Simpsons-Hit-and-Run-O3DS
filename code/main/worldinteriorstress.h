#ifndef SRR_WORLD_INTERIOR_STRESS_H
#define SRR_WORLD_INTERIOR_STRESS_H
#ifdef SRR2_OLD3DS_NATIVE_BENCH
namespace WorldInteriorStress {
// Diagnostic round trip through authored preload, entry actions, iris callbacks
// and the same exit event used by the normal action button. No stage forcing.
// Returns -1 on failure, 0 while pending, 1 after entry and exit ground checks.
int Step(unsigned phase,unsigned elapsed,int context);
}
#endif
#endif

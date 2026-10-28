#ifndef OLD3DS_PROFILE_H
#define OLD3DS_PROFILE_H

// Runtime reports exist only in explicitly instrumented builds. Frame pacing,
// presentation deadlines and allocation budgets remain independent of profiling.
#if defined(SRR2_OLD_3DS) && (defined(SRR2_OLD3DS_NATIVE_BENCH) || \
    defined(SRR2_OLD3DS_OPTIONAL_DIAG) || defined(SRR2_OLD3DS_CAUSAL_DIAG) || \
    defined(SRR2_OLD3DS_GPU_SKIN_METRICS) || defined(SRR2_OLD3DS_COLLVOL_VERIFY))
#define SRR2_OLD3DS_RUNTIME_PROFILE
#endif

#endif

#ifndef OLD3DS_HOT_H
#define OLD3DS_HOT_H
// Keep stronger loop optimization scoped to measured hot functions. Full-program
// O3 grew code substantially; this feature retains the existing global O2 and
// floating-point controls. Native section size and frame time decide promotion.
#if defined(SRR2_OLD3DS_HOT_PATHS) && defined(__GNUC__) && !defined(__clang__)
#define OLD3DS_HOT_PATH __attribute__((optimize("O3")))
// Directed-rounding probes found O2/O3 differences in tensor inversion. Keep
// that kernel at its reference level, including when called from an O3 function.
// Interpreter repeats passed; the JIT also differed on identical O2 kernels,
// so these observations do not establish a hardware or compiler discrepancy.
#define OLD3DS_REFERENCE_PATH __attribute__((optimize("O2"), noinline))
#else
#define OLD3DS_HOT_PATH
#define OLD3DS_REFERENCE_PATH
#endif
#endif

#ifndef RADMATH_NATIVE_MATRIX_HPP
#define RADMATH_NATIVE_MATRIX_HPP
#include <3ds.h>
// Experimental benchmark control. Mode 0 uses scalar arithmetic; mode 1 uses
// the guarded vector kernel; mode 2 returns scalar results and checks vectors.
extern unsigned g_nativeMatrixMode;
extern bool g_nativeMatrixCollecting;
void old3dsNativeMatrixMultiply(float out[4][4],const float a[4][4],const float b[4][4]);
void old3dsNativeMatrixDump(void* file);
#endif

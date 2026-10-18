#include <radmath/nativematrix.hpp>
#include <radmath/nativematrixkernel.hpp>
#include <cstdio>
#include <cstring>
unsigned g_nativeMatrixMode=0;
bool g_nativeMatrixCollecting=false;
namespace {
struct Counter {unsigned calls,fast,fallback,samples,checked,mismatches;u64 ticks;} capture={};
unsigned allChecked=0,allMismatches=0;
static void scalar(float out[4][4],const float a[4][4],const float b[4][4])
{
    out[0][0] = (a[0][0] * b[0][0]) + (a[0][1] * b[1][0]) + (a[0][2] * b[2][0]);
    out[0][1] = (a[0][0] * b[0][1]) + (a[0][1] * b[1][1]) + (a[0][2] * b[2][1]);
    out[0][2] = (a[0][0] * b[0][2]) + (a[0][1] * b[1][2]) + (a[0][2] * b[2][2]);
    out[0][3] = 0.0f;

    out[1][0] = (a[1][0] * b[0][0]) + (a[1][1] * b[1][0]) + (a[1][2] * b[2][0]);
    out[1][1] = (a[1][0] * b[0][1]) + (a[1][1] * b[1][1]) + (a[1][2] * b[2][1]);
    out[1][2] = (a[1][0] * b[0][2]) + (a[1][1] * b[1][2]) + (a[1][2] * b[2][2]);
    out[1][3] = 0.0f;

    out[2][0] = (a[2][0] * b[0][0]) + (a[2][1] * b[1][0]) + (a[2][2] * b[2][0]);
    out[2][1] = (a[2][0] * b[0][1]) + (a[2][1] * b[1][1]) + (a[2][2] * b[2][1]);
    out[2][2] = (a[2][0] * b[0][2]) + (a[2][1] * b[1][2]) + (a[2][2] * b[2][2]);
    out[2][3] = 0.0f;

    out[3][0] = (a[3][0] * b[0][0]) + (a[3][1] * b[1][0]) + (a[3][2] * b[2][0]) + b[3][0];
    out[3][1] = (a[3][0] * b[0][1]) + (a[3][1] * b[1][1]) + (a[3][2] * b[2][1]) + b[3][1];
    out[3][2] = (a[3][0] * b[0][2]) + (a[3][1] * b[1][2]) + (a[3][2] * b[2][2]) + b[3][2];
    out[3][3] = 1.0f;
}
}
void old3dsNativeMatrixMultiply(float out[4][4],const float a[4][4],const float b[4][4])
{
    const bool collect=g_nativeMatrixCollecting;
    const bool sample=collect && ((capture.calls++ & 127u)==0);
    const u64 start=sample?svcGetSystemTick():0;
    bool used=false;
    if(g_nativeMatrixMode==1) {
        used=old3dsMatrix::TryMultiply(&out[0][0],&a[0][0],&b[0][0]);
        if(!used)scalar(out,a,b);
    } else if(g_nativeMatrixMode==2 && out!=a && out!=b) {
        float candidate[4][4];
        used=old3dsMatrix::TryMultiply(&candidate[0][0],&a[0][0],&b[0][0]);
        scalar(out,a,b);
        if(used) {
            const bool mismatch=memcmp(out,candidate,sizeof(candidate))!=0;
            ++allChecked;allMismatches+=mismatch;
            if(collect){++capture.checked;capture.mismatches+=mismatch;}
        }
    } else scalar(out,a,b);
    if(collect){capture.fast+=used;capture.fallback+=!used;}
    if(sample){capture.ticks+=svcGetSystemTick()-start;++capture.samples;}
}
void old3dsNativeMatrixDump(void* output)
{
    FILE* file=static_cast<FILE*>(output);
    fprintf(file,"NATIVE_MATRIX mode=%u calls=%u fast=%u fallback=%u samples=%u sampled_ms=%.6f checked=%u mismatches=%u all_checked=%u all_mismatches=%u validation_returns_scalar=%u\n",
        g_nativeMatrixMode,capture.calls,capture.fast,capture.fallback,capture.samples,
        capture.ticks*1000.0/SYSCLOCK_ARM11,capture.checked,capture.mismatches,
        allChecked,allMismatches,g_nativeMatrixMode==2);
}

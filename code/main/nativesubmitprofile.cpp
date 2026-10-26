#include <main/nativebench.h>
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_SUBMIT_PROFILE)
#include <main/old3dssampling.h>
#include <citro3d.h>
#include <cstdio>
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include <pddi/pica/nativecontext.h>
#endif

namespace {
struct Counter { u64 ticks; unsigned calls,samples; } counters[4];
unsigned codeUploads=0,codeWords=0;
class Sample {
    Counter* counter;
    u64 start;
public:
    explicit Sample(unsigned slot):counter(nullptr),start(0) {
        if(!NativeBench::collecting || !NativeBench::submissionProfile) return;
        counter=&counters[slot];
        const unsigned ordinal=counter->calls++;
        if(Old3dsRotatingSample(ordinal,5)) {
            ++counter->samples;start=svcGetSystemTick();
        }
    }
    ~Sample(){if(start)counter->ticks+=svcGetSystemTick()-start;}
};
}
extern "C" void __real_C3Di_UpdateContext(void);
extern "C" void __wrap_C3Di_UpdateContext(void) {
    Sample sample(0);
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    picaNativeUpdateContext();
#else
    __real_C3Di_UpdateContext();
#endif
}
extern "C" void __real_C3D_UpdateUniforms(GPU_SHADER_TYPE);
extern "C" void __wrap_C3D_UpdateUniforms(GPU_SHADER_TYPE type) {
    Sample sample(type==GPU_VERTEX_SHADER?1:2);__real_C3D_UpdateUniforms(type);
}
extern "C" Result __real_shaderProgramConfigure(shaderProgram_s*,bool,bool);
extern "C" Result __wrap_shaderProgramConfigure(shaderProgram_s* program,bool vertex,bool geometry) {
    Sample sample(3);
    if(NativeBench::collecting && NativeBench::submissionProfile && vertex) {
        ++codeUploads;
        codeWords+=program->vertexShader->dvle->dvlp->codeSize;
    }
    return __real_shaderProgramConfigure(program,vertex,geometry);
}
extern "C" void nativeBenchDumpSubmissionProfile(FILE* file) {
    fprintf(file,"NATIVE_SUBMISSION_PROFILE enabled=%u stride=32 route_only=1 in_memory=1 diagnostic_only=1 sampled_wall_time=1\n",NativeBench::submissionProfile);
    const char* names[]={"context","vertex_uniforms","geometry_uniforms","program_configure"};
    for(unsigned i=0;i<4;++i) {
        const Counter& c=counters[i];
        fprintf(file,"NATIVE_SUBMISSION scope=%s calls=%u samples=%u sampled_ms=%.6f estimated_total_ms=%.6f nested=1\n",
            names[i],c.calls,c.samples,c.ticks*1000.0/SYSCLOCK_ARM11,
            c.samples?c.ticks*1000.0/SYSCLOCK_ARM11*c.calls/c.samples:0.0);
    }
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    unsigned fast,fallback,words;picaNativeContextStats(&fast,&fallback,&words);
    fprintf(file,"NATIVE_CONTEXT fast=%u fallback=%u words=%u counters_include_warmup=1\n",fast,fallback,words);
#endif
    fprintf(file,"NATIVE_SUBMISSION_CODE uploads=%u instruction_words=%u\n",codeUploads,codeWords);
}
#endif

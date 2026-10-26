#!/usr/bin/env python3
"""Verify submission sampling, nested clocks and opt-in capture with an exact clock."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <main/nativebench.h>
#include <citro3d.h>
static u64 tick=100;
u64 svcGetSystemTick(){return tick;}
namespace NativeBench {bool collecting=false,submissionProfile=false;}
extern "C" void __wrap_C3Di_UpdateContext();
extern "C" void __wrap_C3D_UpdateUniforms(GPU_SHADER_TYPE);
extern "C" Result __wrap_shaderProgramConfigure(shaderProgram_s*,bool,bool);
extern "C" void nativeBenchDumpSubmissionProfile(FILE*);
static void contextBody(){tick+=100;__wrap_C3D_UpdateUniforms(GPU_VERTEX_SHADER);}
extern "C" void __real_C3Di_UpdateContext(){contextBody();}
extern "C" void picaNativeUpdateContext(){contextBody();}
extern "C" void picaNativeContextStats(unsigned* a,unsigned* b,unsigned* c){*a=1;*b=2;*c=3;}
extern "C" void __real_C3D_UpdateUniforms(GPU_SHADER_TYPE type){tick+=type==GPU_VERTEX_SHADER?200:50;}
extern "C" Result __real_shaderProgramConfigure(shaderProgram_s*,bool,bool){tick+=400;return 123;}
static char* dump(){char* output=nullptr;size_t length=0;FILE* file=open_memstream(&output,&length);assert(file);nativeBenchDumpSubmissionProfile(file);fclose(file);return output;}
static void check(const char* text,const char* scope,unsigned calls,unsigned samples,double total){
 char tag[128];snprintf(tag,sizeof(tag),"NATIVE_SUBMISSION scope=%s ",scope);const char* line=strstr(text,tag);assert(line);
 unsigned actualCalls=0,actualSamples=0;double sampled=0,estimated=0;
 assert(sscanf(line+strlen(tag),"calls=%u samples=%u sampled_ms=%lf estimated_total_ms=%lf",&actualCalls,&actualSamples,&sampled,&estimated)==4);
 assert(actualCalls==calls && actualSamples==samples);assert(estimated==total);
}
int main(){
 DVLP dvlp{98};DVLE dvle{&dvlp};shaderInstance_s instance{&dvle};shaderProgram_s program{&instance};
 // Disabled and outside-route calls retain the original functions and result.
 for(unsigned mode=0;mode<2;++mode){NativeBench::collecting=mode!=0;NativeBench::submissionProfile=mode==0;
  for(unsigned i=0;i<1024;++i){__wrap_C3Di_UpdateContext();assert(__wrap_shaderProgramConfigure(&program,true,false)==123);}}
 char* output=dump();check(output,"context",0,0,0);check(output,"vertex_uniforms",0,0,0);check(output,"program_configure",0,0,0);free(output);
 NativeBench::collecting=NativeBench::submissionProfile=true;
 for(unsigned i=0;i<1024;++i){__wrap_C3Di_UpdateContext();__wrap_C3D_UpdateUniforms(GPU_GEOMETRY_SHADER);assert(__wrap_shaderProgramConfigure(&program,true,false)==123);}
 output=dump();check(output,"context",1024,32,307200);check(output,"vertex_uniforms",1024,32,204800);
 check(output,"geometry_uniforms",1024,32,51200);check(output,"program_configure",1024,32,409600);
 assert(strstr(output,"NATIVE_SUBMISSION_CODE uploads=1024 instruction_words=100352"));free(output);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-submit-profile-') as name:
    folder = Path(name)
    (folder / '3ds.h').write_text('#include <cstdint>\nusing u64=std::uint64_t;u64 svcGetSystemTick();\n#define SYSCLOCK_ARM11 1000\n')
    (folder / 'citro3d.h').write_text('''#pragma once
enum GPU_SHADER_TYPE {GPU_VERTEX_SHADER,GPU_GEOMETRY_SHADER};using Result=int;
struct DVLP{unsigned codeSize;};struct DVLE{DVLP* dvlp;};
struct shaderInstance_s{DVLE* dvle;};struct shaderProgram_s{shaderInstance_s* vertexShader;};
''')
    header = folder / 'pddi/pica/nativecontext.h'
    header.parent.mkdir(parents=True)
    header.write_text('extern "C" void picaNativeUpdateContext();\nextern "C" void picaNativeContextStats(unsigned*,unsigned*,unsigned*);\n')
    (folder / 'test.cpp').write_text(fixture)
    for native in (False, True):
        flags = ['-DSRR2_OLD3DS_NATIVE_CONTEXT'] if native else []
        subprocess.run(['clang++', '-std=c++17', '-O2', '-fsanitize=address,undefined',
                        '-DSRR2_OLD3DS_NATIVE_BENCH', '-DSRR2_OLD3DS_SUBMIT_PROFILE', *flags,
                        '-I'+str(folder), '-I'+str(root/'code'),
                        str(root/'code/main/nativesubmitprofile.cpp'), str(folder/'test.cpp'),
                        '-o', str(folder/'test')], check=True)
        subprocess.run([str(folder/'test')], check=True)
print('PASS: opt-in route-only capture, rotating 1/32 samples, exact nested clocks, preserved return values and upload counts; native and SDK paths.')

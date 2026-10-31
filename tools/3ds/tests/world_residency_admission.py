#!/usr/bin/env python3
"""Exercise actual file admission, including missing sets and same-size corruption."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--set',type=Path,required=True)
parser.add_argument('--bundle',type=Path,required=True)
args=parser.parse_args();root=Path(__file__).resolve().parents[3]
metadata=json.loads((args.set/'manifest.json').read_text());digest=metadata['native_bundle_sha256'];bundle_bytes=args.bundle.stat().st_size
with tempfile.TemporaryDirectory(prefix='world-admission-') as directory:
    work=Path(directory);assets=work/'assets';assets.mkdir()
    for source in args.set.rglob('*'):
        if source.is_file():
            dest=assets/source.relative_to(args.set);dest.parent.mkdir(exist_ok=True,parents=True);dest.symlink_to(source.resolve())
    include=work/'include';(include/'libavutil').mkdir(parents=True)
    (include/'3ds.h').write_text('#include <thread>\n#include <chrono>\ninline void svcSleepThread(long long n){std::this_thread::sleep_for(std::chrono::nanoseconds(n));}\n')
    (include/'libavutil/sha.h').write_text('''#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>
extern bool failHashAllocation;
struct AVSHA {CC_SHA256_CTX context;};
inline AVSHA* av_sha_alloc(){return failHashAllocation?nullptr:(AVSHA*)malloc(sizeof(AVSHA));}
inline int av_sha_init(AVSHA* s,int bits){return bits==256 && CC_SHA256_Init(&s->context)?0:-1;}
inline void av_sha_update(AVSHA* s,const unsigned char* data,unsigned bytes){CC_SHA256_Update(&s->context,data,bytes);}
inline void av_sha_final(AVSHA* s,unsigned char* hash){CC_SHA256_Final(hash,&s->context);}
''')
    (include/'libavutil/mem.h').write_text('#include <stdlib.h>\ninline void av_free(void* p){free(p);}\n')
    (include/'readcheck.h').write_text('''#include <cstdio>
#include <cstdlib>
#include <atomic>
extern std::atomic<bool> boundedReads;
extern std::atomic<unsigned> admissionReads;
namespace std {
inline size_t admissionRead(void* data,size_t size,size_t count,FILE* file){
 if(boundedReads.load()){if(size*count>16384)std::abort();++admissionReads;}
 return std::fread(data,size,count,file);
}
}
#define fread admissionRead
''')
    code='''#include <main/worldresidencyset.h>
#include <worldresidencyassets.hpp>
#include <cassert>
#include <cstring>
#include <thread>
std::atomic<bool> boundedReads{false};
std::atomic<unsigned> admissionReads{0};
bool failHashAllocation=false;
static const char* bundlePath;static bool policy=true;
extern "C" const char* old3dsNativeBundleHash(){return "DIGEST";}
extern "C" const char* old3dsNativeBundlePath(){return bundlePath;}
extern "C" unsigned old3dsNativeBundleBytes(){return BUNDLE_BYTES;}
extern "C" bool old3dsResidencyTexturePolicy(){return policy;}
int main(int argc,char** argv){
 assert(argc==3);bundlePath=argv[2];policy=strcmp(argv[1],"policy_off");
 const bool opened=WorldResidencySet::Open();
 if(!strcmp(argv[1],"reject" ) || !policy){assert(!opened && !g_old3dsResidencyRoot);return 0;}
 assert(opened && !g_old3dsResidencyRoot);
 if(!strcmp(argv[1],"sd_valid"))assert(!strcmp(WorldResidencySet::Root(),"sdmc:/3ds/simpsons/world-residency"));
 if(!strcmp(argv[1],"romfs_valid"))assert(!strcmp(WorldResidencySet::Root(),"romfs:/world-residency"));
 WorldResidencySet::Activate();assert(g_old3dsResidencyRoot);
 boundedReads=true;
 assert(!old3dsWorldResidencyAdmit(0) && !old3dsWorldResidencyAdmit(8));
 if(!strcmp(argv[1],"allocation_failure")){
  failHashAllocation=true;assert(old3dsWorldResidencyAdmitStep(1)==-1 && !WorldResidencySet::Allows(1));
  failHashAllocation=false;assert(old3dsWorldResidencyAdmitStep(1)==-1);
  assert(old3dsWorldResidencyAdmit(2) && WorldResidencySet::Allows(2));
 }
 else if(!strcmp(argv[1],"corrupt_level")){assert(!old3dsWorldResidencyAdmit(1) && !WorldResidencySet::Allows(1));assert(!old3dsWorldResidencyAdmit(1));assert(old3dsWorldResidencyAdmit(2) && WorldResidencySet::Allows(2));}
 else {
  // Each pending step returns before opening prepared scenery or admitting a map.
  unsigned pending=0;int result;
  while((result=old3dsWorldResidencyAdmitStep(7))==0){++pending;assert(!WorldResidencySet::Allows(7));}
  assert(result==1 && pending>1 && WorldResidencySet::Allows(7));
  const unsigned reads=admissionReads;assert(old3dsWorldResidencyAdmitStep(7)==1 && admissionReads==reads);
  bool a=false,b=false;std::thread first([&]{a=old3dsWorldResidencyAdmit(1);});std::thread second([&]{b=old3dsWorldResidencyAdmit(1);});first.join();second.join();assert(a && b);for(unsigned i=1;i<=7;++i)assert(old3dsWorldResidencyAdmit(i) && WorldResidencySet::Allows(i));
 }
 WorldResidencySet::Abort();assert(g_old3dsResidencyRoot);WorldResidencySet::Dump(stdout);
}
'''.replace('DIGEST',digest).replace('BUNDLE_BYTES',str(bundle_bytes))
    (work/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-fsanitize=address,undefined','-DSRR2_OLD3DS_WORLD_RESIDENCY',
                    '-DOLD3DS_WORLD_RESIDENCY_ROOT="'+str(assets)+'"','-include',str(include/'readcheck.h'),'-I'+str(include),'-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),
                    str(work/'test.cpp'),str(root/'code/main/worldresidencyset.cpp'),'-o',str(work/'test')],check=True)
    # Test default production paths with host directories named romfs:/sdmc:.
    subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-fsanitize=address,undefined','-DSRR2_OLD3DS_WORLD_RESIDENCY',
                    '-include',str(include/'readcheck.h'),'-I'+str(include),'-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),
                    str(work/'test.cpp'),str(root/'code/main/worldresidencyset.cpp'),'-o',str(work/'default')],check=True)
    def run_default(mode):subprocess.run([str(work/'default'),mode,str(args.bundle.resolve())],cwd=work,check=True)
    sd=work/'sdmc:/3ds/simpsons/world-residency';sd.parent.mkdir(parents=True);sd.symlink_to(assets)
    run_default('sd_valid')
    romfs=work/'romfs:/world-residency';romfs.parent.mkdir()
    romfs.write_bytes(b'not a directory');run_default('reject');romfs.unlink()
    romfs.mkdir()
    (romfs/'asset-admission.bin').write_bytes(b'invalid')
    run_default('reject')
    (romfs/'asset-admission.bin').chmod(0)
    run_default('reject')
    (romfs/'asset-admission.bin').chmod(0o600)
    (romfs/'asset-admission.bin').unlink();romfs.rmdir();romfs.symlink_to(assets)
    run_default('romfs_valid')
    def run(mode,bundle=args.bundle):subprocess.run([str(work/'test'),mode,str(bundle.resolve())],check=True)
    def detach(path):
        data=path.read_bytes();assert path.is_symlink();path.unlink();return data
    def restore(path):
        path.unlink(missing_ok=True);path.symlink_to((args.set/path.relative_to(assets)).resolve())
    run('valid');run('policy_off');run('allocation_failure')
    absent=assets/'art/L7_TERRA.p3d';absent.unlink();run('reject');restore(absent)
    corrupt=assets/'art/l1r1.p3d';data=detach(corrupt);corrupt.write_bytes(data[:-1]+bytes([data[-1]^1]));run('corrupt_level');restore(corrupt)
    corrupt=assets/'resident-visibility7.bin';data=detach(corrupt);corrupt.write_bytes(data[:-1]+bytes([data[-1]^1]));run('reject');restore(corrupt)
    missing=assets/'asset-admission.bin';missing.unlink();run('reject');restore(missing)
    bad_bundle=work/'bad.bundle';bad_bundle.write_bytes(b'bad');run('reject',bad_bundle)
print('PASS: actual startup and per-level SHA admission, complete-set fallback, same-size corruption, immutable publication, concurrent first opens and unchanged source files.')

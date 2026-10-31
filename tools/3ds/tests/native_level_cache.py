#!/usr/bin/env python3
"""Validate exact cache preparation, bounded parsing and real worker failure paths."""
import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/3ds'))
from prepare_native_level_cache import prepare

def chunk(kind, body=b'', children=b''):
    return struct.pack('<3I',kind,12+len(body),12+len(body)+len(children))+body+children

def payload(source, value):
    data=bytes([value])*43680  # ETC1 256x256, six levels ending at 8x8.
    header=struct.pack('<11I',0x3158544e,2,256,256,65536,256,256,4,5,len(source),len(data))
    header+=hashlib.sha256(source).digest()
    result=header+hashlib.sha256(data).digest()+data
    return hashlib.sha256(header).hexdigest(),result

codec=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <CommonCrypto/CommonDigest.h>
#include <pddi/pica/nativelevelcache.hpp>
#include <pddi/pica/nativetexture.hpp>
struct Hash {bool operator()(const void* p,size_t n,unsigned char* out){return CC_SHA256(p,CC_LONG(n),out)!=nullptr;}};
int main(int argc,char** argv){
 assert(argc==2);FILE* file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);long size=ftell(file);rewind(file);
 std::vector<unsigned char> bytes(size);assert(fread(bytes.data(),1,size,file)==size);
 using namespace picaNativeLevel;Info info;assert(Header(bytes.data(),64,info));assert(info.bytes==bytes.size());
 assert(!Header(bytes.data(),63,info));Hash hash;assert(Verify(bytes.data(),info,bytes.data()+64,hash));
 for(unsigned i=0;i<info.count;++i){
  const unsigned char* e=bytes.data()+64+i*40;char key[65];for(unsigned n=0;n<32;++n)sprintf(key+n*2,"%02x",e[n]);
  const unsigned length=Word(e+36);assert(Find(info,bytes.data()+64,key,length)==e);assert(!Find(info,bytes.data()+64,key,length+1));
  const unsigned offset=info.payloadOffset+Word(e+32);const unsigned char* record=bytes.data()+offset;
  std::vector<unsigned char> memory(length-108),disk(length-108);MemorySource source{record,length};
  assert(picaNativeTexture::ReadSource(source,record,length-108,memory.data(),hash,nullptr));
  assert(fseek(file,offset,SEEK_SET)==0);assert(picaNativeTexture::Read(file,record,length-108,disk.data(),hash,nullptr,false));
  assert(memory==disk&&!memcmp(memory.data(),record+108,memory.size()));
  unsigned char different[76];memcpy(different,record,76);different[44]^=1;MemorySource mismatch{record,length};
  assert(!picaNativeTexture::ReadSource(mismatch,different,length-108,memory.data(),hash,nullptr));
 }
 auto corrupt=bytes;corrupt.back()^=1;assert(!Verify(corrupt.data(),info,corrupt.data()+64,hash));
 corrupt=bytes;corrupt[64+32]=1;assert(!Index(info,corrupt.data()+64,info.count*40));
 corrupt=bytes;memset(corrupt.data()+8,255,4);Info bad;assert(!Header(corrupt.data(),64,bad));
 corrupt=bytes;corrupt[16]^=1;assert(!Header(corrupt.data(),64,bad));
 Info invalid=info;invalid.bytes=1;assert(!Index(invalid,bytes.data()+64,info.count*40));
 fclose(file);
}
'''
source=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text()
a=source.index('#ifdef SRR2_OLD3DS_LEVEL_CACHE\n#include <pddi/pica/nativelevelcache.hpp>')
b=source.index('\n#endif\n\n#endif\nbool g_picaNativeTexturesEnabled',a)
module=source[a:b+len('\n#endif')]
worker=r'''
#define SRR2_OLD3DS_LEVEL_CACHE
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <CommonCrypto/CommonDigest.h>
using u64=unsigned long long;using u32=unsigned;using Result=int;
#define R_SUCCEEDED(x) ((x)>=0)
#define R_FAILED(x) ((x)<0)
static constexpr u64 U64_MAX=~0ULL,SYSCLOCK_ARM11=268123480;
extern "C" {u32 __ctru_heap_size=16*1024*1024;}
static unsigned heapFree=16*1024*1024,createCalls=0,joinCalls=0,freeCalls=0;
static bool allowCreate=true;static int requestedCore=-1;
struct AVSHA{CC_SHA256_CTX context;};
AVSHA* av_sha_alloc(){return static_cast<AVSHA*>(malloc(sizeof(AVSHA)));}
void av_free(void* p){free(p);}
int av_sha_init(AVSHA* p,int bits){assert(bits==256);return CC_SHA256_Init(&p->context)?0:-1;}
void av_sha_update(AVSHA* p,const unsigned char* data,size_t size){assert(CC_SHA256_Update(&p->context,data,CC_LONG(size)));}
void av_sha_final(AVSHA* p,unsigned char* result){assert(CC_SHA256_Final(result,&p->context));}
struct ThreadData{void(*entry)(void*);void* data;bool done=false;};using Thread=ThreadData*;
struct Exit{};void threadExit(int code){assert(code==0);throw Exit{};}
u64 svcGetSystemTick(){static u64 tick=0;return tick+=1000;}
int svcGetProcessorID(){return 1;}
Thread threadCreate(void(*fn)(void*),void* data,size_t stack,int priority,int core,bool detached){
 ++createCalls;assert(stack==65536&&priority==0x30&&core==1&&!detached);return allowCreate?new ThreadData{fn,data}:nullptr;
}
void execute(Thread t){try{t->entry(t->data);assert(false);}catch(Exit&){}t->done=true;}
Result threadJoin(Thread t,u64 wait){++joinCalls;if(!t->done&&wait==0)return -1;if(!t->done)execute(t);return 0;}
void threadFree(Thread t){assert(t->done);++freeCalls;delete t;}
struct NativeBundleSlot{void* owner=nullptr;FILE* file=nullptr;bool active=false;};
extern "C" int radLoadCoreExperimentRequested(){return requestedCore;}
extern "C" void radLoadSetCoreExperiment(int core){requestedCore=core;}
MODULE
int main(int argc,char** argv){
 assert(argc==2);const char* scenario=argv[1];using namespace picaNativeLevel;
 assert(!SetMode(3)&&Mode()==0);requestedCore=1;assert(!SetMode(2)&&Mode()==0);requestedCore=-1;
 if(!strcmp(scenario,"off")){Start(2);assert(!attempted&&createCalls==0);return 0;}
 const bool direct=!strcmp(scenario,"direct");assert(SetMode(direct?1:2)&&requestedCore==0);
 if(!strcmp(scenario,"heap"))heapFree=0;
 if(!strcmp(scenario,"thread"))allowCreate=false;
 Start(2);assert(!SetMode(0));
 if(!strcmp(scenario,"missing")||!strcmp(scenario,"heap")||!strcmp(scenario,"thread")){
  assert(state==Failed&&!picaNativeLevel::index&&!body&&!worker);Finish();return 0;
 }
 char key[65];for(unsigned n=0;n<32;++n)sprintf(key+n*2,"%02x",picaNativeLevel::index[n]);const unsigned length=Word(picaNativeLevel::index+36);
 if(direct){assert(state==Direct&&!worker&&!body&&createCalls==0);const auto view=Resolve(key,length);assert(view.valid&&!view.memory&&view.fileOffset==info.payloadOffset);}
 else {
  assert(state==Reading&&worker&&body&&createCalls==1);assert(!Resolve(key,length).valid);Tick();assert(pendingFrames==1);
  execute(worker);Tick();assert(!worker&&freeCalls==1&&!digestContext);
  if(!strcmp(scenario,"corrupt")){assert(state==Failed&&!body&&!Resolve(key,length).valid);Finish();return 0;}
  assert(state==Ready&&error==0&&coreSamples[1]>1&&coreSamples[0]==0&&readBytes==info.bytes-64);
  const auto view=Resolve(key,length);assert(view.valid&&view.memory&&!memcmp(view.memory,body+info.count*40,length));
 }
 Finish();free(picaNativeLevel::index);picaNativeLevel::index=nullptr;free(body);body=nullptr;
}
'''.replace('MODULE',module)
parser=argparse.ArgumentParser();parser.add_argument('--cache',type=Path);args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='native-level-cache-') as directory:
    base=Path(directory);art=base/'art';art.mkdir()
    images=[b'\x89PNG\r\n\x1a\n'+label for label in (b'source A',b'source B')]
    for name,image in zip(('l2z2.p3d','l2r2.p3d'),images):
        data=chunk(0xff443350,children=chunk(0x19002,struct.pack('<I',len(image))+image));(art/name).write_bytes(data)
    records=[payload(image,i+1) for i,image in enumerate(images)]
    bundle=base/'native.bundle';bundle.write_bytes(struct.pack('<II',0x314e4233,len(records))+b''.join(key.encode()+b'.ntx'+struct.pack('<II',len(data),zlib.crc32(data))+data for key,data in records))
    before={p.name:p.read_bytes() for p in art.iterdir()};result=prepare(art,bundle,2,base/'prepared');assert result['records']==2
    selected=prepare(art,bundle,2,base/'selected',selected_files=['l2z2.p3d']);assert selected['records']==1
    assert before=={p.name:p.read_bytes() for p in art.iterdir()}
    bad=bytearray(bundle.read_bytes());bad[-1]^=1;(base/'bad.bundle').write_bytes(bad)
    try:prepare(art,base/'bad.bundle',2,base/'invalid')
    except ValueError:assert not (base/'invalid').exists()
    else:raise AssertionError('Corrupted bundle accepted')
    (base/'codec.cpp').write_text(codec);(base/'worker.cpp').write_text(worker)
    (base/'malloc.h').write_text('struct mallinfo {unsigned fordblks,uordblks;};inline struct mallinfo mallinfo(){return {heapFree,__ctru_heap_size-heapFree};}\n')
    for label in ('codec','worker'):
        subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-fsanitize=address,undefined','-I'+str(base),'-I'+str(root/'libs/pure3d'),str(base/(label+'.cpp')),'-o',str(base/label)],check=True)
    for cache in [base/'prepared/level2.nsc']+([args.cache.resolve()] if args.cache else []):
        subprocess.run([str(base/'codec'),str(cache)],check=True)
    sd=base/'sdmc:/3ds/hitrun-dev/native-cache-01';sd.mkdir(parents=True)
    good=(base/'prepared/level2.nsc').read_bytes()
    for scenario in ('off','direct','staged','missing','heap','thread','corrupt'):
        target=sd/'level2.nsc';target.write_bytes(good)
        if scenario=='missing':target.unlink()
        if scenario=='corrupt':corrupted=bytearray(good);corrupted[-1]^=1;target.write_bytes(corrupted)
        subprocess.run([str(base/'worker'),scenario],cwd=base,check=True)
print('PASS: exact payload preparation, bounded SHA-verified codec, production memory/file readers, independent worker publication/join, missing/corrupt/budget/thread fallback and unchanged sources.')

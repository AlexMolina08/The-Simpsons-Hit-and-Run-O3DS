#!/usr/bin/env python3
"""Check raw-range ownership, existing loader handoffs and unchanged validation."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/p3d/loadmanager.cpp').read_text()
start = source.index('extern "C" unsigned g_picaPresentedFrames;')
end = source.index('\n#endif', start)
bridge = source[start:end]
manager = (root / 'libs/radcontent/src/radload/manager.cpp').read_text()
start = manager.index('void radLoadManager::WaitForRawRead(')
wait = manager[start:manager.index('\n#endif', start)]
code = r'''
#include <cassert>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
#include <pddi/pica/nativetexture.hpp>
#include <pddi/pica/nativereadahead.hpp>
using u32=unsigned;using u64=uint64_t;
constexpr u64 SYSCLOCK_ARM11=268000000;
static unsigned g_p3dMainThreadId=1;
static bool engineHeld=true,mainThread=false,failRead=false,completeEarly=false;
static unsigned opened=0,closed=0,largestRead=0;
extern "C" {unsigned g_picaPresentedFrames=0;}
namespace picaNativeRead {Hooks hooks;bool enabled=true;}
enum {CUR_THREAD_HANDLE,OpenExisting,NormalPriority,RADMEMORY_ALLOC_DEFAULT,Success,ReadError};
void svcGetThreadId(u32* out,int){*out=mainThread?1:2;}
u64 svcGetSystemTick(){static u64 tick=0;return ++tick;}
struct IRadThreadSemaphore {
 bool signaled=false;void Wait();void Signal(){signaled=true;}void Release(){delete this;}
};
static void finishDrive();
struct IRadFile {
 std::vector<unsigned char> bytes;unsigned position=0,count=0;
 void* output=nullptr;bool pending=true,opening=true;
 bool CheckForCompletion(){assert(engineHeld);return !pending;}
 bool IsOpen(){return !pending;}
 unsigned GetSize(){return bytes.size();}
 int GetLastError(){return failRead?ReadError:Success;}
 void SetPositionAsync(unsigned p){assert(engineHeld);position=p;}
 void ReadAsync(void* p,unsigned n){
  assert(engineHeld&&!pending&&position+n<=bytes.size());output=p;count=n;pending=true;
  largestRead=std::max(largestRead,n);
 }
 void AddCompletionEvent(IRadThreadSemaphore* sem){assert(engineHeld);if(completeEarly){finishDrive();sem->Signal();}}
 void Release(){assert(engineHeld&&!pending);++closed;delete this;}
};
static IRadFile* active=nullptr;static std::vector<unsigned char> payload;
void radFileOpen(IRadFile** out,const char*,bool,int,int,unsigned,int){
 assert(engineHeld&&!active);active=new IRadFile;active->bytes=payload;*out=active;++opened;
}
namespace p3d {
struct Manager {};
static Manager manager;static Manager* loadManager=&manager;
}
static void finishDrive(){
 assert(active&&active->pending);
 // The existing drive worker fills only the private raw range while gameplay
 // can present a frame. Protected construction resumes after the handoff.
 if(!active->opening)std::memcpy(active->output,active->bytes.data()+active->position,active->count);
 active->opening=false;active->pending=false;
}
void IRadThreadSemaphore::Wait(){
 assert(!engineHeld);
 if(!signaled){finishDrive();Signal();++g_picaPresentedFrames;}
 assert(signaled);signaled=false;
}
void radThreadCreateSemaphore(IRadThreadSemaphore** out,unsigned){*out=new IRadThreadSemaphore;}
static int worker=0;
static int* radThreadGetActiveThread(){return &worker;}
static void PauseWorkerTiming(){assert(engineHeld);}
static void ResumeWorkerTiming(){assert(engineHeld);}
struct Mutex {void Unlock(){assert(engineHeld);engineHeld=false;}void Lock(){assert(!engineHeld);engineHeld=true;}} mutex;
#define rAssert(value) assert(value)
struct radLoadManager {
 int* m_pThread=&worker;Mutex* m_pMutex=&mutex;
 void WaitForRawRead(IRadThreadSemaphore*);
} loader;
WAIT
static radLoadManager* radLoad=&loader;
BRIDGE
struct Hash {bool operator()(const void* p,size_t n,unsigned char* out){assert(engineHeld);
#ifdef __APPLE__
 return CC_SHA256(p,static_cast<CC_LONG>(n),out)!=nullptr;
#else
 return SHA256(static_cast<const unsigned char*>(p),n,out)!=nullptr;
#endif
}};
int main(){
 picaNativeRead::Hooks hooks{p3dNativeOpen,p3dNativeRead,p3dNativeClose};Hash hash;
 for(unsigned size:{1u,32659u,32768u,32769u,100000u}){
  std::vector<unsigned char> body(size),output(size+16,0xa5);
  for(unsigned i=0;i<size;++i)body[i]=i*37;
  unsigned char expected[picaNativeTexture::HeaderBytes]={};
  picaNativeTexture::Description description{32,32,256,32,32,1,0,7,size};
  picaNativeTexture::Encode(description,expected);hash(body.data(),body.size(),expected+76);
  payload.assign(expected,expected+sizeof(expected));payload.insert(payload.end(),body.begin(),body.end());
  for(unsigned trial=0;trial<3;++trial){
   completeEarly=trial==1;
   std::fill(output.begin(),output.end(),0xa5);
   auto wire=payload;
   if(trial==1)payload[0]^=1;
   if(trial==2)payload.back()^=1;
   {
    picaNativeRead::Stream stream(hooks,"bundle",payload.size(),0,payload.size());assert(stream.Ready());
    const bool ok=picaNativeTexture::ReadSource(stream,expected,size,output.data(),hash);
    assert(ok==(trial==0));
    if(ok)assert(std::equal(body.begin(),body.end(),output.begin()));
    for(unsigned n=size;n<output.size();++n)assert(output[n]==0xa5);
    if(trial==1)assert(output[0]==0xa5); // A mismatched descriptor does not write the body.
   }
   active=nullptr;payload=wire;
  }
  failRead=true;
  {picaNativeRead::Stream stream(hooks,"bundle",payload.size(),0,payload.size());
   assert(stream.Ready()&&!picaNativeTexture::ReadSource(stream,expected,size,output.data(),hash)&&stream.Failed());}
  active=nullptr;failRead=false;
 }
 mainThread=true;
 {picaNativeRead::Stream stream(hooks,"bundle",payload.size(),0,payload.size());assert(!stream.Ready());}
 mainThread=false;
 {picaNativeRead::Stream invalid(hooks,"bundle",payload.size(),payload.size(),1);assert(!invalid.Ready());}
 assert(opened==closed&&largestRead<=32768&&nativeIoFrames>0&&nativeIoTurns>0);
 assert(nativeIoTurns<=nativeIoOpens+nativeIoReads); // At most one sleep per pending operation; no busy polling.
 assert(nativeIoErrors>0&&engineHeld);
}
'''.replace('BRIDGE', bridge).replace('WAIT', wait)
with tempfile.TemporaryDirectory(prefix='native-read-ahead-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(code)
    link = [] if sys.platform == 'darwin' else ['-lcrypto']
    subprocess.run(['clang++', '-std=c++17', '-O1', '-g', '-Wno-deprecated-declarations',
                    '-fsanitize=address,undefined', '-I' + str(root / 'libs/pure3d'),
                    str(path / 'test.cpp'), *link, '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: bounded raw ranges, actual async bridge handoffs, balanced handles, main-thread fallback, corruption/read-error rejection and unchanged protected payload validation.')

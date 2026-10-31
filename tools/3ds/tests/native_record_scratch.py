#!/usr/bin/env python3
"""Check bounded native-record staging against the production reader and guards."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
texture = (root / 'libs/pure3d/pddi/pica/texture.cpp').read_text()
start = texture.index('const bool unpublished=etc && !valid')
end = texture.index(';', start) + 1
guard = texture[start:end]
code = r'''
#include <cassert>
#include <climits>
#include <vector>
#include <cstdio>
#include <cstring>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
#include <pddi/pica/nativetexture.hpp>
struct Hash {
 bool operator()(const void* bytes,size_t size,unsigned char* out){
#ifdef __APPLE__
  return CC_SHA256(bytes,static_cast<CC_LONG>(size),out)!=nullptr;
#else
  return SHA256(static_cast<const unsigned char*>(bytes),size,out)!=nullptr;
#endif
 }
};
constexpr unsigned GPU_ETC1=12;
bool Eligible(bool etc,bool valid,void* vramAllocation,void* nativeLinearAllocation,
              void* reuploadLinear,unsigned format){
 struct {unsigned fmt;} tex{format};
''' + guard + r'''
 return unpublished;
}
unsigned yields=0;void Yield(){++yields;}
int main(){
 using namespace picaNativeTexture;
 Hash hash;
 for(unsigned bits=0;bits<64;++bits){
  bool etc=bits&1,valid=bits&2;void* vram=(bits&4)?(void*)1:nullptr;
  void* compact=(bits&8)?(void*)1:nullptr;void* reupload=(bits&16)?(void*)1:nullptr;
  unsigned format=(bits&32)?GPU_ETC1:0;
  assert(Eligible(etc,valid,vram,compact,reupload,format)==(bits==1));
 }
 for(unsigned bytes:{1u,2u,16u,255u,4096u,8192u,16384u,32660u}){
  const unsigned recordBytes=bytes+HeaderBytes,capacity=4*recordBytes;
  std::vector<unsigned char> allocation(capacity+64,0xa5),body(bytes),record(recordBytes);
  for(unsigned i=0;i<bytes;++i)body[i]=(i*37+i/5)&255;
  Description description={128,128,EncodedPNG,128,128,ETC1,0,bytes,bytes};
  unsigned char expected[HeaderBytes];char key[65];
  assert(Key(description,body.data(),hash,expected,key));
  memcpy(record.data(),expected,76);assert(hash(body.data(),bytes,record.data()+76));
  memcpy(record.data()+HeaderBytes,body.data(),bytes);
  auto* scratch=RecordScratch(allocation.data(),capacity,bytes,recordBytes,true);
  assert(scratch==allocation.data()+capacity-recordBytes && scratch>=allocation.data()+bytes);
  assert(!RecordScratch(allocation.data(),capacity,bytes,recordBytes,false));
  assert(!RecordScratch(nullptr,capacity,bytes,recordBytes,true));
  assert(!RecordScratch(allocation.data(),bytes+recordBytes-1,bytes,recordBytes,true));
  assert(!RecordScratch(allocation.data(),capacity,bytes,recordBytes-1,true));
  assert(!RecordScratch(allocation.data(),capacity,bytes,recordBytes+1,true));
  memcpy(scratch,record.data(),recordBytes);
  MemorySource input{scratch,recordBytes};yields=0;
  assert(ReadSource(input,expected,bytes,allocation.data(),hash,Yield));
  assert(yields==1 && input.cursor==recordBytes && !memcmp(allocation.data(),body.data(),bytes));
  assert(!memcmp(scratch,record.data(),recordBytes));
  for(unsigned i=capacity;i<allocation.size();++i)assert(allocation[i]==0xa5);
  // Match the original file reader byte for byte.
  FILE* file=tmpfile();assert(file);
  assert(fwrite(record.data(),1,record.size(),file)==record.size());rewind(file);
  std::vector<unsigned char> original(bytes);
  assert(Read(file,expected,bytes,original.data(),hash));fclose(file);
  assert(original==body);
  for(unsigned field=0;field<HeaderBytes;++field){
   memcpy(scratch,record.data(),recordBytes);scratch[field]^=1;
   memset(allocation.data(),0xa5,bytes);
   MemorySource corrupt{scratch,recordBytes};
   assert(!ReadSource(corrupt,expected,bytes,allocation.data(),hash));
   if(field<76)for(unsigned i=0;i<bytes;++i)assert(allocation[i]==0xa5);
  }
  for(unsigned offset:{0u,bytes/2,bytes-1}){
   memcpy(scratch,record.data(),recordBytes);scratch[HeaderBytes+offset]^=1;
   MemorySource corrupt{scratch,recordBytes};
   assert(!ReadSource(corrupt,expected,bytes,allocation.data(),hash));
  }
  memcpy(scratch,record.data(),recordBytes);
  for(unsigned length:{0u,75u,107u,recordBytes-1}){
   MemorySource truncated{scratch,length};
   assert(!ReadSource(truncated,expected,bytes,allocation.data(),hash));
  }
 }
 unsigned char sentinel=0;
 assert(!RecordScratch(&sentinel,UINT_MAX,UINT_MAX,UINT_MAX,true));
 assert(!RecordScratch(&sentinel,UINT_MAX,32768,32768+HeaderBytes,true));
 assert(!RecordScratch(&sentinel,UINT_MAX,0,HeaderBytes,true));
 puts("PASS: exact legacy output; 64 actual ownership guards; disjoint bounded scratch; every header byte, body corruption, truncation and overflow rejected under ASan/UBSan.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-record-') as temporary:
    folder = Path(temporary)
    (folder / 'test.cpp').write_text(code)
    link = [] if sys.platform == 'darwin' else ['-lcrypto']
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wno-deprecated-declarations',
                    '-fsanitize=address,undefined', '-I' + str(root / 'libs/pure3d'),
                    str(folder / 'test.cpp'), *link, '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)

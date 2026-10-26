#!/usr/bin/env python3
"""Check bounded native reads, exact output, corrupt inputs and allocation fallback."""
from pathlib import Path
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[3]
program=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
#include <pddi/pica/nativetexture.hpp>
struct Hash {
 bool fail=false;
 bool operator()(const void* p,size_t n,unsigned char* out){
  if(fail)return false;
#ifdef __APPLE__
  return CC_SHA256(p,static_cast<CC_LONG>(n),out)!=nullptr;
#else
  return SHA256(static_cast<const unsigned char*>(p),n,out)!=nullptr;
#endif
 }
};
static unsigned yields=0,allocations=0;
static void Yield(){++yields;}
static void* Allocate(size_t n){assert(n<=65644);++allocations;return std::malloc(n);}
static void* FailAllocate(size_t){return nullptr;}
static FILE* Input(const std::vector<unsigned char>& data){
 FILE* f=fopen("input.bin","wb");assert(f);
 assert(fwrite(data.data(),1,data.size(),f)==data.size());fclose(f);
 f=fopen("input.bin","rb");assert(f && !setvbuf(f,nullptr,_IONBF,0));return f;
}
int main(){
 for(unsigned bytes:{0u,32u,128u,512u,2048u,16384u,32768u,65536u,65537u}){
  Hash hash;unsigned char source[32]={},expected[108];char key[65];
  picaNativeTexture::Description d={8,8,picaNativeTexture::EncodedPNG,8,8,picaNativeTexture::ETC1,0,32,bytes};
  assert(picaNativeTexture::Key(d,source,hash,expected,key));
  std::vector<unsigned char> data(108+bytes),body(bytes),output(bytes+32,0xa5);
  for(unsigned i=0;i<bytes;++i)body[i]=static_cast<unsigned char>(i*37+i/11);
  memcpy(data.data(),expected,76);assert(hash(body.data(),body.size(),data.data()+76));
  memcpy(data.data()+108,body.data(),bytes);
  for(bool failAllocation:{false,true}){
   FILE* f=Input(data);bool used=true;yields=0;
   assert(picaNativeTexture::ReadContiguous(f,expected,bytes,output.data(),hash,Yield,true,&used,failAllocation?FailAllocate:Allocate));
   assert(used==(!failAllocation && bytes<=65536));
   assert(!memcmp(output.data(),body.data(),bytes));
   for(unsigned i=bytes;i<output.size();++i)assert(output[i]==0xa5);
   assert(ftell(f)==long(data.size()));fclose(f);
  }
  if(bytes>65536)continue;
  for(unsigned corruption:{0u,1u,2u,3u}){
   if(corruption==1 && !bytes)continue;
   auto bad=data;std::fill(output.begin(),output.end(),0xa5);
   if(corruption==0)bad[44]^=1;
   if(corruption==1)bad.back()^=1;
   if(corruption==2)bad.resize(53);
   if(corruption==3)bad.push_back(0x7f);
   FILE* f=Input(bad);bool used=false;
   assert(!picaNativeTexture::ReadContiguous(f,expected,bytes,output.data(),hash,Yield,true,&used,Allocate));
   assert(used);for(auto v:output)assert(v==0xa5);fclose(f);
   if(corruption==3){ // Adjacent bundle records are allowed in non-exact mode.
    f=Input(bad);assert(picaNativeTexture::ReadContiguous(f,expected,bytes,output.data(),hash,Yield,false));
    assert(ftell(f)==long(data.size()) && !memcmp(output.data(),body.data(),bytes));fclose(f);
   }
  }
 }
 assert(allocations);puts("PASS: exact native bytes, guards, bounded allocation, oversized/OOM fallback, corrupt headers/bodies, truncation and adjacent bundle records.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-contiguous-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(program)
    link=[] if sys.platform=='darwin' else ['-lcrypto']
    subprocess.run(['clang++','-std=c++14','-O2','-Wno-deprecated-declarations','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),*link,'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True)

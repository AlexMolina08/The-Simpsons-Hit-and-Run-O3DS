#!/usr/bin/env python3
"""Exercise the production prepared-audio cache, fallback and malformed inputs."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('cache', type=Path)
args = parser.parse_args()
source = (root/'libs/radsound/src/hal/ctr/ndsphal.cpp').read_text()
fragment = source[source.index('static u16 s_AdpcmCoefs'):source.index('class radSoundHalBufferCtr')]
prelude = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include "preparedaudio.hpp"
using u16=uint16_t;using s16=int16_t;using u8=uint8_t;
#define NB_SCOPE(s) ((void)0)
namespace Old3dsPerformance { bool reduced=true;bool ReducedAudioSearch(){return reduced;} }
bool g_old3dsPreparedAudio=true,g_old3dsPreparedAudioShort=true;
static unsigned s_PreparedAudioShortHits=0;
static ctrPreparedAudio::Cache s_PreparedAudio;
'''
main = r'''
static void put32(std::vector<u8>& v,unsigned at,unsigned value){
 for(unsigned i=0;i<4;++i)v[at+i]=u8(value>>(8*i));
}
static void write(const char* path,const std::vector<u8>& data){
 FILE* file=std::fopen(path,"wb");assert(file);
 assert(std::fwrite(data.data(),1,data.size(),file)==data.size());std::fclose(file);
}
int main(int argc,char** argv){
 assert(argc==3);using namespace ctrPreparedAudio;
 std::ifstream file(argv[1],std::ios::binary);
 const std::vector<u8> original((std::istreambuf_iterator<char>(file)),{});
 assert(s_PreparedAudio.Load(argv[1]));
 assert(s_PreparedAudio.Bytes()==original.size() && s_PreparedAudio.Entries()>0);
 // The source file can disappear after initialization. Clip callbacks retain
 // only immutable memory and perform no file access.
 write(argv[2],original);assert(s_PreparedAudio.Load(argv[2]));assert(!std::remove(argv[2]));
 for(unsigned i=0;i<s_PreparedAudio.Entries();++i){
  const u8* e=original.data()+HeaderBytes+i*EntryBytes;
  const unsigned samples=Read32(e),pcm=Read32(e+4),dsp=Read32(e+8),bytes=Read32(e+12);
  std::vector<u8> guarded(samples*2+64,0xa5);std::memcpy(guarded.data()+32,original.data()+pcm,samples*2);
  std::vector<s16> reference(samples);std::memcpy(reference.data(),original.data()+pcm,samples*2);
  assert(AdpcmEncodeInPlaceKernel<3>(reference.data(),samples)==bytes);
  assert(!std::memcmp(reference.data(),original.data()+dsp,bytes));
  unsigned hits=s_PreparedAudio.hits;
  assert(AdpcmEncodeInPlace(guarded.data()+32,samples)==bytes);
  assert(s_PreparedAudio.hits==hits+1 && !std::memcmp(guarded.data()+32,original.data()+dsp,bytes));
  for(unsigned j=0;j<32;++j)assert(guarded[j]==0xa5 && guarded[32+samples*2+j]==0xa5);
  if(samples<16384){
   // Disabling short clips retains the production encoder and the long cache.
   std::vector<s16> input(samples);std::memcpy(input.data(),original.data()+pcm,samples*2);
   auto expected=input;const unsigned encoded=AdpcmEncodeInPlaceKernel<3>(expected.data(),samples);
   g_old3dsPreparedAudioShort=false;const unsigned beforeHits=s_PreparedAudio.hits;
   assert(AdpcmEncodeInPlace(input.data(),samples)==encoded);
   assert(s_PreparedAudio.hits==beforeHits && !std::memcmp(input.data(),expected.data(),encoded));
   g_old3dsPreparedAudioShort=true;
  }
  std::vector<u8> mismatch(original.begin()+pcm,original.begin()+pcm+samples*2);
  mismatch[samples]^=1;const auto before=mismatch;unsigned unchanged=123;
  assert(!s_PreparedAudio.Copy(mismatch.data(),samples,unchanged));
  assert(mismatch==before && unchanged==123);
  // Full-search modes and modified coefficient tables must bypass this
  // three-predictor cache even when the PCM key matches exactly.
  std::vector<s16> input(samples);std::memcpy(input.data(),original.data()+pcm,samples*2);
  auto expected=input;Old3dsPerformance::reduced=false;
  unsigned full=AdpcmEncodeInPlaceKernel<2>(expected.data(),samples);hits=s_PreparedAudio.hits;
  assert(AdpcmEncodeInPlace(input.data(),samples)==full);
  assert(s_PreparedAudio.hits==hits && !std::memcmp(input.data(),expected.data(),full));
  Old3dsPerformance::reduced=true;
  std::memcpy(input.data(),original.data()+pcm,samples*2);expected=input;s_AdpcmCoefs[2]=2016;
  full=AdpcmEncodeInPlaceKernel<0>(expected.data(),samples);
  assert(AdpcmEncodeInPlace(input.data(),samples)==full && s_PreparedAudio.hits==hits);
  assert(!std::memcmp(input.data(),expected.data(),full));s_AdpcmCoefs[2]=2048;
 }
 for(unsigned bad=0;bad<16;++bad){
  auto data=original;
  switch(bad){
   case 0:data[0]^=1;break;
   case 1:put32(data,8,2);break;
   case 2:put32(data,12,0);break;
   case 3:put32(data,12,257);break;
   case 4:put32(data,16,data.size()+1);break;
   case 5:put32(data,24,8);break;
   case 6:put32(data,28,1);break;
   case 7:put32(data,32,0xffffffffu);break;
   case 8:put32(data,36,data.size());break;
   case 9:put32(data,40,data.size());break;
   case 10:put32(data,44,1);break;
   case 11:put32(data,32,441001);break;
   case 12:data[Read32(data.data()+40)]=0xff;break;
   case 13:data.resize(31);break;
   case 14:data.resize(Limit+1);break;
   case 15:put32(data,32,MinSamples-1);break;
  }
  if(data.size()>=HeaderBytes)put32(data,20,Checksum(data.data()+HeaderBytes,data.size()-HeaderBytes));
  write(argv[2],data);assert(!s_PreparedAudio.Load(argv[2]));assert(!s_PreparedAudio.Bytes());
 }
 auto badChecksum=original;badChecksum.back()^=1;write(argv[2],badChecksum);
 assert(!s_PreparedAudio.Load(argv[2]));
 // The shortest supported prepared clip retains the same exact-key checks.
 const unsigned boundaryDsp=((MinSamples+13)/14)*8;
 std::vector<u8> boundary(HeaderBytes+EntryBytes+MinSamples*2+boundaryDsp,0);
 std::memcpy(boundary.data(),"SRRDSP1\0",8);
 put32(boundary,8,1);put32(boundary,12,1);put32(boundary,16,boundary.size());
 put32(boundary,24,3);put32(boundary,32,MinSamples);
 put32(boundary,36,HeaderBytes+EntryBytes);
 put32(boundary,40,HeaderBytes+EntryBytes+MinSamples*2);put32(boundary,44,boundaryDsp);
 put32(boundary,20,Checksum(boundary.data()+HeaderBytes,boundary.size()-HeaderBytes));
 write(argv[2],boundary);assert(s_PreparedAudio.Load(argv[2]));
 std::vector<s16> silence(MinSamples);unsigned cachedBytes=0;
 assert(s_PreparedAudio.Copy(silence.data(),MinSamples,cachedBytes) && cachedBytes==boundaryDsp);
 put32(boundary,32,MinSamples-1);
 put32(boundary,20,Checksum(boundary.data()+HeaderBytes,boundary.size()-HeaderBytes));
 write(argv[2],boundary);assert(!s_PreparedAudio.Load(argv[2]));
 assert(s_PreparedAudio.Load(argv[1]));
 assert(!s_PreparedAudio.Load("missing-prepared-audio-fixture") && !s_PreparedAudio.Bytes());
 unsigned unchanged=7;assert(!s_PreparedAudio.Copy(nullptr,0,unchanged) && unchanged==7);
 puts("PASS: actual prepared clips match production DSP bytes; exact whole-PCM keys, guard bytes, no callback I/O, mode/coefficient fallback, minimum-length boundary and 18 malformed inputs.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-prepared-audio-') as directory:
    folder = Path(directory)
    (folder/'test.cpp').write_text(prelude+fragment+main)
    subprocess.run(['clang++', '-std=c++17', '-O2', '-DSRR2_OLD_3DS', '-DSRR2_OLD3DS_PREPARED_AUDIO',
                    '-fsanitize=address,undefined', '-fno-sanitize=shift-base',
                    '-I'+str(root/'libs/radsound/src/hal/ctr'), str(folder/'test.cpp'),
                    '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test'), str(args.cache.resolve()), str(folder/'mutated.bin')], check=True)

#!/usr/bin/env python3
"""Validate lower-cost CTR ADPCM against the DSP decoder and real PCM assets."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/radsound/src/hal/ctr/ndsphal.cpp').read_text()
fragment=s[s.index('static u16 s_AdpcmCoefs'):s.index('class radSoundHalBufferCtr')]
fixture=r'''
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <random>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iterator>
using u16=uint16_t;using s16=int16_t;using u8=uint8_t;
#define NB_SCOPE(s) ((void)0)
namespace Old3dsPerformance { bool reduced=false;bool ReducedAudioSearch(){return reduced;} }
'''
main=r'''
static double quality(const std::vector<s16>& pcm,const std::vector<s16>& packed,unsigned bytes){
 const u8* data=(const u8*)packed.data();int h1=0,h2=0;double signal=0,error=0;
 for(unsigned f=0;f<bytes/8;++f){
  unsigned predictor=data[f*8]>>4,shift=data[f*8]&15;assert(predictor<8 && shift<=12);
  assert(!Old3dsPerformance::reduced || predictor<=2);
  for(unsigned i=0;i<14 && f*14+i<pcm.size();++i){
   int n=(data[f*8+1+i/2]>>(i%2?0:4))&15;if(n>=8)n-=16;
   int decoded=AdpcmClamp16(((s16(s_AdpcmCoefs[predictor*2])*h1+s16(s_AdpcmCoefs[predictor*2+1])*h2+1024)>>11)+n*(1<<shift));
   h2=h1;h1=decoded;double v=pcm[f*14+i],d=v-decoded;signal+=v*v;error+=d*d;
  }
 }
 return error?10*log10(signal/error):120;
}
static void check(std::vector<s16> pcm,const char* name){
 auto reference=pcm;Old3dsPerformance::reduced=false;
 unsigned bytes=AdpcmEncodeInPlace(reference.data(),pcm.size());
 double full=quality(pcm,reference,bytes);
 std::vector<s16> reduced(pcm.size()+32,s16(0x5a5a));memcpy(reduced.data()+16,pcm.data(),pcm.size()*2);
 unsigned beforeBudget=g_old3dsAdpcmBudgetCalls;
 Old3dsPerformance::reduced=true;unsigned compact=AdpcmEncodeInPlace(reduced.data()+16,pcm.size());
 assert(g_old3dsAdpcmBudgetCalls==beforeBudget+1);
 assert(compact==bytes && bytes==((pcm.size()+13)/14)*8);
 for(unsigned i=0;i<16;++i)assert(reduced[i]==s16(0x5a5a) && reduced[16+pcm.size()+i]==s16(0x5a5a));
 std::vector<s16> output(reduced.begin()+16,reduced.begin()+16+pcm.size());
 double limited=quality(pcm,output,bytes);
 assert(std::isfinite(limited));
 printf("ADPCM_QUALITY name=%s samples=%zu bytes=%u full_snr_db=%.3f budget_snr_db=%.3f duration_preserved=1 guards_intact=1\n",name,pcm.size(),bytes,full,limited);
}
int main(int argc,char** argv){
 std::mt19937 rng(147);
 for(unsigned n: {14u,15u,27u,28u,29u,4096u,220500u}){
  std::vector<s16> pcm(n);for(auto& v:pcm)v=s16(rng());check(pcm,"noise");
  for(unsigned i=0;i<n;++i)pcm[i]=s16(22000*sin(i*.071)+7000*sin(i*.317));check(pcm,"harmonics");
  for(unsigned i=0;i<n;++i)pcm[i]=i%2?-32768:32767;check(pcm,"fullscale");
  for(auto& v:pcm)v=0;check(pcm,"silence");
 }
 for(int a=1;a<argc;++a){
  std::ifstream f(argv[a],std::ios::binary);std::vector<char> b((std::istreambuf_iterator<char>(f)),{});
  assert(b.size()>2048 && !memcmp(b.data(),"RSD4PCM ",8));
  std::vector<s16> pcm((b.size()-2048)/2);memcpy(pcm.data(),b.data()+2048,pcm.size()*2);check(pcm,argv[a]);
 }
 // A changed DSP coefficient table must retain the generic encoder fallback.
 s_AdpcmCoefs[2]=2016;std::vector<s16> pcm(8192);for(auto& v:pcm)v=s16(rng());
 auto a=pcm,b=pcm;Old3dsPerformance::reduced=false;AdpcmEncodeInPlace(a.data(),a.size());
 Old3dsPerformance::reduced=true;AdpcmEncodeInPlace(b.data(),b.size());assert(a==b);
 puts("PASS: duration, partial frames, valid predictor/scale fields, DSP reconstruction, in-place guards and mutable-table fallback.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-adpcm-budget-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+fragment+main)
 subprocess.run(['clang++','-std=c++17','-O2','-DSRR2_OLD_3DS','-fsanitize=address,undefined','-fno-sanitize=shift-base',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 assets=[root/'sdcard/3ds/simpsons/sound'/name for name in ('accept.rsd','scroll.rsd')]
 subprocess.run([str(p/'test'),*[str(asset) for asset in assets if asset.is_file()]],check=True)

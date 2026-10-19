#!/usr/bin/env python3
"""Compare production channel bracket selection, including shared-track seeks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/p3d/anim/channel.cpp').read_text()
header=(root/'libs/pure3d/p3d/anim/channel.hpp').read_text()
def extract(text, signature):
 a=text.index(signature);b=text.index('{',a);e=b+1;depth=1
 while depth:
  depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
body=extract(source,'bool tChannel::FindBracketKeys(')
cls=extract(header,'class tChannel :')+';'
cls=cls.replace('protected:','public:')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>
#define P3DASSERT assert
struct tNonCopyable {};
struct tAnimationMemoryBlock {};
class tChannelLoader;
'''
fixture='unsigned long long referenceSearch=0, optimizedSearch=0;\n'
for name,enabled in [('Reference',False),('Optimized',True)]:
 fixture+=('#define SRR2_OLD3DS_CHANNEL_HINT\n' if enabled else '#undef SRR2_OLD3DS_CHANNEL_HINT\n')
 instrumented=body.replace('while(1)', 'while(1)').replace('    {      \n', '    { ++'+('optimizedSearch' if enabled else 'referenceSearch')+';\n')
 fixture+=cls.replace('tChannel',name)+'\n'+instrumented.replace('tChannel::',name+'::')+'\nvoid '+name+'::CleanUp() {}\n'
main=r'''
int main(){
 static_assert(sizeof(Reference)==sizeof(Optimized),"Hint fits existing channel padding");
 std::mt19937 random(819920);unsigned long long comparisons=0;
 for(int n: {1,2,3,4,7,32,63,128,255,256,257,1024,32767}){
  std::vector<short> frames(n);Reference reference;Optimized optimized;
  reference.frames=optimized.frames=frames.data();reference.numKeys=optimized.numKeys=n;
  for(unsigned track=0;track<25;++track){
   int key=-15000;
   for(int i=0;i<n;++i){key+=track%4 ? 1+random()%3 : 1;frames[i]=short(key);}
   // Largest legal tracks use a complete sorted range without short overflow.
   if(n>10000)for(int i=0;i<n;++i)frames[i]=short(i-16000);
   for(unsigned step=0;step<3000;++step){
    float frame;
    switch(track%5){
     case 0:frame=float(frames[0])+float(step%300)*0.125f;break;
     case 1:frame=float(frames[n-1])-float(step%300)*0.125f;break;
     case 2:frame=float(int(random()%50000)-25000)*0.5f;break;
     case 3:frame=float(frames[random()%n]);break;
     default:frame=std::nextafter(float(frames[random()%n]),step%2 ? INFINITY : -INFINITY);break;
    }
    if(step%71==0)frame=-INFINITY;
    if(step%73==0)frame=INFINITY;
    // Mutate the same key allocation after a cached selection.
    if(step%499==0 && n>3 && n<10000){
     for(auto& v:frames)++v;
    }
    int a=-1,b=-1,c=-1,d=-1;
    bool ref=reference.FindBracketKeys(frame,a,b), opt=optimized.FindBracketKeys(frame,c,d);
    assert(ref==opt && a==c && b==d);++comparisons;
    assert(optimized.dataType==Optimized::UNASSIGNED && optimized.channelCode==0);
   }
  }
 }
 printf("Binary search iterations: reference=%llu optimized=%llu\n",referenceSearch,optimizedSearch);
 printf("PASS: %llu production bracket comparisons; forward/reverse/shared seeks, exact/adjacent boundaries, infinities, same-address edits and large-track fallback; unchanged channel size %zu bytes\n",comparisons,sizeof(Optimized));
}
'''
with tempfile.TemporaryDirectory(prefix='srr-channel-hint-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+fixture+main)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=30)

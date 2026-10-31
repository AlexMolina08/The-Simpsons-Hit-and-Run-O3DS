#!/usr/bin/env python3
"""Check production probe identity without release-stripped object names."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/3ds'))
from prepare_stream_probes import uid, binary_key

source = (root / 'code/main/worldstreaming.cpp').read_text()
key_source=(root/'libs/radcore/inc/radkey.hpp').read_text()
ka=key_source.index('inline radKey radMakeKey(');kb=key_source.index('{',ka);kend=kb+1;depth=1
while depth:
    depth+=(key_source[kend]=='{')-(key_source[kend]=='}');kend+=1
native_key=key_source[ka:kend]

fragment = source[source.index('static unsigned PointerSlot'):source.index('void Init()')] + source[source.index('void Loaded('):source.index('void EndFrame()')]
record = source[source.index('static void RecordObservation'):source.index('static unsigned PointerSlot')]
prelude = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <main/tuidunaligned.h>
typedef tUidUnaligned radKey;
NATIVE_KEY
namespace rmt {struct Vector {float x,y,z;};struct Sphere {Vector centre;float radius;};}
struct StaticEntityDSG {
 unsigned long long id; rmt::Sphere sphere;
 void GetBoundingSphere(rmt::Sphere* p){*p=sphere;}
 tUidUnaligned GetUID(){return tUidUnaligned(id);}
 const char* GetName()=delete;
};
namespace NativeBench {bool collecting=true;}
static bool passive=false,passiveActive=false,catalogValid=true;
struct ProbeDisk {unsigned key,zone,level;float sphere[4];};
struct Probe {ProbeDisk disk;StaticEntityDSG* entity;unsigned drawFrame,lastFlags,lastRoute,loadedAt,firstDispatch;};
struct Observation {unsigned route,probe,flags,presented;};
static constexpr unsigned ObservationCapacity=65536;
static Probe* probes;static Observation* observations;
static unsigned probeCount,probeBindings=0,observationLost=0,observationCount=0,pointers[4096]={},frameTag=0,route=0,g_picaPresentedFrames=0;
static float progress=0;
'''
test = r'''
struct Input {unsigned long long zone,entity;float sphere[4];};
int main(int argc,char** argv){
 assert(argc==4);FILE* names=fopen(argv[3],"rb");assert(names);
 for(;;){unsigned long long expected;unsigned length;char text[512];
  if(fread(&expected,8,1,names)!=1)break;assert(fread(&length,4,1,names)==1&&length<512);
  assert(fread(text,1,length,names)==length);text[length]=0;
  assert(static_cast<radInt64>(radMakeKey(text))==expected);
 }fclose(names);
 FILE* catalog=fopen(argv[1],"rb");assert(catalog);
 unsigned h[4];assert(fread(h,sizeof(h),1,catalog)==1);assert(h[0]==0x32504753&&h[1]==2&&h[3]==28);
 probeCount=h[2];probes=static_cast<Probe*>(calloc(probeCount,sizeof(Probe)));
 for(unsigned i=0;i<probeCount;++i)assert(fread(&probes[i].disk,sizeof(ProbeDisk),1,catalog)==1);
 fclose(catalog);FILE* file=fopen(argv[2],"rb");assert(file);
 Input* input=new Input[probeCount];StaticEntityDSG* entities=new StaticEntityDSG[probeCount];
 assert(fread(input,sizeof(Input),probeCount,file)==probeCount);fclose(file);
 for(unsigned level=1;level<=7;++level){
  ResetResidents();unsigned first=probeCount;
  for(unsigned i=0;i<probeCount;++i)if(probes[i].disk.level==level){
   entities[i].id=input[i].entity;memcpy(&entities[i].sphere,input[i].sphere,16);
   route=123;Loaded(input[i].zone,&entities[i]);assert(probes[i].entity==&entities[i]);
   if(first==probeCount)first=i;
  }
  assert(first<probeCount);BeginFrame(140,10.f);assert(frameTag);
  for(unsigned i=0;i<probeCount;++i)assert(probes[i].drawFrame!=frameTag);
  Displayed(&entities[first],0);assert(probes[first].drawFrame!=frameTag);
  Displayed(&entities[first],3);assert(probes[first].drawFrame==frameTag&&probes[first].firstDispatch==140);
  BeginFrame(180,12.f);assert(probes[first].drawFrame!=frameTag);
  Unloaded(input[first].zone);
  for(unsigned i=0;i<probeCount;++i)if(input[i].zone==input[first].zone)assert(!probes[i].entity);
 }
 assert(probeBindings==probeCount&&!observationLost);
 observations=static_cast<Observation*>(calloc(ObservationCapacity,sizeof(Observation)));
 for(unsigned i=0;i<ObservationCapacity+2;++i){route=i;RecordObservation(3,4);}
 assert(observationCount==ObservationCapacity&&observationLost==2&&observations[0].route==0);
 passive=true;observationCount=0;observationLost=0;
 for(unsigned i=0;i<150000;++i){route=i;RecordObservation(3,4);}
 assert(observationCount==150000&&!observationLost);
 for(unsigned i=150000-ObservationCapacity;i<150000;++i)assert(observations[i%ObservationCapacity].route==i);
 delete[] input;delete[] entities;free(probes);free(observations);
}
'''

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--catalog',type=Path)
args=parser.parse_args()
if args.catalog:
    base=args.catalog
    manifest=json.loads((base/'stream-probes.json').read_text())
    catalog_data=(base/'stream-probes.bin').read_bytes()
else:
    records=[]
    for level in range(1,8):
        for offset in range(2):
            sphere=[float(level*10),2.,float(offset*5),8.]
            zone=f'l{level}r1.p3d';name='release_name_is_unavailable'
            records.append(dict(file=zone,entity=name,level=level,sphere=sphere,
                key=binary_key(struct.pack('<QQ4f',uid(zone),uid(name),*sphere)),
                zone=binary_key(struct.pack('<Q',uid(zone)))))
    records.sort(key=lambda p:p['key']);manifest={'probe_count':len(records),'probes':records}
    catalog_data=struct.pack('<4I',0x32504753,2,len(records),28)+b''.join(struct.pack('<3I4f',r['key'],r['zone'],r['level'],*r['sphere']) for r in records)
vectors = b''.join(struct.pack('<QQ4f', uid(p['file']), uid(p['entity']), *p['sphere']) for p in manifest['probes'])
with tempfile.TemporaryDirectory(prefix='stream-probe-runtime-') as td:
    p = Path(td)
    (p / 'input.bin').write_bytes(vectors)
    (p / 'names.bin').write_bytes(b''.join(struct.pack('<QI',uid(name),len(name.encode()))+name.encode() for r in manifest['probes'] for name in (r['file'],r['entity'])))
    (p / 'catalog.bin').write_bytes(catalog_data)
    (p / 'test.cpp').write_text(prelude.replace('NATIVE_KEY',native_key) + record + fragment + test)
    subprocess.run(['c++', '-std=c++17', '-DRAD_WIN32', '-I'+str(root/'code'), '-fsanitize=address,undefined', str(p / 'test.cpp'), str(root/'code/main/tuidunaligned.cpp'),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test'), str(p / 'catalog.bin'), str(p / 'input.bin'), str(p/'names.bin')], check=True)
print(f"PASS: {manifest['probe_count']} active-asset probes match native IDs with names unavailable; draw generations, unload/reset and 150,000 rolling records")

#!/usr/bin/env python3
"""Check packed GPU vertices, bounded error and every available character asset."""
from pathlib import Path
import importlib.util,struct,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('inventory',root/'tools/3ds/skin_inventory.py');inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
harness=r'''
#include <pddi/pica/skincore.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
int main(int argc,char** argv){
 assert(argc==2);FILE* f=fopen(argv[1],"rb");assert(f);unsigned count=0;
 picaSkin::Source source;unsigned char original[36];unsigned bones;
 while(fread(&source,1,sizeof(source),f)==sizeof(source)){
  assert(fread(original,1,36,f)==36&&fread(&bones,1,4,f)==4);
  picaSkin::Vertex full;assert(picaSkin::PackVertex(full,source,original,bones));
  bool rigid=source.weights[0]==1;unsigned char bytes[64];memset(bytes,0xa5,sizeof(bytes));
  picaSkin::StoreVertex(bytes,full,rigid);
  picaSkin::PackedVertex decoded{};
  if(rigid){picaSkin::PackedRigidVertex record;memcpy(&record,bytes,sizeof(record));
   memcpy(&decoded,&record,20);memcpy(decoded.offsets,record.offsets,4);
  }else{memcpy(&decoded,bytes,sizeof(decoded));assert(!memcmp(decoded.weights,full.weights,16));}
  assert(!memcmp(decoded.offsets,full.offsets,4)&&!memcmp(decoded.colour,full.colour,4));
  for(unsigned i=0;i<3;++i){assert(fabsf(decoded.position[i]/8192.f-full.position[i])<=1/8192.f+1e-7f);
   assert(fabsf(decoded.normal[i]/32768.f-full.normal[i])<=1/32768.f+1e-7f);}
  for(unsigned i=0;i<2;++i)assert(fabsf(decoded.uv[i]/4096.f-full.uv[i])<=1/4096.f+1e-7f);
  for(unsigned i=picaSkin::PackedStride(rigid);i<64;++i)assert(bytes[i]==0xa5);
  ++count;
 }
 fclose(f);assert(count);
 picaSkin::Vertex invalid{};invalid.position[0]=5;assert(!picaSkin::CanPack(invalid));
 invalid={};invalid.normal[1]=1.001f;assert(!picaSkin::CanPack(invalid));
 invalid={};invalid.uv[1]=8.001f;assert(!picaSkin::CanPack(invalid));
 invalid={};invalid.uv[0]=std::numeric_limits<float>::quiet_NaN();assert(!picaSkin::CanPack(invalid));
 assert(picaSkin::PackedStride(true)==24&&picaSkin::PackedStride(false)==40);
 printf("PASS: %u production source vertices; 24/40-byte layouts, exact weights/colors/indices, bounded position/normal/UV error and safe range rejection\n",count);
}
'''
assets=Path(sys.argv[1]) if len(sys.argv)>1 else root/'out/whole-frame-1/emulator/sdmc/3ds/simpsons/art/chars'
assert assets.exists(),'Character assets required for this audit'
with tempfile.TemporaryDirectory(prefix='srr-packed-skin-')as td:
 p=Path(td);groups=skins=0
 with (p/'vertices.bin').open('wb') as output:
  for asset in assets.glob('*_m.p3d'):
   data=asset.read_bytes()
   def walk(start,end,skin=False):
    global groups,skins
    for kind,body,child,stop in inventory.chunks(data,start,end):
     if kind==0x10001:skins+=1
     if kind==0x10002 and skin:
      material,o=inventory.pstring(data,body+4);topology,fmt,vertices,indices,bones=struct.unpack_from('<5I',data,o)
      sub={sk:data[sb:sc] for sk,sb,sc,se in inventory.chunks(data,child,stop)}
      groups+=1
      for i in range(vertices):
       position=struct.unpack_from('<3f',sub[0x10005],4+12*i)
       normal=struct.unpack_from('<3f',sub[0x10006],4+12*i)
       uv=struct.unpack_from('<2f',sub[0x10007],8+8*i) if 0x10007 in sub else (0,0)
       weights=struct.unpack_from('<3f',sub[0x1000c],4+12*i) if 0x1000c in sub else (1,0,0)
       ids=sub[0x1000b][4+4*i:8+4*i][::-1] if 0x1000b in sub else bytes(4)
       output.write(struct.pack('<3f4B6f',*weights,*ids,*normal,*position))
       output.write(struct.pack('<8f4BI',*position,*normal,*uv,17,91,163,255,bones))
     walk(child,stop,skin or kind==0x10001)
   walk(0,len(data))
 (p/'test.cpp').write_text(harness)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_GPU_SKIN_PACKED','-DSRR2_OLD3DS_GPU_SKIN_EYE_PALETTE','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'vertices.bin')],check=True)
 print(f'Audited {skins} complete skins and {groups} groups from original unmodified assets')

#!/usr/bin/env python3
"""Check bounded landmark selection and the production root-placement fallback."""
from pathlib import Path
import struct,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/render/Culling/WorldScene.cpp').read_text()
begin=source.index('   if(ipStaticEntity->mOld3dsLandmark)')
end=source.index('   // LandmarkRootEnd',begin)
block=source[begin:end]
fixture=r'''
#define SRR2_OLD3DS_VISUAL_UPGRADE
#define SRR2_OLD3DS_NATIVE_BENCH
extern const char* profilePath;
#define OLD3DS_LANDMARK_PROFILE_PATH profilePath
#include <main/landmarkvisibility.h>
#include <cassert>
#include <vector>
#include <cstring>
namespace Old3dsLandmark {bool enabled=false;unsigned selected=0,rootPlaced=0,rootFallback=0,visible=0;}
struct Entity;
struct List {unsigned capacity=1;std::vector<Entity*> items;bool Add(Entity* e){if(items.size()==capacity)return false;items.push_back(e);return true;}};
struct SpatialNode {List mSEntityElems;};
struct Entity {bool mOld3dsLandmark=false;SpatialNode* mpSpatialNode=nullptr;float bounds[4]={1,2,3,4};unsigned refs=1;};
struct World {SpatialNode root,node;struct Walker{SpatialNode* root;SpatialNode& rIthNode(int i){assert(i==0);return *root;}} mStaticTreeWalker{&root};
 void Place(Entity* ipStaticEntity,bool originalRoot){SpatialNode* pSpatialNode=originalRoot?&root:&node;
 BLOCK
 assert(pSpatialNode->mSEntityElems.Add(ipStaticEntity));ipStaticEntity->mpSpatialNode=pSpatialNode;
 }
};
const char* profilePath=nullptr;
int main(int argc,char** argv){
 assert(argc==3);profilePath=argv[1];
 const float bounds[]={1,2,3,4,5,6,7,8,9,10};
 assert(!Old3dsLandmark::SelectStatic("ART/L3R2.P3D","architecture",bounds));
 assert(!Old3dsLandmark::ProfileCache().attempted);
 Old3dsLandmark::enabled=true;
 assert(Old3dsLandmark::SelectStatic("sdmc:/3ds/art/L3R2.P3D","architecture",bounds));
 assert(Old3dsLandmark::SelectStatic("ART\\l3r2.p3d","architecture",bounds));
 assert(!Old3dsLandmark::SelectStatic("art/l6r2.p3d","architecture",bounds));
 assert(!Old3dsLandmark::SelectStatic("art/l3r2.p3d","architecture_extra",bounds));
 assert(!Old3dsLandmark::SelectStatic(nullptr,"architecture",bounds));
 assert(!Old3dsLandmark::SelectStatic("l3r2.p3d",nullptr,bounds));
 float altered[10];memcpy(altered,bounds,sizeof(bounds));altered[3]+=.01f;
 assert(!Old3dsLandmark::SelectStatic("l3r2.p3d","architecture",altered));
 // Cached lookup must work after the profile is removed; no per-frame file access.
 assert(std::remove(argv[1])==0);
 assert(Old3dsLandmark::SelectStatic("l3r2.p3d","architecture",bounds));
 Old3dsLandmark::PreparedProfile missing;missing.Load(argv[1]);assert(missing.count==0);
 Old3dsLandmark::PreparedProfile malformed;malformed.Load(argv[2]);assert(malformed.count==0);
 for(bool selected:{false,true})for(bool originalRoot:{false,true})for(unsigned capacity:{0u,1u,2u}){
  if(originalRoot && capacity==0)continue;
  World w;w.root.mSEntityElems.capacity=capacity;Entity e;e.mOld3dsLandmark=selected;float bounds[4];memcpy(bounds,e.bounds,sizeof(bounds));w.Place(&e,originalRoot);
  assert(w.root.mSEntityElems.items.size()+w.node.mSEntityElems.items.size()==1);assert(e.refs==1 && !memcmp(bounds,e.bounds,sizeof(bounds)));
  assert(e.mpSpatialNode==(originalRoot || (selected && capacity)?&w.root:&w.node));
 }
 assert(Old3dsLandmark::rootPlaced==2 && Old3dsLandmark::rootFallback==1);
 const float distance=241,radius=51;assert(distance>120+radius);assert(distance<=120+radius+Old3dsLandmark::ExtraDistance);
}
'''.replace('BLOCK',block)
with tempfile.TemporaryDirectory(prefix='srr-landmark-') as name:
    p=Path(name);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    def fnv(data):
        h=2166136261
        for b in data:h=((h^b)*16777619)&0xffffffff
        return h
    name=fnv(b'l3r2.p3d\0architecture');geometry=fnv(struct.pack('<10f',*range(1,11)))
    record=struct.pack('<II',name,geometry);header=struct.pack('<4sIII',b'LMV1',1,1,8)
    invalid=[b'',header,header+record+b'extra',struct.pack('<4sIII',b'LMV1',2,1,8)+record,
             struct.pack('<4sIII',b'LMV1',1,449,8),struct.pack('<4sIII',b'LMV1',1,1,12)+record,
             struct.pack('<4sIII',b'LMV1',1,2,8)+record+record,
             struct.pack('<4sIII',b'LMV1',1,2,8)+struct.pack('<IIII',name,geometry+1,name,geometry)]
    for data in invalid:
        (p/'valid.bin').write_bytes(header+record);(p/'invalid.bin').write_bytes(data)
        subprocess.run([str(p/'test'),str(p/'valid.bin'),str(p/'invalid.bin')],check=True)
print('PASS: prepared profile lookup, path normalization, bounds binding, disabled/missing/malformed cases, read-once cache and bounded root placement. Existing ownership and collision bounds remain exact.')

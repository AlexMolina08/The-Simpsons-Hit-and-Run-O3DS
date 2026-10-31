#!/usr/bin/env python3
"""Exercise actual resident policy, strict metadata and authored trigger order."""
from pathlib import Path
import argparse, hashlib, struct, subprocess, sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_stream_probes import uid
root=Path(__file__).resolve().parents[3]
crypto=[] if sys.platform=='darwin' else ['-lcrypto']
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--catalog',type=Path)
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='resident-visibility-') as directory:
 p=Path(directory)
 def put(name,data):
  f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(data)
 put('main/nativebench.h','namespace NativeBench {unsigned WorldResidencyChoice();inline bool WorldResidencyOverridden(){return false;}}\n')
 put('render/DSG/StaticEntityDSG.h', '''#pragma once
using radInt64=long long;
namespace rmt {struct Sphere{struct {float x,y,z;}centre;float radius;};}
struct StaticEntityDSG{unsigned long long id;bool mOld3dsLandmark=false;unsigned short mWorldResidencyIndex=0;float radius=4;unsigned long long GetUID(){return id;}void GetBoundingSphere(rmt::Sphere* s){*s={{1,2,3},radius};}};
''')
 put('meta/zoneeventlocator.h','''#pragma once
struct ZoneEventLocator {bool entered=true;const char* load=nullptr;const char* dump=nullptr;bool GetPlayerEntered(){return entered;}int GetNumDumpZones(){return dump?1:0;}int GetNumLoadZones(){return load?1:0;}const char* GetDumpZone(int){return dump;}const char* GetLoadZone(int){return load;}};
''')
 put('libavutil/sha.h','''#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#define CC_SHA256_CTX SHA256_CTX
#define CC_SHA256_Init SHA256_Init
#define CC_SHA256_Update SHA256_Update
#define CC_SHA256_Final SHA256_Final
#endif
struct AVSHA {CC_SHA256_CTX context;};
static AVSHA* av_sha_alloc(){return (AVSHA*)malloc(sizeof(AVSHA));}
static int av_sha_init(AVSHA* s,int){return CC_SHA256_Init(&s->context)?0:-1;}
static void av_sha_update(AVSHA* s,const unsigned char* b,unsigned n){CC_SHA256_Update(&s->context,b,n);}
static void av_sha_final(AVSHA* s,unsigned char* b){CC_SHA256_Final(b,&s->context);}
''')
 put('libavutil/mem.h','static void av_free(void* p){free(p);}\n')
 event_source=(root/'code/events/eventmanager.cpp').read_text()
 event_hook=event_source[event_source.index('    // Real dispatch'):event_source.index('#endif',event_source.index('    // Real dispatch'))]
 a,b=sorted((uid('alias-a'),uid('alias-b')));terrain=uid('art\\l2_TERRA.p3d')
 put('test.cpp', f'''#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {{unsigned g_old3dsWorldResidencyChoice=3;}}
namespace NativeBench {{unsigned WorldResidencyChoice(){{return g_old3dsWorldResidencyChoice;}}}}
#include "{root/'code/main/worldresidency.cpp'}"
const int EVENT_FIRST_DYNAMIC_ZONE_START=1,EVENT_LOCATOR=100;
namespace LocatorEvent {{const int DYNAMIC_ZONE=2;}}
void Dispatch(int id,void* pEventData){{{event_hook}}}
int main(int argc,char** argv){{
 WorldResidency::Init();
 if(argc>1){{assert(g_old3dsWorldResidencyChoice==2);return 0;}}
 assert(g_old3dsWorldResidencyChoice==3);
 StaticEntityDSG a={{{a}ull}},b={{{b}ull}},unknown={{123ull}};
 assert(!WorldResidency::Visible(&a) && WorldResidency::Visible(&unknown));
 WorldResidency::Bind({terrain}ull,&a);WorldResidency::Bind({terrain}ull,&b);
 WorldResidency::InitialDemand("ART\\\\L2Z1.P3D");assert(WorldResidency::Visible(&a));assert(!WorldResidency::Visible(&b));
 WorldResidency::Gameplay(2);WorldResidency::InitialDemand("l2z2.p3d");assert(!WorldResidency::Visible(&b));
 ZoneEventLocator trigger;trigger.dump="l2z1.p3d";trigger.load="l2z2.p3d";
 WorldResidency::Trigger(&trigger,1);assert(WorldResidency::Visible(&a));
 trigger.entered=false;WorldResidency::Trigger(&trigger,0);assert(WorldResidency::Visible(&a));
 trigger.entered=true;Dispatch(999,&trigger);Dispatch(EVENT_FIRST_DYNAMIC_ZONE_START,nullptr);assert(WorldResidency::Visible(&a));Dispatch(EVENT_FIRST_DYNAMIC_ZONE_START,&trigger);assert(!WorldResidency::Visible(&a)&&WorldResidency::Visible(&b));
 WorldResidency::InitialDemand("l2z1.p3d");assert(!WorldResidency::Visible(&a));
 WorldResidency::ClearDemand();assert(!WorldResidency::Visible(&b));WorldResidency::InitialDemand("l2z2.p3d");assert(!WorldResidency::Visible(&b));
 trigger.dump="l2z2.p3d";trigger.load="l2z1.p3d";WorldResidency::Trigger(&trigger,0);assert(WorldResidency::Visible(&a)&&!WorldResidency::Visible(&b));
 WorldResidency::Unloaded({uid('l2z1.p3d')}ull);assert(WorldResidency::Visible(&a));
 b.mOld3dsLandmark=true;assert(WorldResidency::Visible(&b));b.mOld3dsLandmark=false;
 WorldResidency::Unloaded({terrain}ull);assert(!WorldResidency::Visible(&a));
 WorldResidency::InitialDemand("l2z1.p3d");assert(!WorldResidency::Visible(&a));a.radius=5;WorldResidency::Bind({terrain}ull,&a);assert(!WorldResidency::Visible(&a));a.radius=4;WorldResidency::Bind({terrain}ull,&a);assert(WorldResidency::Visible(&a));
 WorldResidency::Dump(stdout);
}}
''')
 exe=p/'test';subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_VISUAL_UPGRADE','-I'+str(p),'-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),'-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(exe),*crypto],check=True)
 path=p/'sdmc:/3ds/hitrun-dev/world-residency-04/opaque/resident-visibility.bin';path.parent.mkdir(parents=True)
 body=struct.pack('<QQ4f',a,uid('l2z1.p3d'),1,2,3,4)+struct.pack('<QQ4f',b,uid('l2z2.p3d'),1,2,3,4)
 def wire(data=body,section=terrain):return struct.pack('<4sIIIQQ',b'WRV1',1,len(data)//32,32,section,2)+hashlib.sha256(data).digest()+data
 good=wire();path.write_bytes(good);subprocess.run([str(exe)],cwd=p,check=True)
 for bad in [good[:12],good[:-1],good+b'x',good[:70]+bytes([good[70]^1])+good[71:],wire(body[32:]+body[:32]),wire(body[:32]*2),wire(body,0),wire(b''.join(struct.pack('<QQ4f',i+1,i+100,1,2,3,4) for i in range(33))),wire(body[:28]+struct.pack('<f',float('nan'))+body[32:]),wire(body[:28]+bytes(4)+body[32:])]:
  path.write_bytes(bad);subprocess.run([str(exe),'invalid'],cwd=p,check=True,stdout=subprocess.DEVNULL)
 path.unlink();subprocess.run([str(exe),'missing'],cwd=p,check=True,stdout=subprocess.DEVNULL)
 # Load all seven catalogs together. Mixed-level entities must never inherit
 # another level's mask or lose their binding when a different terrain unloads.
 multi=p/'sdmc:/3ds/hitrun-dev/world-residency-05/all';multi.mkdir(parents=True)
 ids=[uid(f'multi-level-{level}') for level in range(1,8)]
 terrains=[uid('art\\'+('L' if level in (1,4,7) else 'l')+f'{level}_TERRA.p3d') for level in range(1,8)]
 def all_files():
  for level in range(1,8):
   data=struct.pack('<QQ4f',ids[level-1],uid(f'l{level}z1.p3d'),1,2,3,4)
   header=struct.pack('<4sIIIQQ',b'WRV1',1,1,32,terrains[level-1],level)
   (multi/f'resident-visibility{level}.bin').write_bytes(header+hashlib.sha256(data).digest()+data)
 prefix=(p/'test.cpp').read_text().split('int main(int argc,char** argv)')[0].replace('g_old3dsWorldResidencyChoice=3','g_old3dsWorldResidencyChoice=4')
 program='''int main(int argc,char** argv){
 WorldResidency::Init();
 if(argc>1 && strcmp(argv[1],"unbound")){assert(g_old3dsWorldResidencyChoice==2);return 0;}
 assert(g_old3dsWorldResidencyChoice==4);
 if(argc>1 && !strcmp(argv[1],"unbound")){WorldResidency::Gameplay(1);assert(WorldResidency::Failed());auto status=WorldResidency::Status(1);assert(status.expected==1 && status.bound==0 && status.bad==1);return 0;}
 const unsigned long long ids[]={IDS},terrains[]={TERRAINS};
 StaticEntityDSG entities[7];
 for(unsigned pass=0;pass<3;++pass){
  for(unsigned i=0;i<7;++i){
   entities[i].id=ids[i];WorldResidency::Bind(terrains[i],entities+i);
   char filename[32];snprintf(filename,sizeof(filename),"ART\\\\L%uZ1.P3D",i+1);
   WorldResidency::InitialDemand(filename);WorldResidency::Gameplay(i+1);
   assert(WorldResidency::Visible(entities+i));
  }
  WorldResidency::Unloaded(terrains[0]);assert(!WorldResidency::Visible(entities));
  for(unsigned i=1;i<7;++i)assert(WorldResidency::Visible(entities+i));
  WorldResidency::Bind(terrains[0],entities);WorldResidency::InitialDemand("l1z1.p3d");assert(WorldResidency::Visible(entities));
  WorldResidency::ClearDemand();
  for(unsigned i=0;i<7;++i)assert(!WorldResidency::Visible(entities+i));
  ZoneEventLocator zone;zone.load="l4z1.p3d";WorldResidency::Trigger(&zone,0);
  assert(WorldResidency::Visible(entities+3));assert(!WorldResidency::Visible(entities+4));
  WorldResidency::Reset();
 }
 WorldResidency::Dump(stdout);
}
'''.replace('IDS',','.join(str(i)+'ull' for i in ids)).replace('TERRAINS',','.join(str(i)+'ull' for i in terrains))
 (p/'all.cpp').write_text(prefix+program);all_exe=p/'all'
 subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_VISUAL_UPGRADE','-I'+str(p),'-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),'-fsanitize=address,undefined',str(p/'all.cpp'),'-o',str(all_exe),*crypto],check=True)
 all_files();subprocess.run([str(all_exe)],cwd=p,check=True)
 subprocess.run([str(all_exe),"unbound"],cwd=p,check=True,stdout=subprocess.DEVNULL)
 (multi/'resident-visibility7.bin').unlink();subprocess.run([str(all_exe),'missing'],cwd=p,check=True,stdout=subprocess.DEVNULL)
 all_files();data=(multi/'resident-visibility1.bin').read_bytes();body=struct.pack('<QQ4f',ids[0],uid('l7z1.p3d'),1,2,3,4)
 (multi/'resident-visibility7.bin').write_bytes(struct.pack('<4sIIIQQ',b'WRV1',1,1,32,terrains[6],7)+hashlib.sha256(body).digest()+body)
 subprocess.run([str(all_exe),'cross-level-duplicate'],cwd=p,check=True,stdout=subprocess.DEVNULL)
 all_files();body=struct.pack('<QQ4f',ids[6],uid('l2z1.p3d'),1,2,3,4)
 (multi/'resident-visibility7.bin').write_bytes(struct.pack('<4sIIIQQ',b'WRV1',1,1,32,terrains[6],7)+hashlib.sha256(body).digest()+body)
 subprocess.run([str(all_exe),'wrong-level-zone'],cwd=p,check=True,stdout=subprocess.DEVNULL)
 if args.catalog:
  for level in range(1,8):
   (multi/f'resident-visibility{level}.bin').write_bytes((args.catalog/f'resident-visibility{level}.bin').read_bytes())
  header=p/'render/DSG/StaticEntityDSG.h'
  header.write_text(header.read_text().replace('float radius=4;','float radius=4,x=1,y=2,z=3;').replace('*s={{1,2,3},radius};','*s={{x,y,z},radius};'))
  full=prefix+'''int main(){
 WorldResidency::Init();assert(g_old3dsWorldResidencyChoice==4 && WorldResidency::readyMask==127);
 unsigned expected[7]={};
 for(unsigned i=0;i<WorldResidency::count;++i){
  const auto& entry=WorldResidency::entries[i];StaticEntityDSG entity;
  entity.id=entry.disk.alias;entity.x=entry.disk.sphere[0];entity.y=entry.disk.sphere[1];entity.z=entry.disk.sphere[2];entity.radius=entry.disk.sphere[3];
  WorldResidency::Bind(WorldResidency::catalogs[entry.level-1].terrain,&entity);++expected[entry.level-1];
 }
 for(unsigned i=0;i<7;++i){assert(expected[i]>0 && WorldResidency::catalogs[i].bound==expected[i] && !WorldResidency::catalogs[i].bad);WorldResidency::Gameplay(i+1);}
 // Compare the optimized lookup with a linear identity oracle. Each lookup
 // still reads current level admission, binding and demand. Reuse one object
 // while renaming it, poisoning its hint, unloading levels and resetting masks.
 unsigned comparisons=0;
 for(unsigned pass=0;pass<4;++pass){
  WorldResidency::CacheLookup(pass!=0);
  for(unsigned i=0;i<WorldResidency::count;++i){
   auto& item=WorldResidency::entries[i];
   item.bound=pass!=2;
   WorldResidency::catalogs[item.level-1].wanted=pass==1?~0u:1u<<item.zone;
   StaticEntityDSG entity;entity.id=item.disk.alias;
   const unsigned hints[]={0,1,WorldResidency::count,WorldResidency::count+1,65535};
   for(unsigned hint:hints){
    entity.mWorldResidencyIndex=hint;
    for(unsigned rename=0;rename<3;++rename){
     entity.id=rename==0?item.disk.alias:rename==1?0x123ull:WorldResidency::entries[(i+1)%WorldResidency::count].disk.alias;
     const WorldResidency::Entry* found=nullptr;
     for(unsigned j=0;j<WorldResidency::count;++j)if(WorldResidency::entries[j].disk.alias==entity.id){found=WorldResidency::entries+j;break;}
     const bool expected=!found || !WorldResidency::ResidentLevel(found->level) || (found->bound && (WorldResidency::catalogs[found->level-1].wanted&(1u<<found->zone)));
     assert(WorldResidency::Visible(&entity)==expected);++comparisons;
     assert(WorldResidency::Visible(&entity)==expected);++comparisons;
     entity.mOld3dsLandmark=true;assert(WorldResidency::Visible(&entity));entity.mOld3dsLandmark=false;
    }
   }
  }
  if(pass==1)WorldResidency::ClearDemand();
  if(pass==2)WorldResidency::Reset();
 }
 printf("PASS: %u cached/uncached identity, rename, corrupt-hint and live-state comparisons\\n",comparisons);
 WorldResidency::Reset();for(const auto& catalog:WorldResidency::catalogs)assert(!catalog.bound && !catalog.wanted);
 printf("PASS: actual seven-level catalog binds %u exact native identities and spheres\\n",WorldResidency::count);
}
'''
  (p/'full.cpp').write_text(full)
  subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_VISUAL_UPGRADE','-I'+str(p),'-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),'-fsanitize=address,undefined',str(p/'full.cpp'),'-o',str(p/'full'),*crypto],check=True)
  subprocess.run([str(p/'full')],cwd=p,check=True)
 # Release object contains no active policy and needs no engine/dependency headers.
 subprocess.run(['clang++','-std=c++17','-I'+str(root/'libs/radcore/inc'),'-I'+str(root/'code'),'-c',str(root/'code/main/worldresidency.cpp'),'-o',str(p/'release.o')],check=True)
assert 'WorldResidency::Trigger' in (root/'code/events/eventmanager.cpp').read_text()
assert 'WorldResidency::Trigger' not in (root/'code/meta/eventlocator.cpp').read_text()
assert 'WorldResidency::Trigger' not in (root/'code/render/RenderManager/RenderManager.cpp').read_text()
print('PASS: production resident policy validates metadata, honors real trigger order, ignores stale enqueues/unloads and resets bindings.')

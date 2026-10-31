#!/usr/bin/env python3
"""Exercise actual character GC during mission approach, handover and teardown."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'code/worldsim/character/charactermanager.cpp').read_text()
start=source.index('void CharacterManager::GarbageCollect(bool ignoreDist)')
end=source.index('{',start)+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
method=source[start:end]
program=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
namespace rmt {
struct Vector {float x=0,y=0,z=0;void Sub(const Vector& a,const Vector& b){x=a.x-b.x;y=a.y-b.y;z=a.z-b.z;}float Magnitude(){return std::sqrt(x*x+y*y+z*z);}};
float Abs(float f){return std::fabs(f);}
}
struct Character {
 enum Role {ROLE_UNKNOWN,ROLE_DRIVER,ROLE_REWARD,ROLE_ACTIVE_BONUS,ROLE_COMPLETED_BONUS,ROLE_PEDESTRIAN,ROLE_MISSION};
 Role role=ROLE_UNKNOWN;bool mbAllowUnload=true,ambient=false,inCar=false;
 rmt::Vector position;unsigned removedPhysics=0,addedPhysics=0,resets=0;
 void GetPosition(rmt::Vector& out){out=position;}Role GetRole(){return role;}
 bool IsAmbient(){return ambient;}void EnableAmbientDialogue(bool){}void ResetAmbientPosition(){++resets;}
 void RemoveFromPhysics(){++removedPhysics;}void AddToPhysics(){++addedPhysics;}bool IsInCar(){return inCar;}
};
struct Mission {bool bonus=false,race=false,wager=false,sunday=false;bool IsBonusMission(){return bonus;}bool IsRaceMission(){return race;}bool IsWagerMission(){return wager;}bool IsSundayDrive(){return sunday;}} mission;
struct Gameplay {bool closed=false;Mission* GetCurrentMission(){return &mission;}bool IsIrisClosed(){return closed;}} gameplay;
Gameplay* GetGameplayManager(){return &gameplay;}
struct Inventory {void RemoveSectionElements(unsigned){}void DeleteSection(unsigned){}} inventory;
namespace p3d {Inventory* inventory=&::inventory;}
struct CommandLineOptions {static bool Get(int){return false;}};
const int CLO_NO_PEDS=0,MAX_PLAYERS=1;
struct CharacterManager {
 enum {MAX_CHARACTERS=4,MAX_LOADS=4};enum {NOT_LOADED,GARBAGE,LOADING,LOADING_GARBAGE,LOADED};
 struct Load {int state=NOT_LOADED;float gracePeriod=0;unsigned section=0;};
 Character* mpCharacter[4]={};bool mLoaded[4]={},mGarbage[4]={},mGarbageCollect=true;
 char mRealModelNames[4][16]={{0},{'t','e','e','n',0}},mRealAnimNames[4][16]={{0},{'n','p','d',0}};
 Load mModelData[4],mAnimData[4];unsigned unloads=0,loads=0,removals=0;
 Character* GetCharacter(unsigned i){return mpCharacter[i];}
 void RemoveCharacter(Character* c){for(auto& item:mpCharacter)if(item==c)item=nullptr;++removals;}
 void InternalSwapData(Character*,const char* model,const char*){if(!std::strcmp(model,"npd"))++unloads;else ++loads;}
 void GarbageCollect(bool ignoreDist=false);
};
METHOD
int main(){unsigned cases=0;
 for(unsigned repetition=0;repetition<100;++repetition)
 for(int role=Character::ROLE_UNKNOWN;role<=Character::ROLE_MISSION;++role)
 for(bool allow:{false,true}){
  Character player,npc;npc.role=Character::Role(role);npc.mbAllowUnload=allow;npc.position.x=500;
  CharacterManager m;m.mpCharacter[0]=&player;m.mpCharacter[1]=&npc;m.mLoaded[1]=true;
  mission={};gameplay={};m.GarbageCollect();
#ifdef SRR2_OLD_3DS
  const bool protectedMission=role==Character::ROLE_MISSION;
#else
  const bool protectedMission=false;
#endif
  const bool suppress=role==Character::ROLE_ACTIVE_BONUS;
  const bool shouldUnload=allow&&!protectedMission;
  assert(m.unloads==unsigned(shouldUnload)&&m.mLoaded[1]==!shouldUnload);
  // Approaching must not queue a replacement for an active mission model.
  npc.position.x=100;m.GarbageCollect();
  const bool shouldReload=shouldUnload&&!suppress;
  assert(m.loads==unsigned(shouldReload)&&m.mLoaded[1]==(!shouldUnload||shouldReload));
  // A previously unloaded active mission NPC must start loading even when far.
  npc.position.x=500;m.mLoaded[1]=false;const unsigned before=m.loads;m.GarbageCollect();
  assert(m.loads==before+unsigned(protectedMission&&!suppress));
  // Objective handover keeps the same actor; finalization/cancel still removes
  // a distant mission actor through the original garbage branch.
  m.mGarbage[1]=true;m.GarbageCollect();
  if(role==Character::ROLE_ACTIVE_BONUS)assert(m.mpCharacter[1]==&npc&&!m.mGarbage[1]);
  else assert(m.mpCharacter[1]==nullptr&&m.removals==1);
  ++cases;
 }
 printf("PASS: %u actual GC cases preserve active mission models, distant reload, role policies, objective handover and teardown.\n",cases);
}
'''.replace('METHOD',method).replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='srr-mission-npc-') as temporary:
    p=Path(temporary);(p/'test.cpp').write_text(program)
    for flags in ([],['-DSRR2_OLD_3DS']):
        subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined',*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)

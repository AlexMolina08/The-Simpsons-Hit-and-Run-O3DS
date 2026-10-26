#!/usr/bin/env python3
"""Exercise benchmark entry and the production engine-sound event listener."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
bench = (root / 'code/main/nativebench.cpp').read_text()
start = bench.index(' GetAvatarManager()->PutCharacterInCar(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());')
end = bench.index('\n avatar->SetCameraTargetToVehicle(true);', start)
entry = bench[start:end]
sound = (root / 'code/sound/avatar/soundavatar.cpp').read_text()
start = sound.index('        case EVENT_GETINTOVEHICLE_END:')
end = sound.index('            break;', start) + len('            break;')
listener = sound[start:end]
fixture = r'''
#include <cassert>
#define rAssert assert
enum EventEnum {EVENT_GETINTOVEHICLE_END};
struct Character {} character,otherCharacter;
struct Vehicle {} vehicle;
struct Avatar {
 Character* GetCharacter(){return &character;}
 Vehicle* GetVehicle(){return &vehicle;}
} player;
Avatar* avatar=&player;
struct Engine {unsigned starts=0;Vehicle* last=nullptr;
 void StartCarSounds(Vehicle* v){++starts;last=v;}
};
struct SoundAvatar {
 Avatar* m_avatar=&player;bool m_isInCar=false;Engine m_vehicleSoundPlayer;
 void HandleEvent(EventEnum id,void* pEventData){switch(id){
LISTENER
 }}
} audio;
bool placed=false,portrait=false;
struct AvatarManager {void PutCharacterInCar(Character* c,Vehicle* v){
 assert(c==&character && v==&vehicle);placed=true;
}} avatarManager;
AvatarManager* GetAvatarManager(){return &avatarManager;}
struct Gameplay {Vehicle* GetCurrentVehicle(){return &vehicle;}} gameplay;
Gameplay* GetGameplayManager(){return &gameplay;}
struct Events {unsigned entries=0;
 void TriggerEvent(EventEnum e,void* data){assert(placed);++entries;audio.HandleEvent(e,data);}
} events;
Events* GetEventManager(){return &events;}
void Enter(){
ENTRY
}
int main(){
 Enter();assert(placed && events.entries==1 && audio.m_isInCar);
 assert(audio.m_vehicleSoundPlayer.starts==1 && audio.m_vehicleSoundPlayer.last==&vehicle);
 // A different character's entry must not take over this avatar's engine.
 audio.HandleEvent(EVENT_GETINTOVEHICLE_END,&otherCharacter);
 assert(audio.m_vehicleSoundPlayer.starts==1);
 // On-foot portrait captures do not start a player engine.
 placed=false;portrait=true;audio.m_isInCar=false;Enter();
 assert(placed && events.entries==1 && !audio.m_isInCar && audio.m_vehicleSoundPlayer.starts==1);
}
'''.replace('LISTENER', listener).replace('ENTRY', entry)
with tempfile.TemporaryDirectory(prefix='srr-road-audio-entry-') as directory:
    folder = Path(directory)
    (folder / 'test.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(folder / 'test.cpp'),
                    '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)
print('PASS: production benchmark entry starts the actual SoundAvatar engine path after placement, ignores other avatars and preserves on-foot portraits.')

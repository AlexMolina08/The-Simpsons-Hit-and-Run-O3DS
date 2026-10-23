#!/usr/bin/env python3
"""A missing DSP must not block movie startup on the 3DS."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/sound/soundmanager.cpp').read_text()
signature = 'bool SoundManager::IsStoppedForMovie()'
start = source.index(signature)
brace = source.index('{', start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
method = source[start:end]
fixture = r'''
#include <cassert>
bool available=true;
unsigned queries=0;
bool radSoundHal3dsAvailable(){return available;}
struct Music {bool stopped=false;bool IsStoppedForMovie(){++queries;return stopped;}} music;
struct Players {bool stopped=false;bool AreAllPlayersStopped(){++queries;return stopped;}} players;
struct Renderer {Players* GetPlayerManager(){return &players;}} renderer;
struct SoundManager {
 bool m_isMuted=false,m_stoppedForMovie=false;
 Music* m_musicPlayer=&music;Renderer* m_pSoundRenderMgr=&renderer;
 bool IsStoppedForMovie();
};
'''
main = r'''
int main(){
 SoundManager sound;
 assert(!sound.IsStoppedForMovie());
 sound.m_stoppedForMovie=true;
 assert(!sound.IsStoppedForMovie());
 music.stopped=true;
 assert(!sound.IsStoppedForMovie());
 players.stopped=true;
 assert(sound.IsStoppedForMovie());
 available=false;sound.m_stoppedForMovie=false;music.stopped=players.stopped=false;
 queries=0;
#ifdef RAD_3DS
 sound.m_musicPlayer=nullptr;sound.m_pSoundRenderMgr=nullptr;
 assert(sound.IsStoppedForMovie()&&queries==0);
#else
 assert(!sound.IsStoppedForMovie());
#endif
 sound.m_isMuted=true;assert(sound.IsStoppedForMovie());
}
'''
with tempfile.TemporaryDirectory(prefix='srr-movie-audio-') as temporary:
    path = Path(temporary)
    (path / 'test.cpp').write_text(fixture + method + main)
    for platform in ('RAD_3DS', 'RAD_PC'):
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-D' + platform,
                        str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
print('PASS: unavailable DSP bypasses audio-clock wait; working DSP still waits for music and players; PC behavior is unchanged.')

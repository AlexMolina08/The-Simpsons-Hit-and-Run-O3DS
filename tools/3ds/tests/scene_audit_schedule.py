#!/usr/bin/env python3
"""Verify the production scene-audit schedule spans short and long routes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/main/nativebench.cpp').read_text()
a=source.index('static unsigned SceneAuditIntervalMs()');b=source.index('static void SampleScene(',a)
function=source[a:b]
a=source.index('if(sceneAudit && elapsedMs>=sceneNextMs)');b=source.index('\n',a)
schedule=source[a:b]
fixture=r'''
#include <cassert>
#include <vector>
static unsigned seconds=25,elapsedMs=0,sceneNextMs=0;
static bool sceneAudit=true;
static std::vector<unsigned> samples;
void SampleScene(unsigned time){if(samples.size()<64)samples.push_back(time);}
FUNCTION
void Tick(){SCHEDULE}
int main(){
 for(unsigned duration:{5u,25u,60u})for(unsigned rate:{16u,33u,38u,50u,100u}){
  seconds=duration;elapsedMs=sceneNextMs=0;samples.clear();sceneAudit=true;
  while(elapsedMs<duration*1000){Tick();elapsedMs+=rate;}
  assert(!samples.empty()&&samples.size()<=64&&samples.front()==0);
  assert(samples.back()+SceneAuditIntervalMs()+rate>=duration*1000);
  if(duration<=25)assert(SceneAuditIntervalMs()==500);
  for(unsigned i=1;i<samples.size();++i)assert(samples[i]-samples[i-1]>=SceneAuditIntervalMs());
 }
 // Irregular stalls must not create catch-up probes or exhaust slots early.
 seconds=60;elapsedMs=sceneNextMs=0;samples.clear();unsigned frame=0;
 while(elapsedMs<60000){Tick();elapsedMs+=(++frame%61==0)?240:38;}
 assert(samples.size()<=64&&samples.back()>=58000);
 // Timing-only runs perform no scene queries.
 sceneAudit=false;elapsedMs=sceneNextMs=0;samples.clear();
 for(unsigned i=0;i<2000;++i){Tick();elapsedMs+=33;}
 assert(samples.empty()&&sceneNextMs==0);
}
'''.replace('FUNCTION',function).replace('SCHEDULE',schedule)
with tempfile.TemporaryDirectory(prefix='srr-scene-audit-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production audit cadence covers 5/25/60-second routes at 10-60 FPS and irregular stalls within 64 slots; clean timing runs issue no scene query.')

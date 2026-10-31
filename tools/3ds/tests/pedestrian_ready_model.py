#!/usr/bin/env python3
"""Exercise actual ready-model selection without increasing pool or model limits."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/worldsim/ped/pedestrianmanager.cpp').read_text()
start = source.index('int PedestrianManager::SelectReadyModel(')
end = source.index('void PedestrianManager::GetPopulationCounts(', start)
method = source[start:end]
code = r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <initializer_list>
struct DListArray { static const int MAX_ELEMS=7; };
struct Characters {
    bool ready[4]={};unsigned queries=0;
    bool IsModelLoaded(const char* name) {
        ++queries;return name[0]>='a' && name[0]<='d' && ready[name[0]-'a'];
    }
} characters;
Characters* GetCharacterManager(){return &characters;}
struct PedestrianManager {
    struct PedestrianStruct { const char* modelName; } slots[7];
    struct Pool {
        std::vector<PedestrianStruct*> slots;
        int GetHead(){return slots.empty()?-1:0;}
        int GetNextOf(int i){return i+1<int(slots.size())?i+1:-1;}
        void* GetDataAt(int i){return slots.at(i);}
    } mFreePeds;
    struct Model {bool free=false;int currCount=0,maxCount=2;} mModels[4];
    int GetModelIDByName(const char* name) {
        return name[0]>='a' && name[0]<='d'?name[0]-'a':-1;
    }
    int SelectReadyModel(int);
    void Free(std::initializer_list<const char*> names){
        mFreePeds.slots.clear();int i=0;
        for(auto name:names){slots[i].modelName=name;mFreePeds.slots.push_back(&slots[i++]);}
    }
};
METHOD
int main(){
    PedestrianManager p;
    // Empty or cold pools retain the normal path, allowing initial population.
    assert(p.SelectReadyModel(2)==2);
    p.Free({"a","b"});assert(p.SelectReadyModel(2)==2);
    characters.ready[0]=true;
    assert(p.SelectReadyModel(2)==0);
    // Keep the preferred ready model even when another ready slot comes first.
    characters.ready[1]=true;assert(p.SelectReadyModel(1)==1);
    // Never use models outside the current group or beyond per-model counts.
    p.mModels[0].free=true;assert(p.SelectReadyModel(2)==1);
    p.mModels[1].currCount=2;assert(p.SelectReadyModel(2)==2);
    p.mModels[0].free=false;characters.ready[0]=false;
    p.Free({"npd","retired","a","b"});assert(p.SelectReadyModel(2)==2);
    // No cached slot means no reuse, even if a globally loaded model exists.
    characters.ready[2]=true;p.Free({"a"});assert(p.SelectReadyModel(1)==1);
    // Selection neither changes counts nor allocates a new pedestrian/model.
    characters.ready[0]=true;p.Free({"a","a","a","a","a","a","a"});
    PedestrianManager::Model before[4];std::memcpy(before,p.mModels,sizeof(before));
    characters.queries=0;assert(p.SelectReadyModel(2)==0);
    assert(characters.queries<=7 && !std::memcmp(before,p.mModels,sizeof(before)));
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='pedestrian-ready-') as td:
    path = Path(td)
    (path / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual bounded ready-model choice preserves preferred cache, group/count limits and cold-load fallback; no new slots.')

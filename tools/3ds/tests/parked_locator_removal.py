#!/usr/bin/env python3
"""Exercise the actual parked-locator removal seam, ownership and fallback."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/worldsim/parkedcars/parkedcarmanager.cpp').read_text()
start = source.index('static void RemoveParkedLocator(')
end = source.index('{', start) + 1
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
method = source[start:end]
code = r'''
#include <cassert>
#include <vector>
#include <cstdio>
#include <algorithm>
using tUID=unsigned long long;
bool g_old3dsParkedSectionRemove=true;
unsigned parkedSectionRemovals=0,parkedRemovalFallbacks=0;
struct IRefCount {
 static unsigned live;unsigned references=0;
 IRefCount(){++live;}virtual ~IRefCount(){--live;}
 void AddRef(){++references;}void Release(){assert(references);if(!--references)delete this;}
};unsigned IRefCount::live=0;
struct CarStartLocator:IRefCount {
 tUID uid;explicit CarStartLocator(tUID id):uid(id){}tUID GetUID(){return uid;}
};
struct tEntityTable {
 struct Entry {tUID key;IRefCount* object;};
 std::vector<Entry> entries;unsigned removes=0;
 ~tEntityTable(){for(auto e:entries)e.object->Release();}
 void Store(tUID key,IRefCount* object){object->AddRef();entries.push_back({key,object});}
 void Remove(IRefCount* object){
  ++removes;
  auto it=std::find_if(entries.begin(),entries.end(),[&](Entry e){return e.object==object;});
  if(it!=entries.end()){entries.erase(it);object->Release();}
 }
};
struct radLoadHashedStore {
 struct Iterator {
  tEntityTable* table;tUID key=0;unsigned cursor=0;
  explicit Iterator(tEntityTable* value):table(value){}
  IRefCount* First(tUID value){key=value;cursor=0;return Next();}
  IRefCount* Next(){while(cursor<table->entries.size()){
   auto e=table->entries[cursor++];if(e.key==key)return e.object;
  }return nullptr;}
 };
};
struct Inventory {
 tEntityTable sections[8];unsigned globalRemoves=0;
 tEntityTable* GetSection(tUID uid){return uid>=1&&uid<=8?sections+uid-1:nullptr;}
 void Remove(IRefCount* object){++globalRemoves;for(auto& section:sections)section.Remove(object);}
};
namespace p3d {Inventory* inventory=nullptr;}
METHOD
int main(){
 for(unsigned repetition=0;repetition<64;++repetition)for(unsigned mode=0;mode<7;++mode){
  for(bool fast:{false,true}){
#ifndef SRR2_OLD3DS_NATIVE_BENCH
   if(!fast)continue;
#endif
   Inventory inventory;p3d::inventory=&inventory;
   for(auto& section:inventory.sections)for(unsigned i=0;i<128;++i)
    section.Store(10000+i,new CarStartLocator(10000+i));
   g_old3dsParkedSectionRemove=fast;parkedSectionRemovals=parkedRemovalFallbacks=0;
   auto* locator=new CarStartLocator(42);locator->AddRef();
   tUID expectedSection=3;unsigned actualSection=2;
   if(mode==1)expectedSection=99;
   if(mode==4||mode==6)actualSection=5;
   if(mode!=5)inventory.sections[actualSection].Store(mode==2?99:42,locator);
   CarStartLocator* collision=nullptr;
   if(mode==3||mode==6){
    collision=new CarStartLocator(42);collision->AddRef();
    inventory.sections[2].entries.insert(inventory.sections[2].entries.begin(),{42,collision});collision->AddRef();
   }
   RemoveParkedLocator(locator,expectedSection);
   assert(locator->references==1);
   if(collision)assert(collision->references==2);
   const bool expectedFast=fast&&(mode==0||mode==3);
   assert(inventory.globalRemoves==unsigned(!expectedFast));
   for(unsigned i=0;i<8;++i)assert(inventory.sections[i].removes==(expectedFast?unsigned(i==2):1u));
#ifdef SRR2_OLD3DS_NATIVE_BENCH
   assert(parkedSectionRemovals==unsigned(expectedFast));
   assert(parkedRemovalFallbacks==unsigned(!expectedFast));
#endif
   locator->Release();if(collision)collision->Release();
  }
  assert(!IRefCount::live);
 }
 puts("PASS: actual removal preserves references and hash collisions; missing section, changed identity and wrong owner use original fallback; unrelated resident sections are untouched.");
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='srr-parked-removal-') as temporary:
    folder = Path(temporary)
    (folder / 'test.cpp').write_text(code)
    for flags in ([], ['-DSRR2_OLD3DS_NATIVE_BENCH']):
        subprocess.run(['clang++', '-std=c++14', '-O2', '-fsanitize=address,undefined',
                        *flags, str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
        subprocess.run([str(folder / 'test')], check=True)

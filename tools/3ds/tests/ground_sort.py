#!/usr/bin/env python3
"""Check production ground sort hints without changing shader or entity identity."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/render/DSG/IEntityDSG.cpp').read_text()
def method(text):
    start=text.index('tShader* IEntityDSG::Process(');opening=text.index('{',start);end=opening+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
# Keep the original callback contract independent of the current Git HEAD.
reference="""tShader* IEntityDSG::Process(tShader* pShader) {
 if(mShaderName != "__none__" && mShaderName != pShader->GetNameObject())
  mShaderName="__none__";
 else mShaderName=pShader->GetName();
 return pShader;
}"""
fixture=r'''
#define SRR2_OLD3DS_VISUAL_UPGRADE
#define SRR2_OLD3DS_NATIVE_BENCH
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <vector>
using tUID=std::uint64_t;
static tUID key(const char* p){tUID value=0;while(*p)value=(value*65599)^static_cast<unsigned char>(*p++);return value;}
struct tName {tUID uid=key("__none__");tName()=default;tName(const char* p):uid(key(p)){}explicit tName(tUID v):uid(v){}void SetUID(tUID v){uid=v;}tUID GetUID()const{return uid;}bool operator!=(const tName& b)const{return uid!=b.uid;}bool operator!=(const char* p)const{return uid!=key(p);}tName& operator=(const char* p){uid=key(p);return *this;}};
struct picaMat {bool world=false;bool WorldSurface()const{return world;}};
struct tShader {tName name;picaMat material;const tName& GetNameObject()const{return name;}bool retained=false;const char* GetName()const{return retained?"material":"NO_NAME_TEXT";}picaMat* GetShader(){return &material;}};
namespace picaVisual {bool enabled=false;unsigned groundBatchNames=0;bool GroundBatch(){return enabled;}}
namespace Candidate {struct IEntityDSG {tName mShaderName;tShader* Process(tShader*);};CANDIDATE}
namespace Reference {struct IEntityDSG {tName mShaderName;tShader* Process(tShader*);};REFERENCE}
int main(){unsigned random=19;
 for(bool retained:{false,true})for(unsigned i=0;i<100000;++i){Candidate::IEntityDSG a;Reference::IEntityDSG b;
  picaVisual::enabled=false;
  for(unsigned j=0;j<4;++j){random=random*1664525u+1013904223u;tShader material{tName(retained?key("material"):tUID(random)),{bool(random&1)},retained};const tUID original=material.name.uid;
   assert(a.Process(&material)==&material && b.Process(&material)==&material);assert(a.mShaderName.uid==b.mShaderName.uid);assert(material.name.uid==original);
  }
 }
 picaVisual::enabled=true;
 struct Item {tUID key;unsigned identity;bool world;};std::vector<Item> before,after;
 for(unsigned i=0;i<16;++i){tShader material{tName(tUID(i+1)*0x10000011u),{bool(i%2)}};const tUID original=material.name.uid;Candidate::IEntityDSG entity;assert(entity.Process(&material)==&material);assert(material.name.uid==original);
  assert(entity.mShaderName.uid==(material.material.world?(original&0xffffffu):original));before.push_back({original,i,material.material.world});after.push_back({entity.mShaderName.uid,i,material.material.world});
 }
 auto order=[](const Item&a,const Item&b){return a.key<b.key;};std::sort(before.begin(),before.end(),order);std::sort(after.begin(),after.end(),order);
 auto transitions=[](const std::vector<Item>& list){unsigned n=0;for(unsigned i=1;i<list.size();++i)n+=list[i].world!=list[i-1].world;return n;};assert(transitions(before)==15 && transitions(after)==1);
 std::vector<unsigned> ids;for(auto& item:after)ids.push_back(item.identity);std::sort(ids.begin(),ids.end());for(unsigned i=0;i<16;++i)assert(ids[i]==i);
 assert(picaVisual::groundBatchNames==8);
}
'''.replace('CANDIDATE',method(source)).replace('REFERENCE',method(reference))
with tempfile.TemporaryDirectory(prefix='srr-ground-sort-') as name:
    p=Path(name);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: 200000 production callback sequences match legacy behavior with retained and stripped release names when disabled; opt-in ground hints preserve shader identities and draw membership, grouping a mixed synthetic sort from 15 entry changes to one. Hardware timing remains required.')

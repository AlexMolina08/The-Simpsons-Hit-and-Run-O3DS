#!/usr/bin/env python3
"""Check production material sorting with retained and stripped release names."""
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
namespace Candidate {struct IEntityDSG {tName mShaderName;tShader* Process(tShader*);};CANDIDATE}
namespace Reference {struct IEntityDSG {tName mShaderName;tShader* Process(tShader*);};REFERENCE}
int main(){unsigned random=19;
 for(bool retained:{false,true})for(unsigned i=0;i<100000;++i){Candidate::IEntityDSG a;Reference::IEntityDSG b;
  for(unsigned j=0;j<4;++j){random=random*1664525u+1013904223u;tShader material{tName(retained?key("material"):tUID(random)),{bool(random&1)},retained};const tUID original=material.name.uid;
   assert(a.Process(&material)==&material && b.Process(&material)==&material);assert(a.mShaderName.uid==b.mShaderName.uid);assert(material.name.uid==original);
  }
 }

}
'''.replace('CANDIDATE',method(source)).replace('REFERENCE',method(reference))
with tempfile.TemporaryDirectory(prefix='srr-ground-sort-') as name:
    p=Path(name);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: 200000 production material callback sequences match legacy ordering with retained and stripped release names; shader identities remain unchanged.')

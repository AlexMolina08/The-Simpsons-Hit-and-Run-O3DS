#!/usr/bin/env python3
"""Exercise the production prepared shader path against ordered legacy callbacks."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[3]
helper=root/'libs/pure3d/p3d/anim/vehicleshaderbindings.hpp'
code=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <functional>
#include <new>
static int failArrayAllocation=-1;
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept
{
 if(failArrayAllocation==0)return nullptr;
 if(failArrayAllocation>0)--failArrayAllocation;
 try{return ::operator new[](size);}catch(...){return nullptr;}
}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}
struct tShader {int value;};
struct tDrawable {
 struct ShaderCallback {virtual tShader* Process(tShader*)=0;};
 unsigned refs=0,legacy=0;
 virtual ~tDrawable(){assert(!refs);}
 void AddRef(){++refs;}void Release(){assert(refs);--refs;}
 virtual void ProcessShaders(ShaderCallback&)=0;
};
struct tPrimGroup {tShader* shader;tShader* GetShader(){return shader;}void SetShader(tShader* s){shader=s;}};
struct tGeometry:tDrawable {
 unsigned revision=1;std::vector<tPrimGroup*> groups;
 unsigned ShaderBindingRevision()const{return revision;}
 int GetNumPrimGroup(){return int(groups.size());}
 tPrimGroup* GetPrimGroup(unsigned n){return groups.at(n);}
 void SetPrimGroup(unsigned n,tPrimGroup* p){if(revision!=~0u)++revision;groups.at(n)=p;}
 void ProcessShaders(ShaderCallback& cb){++legacy;for(unsigned i=0;i<groups.size();++i){tShader* before=groups[i]->GetShader();tShader* after=cb.Process(before);if(after!=before)groups[i]->SetShader(after);}}
};
struct Wrapped:tDrawable {tGeometry* body=nullptr;void ProcessShaders(ShaderCallback& cb){++legacy;body->ProcessShaders(cb);}};
struct tCompositeDrawable {
 struct Element{tDrawable* drawable;tDrawable* GetDrawable(){return drawable;}};
 std::vector<Element*> elements;
 unsigned GetNumDrawableElement(){return elements.size();}
 Element* GetDrawableElement(unsigned i){return elements.at(i);}
};
'''+f'#include "{helper}"\n'+r'''
struct Callback:tDrawable::ShaderCallback {
 std::vector<int> trace;unsigned calls=0;tShader* replacement=nullptr;
 std::function<void(unsigned)> mutation;
 tShader* Process(tShader* before){trace.push_back(before->value);++calls;if(mutation)mutation(calls);return replacement?replacement:before;}
};
int main(){
 unsigned fixtures=0;
 for(unsigned n=0;n<10000;++n){
  tShader shaders[8];for(int i=0;i<8;++i)shaders[i].value=i;
  tPrimGroup a[4],b[4];for(int i=0;i<4;++i)a[i].shader=b[i].shader=&shaders[(n+i)%8];
  tGeometry original,prepared;for(int i=0;i<4;++i){original.groups.push_back(a+i);prepared.groups.push_back(b+i);}
  Wrapped ow,pw;ow.body=&original;pw.body=&prepared;
  tCompositeDrawable::Element e0{&prepared},e1{&pw},e2{&prepared};
  tCompositeDrawable composite;composite.elements={&e0,&e1,nullptr,&e2};
  {
   tPreparedVehicleShaders bindings;assert(bindings.Prepare(composite));assert(prepared.refs==2&&pw.refs==1);
   for(unsigned pass=0;pass<4;++pass){
    Callback ref,candidate;
    if(pass==1)ref.replacement=candidate.replacement=&shaders[n%8];
    for(unsigned index=0;index<4;++index){
     auto* element=composite.GetDrawableElement(index);if(!element)continue;
     (index==1?static_cast<tDrawable*>(&ow):&original)->ProcessShaders(ref);
     bindings.Process(index,element->GetDrawable(),candidate);
    }
    assert(ref.trace==candidate.trace);
    for(int i=0;i<4;++i)assert(a[i].shader==b[i].shader);
   }
   assert(bindings.preparedCalls==8&&bindings.legacyCalls==4&&bindings.slotCalls==32);
   // Structural replacement invalidates the entry; shader replacement did not.
   tPrimGroup ar{&shaders[6]},br{&shaders[6]};original.SetPrimGroup(1,&ar);prepared.SetPrimGroup(1,&br);
   Callback ref,candidate;original.ProcessShaders(ref);bindings.Process(0,&prepared,candidate);assert(ref.trace==candidate.trace);
   // Replacing an element's drawable must not dereference stale prepared slots.
   e0.drawable=&pw;Callback wrapped;bindings.Process(0,e0.drawable,wrapped);assert(wrapped.trace.size()==4);
   e0.drawable=&prepared;
   original.SetPrimGroup(1,a+1);prepared.SetPrimGroup(1,b+1);
  }
  assert(!prepared.refs&&!pw.refs);
  // A callback can mutate a current or future slot. Resume the legacy live loop.
  original.revision=prepared.revision=1;
  for(int i=0;i<4;++i)a[i].shader=b[i].shader=&shaders[i];
  tPrimGroup ar{&shaders[5]},br{&shaders[5]};
  {
   tPreparedVehicleShaders bindings;assert(bindings.Prepare(composite));Callback ref,candidate;
   ref.replacement=candidate.replacement=&shaders[7];
   unsigned target=n%4;
   ref.mutation=[&](unsigned call){if(call==1)original.SetPrimGroup(target,&ar);};
   candidate.mutation=[&](unsigned call){if(call==1)prepared.SetPrimGroup(target,&br);};
   original.ProcessShaders(ref);bindings.Process(0,&prepared,candidate);assert(ref.trace==candidate.trace);
   for(unsigned i=0;i<4;++i)assert(original.GetPrimGroup(i)->shader==prepared.GetPrimGroup(i)->shader);
  }
  original.SetPrimGroup(n%4,a+n%4);prepared.SetPrimGroup(n%4,b+n%4);
  // Saturated revisions cannot reuse bindings.
  prepared.revision=~0u;
  {tPreparedVehicleShaders bindings;assert(bindings.Prepare(composite));Callback cb;bindings.Process(0,&prepared,cb);assert(bindings.preparedCalls==0&&bindings.legacyCalls==1);}
  ++fixtures;
 }
 // Capacity failure unwinds references acquired before failure.
 tShader shader{1};tPrimGroup group{&shader};tGeometry large;large.groups.assign(257,&group);
 tCompositeDrawable::Element element{&large};tCompositeDrawable composite;composite.elements={&element};
 {tPreparedVehicleShaders bindings;assert(!bindings.Prepare(composite));}assert(!large.refs);
 large.groups.resize(2);composite.elements={&element};
 for(int failed=0;failed<2;++failed){failArrayAllocation=failed;{tPreparedVehicleShaders bindings;assert(!bindings.Prepare(composite));}assert(!large.refs);}
 failArrayAllocation=-1;
 composite.elements.assign(65,&element);{tPreparedVehicleShaders bindings;assert(!bindings.Prepare(composite));}assert(!large.refs);
 printf("PASS: %u production binding fixtures; ordered duplicate callbacks, live shader/group/element replacement, wrapper fallback, mid-callback mutation, saturation, reference lifetime, allocation and capacity failure. Not a native speed measurement.\n",fixtures);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-vehicle-bindings-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

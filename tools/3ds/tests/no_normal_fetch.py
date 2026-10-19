#!/usr/bin/env python3
"""Exercise production static packing, expansion, attribute layout and submission."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
namespace={ '__file__':str(Path(__file__).with_name('skin_stream.py')) }
base=Path(__file__).with_name('skin_stream.py').read_text()
exec(base.split('with tempfile.TemporaryDirectory')[0],namespace)
source=namespace['source'];extract=namespace['extract']
prelude=namespace['prelude'].replace('struct C3D_BufInfo{};', '''static unsigned stride,attributes,permutation;
struct C3D_BufInfo{};''').replace('void BufInfo_Add(C3D_BufInfo*,const void*,unsigned,unsigned,unsigned){}','void BufInfo_Add(C3D_BufInfo*,const void*,unsigned s,unsigned a,unsigned p){stride=s;attributes=a;permutation=p;}')
prelude=prelude.replace('struct picaContext {','''static int layout;static unsigned fixedWrites;using C3D_AttrInfo=int;
void C3D_SetAttrInfo(int* p){layout=*p;}
void C3D_FixedAttribSet(int id,float x,float y,float z,float w){assert(id==1&&x==0&&y==0&&z==1&&w==0);++fixedWrites;}
struct picaContext {
 int attrInfo=1,noNormalAttrInfo=2;bool noNormalLayout=false;
 void SetNoNormalLayout(bool);''')
prelude=prelude.replace(' picaContext* context;unsigned char* buffer;', ''' bool noNormalPacked=false,noNormalAllowed=false,uploadPending=false;unsigned vertexType=0;
 void ExpandNoNormal();void EndUpload();
 picaContext* context;unsigned char* buffer;''')
prelude='#include <pddi/pica/nonormal.hpp>\nconst unsigned PDDI_V_NORMAL=16;\n'+prelude
methods=namespace['methods']+'\n'+extract('void picaContext::SetNoNormalLayout(')+'\n'+extract('void picaPrimBuffer::ExpandNoNormal()')+'\n'+extract('void picaPrimBuffer::EndUpload()')
tests=r'''
void testPacking(){
 unsigned random=19;
 for(unsigned count=1;count<=2048;++count){
  std::vector<unsigned char> data(count*36+16,0xab),reference(data.size());
  for(unsigned i=0;i<count;++i){
   for(unsigned b=0;b<36;++b){random=random*1664525+1013904223;data[i*36+b]=random>>24;}
   const float n[]={0,0,1};memcpy(data.data()+i*36+12,n,12);
  }
  reference=data;picaNoNormal::Pack(data.data(),count);
  for(unsigned i=0;i<count;++i){assert(!memcmp(data.data()+i*24,reference.data()+i*36,12));assert(!memcmp(data.data()+i*24+12,reference.data()+i*36+24,12));}
  assert(!memcmp(data.data()+count*36,reference.data()+count*36,16));
  picaNoNormal::Expand(data.data(),count);assert(data==reference);
 }
 unsigned short indices[]={0,1,2};std::vector<unsigned char> data(3*36+16,0xab);
 for(unsigned i=0;i<3;++i){const float n[]={0,0,1};memcpy(data.data()+i*36+12,n,12);}
 const auto original=data;picaContext c;picaPrimBuffer b;b.context=&c;b.buffer=data.data();b.indices=indices;b.noNormalAllowed=true;b.uploadPending=true;
 b.EndUpload();assert(b.noNormalPacked&&!b.uploadPending);g_picaState.bufInfoSet=false;b.Display();
 assert(layout==2&&fixedWrites&&stride==24&&attributes==3&&permutation==0x320);
 b.ExpandNoNormal();assert(data==original);b.Display();assert(layout==1&&stride==36&&attributes==4&&permutation==0x3210);
 b.vertexType=PDDI_V_NORMAL;b.uploadPending=true;b.EndUpload();assert(!b.noNormalPacked);
 b.vertexType=0;b.noNormalAllowed=false;b.uploadPending=true;b.EndUpload();assert(!b.noNormalPacked);
 b.noNormalAllowed=true;b.uploadPending=true;b.EndUpload();assert(b.noNormalPacked);
 indices[2]=65535;b.elemState=-1;b.drawPrepared=false;b.Display();assert(!b.noNormalPacked&&drawBase==b.expanded&&layout==1&&stride==36);free(b.expanded);
 puts("PASS: 2,098,176 vertices round-trip byte-exact, index guards, default normal, static eligibility, invalid-index expansion and 24/36-byte draw layout switching");
}
'''
tests+=namespace['tests'].replace('int main(){','int main(){testPacking();draws=count=flushCount=0;flushed.clear();g_picaState.bufInfoSet=false;').replace('picaPrimBuffer b{&c,legacy.data()};','picaPrimBuffer b;b.context=&c;b.buffer=legacy.data();')
with tempfile.TemporaryDirectory(prefix='srr-no-normal-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests)
 for mode in ['reference','prepared']:
  cmd=['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_NO_NORMAL_FETCH','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')]
  if mode=='prepared':cmd+=['-DSRR2_OLD3DS_DRAW_PREP']
  subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)

#!/usr/bin/env python3
"""Compare production PICA parameter dispatch and unchanged-write elision."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
base=(root/'libs/pure3d/pddi/base/baseshader.cpp').read_text();mat=(root/'libs/pure3d/pddi/pica/material.cpp').read_text();shade=(root/'libs/pure3d/pddi/pddishade.hpp').read_text()
macro=base[base.index('#define DISPATCH'):base.index('bool pddiBaseShader::SetTexture')]
def method(text,signature):
 start=text.index(signature);pos=text.index('{',start)+1;depth=1
 while depth:depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
 return text[start:pos]+'\n'
base_methods=method(base,'bool pddiBaseShader::SetInt(')+method(base,'bool pddiBaseShader::SetColour(')
handlers=mat[mat.index('void picaMat::SetUVMode('):mat.index('int picaMat::CountDevPasses(')]
params='\n'.join(re.findall(r'^const unsigned PDDI_SP_.*?;',shade,re.M))
int_table=re.search(r'pddiShadeIntTable picaMat::intTable\[\].*?\n};',mat,re.S)[0]
colour_table=re.search(r'pddiShadeColourTable picaMat::colourTable\[\].*?\n};',mat,re.S)[0]
new=mat[mat.index('#ifdef SRR2_OLD3DS_MATERIAL_WRITES'):mat.index('picaMat::picaMat(')]
new=new.replace('extern "C" void picaDumpMaterialWrites()','void picaDumpMaterialWrites()')
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <array>
#define SRR2_OLD3DS_DRAW_PREP
#define PDDI_FOURCC(a,b,c,d) (unsigned(a)|(unsigned(b)<<8)|(unsigned(c)<<16)|(unsigned(d)<<24))
'''+params+r'''
using pddiUVMode=int;using pddiFilterMode=int;using pddiBlendMode=int;using pddiCompareMode=int;using pddiShadeMode=int;
struct pddiColour {unsigned c=0;int Alpha()const{return c>>24;}void SetAlpha(int a){c=(c&0xffffffu)|(unsigned(a)<<24);}};
struct picaTextureEnv {
 int uvMode=0,filterMode=0,shadeMode=0;bool twoSided=false,lit=false;
 pddiColour ambient,diffuse,specular,emissive;
 float shininess=0,alphaRef=.5f;int alphaBlendMode=0,alphaCompareMode=0,writeMask=0;bool alphaTest=false;
};
'''
classes=r'''
class pddiBaseShader;
struct pddiShadeIntTable {unsigned param;void(pddiBaseShader::*func)(int);};
struct pddiShadeFloatTable {unsigned param;void(pddiBaseShader::*func)(float);};
struct pddiShadeColourTable {unsigned param;void(pddiBaseShader::*func)(pddiColour);};
#define SHADE_INT(x) static_cast<void(pddiBaseShader::*)(int)>(x)
#define SHADE_COLOUR(x) static_cast<void(pddiBaseShader::*)(pddiColour)>(x)
class pddiBaseShader {
public:
 static unsigned newUID,currentUID;unsigned uid=0;unsigned passes=0,renders=0;virtual ~pddiBaseShader()=default;
 virtual bool SetInt(unsigned,int);virtual bool SetColour(unsigned,pddiColour);
 virtual pddiShadeIntTable* GetIntTable()=0;virtual pddiShadeColourTable* GetColourTable()=0;
 pddiShadeFloatTable* GetFloatTable(){return nullptr;}
 void SetMaterial(){if(uid!=currentUID){currentUID=uid;++passes;}++renders;}
};
unsigned pddiBaseShader::newUID=1,pddiBaseShader::currentUID=~0u;
class picaMat:public pddiBaseShader {
public:
 picaTextureEnv texEnv[1];int pass=0;bool dirty=true,nativeDirty=true,coloursValid=false;
 static pddiShadeIntTable intTable[];static pddiShadeColourTable colourTable[];
 pddiShadeIntTable* GetIntTable()override{return intTable;}pddiShadeColourTable* GetColourTable()override{return colourTable;}
#ifdef SRR2_OLD3DS_MATERIAL_WRITES
 bool SetInt(unsigned,int)override;bool SetColour(unsigned,pddiColour)override;
#endif
'''+''.join(' void '+name+'('+args+');\n' for name,args in re.findall(r'void picaMat::(\w+)\(([^()]*)\)',handlers))+r'''
};
'''
common=classes+macro+base_methods+int_table+colour_table+handlers
source=prelude+'namespace Reference {\n'+common+'}\n#define SRR2_OLD3DS_MATERIAL_WRITES\nnamespace Optimized {\n'+common+new+'}\n'+r'''
int main(){
 Reference::picaMat a[2];Optimized::picaMat b[2];unsigned random=173,checked=0;
 auto next=[&](){return random=random*1664525u+1013904223u;};
 const unsigned integers[]={PDDI_SP_BLENDMODE,PDDI_SP_EMISSIVEALPHA,PDDI_SP_ISLIT,PDDI_SP_TWOSIDED,PDDI_SP_ALPHATEST,PDDI_SP_COLOURWRITE,PDDI_SP_NULL};
 const unsigned colours[]={PDDI_SP_AMBIENT,PDDI_SP_DIFFUSE,PDDI_SP_SPECULAR,PDDI_SP_EMISSIVE,PDDI_SP_ENVBLEND};
 for(unsigned n=0;n<120000;++n){
  unsigned which=(n/7)&1;auto& original=a[which];auto& optimized=b[which];
  bool left,right;
  if(n%3){
   unsigned parameter=integers[next()%7];int value=parameter==PDDI_SP_EMISSIVEALPHA ? int(next()%600)-10 : int(next()%8);
   if(n%11==0)parameter=PDDI_SP_EMISSIVEALPHA,value=255;
   for(unsigned repeat=0;repeat<3;++repeat){left=original.SetInt(parameter,value);right=optimized.SetInt(parameter,value);assert(left==right);}
  }else{
   unsigned parameter=colours[next()%5];pddiColour value;value.c=next();
   for(unsigned repeat=0;repeat<3;++repeat){left=original.SetColour(parameter,value);right=optimized.SetColour(parameter,value);assert(left==right);}
  }
  const auto& x=original.texEnv[0];const auto& y=optimized.texEnv[0];
  assert(x.uvMode==y.uvMode && x.filterMode==y.filterMode && x.shadeMode==y.shadeMode && x.twoSided==y.twoSided && x.lit==y.lit);
  assert(x.ambient.c==y.ambient.c && x.diffuse.c==y.diffuse.c && x.specular.c==y.specular.c && x.emissive.c==y.emissive.c);
  assert(x.shininess==y.shininess && x.alphaRef==y.alphaRef && x.alphaBlendMode==y.alphaBlendMode && x.alphaCompareMode==y.alphaCompareMode && x.writeMask==y.writeMask && x.alphaTest==y.alphaTest);
  assert(original.dirty==optimized.dirty && original.nativeDirty==optimized.nativeDirty && original.coloursValid==optimized.coloursValid);++checked;
  original.SetMaterial();optimized.SetMaterial();
  // Repeated character broadcasts leave an already current shader current.
  for(unsigned repeat=0;repeat<3;++repeat){
   original.SetInt(PDDI_SP_BLENDMODE,1);optimized.SetInt(PDDI_SP_BLENDMODE,1);
   original.SetInt(PDDI_SP_EMISSIVEALPHA,255);optimized.SetInt(PDDI_SP_EMISSIVEALPHA,255);
   original.SetMaterial();optimized.SetMaterial();
  }
  if(n%31==0){Reference::pddiBaseShader::currentUID=Optimized::pddiBaseShader::currentUID=~0u;original.SetMaterial();optimized.SetMaterial();}
  // Unchanged texture writes still force the original UID path (not overridden).
  if(n%37==0){original.uid=++Reference::pddiBaseShader::newUID;optimized.uid=++Optimized::pddiBaseShader::newUID;original.SetMaterial();optimized.SetMaterial();}
 }
 unsigned before=0,after=0;
 for(unsigned i=0;i<2;++i){before+=a[i].passes;after+=b[i].passes;assert(a[i].renders==b[i].renders);}
 assert(after<before);assert(Optimized::picaMaterialWriteReuses>100000);
 printf("PASS: %u production material-state comparisons; numeric/colour dispatch, emissive sibling alpha, unsupported parameters, interleaved materials and explicit invalidations. Model pass setups %u reference / %u optimized; PreRender counts preserved. Not native performance evidence.\n",checked,before,after);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-material-writes-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-const-variable','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

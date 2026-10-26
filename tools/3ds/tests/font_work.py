#!/usr/bin/env python3
"""Compare production font passes with bounded glyph reuse enabled and disabled."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/p3d/texturefont.cpp').read_text()
a=source.index('tTextureFont::DisplayLine(');b=source.index('\nint \ntTextureFont::FindGlyph(',a)
method='void '+source[a:b]
code=r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include <p3d/fontwork.hpp>
using P3D_UNICODE=char16_t;
#define O3_MAIN_SCOPE(x) ((void)0)
namespace p3dFontWork{bool enabled=false;unsigned preparedLines=0,preparedGlyphs=0,longLines=0;}
enum{CENTRE_X=1,PDDI_SP_BASETEX=1,PDDI_PRIM_TRIANGLES=1,PDDI_V_CT=1};
struct Event{unsigned kind,glyph;float x,width;bool operator==(const Event& b)const{return kind==b.kind&&glyph==b.glyph&&x==b.x&&width==b.width;}};
static std::vector<Event> events;
struct Stack{void Push(){}void Pop(){}void Translate(float x,float y,float z){events.push_back({0,0,x,y+z});}};
struct pddiPrimStream{};
struct Shader{Shader* GetShader(){return this;}void SetTexture(int,void* p){events.push_back({1,unsigned(uintptr_t(p)),0,0});}};
struct Device{pddiPrimStream stream;bool reject=false;pddiPrimStream* BeginPrims(Shader*,int,int,int n){events.push_back({2,unsigned(n),0,0});return reject?nullptr:&stream;}void EndPrims(pddiPrimStream*){events.push_back({3,0,0,0});}};
namespace p3d{Stack* stack;Device* pddi;}
struct Glyph{unsigned texture;float left,right,width,advance;unsigned GetTextureNum()const{return texture;}float GetLeftBearing()const{return left;}float GetRightBearing()const{return right;}float GetWidth()const{return width;}float GetAdvance()const{return advance;}};
struct tTextureFont{
 float fontHeight=18,fontBaseLine=14;unsigned numTextures=3,missingGlyph=1;Shader* shader;void* textures[3]={(void*)1,(void*)2,(void*)3};
 Glyph glyphs[5]={{0,0,0,4,4},{0,1,2,5,8},{1,-1,3,6,8},{2,2,1,7,10},{0,0,1,3,4}};
 mutable unsigned lookups=0;
 int FindGlyph(char16_t c)const{++lookups;return unsigned(c)%5;}
 float GetLineWidth(const char16_t* p)const{float w=0;while(*p && *p!='\n'){unsigned n=unsigned(*p++)%5;if(!n)n=missingGlyph;w+=glyphs[n].advance;}return w;}
 void DrawGlyph(pddiPrimStream*,unsigned g,float x)const{events.push_back({4,g,x,0});}
 void DrawBlank(pddiPrimStream*,float x,float w)const{events.push_back({5,0,x,w});}
 void DisplayLine(const char16_t*,int)const;
};
METHOD
int main(){
 Stack stack;Device device;Shader shader;tTextureFont font;font.shader=&shader;p3d::stack=&stack;p3d::pddi=&device;
 for(unsigned length:{0u,1u,13u,255u,256u,257u,600u})for(int format:{0,1})for(bool reject:{false,true}){
  std::u16string text;for(unsigned i=0;i<length;++i)text+=char16_t(32+i%90);text+=u"\nignored";device.reject=reject;
  p3dFontWork::enabled=false;events.clear();font.lookups=0;font.DisplayLine(text.c_str(),format);auto original=events;unsigned originalLookups=font.lookups;
  p3dFontWork::enabled=true;events.clear();font.lookups=0;font.DisplayLine(text.c_str(),format);
  assert(events==original);
  if(length && length<=256 && !reject)assert(font.lookups<originalLookups);
 }
 assert(p3dFontWork::longLines>0&&p3dFontWork::preparedLines>0);
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='srr-font-work-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_NATIVE_BENCH','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual font texture order, glyph indices/positions, spacing quads, alignment, long-line fallback and failed-reservation behavior match; repeated glyph searches decrease.')

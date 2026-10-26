#!/usr/bin/env python3
"""Compare actual font triangle attributes and texture order with indexed quads."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/p3d/texturefont.cpp').read_text()
line=source[source.index('tTextureFont::DisplayLine('):source.index('\nint \ntTextureFont::FindGlyph(')]
glyph=source[source.index('tTextureFont::DrawGlyph('):source.index('\nvoid \ntTextureFont::DrawBlank(')]
blank=source[source.index('tTextureFont::DrawBlank('):source.index('//**************************************************************',source.index('tTextureFont::DrawBlank('))]
quad=(root/'libs/pure3d/pddi/pica/quadstream.hpp').read_text()
quad=quad[quad.index('namespace picaQuad {'):quad.index('#ifdef SRR2_OLD3DS_QUAD_STREAM')]
code=r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include <p3d/fontwork.hpp>
using P3D_UNICODE=char16_t;
#define O3_MAIN_SCOPE(x) ((void)0)
namespace p3dFontWork{bool enabled=true,quadStream=false;unsigned preparedLines=0,preparedGlyphs=0,longLines=0,quadBatches=0,quadGlyphs=0,quadFallbacks=0;}
enum{CENTRE_X=1,PDDI_SP_BASETEX=1,PDDI_PRIM_TRIANGLES=1,PDDI_V_CT=1,BACKGROUND_GLYPH_INDEX=0};
struct UV{float u,v;};struct pddiVector{float x,y,z;};struct pddiVector2{float u,v;};
struct tColour{unsigned value=0xa34f2618;bool operator==(const tColour& b)const{return value==b.value;}};
struct Vertex{pddiVector p;UV uv;tColour colour;bool operator==(const Vertex& b)const{return p.x==b.p.x&&p.y==b.p.y&&p.z==b.p.z&&uv.u==b.uv.u&&uv.v==b.uv.v&&colour==b.colour;}};
struct Draw{unsigned texture;std::vector<Vertex> vertices;bool operator==(const Draw& b)const{return texture==b.texture&&vertices==b.vertices;}};
static std::vector<Draw> draws;
struct Stack{void Push(){}void Pop(){}void Translate(float,float,float){}};
struct pddiPrimStream{bool indexed=false;unsigned capacity=0,quads=0;std::vector<Vertex> vertices;std::vector<unsigned short> indices;UV uv={};tColour colour;
 void Colour(tColour c){colour=c;}void UV(float u,float v){uv={u,v};}void Coord(float x,float y,float z){vertices.push_back({{x,y,z},uv,colour});}};
struct Shader{unsigned texture=0;Shader* GetShader(){return this;}void SetTexture(int,void* p){texture=unsigned(uintptr_t(p));}};
struct Device{pddiPrimStream stream;bool rejectQuad=false,rejectAll=false;Shader* shader=nullptr;
 pddiPrimStream* BeginPrims(Shader* s,int,int,int n){if(rejectAll)return nullptr;shader=s;stream={};stream.capacity=n;return &stream;}
 void EndPrims(pddiPrimStream* s){Draw d;d.texture=shader->texture;if(s->indexed){for(unsigned i:s->indices)d.vertices.push_back(s->vertices[i]);}else d.vertices=s->vertices;draws.push_back(d);}};
namespace p3d{Stack* stack;Device* pddi;}
QUAD
pddiPrimStream* picaBeginQuadStream(Shader* shader,unsigned count){if(p3d::pddi->rejectQuad||count>picaQuad::MaxQuads)return nullptr;auto* s=p3d::pddi->BeginPrims(shader,0,0,count*4);if(s){s->indexed=true;s->capacity=count;}return s;}
bool picaEmitQuad(pddiPrimStream* s,const pddiVector* p,const pddiVector2* uv,tColour c,picaQuad::Topology topology){if(!s->indexed)return false;assert(s->quads<s->capacity);unsigned short indices[6];assert(picaQuad::Indices(indices,s->quads,topology));for(unsigned i=0;i<6;++i)s->indices.push_back(indices[i]);for(unsigned i=0;i<4;++i)s->vertices.push_back({p[i],{uv[i].u,uv[i].v},c});++s->quads;return true;}
struct Glyph{unsigned texture;float left,right,width,advance;unsigned GetTextureNum()const{return texture;}float GetLeftBearing()const{return left;}float GetRightBearing()const{return right;}float GetWidth()const{return width;}float GetAdvance()const{return advance;}UV GetBottomLeft()const{return {.125f,.25f};}UV GetTopRight()const{return {.875f,.75f};}};
struct tTextureFont{
 float fontHeight=18,fontBaseLine=14;unsigned numTextures=3,missingGlyph=1;tColour colour;Shader* shader;void* textures[3]={(void*)1,(void*)2,(void*)3};
 Glyph glyphs[5]={{0,0,0,4,4},{0,1,2,5,8},{1,-1,3,6,8},{2,2,1,7,10},{0,0,1,3,4}};
 int FindGlyph(char16_t c)const{return unsigned(c)%5;}
 float GetLineWidth(const char16_t* p)const{float w=0;while(*p&&*p!='\n'){unsigned n=unsigned(*p++)%5;if(!n)n=missingGlyph;w+=glyphs[n].advance;}return w;}
 void DrawGlyph(pddiPrimStream*,unsigned,float)const;void DrawBlank(pddiPrimStream*,float,float)const;void DisplayLine(const char16_t*,int)const;
};
void LINE
void GLYPH
void BLANK
int main(){
 Stack stack;Device device;Shader shader;tTextureFont font;font.shader=&shader;p3d::stack=&stack;p3d::pddi=&device;
 for(unsigned length:{0u,1u,13u,255u,256u,257u,600u})for(int format:{0,1})for(bool rejectQuad:{false,true})for(bool rejectAll:{false,true}){
  std::u16string text;for(unsigned i=0;i<length;++i)text+=char16_t(32+i%90);text+=u"\nignored";device.rejectQuad=rejectQuad;device.rejectAll=rejectAll;
  p3dFontWork::quadStream=false;draws.clear();font.DisplayLine(text.c_str(),format);auto original=draws;
  p3dFontWork::quadStream=true;draws.clear();font.DisplayLine(text.c_str(),format);assert(draws==original);
 }
 assert(p3dFontWork::quadBatches>0&&p3dFontWork::quadGlyphs>0&&p3dFontWork::quadFallbacks>0);
}
'''.replace('QUAD',quad).replace('void LINE\n','void '+line+'\n').replace('void GLYPH\n','void '+glyph+'\n').replace('void BLANK\n','void '+blank+'\n')
with tempfile.TemporaryDirectory(prefix='srr-font-quads-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_QUAD_STREAM',
        '-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: production glyph and spacing quads expand to the same ordered position/UV/color triangles and texture passes; long lines and reservation failures retain the original path.')

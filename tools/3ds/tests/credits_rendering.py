#!/usr/bin/env python3
"""Run production text/sprite submission against the fixed 3DS vertex budget."""
from pathlib import Path
import json
import shutil
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
retail = []
advances = {}
data = root / 'sdcard/3ds/simpsons/art/frontend'
bible = data / 'scrooby/resource/txtbible/srr2.p3d'
if bible.is_file():
    b = bible.read_bytes()
    a = 12 + struct.unpack_from('<I', b, 16)[0] + 12
    a += 1 + b[a]
    count, modulo, _ = struct.unpack_from('<III', b, a + 1)
    entries = {}
    for i in range(count):
        key = struct.unpack_from('<I', b, a + 13 + i * 4)[0]
        start = a + 13 + count * 8 + struct.unpack_from('<I', b, a + 13 + count * 4 + i * 4)[0]
        end = start
        while b[end:end + 2] != b'\0\0': end += 2
        entries[key] = b[start:end].decode('utf-16le')
    for i in range(34):
        key = 0
        for c in f'CREDITS_{i:02}': key = (64 * key + ord(c)) % modulo
        if key in entries: retail.append(entries[key])
    assert len(retail) == 34
    b = (data / 'scrooby2/resource/_frontend/font1_14.p3d').read_bytes()
    a = 12
    _, header, total = struct.unpack_from('<III', b, a)
    cursor = a + 16
    cursor += 1 + b[cursor]
    cursor += 1 + b[cursor]
    height = struct.unpack_from('<4f', b, cursor)[2]
    cursor = a + header
    while cursor < a + total:
        kind, _, size = struct.unpack_from('<III', b, cursor)
        if kind == 0x22001:
            for i in range(struct.unpack_from('<I', b, cursor + 12)[0]):
                record = cursor + 16 + i * 40
                advances[struct.unpack_from('<I', b, record + 36)[0]] = struct.unpack_from('<f', b, record + 32)[0] / height
        cursor += size

def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

production = '\n'.join([
    function('libs/pure3d/pddi/pica/context.cpp', 'bool picaContext::TextLineVisible('),
    function('libs/pure3d/p3d/texturefont.cpp', 'tTextureFont::DisplayLine('),
    function('libs/pure3d/p3d/sprite.cpp', 'void tSprite::Display()'),
    function('code/presentation/gui/frontend/guiscreenviewcredits.cpp', 'float\nCGuiScreenViewCredits::FormatTextWithLineBreaks('),
])
production = production.replace('tTextureFont::DisplayLine(', 'void tTextureFont::DisplayLine(')
program = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "libs/pure3d/pddi/pica/clipbounds.hpp"
#include "code/presentation/gui/utility/handheldcredits.h"
#define SRR2_OLD_3DS 1
using P3D_UNICODE=char16_t;
enum {CENTRE_X=1,PDDI_MATRIX_MODELVIEW,PDDI_SP_BASETEX,PDDI_PRIM_TRIANGLES,PDDI_PRIM_TRISTRIP,PDDI_V_CT};
struct Matrix {float v[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};const float* Row4(unsigned col)const{return v[col];}};
using pddiMatrix=Matrix;
struct Stack {
    Matrix value;std::vector<Matrix> saved;
    void Push(){saved.push_back(value);}void Pop(){assert(!saved.empty());value=saved.back();saved.pop_back();}
    void Translate(float x,float y,float z){value.v[3][0]+=x*value.v[0][0];value.v[3][1]+=y*value.v[1][1];value.v[3][2]+=z*value.v[2][2];}
    const Matrix* Top()const{return &value;}
};
struct Row {float x=0,y=0,z=0,w=0;};
struct Projection {Row r[4];Projection(){r[0].x=r[1].y=r[2].z=r[3].w=1;}};
struct Display {bool stereo=false;bool StereoActive()const{return stereo;}};
bool g_picaInFrame=true;
struct picaContext {
    bool stereoOn=false;Display* display;struct {Stack* matrixStack[8];}state;Projection hwProjection;
    bool TextLineVisible(float,float,float,float)const;
};
picaContext* context;
bool bypassClip=false;
bool picaTextLineVisible(float l,float b,float r,float t){return bypassClip || context->TextLineVisible(l,b,r,t);}
struct pddiPrimStream {unsigned expected=0,used=0;void Vertex(const void*,int,const void*){++used;}};
struct Device {
    unsigned used=0,failures=0;bool reject=false;pddiPrimStream stream;
    pddiPrimStream* BeginPrims(void*,int,int,unsigned count){if(reject || count*36>512*1024-used){++failures;return nullptr;}used+=count*36;stream={count,0};return &stream;}
    void EndPrims(pddiPrimStream* p){assert(p && p->used<=p->expected);}
};
namespace p3d {Stack* stack;Device* pddi;}
struct TestShader {TestShader* GetShader(){return this;}void SetTexture(int,void*){}};
struct Glyph {unsigned GetTextureNum()const{return 0;}float GetLeftBearing()const{return 0;}float GetAdvance()const{return 16;}float GetWidth()const{return 16;};float GetRightBearing()const{return 0;}};
struct tTextureFont {
    float fontHeight=37,fontBaseLine=30,fontWidth=16,fontSize=32;
    float GetFontSize()const{return fontSize;}float GetFontHeight()const{return fontHeight;}float GetLineSpacing()const{return 7;}
    unsigned numTextures=1,missingGlyph=0;TestShader* shader;void* textures[1]={nullptr};Glyph glyphs[2];mutable unsigned draws=0;
    int FindGlyph(char16_t)const{return 1;}
    float GetLineWidth(const char16_t* p)const{float w=0;while(*p && *p++!='\n')w+=16;return w;}
    void DrawGlyph(pddiPrimStream* s,unsigned,float)const{s->used+=6;++draws;}
    void DrawBlank(pddiPrimStream* s,float,float)const{s->used+=6;}
    void DisplayLine(const P3D_UNICODE*,int)const;
};
struct Texture {void* GetTexture(){return nullptr;}};
struct Vertex {int v=0,c=0,uv0=0;};
struct tSprite {int nPolys=1;Texture* textures[1];TestShader* Shader;Vertex polys[4];void Display();};
using tFont=tTextureFont;
#define rAssert assert
namespace Scrooby {struct Text {std::u16string text;tFont* font;void GetBoundingBoxSize(int& w,int& h){w=500;h=20;}tFont* GetFont(){return font;}void* GetStringBuffer(){return &text[0];}};}
struct CGuiScreenViewCredits {static float s_numPixelsPerLine;float FormatTextWithLineBreaks(Scrooby::Text*);};
float CGuiScreenViewCredits::s_numPixelsPerLine=0;
''' + production + r'''
std::map<unsigned,float> retailAdvances={''' + ','.join(f'{{{k},{v}f}}' for k,v in advances.items()) + r'''};
int main(){
 Stack stack;Device device;Display display;picaContext renderer;renderer.display=&display;renderer.state.matrixStack[PDDI_MATRIX_MODELVIEW]=&stack;
 context=&renderer;p3d::stack=&stack;p3d::pddi=&device;TestShader shader;tTextureFont font;font.shader=&shader;
 CGuiScreenViewCredits credits;
 for(float size:{28.0f,32.0f,40.0f}){
    font.fontSize=size;Scrooby::Text text{u"FIRST\\SECOND\\FINAL",&font};
    const float row=44*(size<=32?0.75f:0.5f);
    assert(std::fabs(credits.FormatTextWithLineBreaks(&text)-3*row)<0.001f);
    assert(text.text==u"FIRST\nSECOND\nFINAL");
 }
 const std::vector<std::u16string> fixtures={u"FIRST\\SECOND\\FINAL",u"FIRST\n\nLAST\n",std::u16string(1536,u'W'),u"A long original credit with spaces between all the names and roles",''' + ','.join('u'+json.dumps(t,ensure_ascii=True) for t in retail) + r'''};
 auto measure=[](const char16_t* p){float width=0;while(*p){auto found=retailAdvances.find(*p++);width+=found==retailAdvances.end()?7:found->second*HandheldCredits::Height;}return width;};
 for(const auto& source:fixtures){
    std::u16string expected,actual;
    for(auto c:source)if(c!=' ' && c!='\n' && c!='\\')expected+=c;
    unsigned last=0;
    const auto rows=HandheldCredits::Walk(source.c_str(),
        [&](const char16_t* p){return measure(p)<=HandheldCredits::Width;},
        [&](const char16_t* p,unsigned row){
            assert(measure(p)<=HandheldCredits::Width);
            assert(row==last++);while(*p){if(*p!=' ')actual+=*p;++p;}
        });
    assert(rows==last && expected==actual);
 }
 Texture texture;tSprite sprite;sprite.textures[0]=&texture;sprite.Shader=&shader;
 const std::u16string line(32,u'A');
 // Same geometry budget as hardware; unculled credits exceed it immediately.
 stack.value.v[0][0]=stack.value.v[1][1]=0.003f;
 bypassClip=true;
 for(unsigned i=0;i<500;++i){stack.value.v[3][1]=-float(i)*37*0.003f;font.DisplayLine(line.c_str(),CENTRE_X);}
 assert(device.failures>0);
 bypassClip=false;unsigned peak=0;
 // Visit every line, including both screen boundaries and the end of the roll.
 for(unsigned focus=0;focus<500;++focus){
    device.used=device.failures=0;bool focused=false;
    for(unsigned i=0;i<500;++i){
        stack.value.v[3][1]=(float(focus)-i)*37*0.003f;
        unsigned before=font.draws;font.DisplayLine(line.c_str(),CENTRE_X);
        if(i==focus)focused=font.draws>before;
    }
    assert(focused && device.failures==0 && stack.saved.empty());
    sprite.Display();assert(device.failures==0);
    if(device.used>peak)peak=device.used;
 }
 // Partial edge glyphs stay visible. Invalid transforms fail open.
 stack.value.v[3][1]=1.0f;assert(context->TextLineVisible(-16,0,16,37));
 stack.value.v[3][1]=1.2f;assert(!context->TextLineVisible(-16,0,16,37));
 stack.value.v[3][1]=NAN;assert(context->TextLineVisible(-16,0,16,37));
 stack.value.v[3][1]=0;display.stereo=true;assert(!context->TextLineVisible(-16,1000,16,1037));
 renderer.stereoOn=true;assert(context->TextLineVisible(-16,1000,16,1037));display.stereo=false;
 // A full ring must never turn a later sprite into a null dereference.
 device.reject=true;sprite.Display();assert(device.failures==1);
 printf("PASS: production credits culling, all 500 lines, edge visibility, fixed 512 KiB budget (peak %u), and exhausted sprite stream.\n",peak);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-credits-') as work:
    work = Path(work)
    (work / 'test.cpp').write_text(program)
    subprocess.run([shutil.which('c++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root), str(work / 'test.cpp'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True)
print(f'PASS: {len(retail)} retail credit sections retain every character at the shipped font metrics.' if retail else 'Retail assets absent; synthetic credits checked.')

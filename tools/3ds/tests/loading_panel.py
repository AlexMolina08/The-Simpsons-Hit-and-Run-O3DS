#!/usr/bin/env python3
"""Keep original loading text while laying it out on the native lower screen."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'code/presentation/gui/utility/hudmap.cpp').read_text()
begin=source.index('    void LoadingPanel(')
panel=source[begin:source.index('    void DirectionIcon',begin)]
program=r'''
#include <cassert>
#include <cmath>
#include <string>
using P3D_UNICODE=char16_t;
struct tColour {tColour(int=0,int=0,int=0,int=0){}};
tColour Paper,Ink;
struct tFont {float height=37,baseline=29;float GetFontHeight(){return height;}
 float GetFontBaseLine(){return baseline;}float GetTextWidth(const char16_t* p){return std::char_traits<char16_t>::length(p)*18;}};
namespace Scrooby {struct Text {tFont* font=nullptr;const char16_t* text=nullptr;
 tFont* GetFont(){return font;}const char16_t* GetStringBuffer(){return text;}};}
unsigned calls=0;std::u16string rendered;
void Rounded(float,float,float,float,tColour,tColour){}
void Fill(float,float,float,float,tColour){}
void BottomLabel(tFont*font,const char16_t*text,float x,float baseline,float size,tColour,float width){
 const float actual=fminf(width,font->GetTextWidth(text)*size/font->GetFontHeight());
 assert(x>=40 && x+actual<=280.001f);
 assert(fabsf(x+actual*.5f-160)<.001f);
 assert(fabsf(baseline-size*font->GetFontBaseLine()/font->GetFontHeight()+size*.5f-120)<.001f);
 ++calls;rendered=text;
}
'''+panel+r'''
int main(){
 LoadingPanel(nullptr);Scrooby::Text label;LoadingPanel(&label);assert(!calls);
 tFont font;label.font=&font;LoadingPanel(&label);assert(!calls);
 for(auto text:{u"Loading",u"Loading...",u"Cargando...",u"Chargement...",u"An unusually long localized loading label"}){
  label.text=text;LoadingPanel(&label);assert(rendered==text);
 }
 font.height=0;LoadingPanel(&label);assert(calls==5);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-loading-panel-') as temporary:
    folder=Path(temporary);(folder/'test.cpp').write_text(program)
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test')],check=True)
upper=(root/'code/presentation/gui/backend/guiscreenloadingfe.cpp').read_text()
assert 'm_loadingText->SetVisible( false );' in upper
print('PASS: localized retail strings and animated dots preserved; loading label centered inside 240 px native card; upper duplicate hidden.')

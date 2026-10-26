#!/usr/bin/env python3
"""Exercise actual atlas compatibility and common mip-limit adoption."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];source=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text();a=source.index('bool picaTexture::PrepareAtlas(');b=source.index('\nextern "C" bool picaSpriteAtlasPrepare',a);method=source[a:b]
code=r'''
#include <cassert>
#include <p3d/spriteatlas.hpp>
namespace p3dSpriteAtlas {bool allowMipmaps=false;}
class picaTexture {public:bool created=true,valid=true;unsigned hwFormat=4;struct{unsigned maxLevel=2;}tex;
 static bool PrepareAtlas(picaTexture*,picaTexture* const*,unsigned);};
METHOD
int main(){
 picaTexture a,b,atlas;atlas.tex.maxLevel=3;picaTexture* original[]={&a,&b};
 assert(!picaTexture::PrepareAtlas(&atlas,original,2)&&atlas.tex.maxLevel==3);
 p3dSpriteAtlas::allowMipmaps=true;assert(picaTexture::PrepareAtlas(&atlas,original,2)&&atlas.tex.maxLevel==2);
 atlas.tex.maxLevel=3;b.tex.maxLevel=3;assert(!picaTexture::PrepareAtlas(&atlas,original,2)&&atlas.tex.maxLevel==3);b.tex.maxLevel=2;
 b.hwFormat=2;assert(!picaTexture::PrepareAtlas(&atlas,original,2)&&atlas.tex.maxLevel==3);b.hwFormat=4;
 b.valid=false;assert(!picaTexture::PrepareAtlas(&atlas,original,2));b.valid=true;
 atlas.tex.maxLevel=1;assert(!picaTexture::PrepareAtlas(&atlas,original,2));
 assert(!picaTexture::PrepareAtlas(nullptr,original,2));assert(!picaTexture::PrepareAtlas(&atlas,original,0));
 original[0]=nullptr;assert(!picaTexture::PrepareAtlas(&atlas,original,2));
 a.tex.maxLevel=b.tex.maxLevel=atlas.tex.maxLevel=0;original[0]=&a;p3dSpriteAtlas::allowMipmaps=false;assert(picaTexture::PrepareAtlas(&atlas,original,2));
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='srr-sprite-lod-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_SPRITE_ATLAS','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS: original formats and common LOD limits retained; opt-in required for mipmaps; mixed LOD/format, invalid and insufficient atlas chains fail without mutation.')

#!/usr/bin/env python3
"""Exercise HUD icon loading without mutating the shared image handler's policy."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/presentation/gui/utility/hudmap.cpp').read_text()
a = source.index('void CHudMap::LoadBottomAssets3DS()')
b = source.index('\nvoid CHudMap::HandleScreenInput3DS()', a)
production = source[a:b]
fixture = r'''
#include <cassert>
#include <map>
#include <string>
#include <cstdio>
enum {RADMEMORY_ALLOC_TEMP=7,GMA_LEVEL_HUD=12,PDDI_SP_FILTER=1,PDDI_FILTER_NONE=0};
using radMemoryAllocator=int;
int allocator=3;
int radMemorySetCurrentAllocator(int next){const int old=allocator;allocator=next;return old;}
struct Shader {int filter=4;void SetInt(int key,int value){assert(key==PDDI_SP_FILTER);filter=value;}};
struct tSprite {Shader shader;Shader* GetShader(){return &shader;}};
std::map<std::string,tSprite> sprites;
struct Handler {virtual ~Handler()=default;};
struct tImageHandler:Handler {
 enum LoadType {IMAGE,TEXTURE,SPRITE};LoadType type=IMAGE;bool full=true;
 LoadType GetLoadType(){return type;}bool GetFullName(){return full;}
 void SetLoadType(LoadType value){type=value;}void SetFullName(bool value){full=value;}
};
struct tLoadRequest {
 std::string path;int heap=-1;bool async=true;
 explicit tLoadRequest(const char* name):path(name){assert(allocator==RADMEMORY_ALLOC_TEMP);}
 void SetMemoryAllocator(int value){heap=value;}void SetAsync(bool value){async=value;}
};
struct LoadManager {
 tImageHandler handler;bool available=false;unsigned loads=0;
 Handler* GetHandler(const char* extension){assert(std::string(extension)=="png");return available?&handler:nullptr;}
 void Load(tLoadRequest* request){
  assert(allocator==3 && request->heap==GMA_LEVEL_HUD && !request->async);
  assert(handler.type==tImageHandler::SPRITE && !handler.full);
  const auto& path=request->path;
  assert(path.find("art/frontend/scrooby2/resource/images/")==0);
  const auto start=path.find_last_of('/')+1,end=path.find_last_of('.');
  sprites[path.substr(start,end-start)]={};++loads;delete request;
 }
};
namespace p3d {
 LoadManager* loadManager;
 template<class T>T* find(const char* name){auto it=sprites.find(name);return it==sprites.end()?nullptr:&it->second;}
}
tSprite* UIAsset(const char* name){return p3d::find<tSprite>(name);}
struct CHudMap {static void LoadBottomAssets3DS();};
'''
main = r'''
int main(){
 LoadManager manager;p3d::loadManager=&manager;
 CHudMap::LoadBottomAssets3DS();assert(manager.loads==0 && allocator==3);
 manager.available=true;CHudMap::LoadBottomAssets3DS();
 assert(manager.loads==2 && manager.handler.type==tImageHandler::IMAGE && manager.handler.full && allocator==3);
 assert(sprites.at("camicon").shader.filter==PDDI_FILTER_NONE && sprites.at("coins").shader.filter==PDDI_FILTER_NONE);
 manager.handler.type=tImageHandler::TEXTURE;manager.handler.full=false;
 CHudMap::LoadBottomAssets3DS();assert(manager.loads==2 && manager.handler.type==tImageHandler::TEXTURE && !manager.handler.full);
 sprites.erase("camicon");CHudMap::LoadBottomAssets3DS();assert(manager.loads==3 && sprites.size()==2);
 puts("PASS: production HUD asset loader reuses sprites, restores shared loader policy, loads synchronously into the HUD heap and selects point filtering.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-hud-assets-') as folder:
    folder = Path(folder)
    (folder / 'test.cpp').write_text(fixture + production + main)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)

for name, expected in [('hud/mapicons/camicon.png', (40, 27)), ('coins.png', (22, 19))]:
    path = root / 'sdcard/3ds/simpsons/art/frontend/scrooby2/resource/images' / name
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack_from('>II', data, 16) == expected, path
print('PASS: both original bitmap sources retain their authored dimensions and aspect ratios.')

# Every mission icon must fit by its painted bounds. Verify metadata against the
# retail alpha channels so unusually tall locations and wide vehicles cannot clip.
import re
from PIL import Image
icons = root / 'sdcard/3ds/simpsons/art/frontend/dynaload/images/msnicons'
records = re.findall(r'\{"([^"\n]+)",(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\}',
                     (root / 'code/presentation/gui/utility/handheldiconbounds.h').read_text())
assert len(records) == 150
sources = {p.stem: p for p in icons.rglob('*.png')}
assert len(sources) == len(records)
for name, *numbers in records:
    width, height, left, top, right, bottom = map(int, numbers)
    im = Image.open(sources[name]).convert('RGBA')
    assert im.size == (width, height)
    assert im.getchannel('A').getbbox() == (left, top, right, bottom)
    scale = 40 / max(right - left, bottom - top)
    x = 33 + (width - left - right) * scale / 2
    y = 210 + (height - top - bottom) * scale / 2
    painted = (x - width * scale / 2 + left * scale,
               y - height * scale / 2 + top * scale,
               x - width * scale / 2 + right * scale,
               y - height * scale / 2 + bottom * scale)
    assert painted[0] >= 13 - 0.001 and painted[2] <= 53 + 0.001, name
    assert painted[1] >= 190 - 0.001 and painted[3] <= 230 + 0.001, name
print('PASS: all 150 original mission icons fit their visible artwork inside the objective slot without changing aspect ratio.')

# Exercise the production fitting path with the actual .png chunk names as well
# as the extensionless names used by mission scripts.
source = (root / 'code/presentation/gui/utility/hudmap.cpp').read_text()
a = source.index('    void ObjectiveArt(')
b = source.index('    void Art(', a)
fitting = source[a:b]
art = source[b:source.index('    void FooterAction(', b)]
fixture = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include "code/presentation/gui/utility/handheldiconbounds.h"
using tUID=unsigned;
struct tEntity {static tUID MakeUID(const char* p){unsigned h=2166136261u;while(*p)h=(h^unsigned(*p++))*16777619u;return h;}};
struct tSprite {
 tUID id;int w,h;
 tUID GetUID(){return id;}int GetWidth(){return w;}int GetHeight(){return h;}
};
namespace Scrooby {struct Sprite {
 tSprite* raw;bool visible=true;int resolves=0;tSprite* defaultSprite=nullptr;
 const tSprite* GetRawSprite(){return raw;}bool IsVisible(){return visible;}
 void SetIndex(int index){assert(index==0);raw=defaultSprite;++resolves;}
};}
float ax,ay,aw,ah;unsigned calls=0;
void RetailArt(tSprite*,float x,float y,float w,float h){ax=x;ay=y;aw=w;ah=h;++calls;}
'''
main = r'''
int main(){
 for(const auto& icon:HandheldIconBounds::Icons)for(bool extension:{false,true})for(int slot:{40,48}){
  const std::string name=std::string(icon.name)+(extension?".png":"");
  tSprite raw{tEntity::MakeUID(name.c_str()),icon.width,icon.height};Scrooby::Sprite sprite{&raw};
  ObjectiveArt(&sprite,60,120,slot);
  const float sx=aw/icon.width,sy=ah/icon.height;
  assert(fabsf(sx-sy)<0.001f);
  const float left=ax-aw/2+icon.left*sx,right=ax-aw/2+icon.right*sx;
  const float top=ay-ah/2+icon.top*sy,bottom=ay-ah/2+icon.bottom*sy;
  assert(left>=60-slot/2-0.001f && right<=60+slot/2+0.001f);
  assert(top>=120-slot/2-0.001f && bottom<=120+slot/2+0.001f);
  assert(fabsf(fmaxf(right-left,bottom-top)-slot)<0.001f);
 }
 tSprite raw{0,25,30};Scrooby::Sprite lazy{nullptr};lazy.defaultSprite=&raw;
 Art(&lazy,160,120,20,24);assert(lazy.resolves==1 && aw==20 && ah==24);
 Art(&lazy,160,120,20,24);assert(lazy.resolves==1);
 puts("PASS: production portrait fitting handles all 150 retail .png and script names at HUD and pause sizes.");
 puts("PASS: untouched retail controls resolve their default image once, matching Scrooby Display semantics.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-icon-fitting-') as folder:
    folder = Path(folder)
    (folder / 'test.cpp').write_text(fixture + fitting + art + main)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(root),
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)

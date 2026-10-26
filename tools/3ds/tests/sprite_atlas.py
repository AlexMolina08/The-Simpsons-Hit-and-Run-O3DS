#!/usr/bin/env python3
"""Verify immutable fallback chunks, tile pixels, UV sampling and production atlas adoption."""
from pathlib import Path
import copy,io,random,struct,subprocess,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_character_detail import nodes
from prepare_vehicle_reflections import walk,pstring
from prepare_sprite_atlases import ATLAS,MAGIC,image
root=Path(__file__).resolve().parents[3]
original=Path(sys.argv[1]);candidate=Path(sys.argv[2]);tree=nodes(candidate.read_bytes());rng=random.Random(425171);samples=0;atlases=0
for sprite in walk(tree):
 if sprite.kind!=0x19005:continue
 extra=[n for n in sprite.children if n.kind==ATLAS]
 if not extra:continue
 assert len(extra)==1;atlas=extra[0];magic,version,count,aw,ah=struct.unpack_from('<5I',atlas.body);assert (magic,version)==(MAGIC,1)
 records=list(struct.iter_unpack('<4I',atlas.body[20:]));assert len(records)==count
 images=[image(n) for n in sprite.children if n.kind==0x19001];assert len(images)==count
 _,words,packed=image(atlas.children[0]);assert packed.size==(aw,ah)
 def sample(p,u,v):
  x=u*p.width-.5;y=(1-v)*p.height-.5;x0=int(x//1);y0=int(y//1);fx=x-x0;fy=y-y0
  return tuple(sum(p.getpixel((max(0,min(p.width-1,x0+dx)),max(0,min(p.height-1,y0+dy))))[c]*wx*wy for dx,wx in [(0,1-fx),(1,fx)] for dy,wy in [(0,1-fy),(1,fy)]) for c in range(4))
 for old,(x,y,w,h) in zip(images,records):
  assert old[2].size==(w,h);assert packed.crop((x,y,x+w,y+h)).tobytes()==old[2].tobytes()
  for u,v in [(1/w,1/h),((w-1)/w,(h-1)/h),(1.5/w,.75/h),(1-.5/w,1-1.25/h)]+[(rng.uniform(1/w,(w-1)/w),rng.uniform(1/h,(h-1)/h)) for _ in range(24)]:
   nu=(u*w+x)/aw;nv=(v*h+ah-y-h)/ah
   assert max(abs(a-b) for a,b in zip(sample(old[2],u,v),sample(packed,nu,nv)))<1e-9;samples+=1
 sprite.children.remove(atlas);atlases+=1
assert b''.join(n.encode() for n in tree)==original.read_bytes()
source=(root/'libs/pure3d/p3d/sprite.cpp').read_text();a=source.index('bool tSprite::ApplyAtlas(');b=source.index('\n#endif',a);method=source[a:b]
code=r'''
#include <cassert>
#include <cstring>
#include <p3d/spriteatlas.hpp>
class pddiTexture {public:bool compatible=true;};
struct tTexture {unsigned w=32,h=32;int refs=1;pddiTexture gpu;
 int GetWidth(){return w;}int GetHeight(){return h;}pddiTexture* GetTexture(){return &gpu;}
 void AddRef(){++refs;}void Release(){assert(refs>0);--refs;}};
extern "C" bool picaSpriteAtlasPrepare(pddiTexture*atlas,pddiTexture* const* originals,unsigned count){for(unsigned i=0;i<count;++i)if(!originals[i]->compatible||!atlas->compatible)return false;return true;}
namespace p3dSpriteAtlas {bool enabled=true,allowMipmaps=false,diagnostics=false;unsigned loaded=0,applied=0,batched=0,avoided=0,texturesLoaded=0,rejectShape=0,rejectTile=0,rejectFormat=0;}
struct Vertex {float position[3];unsigned colour;struct{float u,v;}uv0;};
struct tSprite {int nPolys=4;tTexture*textures[4];Vertex polys[16];
 bool ApplyAtlas(tTexture*,const unsigned*,unsigned,unsigned,unsigned);};
METHOD
int main(){
 tTexture old[4],atlas;atlas.w=atlas.h=64;tSprite s;
 for(unsigned i=0;i<4;++i)s.textures[i]=&old[i];
 for(unsigned v=0;v<16;++v){s.polys[v].position[0]=float(v);s.polys[v].position[1]=float(v+1);s.polys[v].position[2]=0;s.polys[v].colour=v+42;s.polys[v].uv0={.25f,.75f};}
 Vertex before[16];memcpy(before,s.polys,sizeof before);
 unsigned tiles[]={0,0,32,32,32,0,32,32,0,32,32,32,32,32,32,32};
 assert(!s.ApplyAtlas(&atlas,tiles,3,64,64));assert(!memcmp(before,s.polys,sizeof before));
 old[3].gpu.compatible=false;assert(!s.ApplyAtlas(&atlas,tiles,4,64,64));assert(atlas.refs==1&&!memcmp(before,s.polys,sizeof before));old[3].gpu.compatible=true;
 tiles[12]=63;assert(!s.ApplyAtlas(&atlas,tiles,4,64,64));assert(!memcmp(before,s.polys,sizeof before));tiles[12]=32;
 assert(s.ApplyAtlas(&atlas,tiles,4,64,64)&&atlas.refs==5&&p3dSpriteAtlas::applied==1);
 for(unsigned i=0;i<4;++i){assert(old[i].refs==0&&s.textures[i]==&atlas);
  for(unsigned v=0;v<4;++v){auto&a=s.polys[i*4+v];auto&b=before[i*4+v];assert(!memcmp(a.position,b.position,sizeof a.position)&&a.colour==b.colour);}
 }
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='srr-sprite-atlas-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_SPRITE_ATLAS','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print(f'PASS: {atlases} atlas additions retain every original byte; {samples} exact bilinear samples; production adoption preserves geometry/colors, rejects incompatible input atomically, and retains correct texture ownership.')

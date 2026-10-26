#ifndef P3D_SPRITE_ATLAS_HPP
#define P3D_SPRITE_ATLAS_HPP
#include <cstdio>
class pddiTexture;
namespace p3dSpriteAtlas {
const unsigned Chunk=0x03FFAD01, Magic=0x31544153, MaxTiles=64;
struct Tile { unsigned x,y,width,height; };
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
extern bool enabled,allowMipmaps,diagnostics;
extern unsigned loaded,applied,batched,avoided,texturesLoaded,rejectShape,rejectTile,rejectFormat;
void DumpSprites(FILE*);
#endif
inline bool Valid(unsigned width,unsigned height,const Tile& t)
{
 return width>=8 && height>=8 && width<=256 && height<=256 &&
  !(width&(width-1)) && !(height&(height-1)) && t.width>=4 && t.height>=4 &&
  t.width<=width && t.height<=height && t.x<=width-t.width && t.y<=height-t.height;
}
inline float U(float value,const Tile& t,unsigned width)
{return (value*t.width+t.x)/width;}
inline float V(float value,const Tile& t,unsigned height)
{return (value*t.height+height-t.y-t.height)/height;}
}
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
extern "C" bool picaSpriteAtlasPrepare(pddiTexture*,pddiTexture* const*,unsigned);
#endif
#endif

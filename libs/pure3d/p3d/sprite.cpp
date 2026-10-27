#include <main/old3dsmainprofile.h>
//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <p3d/sprite.hpp>
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
#include <p3d/spriteatlas.hpp>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/nativebench.h>
#endif
#include <cstring>
#include <cstdio>
#include <pddi/pica/quadstream.hpp>
namespace p3dSpriteAtlas { bool enabled=true,allowMipmaps=true,diagnostics=false;unsigned loaded=0,applied=0,batched=0,avoided=0,texturesLoaded=0,rejectShape=0,rejectTile=0,rejectFormat=0; }
#endif
#include <p3d/utility.hpp>
#include <p3d/chunkfile.hpp>
#include <p3d/image.hpp>
#include <p3d/imageconverter.hpp>
#include <p3d/texture.hpp>
#include <p3d/file.hpp>
#include <constants/chunks.h>
#include <constants/chunkids.hpp>
#include <p3d/shader.hpp>
#include <p3d/targa.hpp>

static const int IMAGE_VERSION = 14000;

#ifdef RAD_XBOX 
    bool tSprite::createLinear = true;
    static int minSpriteSize = 2;
    static const int maxSpriteSize = 256;
#else
    bool tSprite::createLinear = false;
    static const int minSpriteSize = 32;
    static const int maxSpriteSize = 256;
#endif

struct tSpriteConstruction
{
    tImage* image;
    tImage* buffer;
    tImageConverter* conv;
    int wantAlpha;
};

struct tRect
{
    int top;
    int left;
    int bottom;
    int right;
    int height;
    int width;
};

tSprite::tSprite(tImage* image, tShader* mat, int border, int nx, int ny, tImageConverter* conv)
{
    P3DASSERT(image);

    linear = createLinear;
    
    if (linear)
    {
        SetBlitBorder(0);
    }
    else
    {
        SetBlitBorder(border);
    }

    nativeX = nx;
    nativeY = ny;

    scaleX = 1.0f / float(nativeX);
    scaleY = 1.0f / float(nativeX);

    box.low.x = box.low.y = box.low.z = box.high.z = 0.0f;
    box.high.x = scaleX * image->GetWidth();
    box.high.y = scaleY * image->GetHeight();

    sphere.centre.Set(box.high.x / 2, box.high.y / 2, 0);
    sphere.radius = rmt::Sqrt(box.high.x*box.high.x + box.high.y*box.high.y);

    Shader = mat ? mat : new tShader;
    Shader->AddRef();

    if(!mat)
    {
        Shader->SetInt(PDDI_SP_ISLIT, 0);
        Shader->SetInt(PDDI_SP_BLENDMODE, image->HasAlpha());
    }

    height = image->GetHeight();
    width = image->GetWidth();

    BuildSections(image, conv);

    SetColour(tColour(255,255,255));
}

tSprite::tSprite(tTexture** images, int w, int h, int count, tShader* mat, int border, int nx, int ny, tImageConverter* conv)
{
    linear = false;

#ifdef RAD_XBOX
    minSpriteSize = 32;
#endif

    SetBlitBorder(border);

    nativeX = nx;
    nativeY = ny;

    scaleX = 1.0f / float(nativeX);
    scaleY = 1.0f / float(nativeX);

    box.low.x = box.low.y = box.low.z = box.high.z = 0.0f;
    box.high.x = scaleX * w;
    box.high.y = scaleY * h;

    sphere.centre.Set(box.high.x / 2, box.high.y / 2, 0);
    sphere.radius = rmt::Sqrt(box.high.x*box.high.x + box.high.y*box.high.y);

    Shader = mat ? mat : new tShader;
    Shader->AddRef();

    if(!mat)
    {
        Shader->SetInt(PDDI_SP_ISLIT, 0);
        Shader->SetInt(PDDI_SP_BLENDMODE, 1);
    }

    height = h;
    width = w;

    textures = images;
    polys = new sprVertex[count*4];
    nPolys = count;

    // We've only got one texture, so ignore the blitborder
    if ((textures[0]->GetHeight() == h) && (textures[0]->GetWidth() == w))
    {
        blitBorder = 0;
    }

    int x = 0;
    int y = -blitBorder;
    tRect dim;
    int index = 0;
    int newX = 0;
    int newY = 0;

    while (y<h-1)
    {
        x = -blitBorder;
        while (x<w-1)
        {
            newX = textures[index]->GetWidth();
            newY = textures[index]->GetHeight();
            dim.left = x+blitBorder;
            dim.top = y+blitBorder;
            dim.width = textures[index]->GetWidth()>w ? w : (newX - (blitBorder<<1));
            dim.height = textures[index]->GetHeight()>h ? h : (newY - (blitBorder<<1));
            dim.right = dim.left+dim.width;
            dim.bottom = dim.top+dim.height;
            BuildPoly(index,dim,newX,newY);
            index++;
            x += newX-(blitBorder<<1);
        }
        y += newY-(blitBorder<<1);
    }

    P3DASSERT(index == count);

    SetColour(tColour(255,255,255));

}

tSprite::~tSprite()
{
    Shader->Release();

    for(int i = 0; i < nPolys; i++)
        textures[i]->Release();

    delete [] textures;
    delete [] polys;
}

void tSprite::ProcessShaders(ShaderCallback& callback)
{
    tRefCounted::Assign(Shader, callback.Process(Shader));
}

int tSprite::SelectSegment(int val)
{
    P3DASSERT(val > 0);

    if (val >= maxSize)
    {
        return maxSize;
    }

    if (val <= minSize)
    {
        return val;
    }
  
    return rmt::PrevPow2(val) - (blitBorder<<1);
}

void tSprite::SetBlitBorder(int border)
{ 
    blitBorder = border>=0 ? border : blitBorder; 

    maxSize = p3d::pddi->GetMaxTextureDimension() - (blitBorder<<1);
    minSize = minSpriteSize - (blitBorder<<1);

    P3DASSERT((minSize>=2)&&(maxSize>=minSize));   
}

int tSprite::CalcSections(tRect** sections, int &numXSect, int &numYSect)
{  
    if (linear)
    {
        if (!sections)
        {
            numXSect = 1;
            numYSect = 1;
            return 1;
        }
        else
        {
            sections[0][0].left = 0;
            sections[0][0].right = width;
            sections[0][0].width = width;
            sections[0][0].top = 0;
            sections[0][0].bottom = height;
            sections[0][0].height = height;
            return 1;
        }
    }
    else
    {
        int x = 0;
        int y = 0;
        int xSect, ySect;
        int current, accumulated;

        xSect = 0;
        accumulated = 0;
        while (x<width)
        {
            current = SelectSegment(width-x);
            accumulated = x + current;
            if (sections)
            {
                for (ySect = 0; ySect<numYSect; ySect++)
                {
                    sections[xSect][ySect].left = x;
                    sections[xSect][ySect].right = accumulated;
                    sections[xSect][ySect].width = current;
                }
            }
            x = accumulated;
            xSect++;
        }

        ySect = 0;
        accumulated = 0;
        while (y<height)
        {
            current = SelectSegment(height-y);
            accumulated = y + current;
            if (sections)
            {
                for (xSect = 0; xSect<numXSect; xSect++)
                {
                    sections[xSect][ySect].top = y;
                    sections[xSect][ySect].bottom = accumulated;
                    sections[xSect][ySect].height = current;
                }
            }
            y = accumulated;
            ySect++;
        }

        if (!sections)
        {
            numXSect = xSect;
            numYSect = ySect;
        }

        P3DASSERT(xSect>0);
        P3DASSERT(ySect>0);
        return xSect*ySect;
    }
}

void tSprite::BuildSections(tImage* image, tImageConverter* conv)
{
    int numXSect, numYSect; 
    int xSect, ySect;
    int sectionNum=0;

    nPolys = CalcSections(NULL, numXSect, numYSect);

    tRect** sections = (tRect**)p3d::MallocTemp(numXSect * sizeof(tRect*));
    for (xSect = 0; xSect < numXSect; xSect++)
    {
        sections[xSect] = (tRect*)p3d::MallocTemp(numYSect * sizeof(tRect));
    }

    CalcSections(sections, numXSect, numYSect);

    //create the polygons and textures for the seperated sections of the sprite
    textures = new tTexture*[nPolys];
    polys = new sprVertex[nPolys*4];

    if(!conv)
    {
        static tImageConverter staticConv;
        conv = &staticConv;
    }
    
    tImage* buffer = NULL;

#ifndef RAD_PS2
    if (linear)
    {
        buffer = new tImage32; 
        buffer->EnableColourKey(image->GetColourKeyStatus());
    }
    else
#endif
    {
        switch(image->GetDepth())
        {
            case 8 :  buffer = new tImage8; 
                         ((tImage8*)buffer)->SetPalette(((tImage8*)image)->GetPalette());

                         break;
#ifndef RAD_PS2
            case 32 : buffer = new tImage32; 
                         buffer->EnableColourKey(image->GetColourKeyStatus());
                         break;
#endif
        }  
    }

    buffer->SetAlpha(image->HasAlpha());
    buffer->SetAlphaDepth(image->GetAlphaDepth());

    P3DASSERT(buffer);
    buffer->AddRef();
    
    int border = blitBorder<<1;
    for (xSect = 0; xSect<numXSect; xSect++)
    {
        for (ySect = 0; ySect<numYSect; ySect++)
        {
            int newX = sections[xSect][ySect].width>minSize ?  sections[xSect][ySect].width + border : minSpriteSize;
            int newY = sections[xSect][ySect].height>minSize ?  sections[xSect][ySect].height + border : minSpriteSize;
            BuildTexture(sectionNum, sections[xSect][ySect], newX, newY, image, buffer, conv);
            BuildPoly(sectionNum, sections[xSect][ySect], newX, newY);
            sectionNum++;
        }
    }

    buffer->Release();

    for (xSect = 0; xSect < numXSect; xSect++)
    {
        p3d::FreeTemp(sections[xSect]);
    }
    p3d::FreeTemp(sections);
}

void tSprite::BuildTexture(int texNum, tRect& src, int newX, int newY, tImage* image, tImage* buffer, tImageConverter* conv)
{
    buffer->SetSize(newX, newY);
    buffer->Clear(0);

    int srcX1, srcX2, srcY1, srcY2;
    int destX1, destX2, destY1, destY2;
    int copyWidth, copyHeight;
    int x, y;
    
    //note all src?1 and dest?1 are the starting pixels and all src?2 and dest?2 are ending pixels that are not included!
    //ie srcX1 = 2 and srcX2 = 10, means that pixels 2 thru to 9 inclusive will be copied!
    srcX1 = src.left>blitBorder ? src.left-blitBorder : 0;
    srcX2 = (width-src.right)>blitBorder ? src.right+blitBorder : width;
    srcY1 = src.top>blitBorder ? src.top-blitBorder : 0;
    srcY2 = (height-src.bottom)>blitBorder ? src.bottom+blitBorder : height;
    copyWidth = srcX2-srcX1;
    copyHeight = srcY2-srcY1;
    destX1 = src.left>blitBorder ? 0 : blitBorder-src.left;
    destX2 = destX1 + copyWidth;
    destY1 = src.top>blitBorder ? 0 : blitBorder-src.top;
    destY2 = destY1 + copyHeight;

    //copy inner region of src image to the new texture
    buffer->CopyRegion(image, srcX1, srcY1, copyWidth, copyHeight, destX1, destY1);

    //copy the sides of the texture that don't overlap the original image (ie srcX<0)
    if (copyWidth<newX)
    {
        if (destX1>0)
        {
            for (x = destX1-1; x>=0; x--)
            {
                buffer->CopyRegion(image, srcX1, srcY1, 1, copyHeight, x, destY1);
            }
        }
        if (destX2<newX)
        {
            for (x = destX2; x<newX; x++)
            {
                buffer->CopyRegion(image, srcX2-1, srcY1, 1, copyHeight, x, destY1);
            }
        }
    }
    if (copyHeight<newY)
    {
        if (destY1>0)
        {
            for (y = destY1-1; y>=0; y--)
            {
                buffer->CopyRegion(image, srcX1, srcY1, copyWidth, 1, destX1, y);
            }
        }
        if (destY2<newY)
        {
            for (y = destY2; y<newY; y++)
            {
                buffer->CopyRegion(image, srcX1, srcY2-1, copyWidth, 1, destX1, y);
            }
        }
    }

    unsigned int corner;

    //copy the corner colors
    if (destX1>0)
    {
        if (destY1>0)
        {
            if (buffer->GetDepth()==8)
            {
                corner = ((tImage8*)(buffer))->GetPixelIndex(destX1, destY1);
            }
            else
            {
                corner = buffer->GetColour(destX1, destY1);
            }
            for (x = destX1-1; x>=0; x--)
            {
                for (y = destY1-1; y>=0; y--)
                {
                    buffer->SetColour(x,y,corner);
                }
            }
        }
        if (destY2<newY)
        {
            if (buffer->GetDepth()==8)
            {
                corner = ((tImage8*)(buffer))->GetPixelIndex(destX1, destY2-1);
            }
            else
            {
                corner = buffer->GetColour(destX1, destY2-1);
            }
            for (x = destX1-1; x>=0; x--)
            {
                for (y = destY2; y<newY; y++)
                {
                    buffer->SetColour(x,y,corner);
                }
            }
        }
    }

    if (destX2<newX)
    {
        if (destY1>0)
        {
            if (buffer->GetDepth()==8)
            {
                corner = ((tImage8*)(buffer))->GetPixelIndex(destX2-1, destY1);
            }
            else
            {
                corner = buffer->GetColour(destX2-1, destY1);
            }
            for (x = destX2; x<newX; x++)
            {
                for (y = destY1-1; y>=0; y--)
                {
                    buffer->SetColour(x,y,corner);
                }
            }
        }
        if (destY2<newY)
        {
            if (buffer->GetDepth()==8)
            {
                corner = ((tImage8*)(buffer))->GetPixelIndex(destX2-1, destY2-1);
            }
            else
            {
                corner = buffer->GetColour(destX2-1, destY2-1);
            }
            for (x = destX2; x<newX; x++)
            {
                for (y = destY2; y<newY; y++)
                {
                    buffer->SetColour(x,y,corner);
                }
            }
        }
    }

    textures[texNum] = conv->ImageToTexture(buffer,linear);
    textures[texNum]->AddRef();

/*
char imageName[256];
sprintf(imageName,"sprite_texture_%d.tga",texNum);
tTargaHandler handler;
handler.SaveImage(buffer,imageName);
*/
}

void tSprite::BuildPoly(int polyNum, tRect& src, int newX, int newY)
{
    float u0, u1, v0, v1;

    if (linear)
    {
        u0 = 0.0f;
        u1 = (float)src.width;
        v0 = (float)src.height;
        v1 = 0.0f;
    }
    else
    {
        #ifdef RAD_PS2
            float fudgeU = 0.0f;
            float fudgeV = 0.0f;
        #else
            float fudgeU = (1/((float)newX * 2));
            float fudgeV = (1/((float)newY * 4));
        #endif
        u0 = float(blitBorder) / float(newX) + fudgeU;
        u1 = float(blitBorder+src.width) / float(newX) + fudgeU;
        v0 = 1.0f - float(blitBorder) / float(newY) - fudgeV;
        v1 = 1.0f - float(blitBorder+src.height) / float(newY) - fudgeV;
    }

    for(int i = 0; i < 4; i++)
    {
        polys[(polyNum * 4) + i].c.Set(255,255,255);
    }

    polys[(polyNum * 4) + 0].v.Set(float(src.right) * scaleX, float(height-src.bottom) * scaleY, 0.0f);
    polys[(polyNum * 4) + 0].uv0.u = u1;
    polys[(polyNum * 4) + 0].uv0.v = v1;

    polys[(polyNum * 4) + 1].v.Set(float(src.left) * scaleX, float(height-src.bottom) * scaleY, 0.0f);
    polys[(polyNum * 4) + 1].uv0.u = u0;
    polys[(polyNum * 4) + 1].uv0.v = v1;

    polys[(polyNum * 4) + 2].v.Set(float(src.right) * scaleX, float(height-src.top) * scaleY, 0.0f);
    polys[(polyNum * 4) + 2].uv0.u = u1;
    polys[(polyNum * 4) + 2].uv0.v = v0;

    polys[(polyNum * 4) + 3].v.Set(float(src.left) * scaleX, float(height-src.top) * scaleY, 0.0f);
    polys[(polyNum * 4) + 3].uv0.u = u0;
    polys[(polyNum * 4) + 3].uv0.v = v0;
}

int tSprite::GetNativeX(void)
{
    return nativeX;
}

int tSprite::GetNativeY(void)
{
    return nativeY;
}


tShader* tSprite::GetShader(void)
{
    return Shader;
}

void tSprite::SetColour(tColour c) 
{
    colour = c;

    for (int a = 0; a < nPolys * 4; a++)
    {
        polys[a].c = colour;
    }
}

#ifdef SRR2_OLD3DS_SPRITE_ATLAS
namespace p3dSpriteAtlas {
namespace { struct SpriteCount {unsigned long long uid;unsigned calls,tiles;};SpriteCount spriteCounts[96]={};unsigned spriteCount=0,overflow=0; }
static void Observe(unsigned long long uid,unsigned tiles)
{
    for(unsigned i=0;i<spriteCount;++i)if(spriteCounts[i].uid==uid){++spriteCounts[i].calls;spriteCounts[i].tiles+=tiles;return;}
    if(spriteCount==96){++overflow;return;}
    auto& slot=spriteCounts[spriteCount++];slot.uid=uid;slot.calls=1;slot.tiles=tiles;
}
void DumpSprites(FILE* file)
{
    fprintf(file,"NATIVE_SPRITE_COUNTS enabled=%u overflow=%u diagnostic_only=1\n",diagnostics,overflow);
    if(diagnostics)for(unsigned i=0;i<spriteCount;++i){const auto& s=spriteCounts[i];fprintf(file,"NATIVE_SPRITE uid=%016llx calls=%u tile_draws=%u\n",s.uid,s.calls,s.tiles);}
}
}
bool tSprite::ApplyAtlas(tTexture* atlas,const unsigned* words,unsigned count,unsigned aw,unsigned ah)
{
    if(!atlas || count!=unsigned(nPolys) || count<2 || count>p3dSpriteAtlas::MaxTiles ||
       unsigned(atlas->GetWidth())!=aw || unsigned(atlas->GetHeight())!=ah){++p3dSpriteAtlas::rejectShape;return false;}
    pddiTexture* originals[p3dSpriteAtlas::MaxTiles];
    for(unsigned i=0;i<count;++i){
        const p3dSpriteAtlas::Tile t={words[i*4],words[i*4+1],words[i*4+2],words[i*4+3]};
        if(!p3dSpriteAtlas::Valid(aw,ah,t) || unsigned(textures[i]->GetWidth())!=t.width ||
           unsigned(textures[i]->GetHeight())!=t.height){++p3dSpriteAtlas::rejectTile;return false;}
        originals[i]=textures[i]->GetTexture();
    }
    if(!picaSpriteAtlasPrepare(atlas->GetTexture(),originals,count)){++p3dSpriteAtlas::rejectFormat;return false;}
    for(unsigned i=0;i<count;++i){
        const p3dSpriteAtlas::Tile t={words[i*4],words[i*4+1],words[i*4+2],words[i*4+3]};
        for(unsigned v=0;v<4;++v){auto& uv=polys[i*4+v].uv0;
            uv.u=p3dSpriteAtlas::U(uv.u,t,aw);uv.v=p3dSpriteAtlas::V(uv.v,t,ah);
        }
        atlas->AddRef();textures[i]->Release();textures[i]=atlas;
    }
    ++p3dSpriteAtlas::applied;return true;
}
#endif

void tSprite::Display()
{
    O3_MAIN_SCOPE(HudSprite);
    int firstPoly=0;
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_SPRITE_ATLAS)
    if(p3dSpriteAtlas::diagnostics && NativeBench::collecting)p3dSpriteAtlas::Observe((unsigned long long)GetUID(),unsigned(nPolys));
#endif
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
    if(p3dSpriteAtlas::enabled && nPolys>1){
        bool shared=true;for(int i=1;i<nPolys;++i)shared&=textures[i]==textures[0];
        if(shared){
            Shader->GetShader()->SetTexture(PDDI_SP_BASETEX,textures[0]->GetTexture());
            pddiPrimStream* stream=picaBeginQuadStream(Shader->GetShader(),unsigned(nPolys));
            if(stream){
                for(;firstPoly<nPolys;++firstPoly){
                    pddiVector positions[4];pddiVector2 uv[4];pddiColour colours[4];
                    for(unsigned v=0;v<4;++v){const auto& p=polys[firstPoly*4+v];positions[v]=p.v;uv[v]=p.uv0;colours[v]=p.c;}
                    if(!picaEmitColourQuad(stream,positions,uv,colours,picaQuad::Sparkle))break;
                }
                p3d::pddi->EndPrims(stream);
                if(firstPoly){++p3dSpriteAtlas::batched;p3dSpriteAtlas::avoided+=unsigned(firstPoly-1);}
            }
        }
    }
#endif
    for(int i = firstPoly; i < nPolys; i++)
    {
//      if(tDrawable2D::RectClip(scratch,uv,colours))
        {
            Shader->GetShader()->SetTexture(PDDI_SP_BASETEX, textures[i]->GetTexture());   

            pddiPrimStream* stream = p3d::pddi->BeginPrims(Shader->GetShader(), PDDI_PRIM_TRISTRIP, PDDI_V_CT, 4);
            if (!stream)
            {
                continue;
            }
            stream->Vertex(&polys[i*4].v, polys[i*4].c, &polys[i*4].uv0);
            stream->Vertex(&polys[(i*4)+1].v, polys[(i*4)+1].c, &polys[(i*4)+1].uv0);
            stream->Vertex(&polys[(i*4)+2].v, polys[(i*4)+2].c, &polys[(i*4)+2].uv0);
            stream->Vertex(&polys[(i*4)+3].v, polys[(i*4)+3].c, &polys[(i*4)+3].uv0);

            p3d::pddi->EndPrims(stream);

        }
    }
}

tSpriteLoader::tSpriteLoader() : tSimpleChunkHandler(Pure3D::Texture::SPRITE)//P3D_SPRITE)
{
    forceAlphaTest = false;
    normalize = false;
    converter = new tImageConverter;

    imageLoader = new tImageLoader;
    imageLoader->AddRef();
    imageFactory = new tImageFactory;
}

tSpriteLoader::~tSpriteLoader()
{
    imageLoader->Release();
    delete converter;
    delete imageFactory;
}

//------------------------------------------------------------------------
tEntity* tSpriteLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    char name[256];
    int nativeX;
    int nativeY;
    char shader[256];
    int imageHeight;
    int imageWidth;
    int blitBorder;
    int imageCount;
    tImage* image = NULL;
    tTexture** images = NULL;
        
    f->GetPString(name);
    nativeX = f->GetLong();
    nativeY = f->GetLong();
    f->GetPString(shader);
    imageWidth = f->GetLong();
    imageHeight = f->GetLong();
    imageCount = f->GetLong();
    blitBorder = f->GetLong();
    int count = 0;
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
    tTexture* atlas=NULL;unsigned atlasCount=0,atlasWidth=0,atlasHeight=0;
    unsigned atlasTiles[p3dSpriteAtlas::MaxTiles*4]={};
#endif
    if ((imageCount>1) || blitBorder)
    {
         images = new tTexture*[imageCount];
    }

    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
            case p3dSpriteAtlas::Chunk:
                if(p3dSpriteAtlas::enabled && !atlas){
                    const unsigned magic=f->GetUInt(),version=f->GetUInt();
                    atlasCount=f->GetUInt();atlasWidth=f->GetUInt();atlasHeight=f->GetUInt();
                    if(magic!=p3dSpriteAtlas::Magic || version!=1 || atlasCount!=unsigned(imageCount) ||
                       atlasCount<2 || atlasCount>p3dSpriteAtlas::MaxTiles){atlasCount=0;break;}
                    for(unsigned i=0;i<atlasCount*4;++i)atlasTiles[i]=f->GetUInt();
                    while(f->ChunksRemaining()){
                        const unsigned kind=f->BeginChunk();
                        if(kind==Pure3D::Texture::IMAGE && !atlas){atlas=LoadTexture(f,32);if(atlas){atlas->AddRef();++p3dSpriteAtlas::texturesLoaded;}}
                        f->EndChunk();
                    }
                    ++p3dSpriteAtlas::loaded;
                }
                break;
#endif
            case P3D_IMAGE:
                {
                    image = imageLoader->LoadImage(f, 32);
                }
                break;
            case Pure3D::Texture::IMAGE:
                 {
                      if (images)
                      {
                            images[count] = LoadTexture(f,32);
                            if (images[count])
                            {
                                images[count]->AddRef();
                            }
                            count++;
                      }
                      else
                      {
                          image = LoadImage(f, 32);
                      }

                 }
                 break;

            default:
                break;
        }
        f->EndChunk();
    }

#ifdef SRR2_OLD3DS_SPRITE_ATLAS
    if(atlas && (!images || image)){atlas->Release();atlas=NULL;}
#endif
    if(image)
    {
        tShader* mat = p3d::find<tShader>(store, shader);
        tSprite* sprite = new tSprite(image, mat, 1, nativeX, nativeY, converter);
        sprite->SetName(name);

        image->Release();
        return sprite;
    }
    else if (images)
    {
         tShader* mat = p3d::find<tShader>(store, shader);
         tSprite* sprite = new tSprite(images, imageWidth, imageHeight, imageCount, mat, 1, nativeX, nativeY, converter);
         sprite->SetName(name);
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
         if(atlas){sprite->ApplyAtlas(atlas,atlasTiles,atlasCount,atlasWidth,atlasHeight);atlas->Release();atlas=NULL;}
#endif

         // don't delete the textures any more, cause we use them right off the bat.
         /*for (int i=0; i < imageCount; i++)
         {
              images[i]->Release();
         }*/
         return sprite;
    }
    return NULL;
}

tImage* tSpriteLoader::LoadImage(tChunkFile* f, int depth /*=32*/)
{
    tImage* image = NULL;
    char name[128];
    f->GetPString(name);
    int version = f->GetLong();
    P3DASSERT(version == IMAGE_VERSION);
    int width = f->GetLong();
    int height = f->GetLong();
    int bpp = f->GetLong();
    bool palettized = f->GetLong() == 1;
    bool alpha = f->GetLong() == 1;
    unsigned format = f->GetLong();

    imageFactory->SetDesiredDepth(bpp);

    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::Texture::IMAGE_DATA:
            {
                unsigned size = f->GetLong();
                tFile* file = f->BeginInset();
                image = imageFactory->ParseAsImage(file, name, (tImageHandler::Format)format);
                f->EndInset(file);
                break;
            }

            case Pure3D::Texture::IMAGE_FILENAME:
            {
                char fileName[255];
                f->GetPString(fileName);
                image = imageFactory->LoadAsImage(fileName, name);
                break;
            }

            default:
                break;
        }
        f->EndChunk();
    }

    if(image)
    {
        image->SetAlpha(alpha);
    }
    return image;
}


tTexture* tSpriteLoader::LoadTexture(tChunkFile* f, int depth /*=32*/)
{
    tTexture* image = NULL;
    char name[128];
    f->GetPString(name);
    int version = f->GetLong();
    P3DASSERT(version == IMAGE_VERSION);
    int width = f->GetLong();
    int height = f->GetLong();
    int bpp = f->GetLong();
    bool palettized = f->GetLong() == 1;
    bool alpha = f->GetLong() == 1;
    unsigned format = f->GetLong();

    imageFactory->SetDesiredDepth(bpp);
    switch(format)
    {
    case tImageHandler::IMG_PS2_4BIT:
         imageFactory->SetTextureHints(8,0,PDDI_TEXTYPE_PS2_4BIT,PDDI_USAGE_STATIC);break;
    case tImageHandler::IMG_PS2_8BIT:
        imageFactory->SetTextureHints(8,0,PDDI_TEXTYPE_PS2_8BIT,PDDI_USAGE_STATIC);break;
    case tImageHandler::IMG_PS2_16BIT:
        imageFactory->SetTextureHints(8,0,PDDI_TEXTYPE_PS2_16BIT,PDDI_USAGE_STATIC);break;
    case tImageHandler::IMG_PS2_32BIT:
        imageFactory->SetTextureHints(8,0,PDDI_TEXTYPE_PS2_32BIT,PDDI_USAGE_STATIC);break;
    default:
         break;
    }


    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::Texture::IMAGE_DATA:
            {
                unsigned size = f->GetLong();
                tFile* file = f->BeginInset();
                image = imageFactory->ParseAsTexture(file, name, size, (tImageHandler::Format)format);
                f->EndInset(file);
                break;
            }

            case Pure3D::Texture::IMAGE_FILENAME:
            {
                char fileName[255];
                f->GetPString(fileName);
                image = imageFactory->LoadAsTexture(fileName, name);
                break;
            }

            default:
                break;
        }
        f->EndChunk();
    }

    return image;
}


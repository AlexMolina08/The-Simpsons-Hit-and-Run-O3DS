#!/usr/bin/env python3
"""Exercise production factory hit/miss ownership and rewind-free fallback."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools/3ds'))
from prepare_native_textures import function

PRELUDE = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#ifdef SRR2_OLD3DS_NATIVE_ETC1
#include "pddi/pica/nativetexture.hpp"
static bool g_picaNativeEtcEnabled=true;
#endif
static const char* fullFilename="assets/original.png";
static bool available=true,hit=true,createOk=true,failAllocation=false;
static unsigned allocations=0,frees=0,creates=0,decodes=0,stored=0,uploads=0;
static unsigned g_picaFontTexture=0,g_picaKeepFullSize=0;
enum {PDDI_USAGE_STATIC,PDDI_USAGE_DYNAMIC};
enum {PDDI_TEXTYPE_RGB,PDDI_TEXTYPE_PALETTIZED,PDDI_TEXTYPE_OTHER};
bool picaNativeTexturesAvailable(){return available;}
void* InputMalloc(size_t n){if(failAllocation)return nullptr;++allocations;return malloc(n);}
void InputFree(void* p){++frees;free(p);}
struct tFile {
 enum {BYTE};std::vector<unsigned char> bytes;size_t position=0;bool readOk=true;
 virtual bool GetData(void* p,unsigned n,int){
  if(!readOk || position+n>bytes.size())return false;
  memcpy(p,bytes.data()+position,n);position+=n;return true;
 }
 const char* GetFilename(){return "original.png";}
 const char* GetFullFilename(){return fullFilename;}
 virtual ~tFile()=default;
};
struct tFileMem:tFile {
 tFileMem(unsigned char* data,unsigned n,bool){bytes.assign(data,data+n);}
 void SetFilename(const char*){}
};
struct picaTexture {bool UploadPreparedPNG(const void*,unsigned,bool world=false){
#ifdef SRR2_OLD3DS_NATIVE_ETC1
 assert(world);
#endif
 ++uploads;return hit;
}};
struct tTexture {
 picaTexture native;std::string name;
 bool Create(unsigned w,unsigned h,int bpp,int,int,int type,int){
  assert(w==16 && h==8 && bpp==32 && type==PDDI_TEXTYPE_RGB);++creates;return createOk;
 }
 void Release(){delete this;}
 void SetName(const char* value){name=value;}
 picaTexture* GetTexture(){return &native;}
};
struct TextureBuilder {
 tTexture* texture=nullptr;
 void SetDesiredDepth(int){}void SetAlphaDepth(int){}void SetMipCount(int){}
 void SetTextureType(int){}void SetUsage(int){}void SetExpectedFileSize(int){}
 void SetTexture(tTexture* p){texture=p;}tTexture* GetTexture(){return texture;}
};
struct tImageHandler {
 enum Format {IMG_PNG,IMG_OTHER};
 void CreateImage(tFile* f,TextureBuilder* builder){
  ++decodes;assert(f->position==0);unsigned char magic[8];assert(f->GetData(magic,8,tFile::BYTE));
  assert(magic[0]==137 && magic[1]==80);
  if(!builder->texture)builder->texture=new tTexture;
 }
};
namespace p3d {struct Inventory {void Store(tTexture*){++stored;}};Inventory instance;Inventory* inventory=&instance;}
struct tImageFactory {
 int desiredDepth=32,alphaDepthHint=8,nMipHint=4,typeHint=PDDI_TEXTYPE_RGB,usageHint=PDDI_USAGE_STATIC;
 bool autoStore=true;tImageHandler handler;
 tImageHandler* FindHandler(tImageHandler::Format){return &handler;}
 tTexture* ParseAsTexture(tFile*,char*,const int,tImageHandler::Format);
};
'''

body = function((ROOT/'libs/pure3d/p3d/imagefactory.cpp').read_text(), 'tTexture* tImageFactory::ParseAsTexture(')
body = body.replace('malloc(size)', 'InputMalloc(size)').replace('free(encoded)', 'InputFree(encoded)')
TEST = r'''
int main(){
 for(unsigned scenario=0;scenario<14;++scenario){
  available=hit=createOk=true;failAllocation=false;g_picaFontTexture=g_picaKeepFullSize=0;
  allocations=frees=creates=decodes=stored=uploads=0;
  tImageFactory factory;tFile file;file.bytes.resize(40);
#ifdef SRR2_OLD3DS_NATIVE_ETC1
  fullFilename="art/l2r1.p3d";
#else
  fullFilename="assets/original.png";
#endif
  const unsigned char magic[]={137,80,78,71,13,10,26,10};memcpy(file.bytes.data(),magic,8);
  file.bytes[19]=16;file.bytes[23]=8;file.bytes[24]=8;file.bytes[25]=6;
  switch(scenario){
   case 1:hit=false;break;
   case 2:available=false;break;
   case 3:g_picaFontTexture=1;break;
   case 4:g_picaKeepFullSize=1;break;
   case 5:factory.usageHint=PDDI_USAGE_DYNAMIC;break;
   case 6:failAllocation=true;break;
   case 7:file.bytes[25]=0;break; // Unsupported grayscale: original decoder.
   case 8:createOk=false;break;
   case 9:file.readOk=false;break;
   case 10:fullFilename="art/frontend/license.p3d";break;
   case 11:fullFilename="art/cars/l2r1.p3d";break;
   case 12:fullFilename="art/L1_TERRA.p3d";break;
   case 13:fullFilename="art/l02_fx.p3d";break;
  }
  char name[]="inventory-name";
  tTexture* texture=factory.ParseAsTexture(&file,name,file.bytes.size(),tImageHandler::IMG_PNG);
  if(scenario==9){assert(!texture && !stored && !decodes);}
  else {assert(texture && texture->name==name && stored==1);delete texture;}
  assert(allocations==frees);
  if(scenario==0){assert(creates==1 && uploads==1 && decodes==0);}
  if(scenario==1){assert(creates==1 && uploads==1 && decodes==1);}
  if(scenario>=2 && scenario<=6){assert(creates==0 && decodes==1 && uploads==0 && allocations==0);}
  if(scenario==7){assert(creates==0 && decodes==1 && uploads==0 && allocations==1);}
  if(scenario==8){assert(creates==1 && decodes==1 && uploads==0);}
  if(scenario>=10){
#ifdef SRR2_OLD3DS_NATIVE_ETC1
   if(scenario!=12){assert(creates==0 && decodes==1 && uploads==0 && allocations==0);}
   else {assert(creates==1 && uploads==1 && decodes==0);}
#else
   assert(creates==1 && uploads==1 && decodes==0);
#endif
  }
 }
 puts("PASS: production factory skips decode on hits, reuses allocation on misses, preserves names/store order, avoids rewind, and retains dynamic/font/movie/OOM/unsupported fallbacks.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-factory-') as temporary:
    folder=Path(temporary)
    (folder/'test.cpp').write_text(PRELUDE+body+TEST)
    subprocess.run(['clang++','-std=c++14','-O1','-g','-DSRR2_OLD3DS_NATIVE_TEXTURES',
                    '-fsanitize=address,undefined',str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test')],check=True)
    subprocess.run(['clang++','-std=c++14','-O1','-g','-DSRR2_OLD3DS_NATIVE_TEXTURES',
                    '-DSRR2_OLD3DS_NATIVE_ETC1','-I'+str(ROOT/'libs/pure3d'),
                    '-fsanitize=address,undefined',str(folder/'test.cpp'),'-o',str(folder/'test-etc1')],check=True)
    subprocess.run([str(folder/'test-etc1')],check=True)

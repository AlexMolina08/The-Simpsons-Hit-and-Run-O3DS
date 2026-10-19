#!/usr/bin/env python3
"""Run production texture conversion with VRAM promotion enabled."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
namespace={'__file__':str(Path(__file__).with_name('texture_upload.py'))}
exec(Path(__file__).with_name('texture_upload.py').read_text().split('with tempfile.TemporaryDirectory')[0],namespace)
source=namespace['source'];extract=namespace['extract'];prelude=namespace['prelude'];tests=namespace['tests']
prelude=prelude.replace('struct Tex {int width,height,maxLevel; void* data;};','struct Tex {int width,height,maxLevel; void* data;unsigned size=0;};using C3D_Tex=Tex;using u32=unsigned;')
prelude=prelude.replace('struct picaTexture {','struct picaVramAllocation;\nstruct picaTexture {').replace(' void UploadLevel(int);',' picaVramAllocation* vramAllocation=nullptr;void* reuploadLinear=nullptr;bool vramEligible=true,vramUploaded=false;void TryPromoteVram();\n void UploadLevel(int);')
prelude+=r'''
unsigned C3D_TexCalcTotalSize(unsigned size,unsigned level){return (size-(size>>(2*(level+1))))*4/3;}
int g_picaInFrame=0;
void*vramAlloc(unsigned n){return malloc(n);}void vramFree(void*p){free(p);}
unsigned long vramSpaceFree(){return 6*1024*1024;}
// Test upload targets belong to std::vector; production owns linear allocations.
void linearFree(void*){}
void C3D_SyncTextureCopy(u32*a,unsigned,u32*b,unsigned,unsigned n,unsigned flags){assert(flags==8);memcpy(b,a,n);}
'''
a=source.index('// Ownership nodes');b=source.index('\n#endif\n\nstatic const int PICA_STAGING_BPP',a)
methods=source[a:b]+'\n'+extract('static inline bool IsCompressed')+extract('void picaTexture::UploadLevel')+extract('void picaTexture::ConvertAndTile')
tests=tests.replace('  texture.UploadLevel(0);','  texture.tex.size=dw*dh*pixelBytes;texture.UploadLevel(0);')
tests=tests.replace('  fwrite(bytes.data(),1,bytes.size(),out);','''  if(texture.vramAllocation){unsigned size=C3D_TexCalcTotalSize(texture.tex.size,texture.tex.maxLevel);assert(!memcmp(texture.tex.data,bytes.data(),size));RetireVram(texture.vramAllocation);picaReclaimVramTextures();}
  fwrite(bytes.data(),1,bytes.size(),out);''')
if '--resource-prep' in sys.argv:
 header=(root/'libs/pure3d/pddi/pica/texture.hpp').read_text()
 start=header.index('C3D_Tex* GetC3DTexture(');opening=header.index('{',start);end=opening+1;depth=1
 while depth:depth+=(header[end]=='{')-(header[end]=='}');end+=1
 getter=header[start:end]
 prelude=prelude.replace('struct picaTexture {', 'unsigned fenceCount=0,copyCount=0;bool g_picaUnexposedResourcePrep=true;void picaPreparedBackendFence(){++fenceCount;}\nstruct picaTexture {'+getter).replace(' void UploadLevel(int);',' bool renderExposed=false;void UploadLevel(int);')
 prelude=prelude.replace('assert(flags==8);memcpy(b,a,n);','assert(flags==8);++copyCount;memcpy(b,a,n);')
 tests=tests.replace('texture.UploadLevel(0);assert(texture.valid);', '''texture.renderExposed=(cases&1)!=0;assert(!texture.GetC3DTexture());
  const unsigned beforeFences=fenceCount,beforeCopies=copyCount;
  texture.UploadLevel(0);assert(texture.valid);
  assert(fenceCount==beforeFences+unsigned(texture.renderExposed)+(copyCount-beforeCopies));
  assert(texture.GetC3DTexture()==&texture.tex && texture.renderExposed);
  texture.valid=false;assert(!texture.GetC3DTexture() && texture.renderExposed);texture.valid=true;''')
with tempfile.TemporaryDirectory(prefix='srr-vram-upload-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests)
 data=[]
 for tiles in [False,True]:
  output=p/('tiles.bin' if tiles else 'reference.bin');data.append(output)
  flags=['-DSRR2_OLD3DS_VRAM_TEXTURES']+(['-DSRR2_OLD3DS_FRAME_PIPELINE','-DSRR2_OLD3DS_PREPARED_BACKEND'] if '--resource-prep' in sys.argv else [])+(['-DSRR2_OLD3DS_TEXTURE_TILES'] if tiles else [])
  subprocess.run(['clang++','-std=c++14','-O2','-fsanitize=address,undefined',*flags,'-I'+str(root),'-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),str(root/'libs/pure3d/pddi/pica/swizzle.cpp'),str(root/'libs/pure3d/pddi/gl/decompress.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test'),str(output)],check=True)
 assert data[0].read_bytes()==data[1].read_bytes()
print('PASS: production upload and VRAM promotion retain every byte across five formats, DXT/RGB, mipmaps, scaling and tile/block paths')

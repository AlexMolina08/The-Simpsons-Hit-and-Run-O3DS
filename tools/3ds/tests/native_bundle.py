#!/usr/bin/env python3
"""Check indexed payload identity, bounded reads and independent yielded cursors."""
from pathlib import Path
import tempfile,hashlib,struct,zlib,subprocess,sys,shutil
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/3ds'))
from prepare_native_bundle import index_bundle
with tempfile.TemporaryDirectory(prefix='srr-bundle-') as directory:
 p=Path(directory);payloads=[]
 for i in range(5):
  source=bytes([i])*7;body=bytes((x+i)%256 for x in range(128));prefix=struct.pack('<11I',0x3158544e,1,8,8,256,8,8,1,0,len(source),len(body))+hashlib.sha256(source).digest();encoded=prefix+hashlib.sha256(body).digest()+body
  payloads.append((hashlib.sha256(prefix).hexdigest(),encoded))
 with (p/'native143.bundle').open('wb') as f:
  f.write(struct.pack('<II',0x314e4233,len(payloads)))
  for key,encoded in payloads:f.write((key+'.ntx').encode()+struct.pack('<II',len(encoded),zlib.crc32(encoded))+encoded)
 index_bundle(p/'native143.bundle',p/'bundle_index.hpp')
 texture=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text();start=texture.index('namespace {\nstruct NativeBundleSlot');end=texture.index('\n#endif\nbool g_picaNativeTexturesEnabled',start);ownership=texture[start:end]
 a=texture.index('bool picaNativeTexturesAvailable()');b=texture.index('bool picaTexture::UploadPreparedPNG',a);availability=texture[a:b].replace('stat(', 'ObservedStat(')
 source=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <CommonCrypto/CommonDigest.h>
#include "pddi/pica/nativetexture.hpp"
#include "pddi/pica/nativebundle.hpp"
#include "bundle_index.hpp"
static void* tls=(void*)1;void* getThreadLocalStorage(){return tls;}
@OWNERSHIP@
bool g_picaNativeTexturesEnabled=true,g_picaNativeBundleEnabled=true;
static std::string lastStatPath;
int ObservedStat(const char* path,struct stat* output){lastStatPath=path;return stat(path,output);}
@AVAILABILITY@
struct Hash{bool operator()(const void* p,size_t n,unsigned char* out){return CC_SHA256(p,n,out)!=nullptr;}};
std::string hex(const unsigned char* p,unsigned size){const char* digits="0123456789abcdef";std::string s;for(unsigned i=0;i<size;i++){s+=digits[p[i]>>4];s+=digits[p[i]&15];}s.resize(64,'0');return s;}
int main(){
 using namespace picaNativeBundle;Hash hash;
 assert(picaNativeTexturesAvailable()&&lastStatPath==BundlePath);
 g_picaNativeTexturesEnabled=false;assert(!picaNativeTexturesAvailable());g_picaNativeTexturesEnabled=true;
 for(unsigned i=0;i<EntryCount;++i){auto key=hex(Entries[i].key,sizeof(Entries[i].key));const auto* e=Find(Entries,EntryCount,key.c_str(),236);assert(e&&e->offset==Entries[i].offset);assert(!Find(Entries,EntryCount,key.c_str(),237));
  FILE* f=fopen("native143.bundle","rb");assert(f);assert(!fseek(f,e->offset,SEEK_SET));unsigned char header[108],body[144];assert(fread(header,1,108,f)==108);memset(body,0xa5,sizeof body);
  assert(!fseek(f,e->offset,SEEK_SET));assert(picaNativeTexture::Read(f,header,128,body,hash,nullptr,false));
  for(unsigned j=128;j<sizeof body;j++)assert(body[j]==0xa5);
  // Even a prefix lookup hit must reject a different full source identity.
  header[44]^=1;assert(!fseek(f,e->offset,SEEK_SET));assert(!picaNativeTexture::Read(f,header,128,body,hash,nullptr,false));header[44]^=1;
  assert(!fseek(f,e->offset,SEEK_SET));assert(picaNativeTexture::Read(f,header,128,body,hash)==(e->offset+e->bytes==BundleBytes));fclose(f);
 }
 assert(!Find(Entries,EntryCount,std::string(64,'z').c_str(),236));assert(!Find(Entries,EntryCount,std::string(64,'0').c_str(),236));
 tls=(void*)1;NativeBundleFile loader;assert(loader.file);assert(!fseek(loader.file,8,SEEK_SET));int first=fgetc(loader.file);assert(first>=0);
 tls=(void*)2;{NativeBundleFile main;assert(main.file&&main.file!=loader.file);assert(!fseek(main.file,300,SEEK_SET));fgetc(main.file);}
 tls=(void*)1;assert(ftell(loader.file)==9);{NativeBundleFile nested;assert(nested.file&&nested.file!=loader.file);assert(!fseek(nested.file,400,SEEK_SET));}
 assert(ftell(loader.file)==9);
 puts("PASS: exact index lookup/size binding, bounded embedded payload reads, legacy trailing-byte rejection, guards, yielded thread cursors and reentrant cursor isolation.");
}
'''.replace('@OWNERSHIP@',ownership).replace('@AVAILABILITY@',availability)
 (p/'test.cpp').write_text(source);subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-I'+str(root/'libs/pure3d'),'-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
 # Generator rejects a damaged payload before emitting a runtime index.
 damaged=bytearray((p/'native143.bundle').read_bytes());damaged[200]^=1;(p/'corrupt.bundle').write_bytes(damaged)
 try:index_bundle(p/'corrupt.bundle',p/'bad.hpp')
 except ValueError:pass
 else:raise AssertionError('Corrupt bundle accepted')
 assert not (p/'bad.hpp').exists()

 # Compact lookup changes only index storage. Every payload still passes the
 # complete descriptor/source SHA and payload SHA checks before publication.
 shutil.copy2(p/'native143.bundle',p/'native144.bundle')
 result=index_bundle(p/'native144.bundle',p/'bundle_index.hpp',16,'native144.bundle')
 assert result['index_bytes']==len(payloads)*24
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-I'+str(root/'libs/pure3d'),'-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)

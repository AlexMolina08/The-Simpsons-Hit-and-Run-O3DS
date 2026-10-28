#!/usr/bin/env python3
"""Exercise actual ETC1-first/exact-alpha fallback with source and payload checks."""
from pathlib import Path
import ast,hashlib,struct,subprocess,sys,tempfile,zlib
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/3ds'))
from prepare_native_textures import function
from prepare_native_bundle import index_bundle
module=ast.parse((root/'tools/3ds/tests/etc1_runtime.py').read_text())
prelude=next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='prelude' for t in n.targets))
prelude+='''\nnamespace radLoadTiming {enum {NativeTextureSeek};struct StageRegion{StageRegion(unsigned){}};}\nbool g_picaNativeExactWorldFallback=false;unsigned nativeExactWorldHits=0;\n'''
fragment=function((root/'libs/pure3d/pddi/pica/texture.cpp').read_text(),'bool picaTexture::ReadPrepared(')
first=b'world-encoded-image';alpha=b'transparent-alpha';etc=bytes([0x55])*8192;legacy=bytes((i*71+i//8)&255 for i in range(32768))
def record(source,version,format,body):
 prefix=struct.pack('<11I',0x3158544e,version,128,128,256,128,128,format,0,len(source),len(body))+hashlib.sha256(source).digest()
 encoded=prefix+hashlib.sha256(body).digest()+body;key=hashlib.sha256(prefix).hexdigest()
 return (key+'.ntx').encode()+struct.pack('<II',len(encoded),zlib.crc32(encoded))+encoded
main=r'''
int main(){
 std::vector<unsigned char> memory(32768+32,0xa5);
 const char source[]="world-encoded-image",alpha[]="transparent-alpha";
 // A prepared ETC1 record always wins, even when an exact record also exists.
 g_picaNativeExactWorldFallback=true;picaTexture opaque;opaque.tex.data=memory.data();
 assert(opaque.ReadPrepared(source,sizeof(source)-1,256,true));
 assert(opaque.hwFormat==GPU_ETC1&&nativeExactWorldHits==0);
 for(unsigned i=0;i<8192;++i)assert(memory[i]==0x55);
 // The disabled control retains the accepted miss/decoder path.
 g_picaNativeExactWorldFallback=false;picaTexture t;t.tex.data=memory.data();
 assert(!t.ReadPrepared(alpha,sizeof(alpha)-1,256,true));assert(t.hwFormat==GPU_RGBA4);
 g_picaNativeExactWorldFallback=true;
 assert(t.ReadPrepared(alpha,sizeof(alpha)-1,256,true));
 assert(t.hwFormat==GPU_RGBA4&&t.tex.fmt==GPU_RGBA4&&t.tex.size==32768&&t.tex.param==0x45002);
 assert(t.tex.maxLevel==0&&nativeExactWorldHits==1&&nativeTextureInvalid==0);
 for(unsigned i=0;i<32768;++i)assert(memory[i]==((i*71+i/8)&255));
 for(unsigned i=32768;i<memory.size();++i)assert(memory[i]==0xa5);
 picaTexture foreign;foreign.tex.data=memory.data();
 assert(!foreign.ReadPrepared(alpha,sizeof(alpha)-1,256,false)); // World scope unchanged.
 assert(!foreign.ReadPrepared("wrong-source",12,256,true));
 assert(foreign.hwFormat==GPU_RGBA4&&foreign.tex.size==32768);
 puts("PASS: accepted ETC1 preferred; exact transparent bytes and format preserved; disabled/world/source controls and destination guards.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-exact-world-test-') as directory:
 p=Path(directory);bundle=p/'native-mixed.bundle'
 bundle.write_bytes(struct.pack('<II',0x314e4233,3)+record(first,2,4,etc)+record(first,1,3,legacy)+record(alpha,1,3,legacy))
 index_bundle(bundle,p/'bundle_index.hpp',16,'native-mixed.bundle',True)
 (p/'test.cpp').write_text(prelude+fragment+main)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-Wno-deprecated-declarations','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_NATIVE_ETC1','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-I'+str(root/'libs/pure3d'),'-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)

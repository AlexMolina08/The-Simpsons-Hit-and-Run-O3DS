#!/usr/bin/env python3
"""Exercise exact linear fallback and disjoint timing of production VRAM copies."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
original=root/'tools/3ds/tests/vram_textures.py'
source=original.read_text();namespace={'__file__':str(original)}
exec(compile(source[:source.index('code=prelude')],str(original),'exec'),namespace)
prelude=namespace['prelude'].replace('#include <cassert>','#include <cassert>\n#include <cstdint>\nusing u64=uint64_t;\nstatic u64 clockTicks=10;\nconst unsigned SYSCLOCK_ARM11=1000000;\nu64 svcGetSystemTick(){return ++clockTicks;}\nnamespace NativeBench{bool collecting=false;}')
prelude=prelude.replace('++copyCount;memcpy(b,a,n);','++copyCount;memcpy(b,a,n);clockTicks+=137;')
extra=r'''
 {auto t=makeTexture();void* old=t.tex.data;std::vector<unsigned char> bytes((unsigned char*)old,(unsigned char*)old+t.tex.size);
  NativeBench::collecting=true;g_picaVramCapturePromote=false;unsigned copies=copyCount;
  t.TryPromoteVram();assert(t.tex.data==old&&!t.vramAllocation&&copyCount==copies&&vramCaptureSkipped==1);
  assert(!memcmp(t.tex.data,bytes.data(),bytes.size()));dispose(t);
 }
 {auto t=makeTexture();std::vector<unsigned char> bytes((unsigned char*)t.tex.data,(unsigned char*)t.tex.data+t.tex.size);
  g_picaVramCapturePromote=true;g_picaVramCopyDiagnostics=true;
  t.TryPromoteVram();assert(t.vramAllocation&&vramCaptureCopies==1&&vramCaptureCopyUs>=137);
  assert(vramCaptureMaxUs==vramCaptureAllocUs+vramCaptureCopyUs+vramCaptureReleaseUs);
  assert(!memcmp(t.tex.data,bytes.data(),bytes.size()));dispose(t);picaReclaimVramTextures();
 }
 NativeBench::collecting=false;g_picaVramCopyDiagnostics=false;freeVram=6*1024*1024;
'''
tests=namespace['tests'].replace('int main(){','int main(){'+extra)
text=namespace['source'];a=text.index('// Ownership nodes');b=text.index('\n#endif\n\nstatic const int PICA_STAGING_BPP',a)
code=prelude+text[a:b]+'\n'+namespace['extract']('static inline bool IsCompressed')+'\n'+namespace['extract']('void picaTexture::FreeStaging')+'\n'+namespace['extract']('pddiLockInfo* picaTexture::Lock')+tests
with tempfile.TemporaryDirectory(prefix='srr-vram-capture-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_VRAM_TEXTURES','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production capture control keeps exact linear texture bytes; timed promotion preserves bytes and disjoint allocation/copy/release accounting; original ownership and retirement cases pass.')

#!/usr/bin/env python3
"""Compare production affine multiplication with guarded disjoint outputs."""
from pathlib import Path
import subprocess,tempfile,json,re
root=Path(__file__).resolve().parents[3];text=(root/'libs/radmath/radmath/matrix.cpp').read_text()
a=text.index('void Matrix::Mult(const Matrix& a, const Matrix& b)');end=text.index('\nvoid Matrix::MultFull',a);method=text[a:end].strip()
common='struct Matrix {float m[4][4];void Mult(const Matrix&,const Matrix&);};\n'+method+'\n'
source=r'''
#pragma STDC FENV_ACCESS ON
#include <radmath/affineproduct.hpp>
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <limits>
'''
# Native/reference compilation retains the original assertion placement.
source+='namespace Reference {\n'+common+'}\n#define SRR2_OLD3DS_MATRIX_NO_ALIAS\nnamespace Optimized {\n'+common+'}\n'
source+=r'''
int main(){
 uint32_t state=291;unsigned checks=0;
 auto word=[&](){return state=state*1664525u+1013904223u;};
 for(int mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(mode));
  for(unsigned trial=0;trial<40000;++trial){
   Reference::Matrix a,b;
   for(auto* value:{&a,&b}) {
    for(unsigned r=0;r<4;++r){for(unsigned c=0;c<3;++c)value->m[r][c]=float(int(word()%100000)-50000)/256;value->m[r][3]=r==3?1:0;}
   }
   if(trial%101==0)a.m[1][0]=-0.f;
   if(trial%103==0)b.m[2][1]=std::numeric_limits<float>::denorm_min();
   if(trial%107==0)a.m[0][0]=std::numeric_limits<float>::infinity();
   if(trial%109==0){uint32_t q=0x7fc00000u|(word()&0x003fffffu);memcpy(&a.m[1][1],&q,4);q^=0x00008421u;memcpy(&b.m[1][1],&q,4);}
   for(unsigned alias=0;alias<4;++alias){
    Reference::Matrix x=a,y=b,z{};Optimized::Matrix p,q,r{};memcpy(&p,&a,64);memcpy(&q,&b,64);
    Reference::Matrix* expected=&z;Optimized::Matrix* actual=&r;
    if(alias==0){z.Mult(x,y);r.Mult(p,q);}
    if(alias==1){x.Mult(x,y);p.Mult(p,q);expected=&x;actual=&p;}
    if(alias==2){y.Mult(x,y);q.Mult(p,q);expected=&y;actual=&q;}
    if(alias==3){z.Mult(x,x);r.Mult(p,p);}
    assert(!memcmp(expected,actual,64));++checks;
   }
  }
 }
 unsigned char bytes[192];assert(rmtAffineProduct::Separate(bytes+64,bytes,bytes+128));
 assert(!rmtAffineProduct::Separate(bytes+4,bytes,bytes+128));assert(!rmtAffineProduct::Separate(bytes,bytes,bytes+128));
 printf("PASS: %u production affine products, all rounding modes, disjoint and both legacy aliases, shared inputs, signed zeros, subnormals, infinity and NaN payloads. Not native performance evidence.\n",checks);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-affine-alias-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/radmath'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 # Inspect actual ARM code shape before paying for another full native build.
 native='#include <radmath/affineproduct.hpp>\n#define assert(x) ((void)0)\nnamespace Reference {\n'+common+'}\n#define SRR2_OLD3DS_MATRIX_NO_ALIAS\nnamespace Optimized {\n'+common+'}\n'
 (p/'native.cpp').write_text(native)
 subprocess.run(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-g++','-std=c++17','-O2','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-fno-ipa-icf','-I',str(root/'libs/radmath'),'-c',str(p/'native.cpp'),'-o',str(p/'native.o')],check=True)
 disassembly=subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-objdump','-dC',str(p/'native.o')],text=True)
 print('ARM object code generated; inspect reference/optimized load counts separately. No native cycle claim.')
 out=root/'out/whole-frame-86';out.mkdir(exist_ok=True);(out/'matrix-kernels.arm.txt').write_text(disassembly)
 (out/'native-kernels.cpp').write_text(native)

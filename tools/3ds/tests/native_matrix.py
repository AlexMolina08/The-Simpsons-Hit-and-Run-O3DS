#!/usr/bin/env python3
"""Check scalar provenance and validation-mode isolation; not ARM kernel proof."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/radmath/radmath/nativematrix.cpp').read_text()
original=(root/'libs/radmath/radmath/matrix.cpp').read_text()
original=original[original.index('    m[0][0] =',original.index('void Matrix::Mult(const Matrix& a')):original.index('\n\n\n}',original.index('void Matrix::Mult(const Matrix& a'))]
original=original.replace('a.m[','a[').replace('b.m[','b[').replace('    m[','    out[')
assert original in source,'Scalar fallback must retain original expression order'
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
using u64=unsigned long long;const unsigned SYSCLOCK_ARM11=268111856;
u64 svcGetSystemTick(){static u64 tick;return ++tick;}
namespace old3dsMatrix {bool TryMultiply(float*,const float*,const float*);}
'''
source='\n'.join(l for l in source.splitlines() if not l.startswith('#include'))
test=r'''
bool corrupt=false,allow=true;
namespace old3dsMatrix {
bool TryMultiply(float* out,const float* a,const float* b){
 if(!allow || out==a || out==b)return false;
 scalar(reinterpret_cast<float(*)[4]>(out),reinterpret_cast<const float(*)[4]>(a),reinterpret_cast<const float(*)[4]>(b));
 if(corrupt)out[0]+=1.0f;
 return true;
}}
int main(){
 float a[4][4],b[4][4],expected[4][4],actual[4][4];
 for(unsigned mode=0;mode<3;++mode)for(unsigned n=0;n<10000;++n){
  for(unsigned i=0;i<16;++i){a[i/4][i%4]=float(int((n+i*7)%97)-48)*.125f;b[i/4][i%4]=float(int((n+i*13)%89)-44)*.125f;}
  allow=n%3!=0;corrupt=mode==2 && n%2==0;g_nativeMatrixMode=mode;g_nativeMatrixCollecting=true;
  scalar(expected,a,b);old3dsNativeMatrixMultiply(actual,a,b);assert(!memcmp(expected,actual,sizeof(actual)));
  memcpy(actual,a,sizeof(a));memcpy(expected,a,sizeof(a));scalar(expected,expected,b);old3dsNativeMatrixMultiply(actual,actual,b);assert(!memcmp(expected,actual,sizeof(actual)));
 }
 assert(allMismatches>0 && capture.mismatches==allMismatches);
 puts("PASS: original scalar expressions; 60000 mode/alias cases; injected vector errors never escape validation mode. ARM arithmetic requires hardware validation.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-matrix-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(prelude+source+test)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

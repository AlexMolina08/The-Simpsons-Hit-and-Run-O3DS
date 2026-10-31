#!/usr/bin/env python3
"""Compile the actual complete-set admission codec against a prepared catalog."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--set', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
code = r'''#include <worldresidencymanifest.hpp>
#include <CommonCrypto/CommonDigest.h>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace WorldResidencyAssets;
struct Hash{bool operator()(const void* data,unsigned bytes,unsigned char* result){return CC_SHA256(data,bytes,result)!=nullptr;}};
static void Digest(Manifest& manifest,Hash&,bool update=false){
 unsigned char result[32];CC_SHA256(manifest.records,sizeof(manifest.records),result);if(update)memcpy(manifest.header+48,result,32);
}
int main(int argc,char** argv){
 assert(argc==2);Manifest original;FILE* file=fopen(argv[1],"rb");assert(file && fread(&original,1,sizeof(original),file)==sizeof(original) && fgetc(file)==EOF);fclose(file);
 char native[65];const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;++i){native[i*2]=hex[original.header[16+i]>>4];native[i*2+1]=hex[original.header[16+i]&15];}native[64]=0;
 Hash hash;Digest(original,hash);assert(original.Validate(hash,native));
 for(unsigned i=0;i<ManifestCount;++i){char path[48];assert(AssetPath(i,path));auto* record=original.Find(path);assert(record && record->bytes);}
 char path[48];assert(!AssetPath(ManifestCount,path) && !original.Find("Save1") && !original.Find(nullptr));
 auto bad=original;bad.header[0]^=1;assert(!bad.Validate(hash,native));
 bad=original;bad.header[8]--;assert(!bad.Validate(hash,native));
 bad=original;bad.header[12]--;assert(!bad.Validate(hash,native));
 bad=original;bad.header[16]^=1;assert(!bad.Validate(hash,native));
 assert(!original.Validate(hash,"bad") && !original.Validate(hash,nullptr));
 bad=original;bad.records[0].bytes++;assert(!bad.Validate(hash,native));
 bad=original;strcpy(bad.records[0].path,"art/../Save1");Digest(bad,hash,true);assert(!bad.Validate(hash,native));
 bad=original;bad.records[0].path[strlen(bad.records[0].path)+1]='x';Digest(bad,hash,true);assert(!bad.Validate(hash,native));
 bad=original;memset(bad.records[0].path,'x',sizeof(bad.records[0].path));Digest(bad,hash,true);assert(!bad.Validate(hash,native));
 bad=original;bad.records[1]=bad.records[0];Digest(bad,hash,true);assert(!bad.Validate(hash,native));
 bad=original;auto saved=bad.records[1];bad.records[1]=bad.records[2];bad.records[2]=saved;Digest(bad,hash,true);assert(!bad.Validate(hash,native));
 for(unsigned bytes:{0u,64u*1024*1024+1}){bad=original;bad.records[0].bytes=bytes;Digest(bad,hash,true);assert(!bad.Validate(hash,native));}
}
'''.replace('#include <cassert>', '#include <cassert>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='world-residency-manifest-') as directory:
    work = Path(directory)
    (work/'test.cpp').write_text(code)
    subprocess.run(['clang++', '-std=c++17', '-Wno-deprecated-declarations', '-fsanitize=address,undefined',
                    '-I'+str(root/'libs/radcore/inc'), str(work/'test.cpp'), '-o', str(work/'test')], check=True)
    subprocess.run([str(work/'test'), str((args.set/'asset-admission.bin').resolve())], check=True)
print('PASS: actual 87-file manifest validates canonical names and bundle/body identities; corruption, duplicates, traversal, padding and invalid sizes fail closed.')

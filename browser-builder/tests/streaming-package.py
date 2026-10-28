#!/usr/bin/env python3
"""Check the actual pinned makerom stream adapter, including 64-bit file offsets."""
from pathlib import Path
import argparse
import hashlib
import runpy
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=ROOT/'out/browser-tools/Project_CTR/makerom')
args = parser.parse_args()
patch = runpy.run_path(str(ROOT/'browser-builder/scripts/makerom-streaming.py'))['patch']

HARNESS = r'''
#include "lib.h"
#include "ncch_build.h"
#include "romfs.h"
#include "stream_package.h"
#include <mbedtls/sha256.h>
#include <assert.h>
int fseek_64(FILE *file, u64 offset) {return fseeko(file, offset, SEEK_SET);}
u64 align(u64 value, u64 boundary) {return (value + boundary - 1) & ~(boundary - 1);}
u32 u8_to_u32(const u8 *bytes, u8 endian) {assert(!endian);return bytes[0]|bytes[1]<<8|bytes[2]<<16|bytes[3]<<24;}
void ShaCalc(void *bytes, u64 size, u8 *hash, int mode) {assert(mode==CTR_SHA_256);mbedtls_sha256(bytes,size,hash,0);}
void BuildRomfsHeader(romfs_buildctx *ctx) {(void)ctx;assert(0);}
void PopulateRomfs(romfs_buildctx *ctx) {(void)ctx;assert(0);}
void BuildIvfcHeader(romfs_buildctx *ctx) {(void)ctx;assert(0);}
static const char *sourcePath=SOURCE_FILE;
int main(void) {
    const u64 high=(u64)3*1024*1024*1024;
    u8 source[32], hash[32], expected[32], prefix[512]={0};
    for(unsigned i=0;i<32;++i)source[i]=(i*37u+91u)&255;
    FILE *input=fopen(sourcePath,"wb");assert(input);
    assert(fwrite(source,1,32,input)==32);fclose(input);
    SrrStreamConfigure(true);assert(SrrStreamRequested()&&!SrrStreamActive());
    assert(!SrrStreamBegin(high+32,512,0));
    SrrStreamFile(sourcePath,high,32);
    assert(!SrrStreamFinishNcch(prefix));
    assert(!SrrStreamHash(high,32,hash));ShaCalc(source,32,expected,CTR_SHA_256);
    assert(!memcmp(hash,expected,32));
    input=fopen(SRR_STREAM_FILE,"rb");assert(input);assert(!fseek_64(input,high-1));
    assert(fgetc(input)==0);assert(fgetc(input)==source[0]);fclose(input);
    assert(SrrStreamHash(high+1,32,hash));SrrStreamClose();
    /* A complete bounded copy preserves prefix, gaps, unaligned payload and EOF. */
    SrrStreamConfigure(true);assert(!SrrStreamBegin(32768,512,256));
    SrrStreamFile(sourcePath,17000,32);assert(!SrrStreamFinishNcch(prefix));
    FILE *output=tmpfile();assert(output);assert(!SrrStreamCopy(output,73,32768));
    assert(!fseek_64(output,73+17000));u8 copied[32];assert(fread(copied,1,32,output)==32);
    assert(!memcmp(copied,source,32));assert(!fseek_64(output,73+32768));assert(fgetc(output)==EOF);
    fclose(output);SrrStreamClose();
    /* Missing and truncated assets fail closed; a new run can recover. */
    SrrStreamConfigure(true);assert(!SrrStreamBegin(32768,512,256));
    SrrStreamFile(sourcePath,17000,33);assert(SrrStreamFinishNcch(prefix));SrrStreamClose();
    SrrStreamConfigure(true);assert(SrrStreamBegin(256,512,0));SrrStreamClose();
    remove(SRR_STREAM_FILE);remove(sourcePath);
    puts("PASS: actual stream adapter: offsets beyond 2 GiB, sparse zero padding, hash/copy parity, bounded sizes, short reads and recovery.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-stream-package-') as directory:
    temporary = Path(directory)
    source = temporary/'src'
    shutil.copytree(args.source/'src', source)
    patch(source)
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()}
    patch(source)
    assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()}
    (temporary/'test.c').write_text(HARNESS)
    dependencies = args.source/'deps/libmbedtls'
    binary = temporary/'test'
    subprocess.run(['clang', '-std=c11', '-O2', '-D_GNU_SOURCE', '-DSRR_BROWSER_STREAMING',
        '-DSRR_STREAM_FILE="'+str(temporary/'content.ncch')+'"',
        '-DSOURCE_FILE="'+str(temporary/'asset.bin')+'"',
        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
        '-I'+str(source), '-I'+str(dependencies/'include'),
        '-I'+str(args.source/'deps/libyaml/include'), str(temporary/'test.c'),
        str(source/'stream_package.c'), str(dependencies/'src/sha256.c'),
        str(dependencies/'src/platform_util.c'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: makerom source patch is idempotent; native/WASM construction uses the same IVFC/NCCH metadata and signing seams.')

#!/usr/bin/env python3
"""Prove native payload identity, allocation policy keys and safe rejection."""
import hashlib
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/3ds'))
from prepare_native_textures import HEADER, build_converter, texture_images, prepare, variants


def chunk(kind, body=b'', children=b''):
    return struct.pack('<III', kind, 12+len(body), 12+len(body)+len(children)) + body + children


def asset(image, width, height, alpha=8, texture_type=0, usage=0):
    image_chunk = chunk(0x19001, b'\x04test' + struct.pack('<8I', 14000, width, height, 32, 0, 1, 1, 0),
                        chunk(0x19002, struct.pack('<I', len(image)) + image))
    return chunk(0xFF443350, children=chunk(0x19000, b'\x04test' + struct.pack('<9I',
                 14000, width, height, 32, alpha, 1, texture_type, usage, 0), image_chunk))


READER = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
#include "pddi/pica/nativetexture.hpp"
struct Hash {
 bool operator()(const void* bytes,size_t size,unsigned char* out){
#ifdef __APPLE__
  return CC_SHA256(bytes,static_cast<CC_LONG>(size),out)!=nullptr;
#else
  return SHA256(static_cast<const unsigned char*>(bytes),size,out)!=nullptr;
#endif
 }
};
unsigned Get(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
int main(int argc,char** argv){
 assert(argc==5);FILE* f=fopen(argv[3],"rb");assert(f);unsigned char wire[44];
 assert(fread(wire,1,44,f)==44);fclose(f);unsigned fields[9];for(unsigned i=0;i<9;++i)fields[i]=Get(wire+8+4*i);
 const picaNativeTexture::Description d={fields[0],fields[1],fields[2],fields[3],fields[4],fields[5],fields[6],fields[7],fields[8]};
 std::vector<unsigned char> source(d.sourceBytes),output(d.bytes+64,0xa5);
 f=fopen(argv[2],"rb");assert(f);assert(fread(source.data(),1,source.size(),f)==source.size());fclose(f);
 Hash hash;unsigned char expected[picaNativeTexture::HeaderBytes];char key[65];
 assert(picaNativeTexture::Key(d,source.data(),hash,expected,key));puts(key);
 f=fopen(argv[1],"rb");assert(f);bool ok=picaNativeTexture::Read(f,expected,d.bytes,output.data(),hash);fclose(f);
 for(size_t i=d.bytes;i<output.size();++i)assert(output[i]==0xa5);
 if(!ok)return 2;
 f=fopen(argv[4],"wb");assert(f);assert(fwrite(output.data(),1,d.bytes,f)==d.bytes);fclose(f);return 0;
}
'''


with tempfile.TemporaryDirectory(prefix='srr-native-payload-test-') as temporary:
    folder = Path(temporary)
    reader = folder / 'reader'
    (folder / 'reader.cpp').write_text(READER)
    link = [] if sys.platform == 'darwin' else ['-lcrypto']
    subprocess.run(['clang++', '-std=c++14', '-O1', '-g', '-Wno-deprecated-declarations',
                    '-fsanitize=address,undefined', '-I', str(ROOT / 'libs/pure3d'),
                    str(folder / 'reader.cpp'), *link, '-o', str(reader)], check=True)
    sources = []
    for mode in ('RGB', 'RGBA', 'P'):
        for size in ((8, 8), (32, 16), (512, 32)):
            w, h = size
            rgba = Image.frombytes('RGBA', size, bytes((i*53+i//7) & 255 for i in range(w*h*4)))
            image = rgba.convert(mode)
            encoded = io.BytesIO()
            image.save(encoded, format='PNG', bits=4 if mode == 'P' else 8)
            path = folder / f'{mode}-{w}.p3d'
            path.write_bytes(asset(encoded.getvalue(), w, h, alpha=8))
            sources.append(path)
    for kind in range(1, 6):
        w, h = 64, 32
        raw = bytes((i*37+i//3) & 255 for i in range((w//4)*(h//4)*(8 if kind == 1 else 16)))
        dds = bytearray(128)
        dds[:4] = b'DDS '
        struct.pack_into('<5I', dds, 4, 124, 0x80000, h, w, len(raw))
        dds[84:88] = f'DXT{kind}'.encode()
        path = folder / f'dxt{kind}.p3d'
        path.write_bytes(asset(dds+raw, w, h, texture_type=kind+3))
        sources.append(path)
    out = folder / 'native-textures/v1'
    manifest = prepare(sources, out)
    raw_inputs = {}
    for source in sources:
        for w, h, kind, raw, wire_format, encoded_source in texture_images(source.read_bytes()):
            for width, height, levels in variants(w, h):
                size = sum((width >> i)*(height >> i)*2 for i in range(levels+1))
                prefix = HEADER.pack(0x3158544E, 1, w, h, 256 if kind == 0 else kind, width, height, wire_format, levels, len(encoded_source), size)
                key = hashlib.sha256(prefix+hashlib.sha256(encoded_source).digest()).hexdigest()
                raw_inputs[key] = encoded_source
    assert manifest['textures'] == len(sources)
    cases = rejected = 0
    for key in manifest['payloads']:
        payload = out / (key+'.ntx')
        encoded = payload.read_bytes()
        source, description, output = folder/'source.bin', folder/'description.bin', folder/'result.bin'
        source.write_bytes(raw_inputs[key])
        description.write_bytes(encoded[:44])
        result = subprocess.run([str(reader), str(payload), str(source), str(description), str(output)], capture_output=True, check=True)
        assert result.stdout.decode().strip() == key
        assert output.read_bytes() == encoded[108:]
        cases += 1
        corruptions = [encoded[:n] for n in (0, 43, 75, 107, len(encoded)-1)]
        corruptions += [encoded+b'\0']
        for offset in (0, 4, 8, 24, 32, 36, 40, 44, 76, 108, len(encoded)-1):
            damaged = bytearray(encoded)
            damaged[offset] ^= 1
            corruptions.append(damaged)
        for damaged in corruptions:
            bad = folder/'bad.ntx'
            bad.write_bytes(damaged)
            result = subprocess.run([str(reader), str(bad), str(source), str(description), str(output)], capture_output=True)
            assert result.returncode == 2, result.stderr.decode()
            rejected += 1
    # Dynamic assets and font/volume trees must never produce prepared payloads.
    first = sources[0].read_bytes()
    image = io.BytesIO();Image.new('RGBA', (16, 16), (1, 2, 3, 4)).save(image, format='PNG')
    assert not list(texture_images(asset(image.getvalue(), 16, 16, usage=1)))
    assert not list(texture_images(chunk(0x22000, children=first)))
    assert not list(texture_images(chunk(0x19004, children=first)))
    # Opt-in standalone images produce the same upload source/format as their
    # ordinary static texture equivalent. Default traversal remains unchanged.
    standalone = chunk(0x19001, b'\x04test' + struct.pack('<7I',
                       14000, 16, 16, 32, 0, 1, 1),
                       chunk(0x19002, struct.pack('<I', len(image.getvalue())) + image.getvalue()))
    assert not list(texture_images(standalone))
    assert list(texture_images(standalone, True)) == list(texture_images(asset(image.getvalue(), 16, 16)))
    assert not list(texture_images(chunk(0x22000, children=standalone), True))
    assert not list(texture_images(chunk(0x19004, children=standalone), True))
    assert not list(texture_images(asset(image.getvalue(), 16, 16, usage=1), True))
    print(f'PASS: {cases} payload reads, {rejected} corruption/truncation/identity rejections; allocation keys, guards and unsupported fallbacks verified.')

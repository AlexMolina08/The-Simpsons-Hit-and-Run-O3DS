#!/usr/bin/env python3
"""Prepare optional native static texture payloads without modifying source assets.

The host converter compiles the production upload functions. Files contain only
explicit little-endian fields and bytes, not pointers or SDK structures. Version
1 covers uncompressed P3D containers containing ordinary PNG/DDS image chunks.
Unsupported assets retain the legacy loader. Install payloads under the game's
native-textures/v1 directory only after correctness and physical acceptance.
"""
import argparse
import hashlib
import json
import io
from pathlib import Path
import struct
import subprocess
import tempfile
import sys

from skin_inventory import chunks, pstring
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
VERSION = 1
HEADER = struct.Struct('<11I')


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def build_converter(folder):
    source = (ROOT / 'libs/pure3d/pddi/pica/texture.cpp').read_text()
    prelude = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "pddi/pica/texturetile.hpp"
#include "pddi/pica/swizzle.hpp"
#include "pddi/gl/decompress.h"
enum {GPU_RGBA8, GPU_RGBA5551, GPU_LA8, GPU_RGBA4, GPU_RGB565};
using pddiTextureType=int;
enum {PDDI_TEXTYPE_RGB, PDDI_TEXTYPE_DXT1, PDDI_TEXTYPE_DXT2,
      PDDI_TEXTYPE_DXT3, PDDI_TEXTYPE_DXT4, PDDI_TEXTYPE_DXT5};
struct Tex {int width,height,maxLevel;void* data;};
void C3D_TexFlush(Tex*) {}
void* C3D_TexGetImagePtr(Tex* t,void* data,int mip,void*) {
 size_t offset=0;int w=t->width,h=t->height;
 for(int i=0;i<mip;++i){offset+=w*h*2;w/=2;h/=2;}
 return static_cast<char*>(data)+offset;
}
void (*g_picaYieldHook)()=nullptr;
struct picaTexture {
 int xSize,ySize,type,hwFormat;Tex tex;unsigned char* staging;bool valid=false;
 void UploadLevel(int);void ConvertAndTile(const unsigned int*,int,int,void*);
};
'''
    main = r'''
int main(){
 unsigned request[8];
 while(fread(request,sizeof(request),1,stdin)==1){
  const unsigned w=request[0],h=request[1],kind=request[2],dw=request[3],dh=request[4];
  const unsigned format=request[5],levels=request[6],size=request[7];
  if(!w || w>1024 || !h || h>1024 || kind>5 || dw<8 || dw>1024 ||
     dh<8 || dh>1024 || (dw&(dw-1)) || (dh&(dh-1)) || levels>7 ||
     (dw>>levels)<8 || (dh>>levels)<8 || format<1 || format>3 ||
     size!=(kind?((w+3)/4)*((h+3)/4)*(kind==1?8:16):w*h*4))return 2;
  std::vector<unsigned> input((size+3)/4);
  if(fread(input.data(),1,size,stdin)!=size)return 3;
  size_t bytes=0;for(unsigned i=0;i<=levels;++i)bytes+=(dw>>i)*(dh>>i)*2;
  std::vector<unsigned char> output(bytes+32,0xa5);
  const int gpu=format==1?GPU_RGB565:format==2?GPU_RGBA5551:GPU_RGBA4;
  picaTexture texture{int(w),int(h),int(kind),gpu,{int(dw),int(dh),int(levels),output.data()},
      reinterpret_cast<unsigned char*>(input.data()),false};
  texture.UploadLevel(0);if(!texture.valid)return 4;
  for(size_t i=bytes;i<output.size();++i)if(output[i]!=0xa5)return 5;
  if(fwrite(output.data(),1,bytes,stdout)!=bytes || fflush(stdout))return 6;
 }
 return ferror(stdin)?7:0;
}
'''
    translation = prelude + function(source, 'static inline bool IsCompressed')
    translation += function(source, 'void picaTexture::UploadLevel')
    translation += function(source, 'void picaTexture::ConvertAndTile') + main
    cpp, binary = folder / 'convert.cpp', folder / 'convert'
    cpp.write_text(translation)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-DSRR2_OLD3DS_TEXTURE_TILES',
                    '-I', str(ROOT / 'libs/pure3d'), str(cpp),
                    str(ROOT / 'libs/pure3d/pddi/pica/swizzle.cpp'),
                    str(ROOT / 'libs/pure3d/pddi/gl/decompress.cpp'), '-o', str(binary)], check=True)
    return binary, hashlib.sha256(translation.encode()).hexdigest()


def texture_images(data, include_images=False):
    """Static texture data; optionally prepare standalone images, excluding fonts/volumes.

    Standalone preparation does not authorize runtime use. The image factory
    still checks static usage, depth, font/full-size exclusions and exact keys.
    """
    def walk(start, end, texture=False, ordinary=False, specification=None):
        for kind, body, child, stop in chunks(data, start, end):
            if kind in (0x22000, 0x22002, 0x19004):
                continue
            is_texture = kind == 0x19000
            static = ordinary
            if is_texture:
                _, offset = pstring(data, body)
                fields = struct.unpack_from('<9I', data, offset)
                static = fields[7] == 0  # PDDI_USAGE_STATIC
                specification = fields
            if kind == 0x19001 and not texture and include_images:
                # Scrooby image factories use default RGB/static/8-bit alpha
                # hints. DDS handling selects its own 5551/4444 destination.
                _, offset = pstring(data, body)
                image_fields = struct.unpack_from('<7I', data, offset)
                specification = (image_fields[0], image_fields[1], image_fields[2],
                                 image_fields[3], 8, 0, 0, 0, 0)
                static = True
            if kind == 0x19002 and texture and ordinary:
                size, = struct.unpack_from('<I', data, body)
                image = data[body+4:child]
                if (size <= len(image) and size >= 33 and image[:8] == b'\x89PNG\r\n\x1a\n'
                        and specification and specification[6] in (0, 1)):
                    depth, colour = image[24], image[25]
                    # Match the production PNG handler's supported, noninterlaced
                    # cases. 16-bit lock packing and uncommon images keep fallback.
                    if (image[28] == 0 and ((colour == 3 and depth in (4, 8)) or
                            (colour in (2, 6) and depth == 8)) and specification[3] != 16):
                        with Image.open(io.BytesIO(image[:size])) as decoded:
                            w, h = decoded.size
                            if 0 < w <= 1024 and 0 < h <= 1024:
                                pixels = decoded.convert('RGBA')
                                if colour == 2:
                                    pixels.putalpha(255)  # RGB tRNS is ignored by the legacy handler.
                                alpha = specification[4]
                                wire_format = 1 if alpha == 0 else 2 if alpha == 1 else 3
                                yield w, h, 0, pixels.tobytes(), wire_format, image[:size]
                if size <= len(image) and size >= 128 and image[:4] == b'DDS ':
                    image = image[:size]
                    flags, h, w, linear = struct.unpack_from('<4I', image, 8)
                    fourcc = image[84:88]
                    if (struct.unpack_from('<I', image, 4)[0] == 124 and flags & 0x80000
                            and fourcc in (b'DXT1', b'DXT2', b'DXT3', b'DXT4', b'DXT5')
                            and 0 < w <= 1024 and 0 < h <= 1024):
                        kind_id = int(chr(fourcc[3]))
                        needed = ((w+3)//4)*((h+3)//4)*(8 if kind_id == 1 else 16)
                        if linear == needed and len(image) >= 128+needed:
                            yield w, h, kind_id, image[128:128+needed], 2 if kind_id == 1 else 3, image[128:128+needed]
            yield from walk(child, stop, texture or is_texture or (include_images and kind == 0x19001), static, specification)
    yield from walk(0, len(data))


def variants(w, h):
    width, height = max(8, 1 << (w-1).bit_length()), max(8, 1 << (h-1).bit_length())
    small_w, small_h = width, height
    while (small_w > 256 or small_h > 256) and small_w > 8 and small_h > 8:
        small_w //= 2
        small_h //= 2
    yield from sorted({(small_w, small_h, 0), (width, height, 0),
                       (width, height, min(width, height).bit_length()-4)})


def prepare(assets, out, limit=None, include_images=False):
    if limit is not None and limit < 1:
        raise ValueError('Texture limit must be positive')
    if sys.byteorder != 'little':
        raise ValueError('The production upload converter requires a little-endian host')
    out.mkdir(parents=True, exist_ok=True)
    entries, skipped, textures = {}, [], 0
    with tempfile.TemporaryDirectory(prefix='srr-native-texture-') as temporary:
        binary, converter_hash = build_converter(Path(temporary))
        with subprocess.Popen([str(binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE) as process:
            try:
                for asset in assets:
                    data = asset.read_bytes()
                    try:
                        images = list(texture_images(data, include_images))
                    except (ValueError, struct.error, IndexError, UnicodeError) as error:
                        skipped.append({'path': str(asset), 'reason': str(error)})
                        continue
                    for w, h, kind, raw, wire_format, source_bytes in images:
                        if limit is not None and textures >= limit:
                            break
                        textures += 1
                        for width, height, levels in variants(w, h):
                            size = sum((width >> i)*(height >> i)*2 for i in range(levels+1))
                            source_kind = 256 if kind == 0 else kind
                            descriptor = HEADER.pack(0x3158544E, VERSION, w, h, source_kind, width, height,
                                                     wire_format, levels, len(source_bytes), size)
                            prefix = descriptor + hashlib.sha256(source_bytes).digest()
                            key = hashlib.sha256(prefix).hexdigest()
                            if key in entries:
                                continue
                            process.stdin.write(struct.pack('<8I', w, h, kind, width, height, wire_format, levels, len(raw)) + raw)
                            process.stdin.flush()
                            payload = process.stdout.read(size)
                            if len(payload) != size:
                                raise RuntimeError('Native converter failed or returned an incomplete image')
                            encoded = prefix + hashlib.sha256(payload).digest() + payload
                            target = out / (key + '.ntx')
                            if not target.exists() or target.read_bytes() != encoded:
                                temporary_file = target.with_suffix('.tmp')
                                temporary_file.write_bytes(encoded)
                                temporary_file.replace(target)
                            entries[key] = {'source': str(asset), 'source_sha256': hashlib.sha256(source_bytes).hexdigest(),
                                            'sha256': hashlib.sha256(encoded).hexdigest(), 'bytes': len(encoded),
                                            'width': width, 'height': height, 'levels': levels, 'kind': source_kind}
                    if limit is not None and textures >= limit:
                        break
            finally:
                process.stdin.close()
                if process.wait() != 0:
                    raise RuntimeError('Native converter exited unsuccessfully')
    if not textures:
        raise ValueError('No supported static texture images found; no prepared bundle was produced')
    manifest = {'version': VERSION, 'standalone_images': include_images, 'converter_sha256': converter_hash, 'textures': textures,
                'payloads': entries, 'unsupported_containers': skipped,
                'validation': 'Preparation only; physical performance and allocation acceptance pending'}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path, nargs='+')
    parser.add_argument('--out', type=Path, required=True, help='Payload directory; normally native-textures/v1')
    parser.add_argument('--limit', type=int, help='Bound source textures during correctness development')
    parser.add_argument('--include-static-images', action='store_true',
                        help='Also prepare standalone image chunks; runtime eligibility remains unchanged')
    args = parser.parse_args()
    assets = sorted({item for root in args.assets for item in (root.rglob('*.p3d') if root.is_dir() else [root])})
    result = prepare(assets, args.out, args.limit, args.include_static_images)
    print(json.dumps({'textures': result['textures'], 'payloads': len(result['payloads']),
                      'unsupported_containers': len(result['unsupported_containers']), 'out': str(args.out)}))


if __name__ == '__main__':
    main()

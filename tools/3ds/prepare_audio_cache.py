#!/usr/bin/env python3
"""Prepare a bounded exact-PCM-keyed DSP clip cache from original PC archives."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LIMIT = 2*1024*1024


def members(path):
    with path.open('rb') as source:
        size = source.seek(0, 2)
        source.seek(0)
        header = source.read(48)
        if not header.startswith(b'RADCORE CEMENT LIBRARY') or header[34] != 0:
            raise ValueError('Unsupported archive: '+str(path))
        offset, = struct.unpack_from('<I', header, 44)
        if offset+16 > size:
            raise ValueError('Archive directory bounds')
        source.seek(offset)
        count, _, _, _ = struct.unpack('<4I', source.read(16))
        if count > 100000 or offset+16+count*12 > size:
            raise ValueError('Archive member count')
        entries = [struct.unpack('<3I', source.read(12)) for _ in range(count)]
        for uid, start, length in entries:
            if length < 20 or start+length > size:
                raise ValueError('Archive member bounds')
            source.seek(start)
            header = source.read(20)
            if header[:8] != b'RSD4PCM ' or length < 2048:
                continue
            channels, bits, rate = struct.unpack_from('<3I', header, 8)
            if channels == 1 and bits == 16 and (length-2048) % 2 == 0:
                yield dict(archive=path.name, uid=uid, offset=start, length=length,
                           samples=(length-2048)//2, sample_rate=rate)


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte)*16777619) & 0xffffffff
    return value


def encoder_source():
    source = (ROOT/'libs/radsound/src/hal/ctr/ndsphal.cpp').read_text()
    fragment = source[source.index('static u16 s_AdpcmCoefs'):source.index('class radSoundHalBufferCtr')]
    return r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using u16=uint16_t;using s16=int16_t;using u8=uint8_t;
#define NB_SCOPE(s) ((void)0)
namespace Old3dsPerformance { bool ReducedAudioSearch(){return true;} }
''' + fragment + r'''
int main(){
 unsigned samples;
 while(std::fread(&samples,4,1,stdin)==1){
  if(samples<8192 || samples>441000)return 2;
  std::vector<s16> pcm(samples);
  if(std::fread(pcm.data(),2,samples,stdin)!=samples)return 3;
  auto expected=pcm;
  const unsigned exact=AdpcmEncodeInPlaceKernel<3>(expected.data(),samples);
  const unsigned bytes=AdpcmEncodeInPlace(pcm.data(),samples);
  if(bytes!=exact || std::memcmp(pcm.data(),expected.data(),bytes))return 4;
  if(std::fwrite(&bytes,4,1,stdout)!=1 || std::fwrite(pcm.data(),1,bytes,stdout)!=bytes)return 5;
  std::fflush(stdout);
 }
}
'''


def prepare(data, output, samples, archives):
    if output.exists():
        raise ValueError('Output already exists: '+str(output))
    if any(value < 8192 or value > 441000 for value in samples):
        raise ValueError('Prepared clips require 8192 to 441000 samples')
    selected = [member for name in archives for member in members(data/name)
                if member['samples'] in samples]
    selected.sort(key=lambda r: (r['samples'], r['archive'], r['uid']))
    if not selected or len(selected) > 256:
        raise ValueError('Select between 1 and 256 clips')
    size = 32+16*len(selected)
    for record in selected:
        size = (size+31)&~31
        size += record['samples']*2
        size = (size+31)&~31
        size += ((record['samples']+13)//14)*8
    if size > LIMIT:
        raise ValueError('Selected PCM and DSP data exceed the two-MiB cache bound')
    index = bytearray(16*len(selected))
    payload = bytearray()
    with tempfile.TemporaryDirectory(prefix='srr-pc-audio-') as directory:
        folder = Path(directory)
        (folder/'encoder.cpp').write_text(encoder_source())
        subprocess.run(['clang++', '-std=c++17', '-O2', '-DSRR2_OLD_3DS',
                        str(folder/'encoder.cpp'), '-o', str(folder/'encoder')], check=True)
        process = subprocess.Popen([str(folder/'encoder')], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        try:
            for i, record in enumerate(selected):
                with (data/record['archive']).open('rb') as original:
                    original.seek(record['offset']+2048)
                    pcm = original.read(record['samples']*2)
                if len(pcm) != record['samples']*2:
                    raise ValueError('Truncated original PCM')
                process.stdin.write(struct.pack('<I', record['samples'])+pcm)
                process.stdin.flush()
                length = process.stdout.read(4)
                if len(length) != 4:
                    raise ValueError('Encoder rejected source PCM')
                byte_count, = struct.unpack('<I', length)
                dsp = process.stdout.read(byte_count)
                if len(dsp) != byte_count or byte_count != ((record['samples']+13)//14)*8:
                    raise ValueError('Unexpected encoded length')
                payload.extend(b'\0'*((-(32+len(index)+len(payload)))%32))
                pcm_offset = 32+len(index)+len(payload)
                payload.extend(pcm)
                payload.extend(b'\0'*((-(32+len(index)+len(payload)))%32))
                dsp_offset = 32+len(index)+len(payload)
                payload.extend(dsp)
                struct.pack_into('<4I', index, i*16, record['samples'], pcm_offset, dsp_offset, byte_count)
                record.update(pcm_sha256=hashlib.sha256(pcm).hexdigest(),
                              dsp_sha256=hashlib.sha256(dsp).hexdigest(), dsp_bytes=byte_count)
        finally:
            process.stdin.close()
            process.stdout.close()
            if process.wait() != 0:
                raise ValueError('Production encoder verification failed')
    body = index+payload
    encoded = struct.pack('<8s6I', b'SRRDSP1\0', 1, len(selected), 32+len(body), checksum(body), 3, 0)+body
    assert len(encoded) == size
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encoded)
    output.with_suffix('.json').write_text(json.dumps(dict(version=1, predictors=3,
        maximum_bytes=LIMIT, bytes=size, entries=selected, pcm_key='exact complete PCM bytes',
        production_encoder_verified=True, sha256=hashlib.sha256(encoded).hexdigest()), indent=2)+'\n')
    print(json.dumps(dict(bytes=size, clips=len(selected), output=str(output))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, nargs='+', required=True)
    parser.add_argument('--archives', nargs='+', default=['soundfx.rcf', 'carsound.rcf', 'nis.rcf'])
    args = parser.parse_args()
    prepare(args.data, args.output, set(args.samples), args.archives)

#!/usr/bin/env python3
"""Exercise offline rays, exact content lookup, mismatches and color invariants."""
import importlib.util
import argparse
import struct
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data', required=True, type=Path)
parser.add_argument('--assets', default=root/'sdcard/3ds/simpsons/art/chars', type=Path)
args=parser.parse_args()
spec = importlib.util.spec_from_file_location('prepare_character_occlusion', root/'tools/3ds/prepare_character_occlusion.py')
import sys
sys.path.insert(0, str(root/'tools/3ds'))
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)

program = r'''
#include <pddi/pica/characterocclusion.hpp>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
using namespace picaCharacterOcclusion;
int main(int argc,char** argv) {
    assert(argc==3);
    std::ifstream file(argv[1],std::ios::binary),cases(argv[2],std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
    unsigned tested=0;
    while(cases.peek()!=EOF) {
        unsigned length,group,count,offset;
        cases.read(reinterpret_cast<char*>(&length),4);
        std::vector<char> name(length+1,0);cases.read(name.data(),length);
        cases.read(reinterpret_cast<char*>(&group),4);cases.read(reinterpret_cast<char*>(&count),4);
        cases.read(reinterpret_cast<char*>(&offset),4);
        std::vector<picaSkin::Source> source(count);
        cases.read(reinterpret_cast<char*>(source.data()),count*sizeof(source[0]));assert(cases.good());
        assert(Find(data.data(),data.size(),name.data(),group,source.data(),count)==data.data()+offset);
        assert(!Find(data.data(),data.size(),"missing_character",group,source.data(),count));
        assert(!Find(data.data(),data.size(),name.data(),group+1000,source.data(),count));
        assert(!Find(data.data(),15,name.data(),group,source.data(),count));
        source[0].normal[0]+=.015f;
        assert(!Find(data.data(),data.size(),name.data(),group,source.data(),count));
        ++tested;
    }
    assert(tested>400);
    for(unsigned shade=0;shade<256;++shade)for(unsigned value=0;value<256;++value) {
        unsigned char color[]={static_cast<unsigned char>(value),91,247,73};
        Apply(color,shade);
        assert(color[0]<=value && color[1]<=91 && color[2]<=247 && color[3]==73);
        if(shade==255)assert(color[0]==value && color[1]==91 && color[2]==247);
    }
}
'''

with tempfile.TemporaryDirectory(prefix='srr-character-occlusion-') as directory:
    folder = Path(directory)
    helper = folder/'rays'
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined',str(root/'tools/3ds/character_occlusion.cpp'),'-o',str(helper)],check=True)
    # The center sample faces up. An overhead plane blocks every finite ray;
    # the same plane below it blocks none. No game assets are involved.
    for height, expected in ((.01,163),(-.01,255)):
        vertices=[(0,0,0,0,1,0),(-2,height,-2,0,1,0),(2,height,-2,0,1,0),
                  (2,height,2,0,1,0),(-2,height,2,0,1,0)]
        request=struct.pack('<IIIf',5,2,64,.5)+b''.join(struct.pack('<6f',*v) for v in vertices)
        request+=struct.pack('<6I',1,2,3,1,3,4)
        result=subprocess.run([str(helper)],input=request,capture_output=True,check=True).stdout
        assert len(result)==5 and result[0]==expected,(height,result)
    blob=args.data
    data=blob.read_bytes()
    records=struct.unpack_from('<I',data,8)[0]
    offsets={struct.unpack_from('<4I',data,16+20*i):struct.unpack_from('<I',data,32+20*i)[0] for i in range(records)}
    fixture=bytearray()
    for path in sorted(args.assets.glob('*.p3d')):
        for name,groups in prepare.read_skins(path):
            for number,group in enumerate(groups):
                if not group:
                    continue
                vertices=group['vertices'];count=len(vertices)
                key=(prepare.fingerprint(name.encode()),number,count,group['signature'])
                if key not in offsets:
                    continue
                encoded=name.encode();fixture.extend(struct.pack('<I',len(encoded))+encoded)
                fixture.extend(struct.pack('<3I',number,count,offsets[key]))
                for vertex in vertices:
                    # Pure3D skin source: weights, indices, normal, position.
                    fixture.extend(struct.pack('<3fI',1,0,0,0)+vertex[12:]+vertex[:12])
    (folder/'cases').write_bytes(fixture)
    (folder/'test.cpp').write_text(program)
    subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test'),str(blob),str(folder/'cases')],check=True)
print(f'PASS: outward/occluded rays, {records} source group lookups, mismatch fallback, exact alpha and 65536 shade cases.')

#!/usr/bin/env python3
"""Check production lookahead math, memory admission and independent probe input."""
from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/3ds'))
from prepare_stream_probes import prepare, extract

code = r'''
#include "code/main/streamingpolicy.h"
#include <cassert>
#include <random>
static float boxRank(float left,float right,float near,float far,float vx,float vz){
 const float x=(left+right)*.5f,z=(near+far)*.5f;
 const float radius=std::hypot((right-left)*.5f,(far-near)*.5f);
 return StreamingPolicy::NominationRank(x,0,z,radius,vx,0,vz,[&](float t,float r){
  const float dx=std::fmax(0.f,std::fmax(left-vx*t,vx*t-right));
  const float dz=std::fmax(0.f,std::fmax(near-vz*t,vz*t-far));
  return dx*dx+dz*dz<=r*r;
 });
}
int main() {
 using namespace StreamingPolicy;
 assert(Admit(5,7,8u<<20,3u<<20));
 assert(!Admit(6,7,8u<<20,3u<<20));
 assert(!Admit(0,7,(8u<<20)-1,3u<<20));
 assert(!Admit(0,7,8u<<20,(3u<<20)-1));
 for(float speed=0;speed<=100;speed+=.25f)
  for(float latency=0;latency<=10000;latency+=17) {
   const float t=LeadSeconds(speed,latency);
   assert(t>0 && t<=2.5f && (speed<=1 || t*speed<=80.001f));
  }
 std::mt19937 random(37839);std::uniform_real_distribution<float> f(-100,100);
 for(unsigned n=0;n<10000;++n) {
  float x=f(random),y=f(random),z=f(random),a=f(random),b=f(random),c=f(random);
  const double length=double(a)*a+double(b)*b+double(c)*c;
  double t=length?(double(x)*a+double(y)*b+double(z)*c)/length:0;
  t=std::fmax(0.,std::fmin(1.,t));
  const double expected=std::pow(x-a*t,2)+std::pow(y-b*t,2)+std::pow(z-c*t,2);
  assert(std::fabs(SweepDistanceSquared(x,y,z,a,b,c)-expected)<.02);
 }
 assert(SweepDistanceSquared(0,10,0,0,0,0)==100);
 // A stopped player can warm an adjacent exterior gate before entering a car.
 assert(boxRank(30,40,-5,5,0,0)<1e30f);
 assert(boxRank(70,80,-5,5,0,0)==1e30f);
 // Prefer the forward crossing over a nearby gate behind the player.
 assert(boxRank(70,80,-5,5,80,0)<boxRank(-50,-40,-5,5,80,0));
 // A bounding sphere near the sweep must not replace the actual trigger shape.
 assert(boxRank(70,80,30,40,80,0)==1e30f);
 assert(boxRank(-5,5,70,80,0,80)<1e30f);
 assert(boxRank(-5,5,70,80,80,0)==1e30f);
}
'''


def chunk(kind, body=b'', children=b''):
    return struct.pack('<3I', kind, len(body) + 12, len(body) + len(children) + 12) + body + children


with tempfile.TemporaryDirectory(prefix='streaming-policy-') as td:
    p = Path(td)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', '-I', str(ROOT),
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
    art = p / 'art'
    art.mkdir()
    sphere = chunk(0x10004, struct.pack('<4f', 10, 5, -20, 8))
    mesh = chunk(0x10000, b'', sphere)
    entity = chunk(0x3f00000, b'\x04test' + struct.pack('<2I', 0, 0), mesh)
    for level in range(1, 8):
        (art / f'l{level}r1.p3d').write_bytes(chunk(0xff443350, b'', entity))
    assert prepare(art, p / 'out') == {level: 1 for level in range(1, 8)}
    data = (p / 'out/stream-probes.bin').read_bytes()
    assert len(data) == 16 + 7 * 28
    assert struct.unpack_from('<4I', data) == (0x32504753, 2, 7, 28)
    original = (art / 'l1r1.p3d').read_bytes()
    (art / 'l1r1.p3d').write_bytes(chunk(0xff443350, b'', entity + entity))
    prepare(art, p / 'duplicates')
    assert json.loads((p / 'duplicates/stream-probes.json').read_text())['identical_duplicate_copies'] == 1
    (art / 'l1r1.p3d').write_bytes(original[:-1])
    try:
        extract(art / 'l1r1.p3d')
    except (ValueError, struct.error):
        pass
    else:
        raise AssertionError('Truncated art accepted')

print('PASS: production admission and prediction bounds, 10,000 swept paths, seven-map catalog, exact duplicates and truncated input')

#!/usr/bin/env python3
"""Check original-asset preservation and the production backdrop depth policy."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/3ds'))
from prepare_character_detail import nodes
from prepare_vehicle_reflections import pstring

original, prepared = (Path(p) for p in sys.argv[1:])
a, b = nodes(original.read_bytes()), nodes(prepared.read_bytes())
assert len(a) == len(b) == 1 and a[0].kind == b[0].kind
assert [n.encode() for n in a[0].children] == [n.encode() for n in b[0].children[:-2]]
material, entity = b[0].children[-2:]
assert material.kind == 0x11000 and entity.kind == 0x3f00000
assert not any(n.kind == 0x11002 for n in material.children)
name, offset = pstring(entity.body, 0)
assert name.startswith('srr_far_l') and struct.unpack_from('<2I', entity.body, offset) == (0, 1)
mesh, = entity.children
group = next(n for n in mesh.children if n.kind == 0x10002)
assert next(n.body for n in mesh.children if n.kind == 0x10017) == struct.pack('<I', 1)
_, offset = pstring(group.body, 4)
topology, fmt, count, index_count, bones = struct.unpack_from('<5I', group.body, offset)
assert topology == 0 and fmt == 0x20 and not bones and 0 < count <= index_count <= 60000
parts = {n.kind: n.body for n in group.children}
assert set(parts) == {0x10005, 0x10008, 0x1000a}
assert len(parts[0x10005]) == 4+count*12
assert len(parts[0x10008]) == 4+count*4
assert all(parts[0x10008][7+i*4] == 255 for i in range(count))
assert len(parts[0x1000a]) == 4+index_count*4
assert max(struct.unpack_from(f'<{index_count}I', parts[0x1000a], 4)) < count

source = (root/'code/render/DSG/StaticEntityDSG.cpp').read_text()
rank = source.split('void StaticEntityDSG::SetRank(')[1].split('//========')[0]
rank = 'void StaticEntityDSG::SetRank('+rank
display = source.split('void StaticEntityDSG::Display()')[1].split('#ifndef RAD_RELEASE')[0]
display = 'void StaticEntityDSG::Display()'+display
program = r'''
#include <cassert>
#include <cfloat>
#define IS_DRAW_LONG false
#define DSG_BEGIN_PROFILE(x)
#define DSG_END_PROFILE(x)
namespace rmt {struct Vector {};}
struct Context {bool depth=true;void SetZWrite(bool v){depth=v;}} context;
namespace p3d {Context* pddi=&context;}
struct Drawable {unsigned draws=0;bool depth=false;void Display(){++draws;depth=context.depth;}};
struct IEntityDSG {float mRank=0;void SetRank(rmt::Vector&,rmt::Vector&){mRank=12;}};
struct StaticEntityDSG: IEntityDSG {
 enum {GEO=1,IS_SHADOW=2,FAR_BACKDROP=4};
 unsigned mIsGeo=GEO;
 Drawable drawable;
 Drawable* mpDrawstuff=&drawable;
 void SetRank(rmt::Vector&,rmt::Vector&);
 void Display();
};
RANK
DISPLAY
int main(){
 rmt::Vector point,view;
 for(unsigned flag:{1u,3u,5u}){
  StaticEntityDSG entity;entity.mIsGeo=flag;
  entity.SetRank(point,view);
  assert(entity.mRank==(flag==1?12:FLT_MAX));
  context.depth=flag==5?false:true;
  entity.Display();
  assert(entity.drawable.draws==1);
  assert(entity.drawable.depth==(flag!=3));
  assert(context.depth);
 }
}
'''.replace('RANK', rank).replace('DISPLAY', display)
program = '#include <initializer_list>\n'+program
with tempfile.TemporaryDirectory(prefix='srr-far-depth-') as directory:
    folder = Path(directory)
    (folder/'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(folder/'test.cpp'),
                    '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
loader = (root/'code/render/Loaders/StaticEntityLoader.cpp').read_text()
skip = loader.split('if(farBackdrop && (!picaVisual::farBackdrop || !HasAlpha))')[1].split('StaticEntityDSG *')[0]
assert 'return placeholder;' in skip and 'OnChunkLoaded' not in skip and 'new tEntity' in skip
print('PASS: existing TERRA children byte-identical; one opaque texture-free mesh; production sort/depth policy preserves ordinary and shadow draws.')

#!/usr/bin/env python3
"""Check production pedestrian selection against the available source assets."""
from pathlib import Path
import importlib.util,json,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
header=root/'libs/pure3d/pddi/pica/skinselection.hpp'
names=re.findall(r'"([a-z0-9]+)"',header.read_text().split('};',1)[0])
assert len(names)==len(set(names))==27
checks=[]
for name in names:
 for mesh,skeleton,expected in [(name+'_h_merged',name,True),(name+'_l_merged',name,False),
                                (name+'_h_mergedx',name,False),(name+'_h_merged','homer',False),
                                ('',name,False),(name,name,False)]:
  checks.append(f'assert(picaSkin::SelectPedestrian({json.dumps(mesh)},{json.dumps(skeleton)})=={str(expected).lower()});')
with tempfile.TemporaryDirectory(prefix='srr-pedestrian-') as td:
 p=Path(td);(p/'test.cpp').write_text('#include <cassert>\n#include <pddi/pica/skinselection.hpp>\nint main(){\n'+ '\n'.join(checks)+'\nassert(!picaSkin::SelectPedestrian(nullptr,nullptr));}\n')
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
assets=root/'out/whole-frame-1/emulator/sdmc/3ds/simpsons/art/chars'
if assets.exists():
 spec=importlib.util.spec_from_file_location('inventory',root/'tools/3ds/skin_inventory.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 for name in names:
  records=m.skins(assets/(name+'_m.p3d'))
  matches=[r for r in records if r['skeleton']==name and r['skin']==name+'_h_merged']
  assert len(matches)==1,(name,matches)
  groups=matches[0]['groups']
  assert all(0<g['bones']<=24 for g in groups),name
  assert sum(32+56*g['vertices'] for g in groups)<40*1024,name
 print('PASS: all 27 selected source assets fit 24 bones per group and less than 40 KiB per full skin')
else:
 print('Source-asset audit skipped: isolated asset copy unavailable')
print('PASS: production pedestrian selection; exact mesh/skeleton pairs, suffix/LOD/identity rejection and null inputs')

#!/usr/bin/env python3
"""Audit additional cast selection and every selected source-asset skin."""
from pathlib import Path
import importlib.util,json,re,subprocess,tempfile,sys
packed="--packed" in sys.argv
root=Path(__file__).resolve().parents[3]
header=(root/'libs/pure3d/pddi/pica/skinselection.hpp').read_text()
assets=root/'out/whole-frame-1/emulator/sdmc/3ds/simpsons/art/chars'
spec=importlib.util.spec_from_file_location('inventory',root/'tools/3ds/skin_inventory.py');inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
records=[r for path in assets.glob('*_m.p3d') for r in inventory.skins(path)] if assets.exists() else []
checks=[];seen=set();count=0
selections=[('SelectAdditionalPlayer',3),('SelectAdditionalPedestrian',16),('SelectNamedCharacter',29),('SelectCostumePlayer',14),('SelectCostumePedestrian',1),('SelectCostumeNamed',1)]
if packed:selections += [('SelectPackedPlayer',8),('SelectPackedNamed',9)]
for function,expected_count in selections:
 body=header.split('inline bool '+function+'(',1)[1].split('};',1)[0]
 names=re.findall(r'"([a-z0-9_]+)"',body);assert len(names)==len(set(names))==expected_count
 for name in names:
  assert name not in seen;seen.add(name);count+=1
  for mesh,skeleton,expected in [(name+'_h_merged',name,True),(name+'_l_merged',name,False),(name+'_h_mergedx',name,False),(name+'_h_merged','bart',False),(name,name,False)]:
   checks.append(f'assert(picaSkin::{function}({json.dumps(mesh)},{json.dumps(skeleton)})=={str(expected).lower()});')
  if records:
   matches=[r for r in records if r['skin']==name+'_h_merged' and r['skeleton']==name];assert len(matches)==1,(name,matches)
   groups=matches[0]['groups'];assert all(0<g['bones']<=25 for g in groups),name
   size=sum(32+g['vertices']*((28 if packed else 44) if g['vertices']==g['rigid_vertices'] else (40 if packed else 56)) for g in groups)
   assert size<=64*1024,(name,size)
 checks.append(f'assert(!picaSkin::{function}(nullptr,nullptr));')
with tempfile.TemporaryDirectory(prefix='srr-cast-skin-')as td:
 p=Path(td);(p/'test.cpp').write_text('#include <cassert>\n#include <pddi/pica/skinselection.hpp>\nint main(){\n'+'\n'.join(checks)+'\n}\n')
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print(f'PASS: {count} exact production cast selections; suffix/identity/LOD/null rejection; '+('all available source skins fit 25 bones and 64 KiB' if records else 'source assets unavailable'))

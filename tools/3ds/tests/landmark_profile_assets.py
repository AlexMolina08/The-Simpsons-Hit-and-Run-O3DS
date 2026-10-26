#!/usr/bin/env python3
"""Verify the prepared visibility index against source geometry and load budgets."""
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_character_detail import nodes
from prepare_landmark_visibility import candidates,fnv,walk
from skin_inventory import pstring

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('art',type=Path)
parser.add_argument('prepared',type=Path)
args=parser.parse_args()
manifest=json.loads((args.prepared/'manifest.json').read_text())
data=(args.prepared/'landmark-visibility.bin').read_bytes()
magic,version,count,size=struct.unpack_from('<4sIII',data)
assert (magic,version,size)==(b'LMV1',1,8) and count<=448
assert len(data)==16+8*count
records=list(struct.iter_unpack('<II',data[16:]))
assert records==sorted(set(records))
expected=[];files={};levels=[]
assert [i['level'] for i in manifest['levels']]==list('1234567')
for level in manifest['levels']:
    assert level['items'] and len(level['items'])==level['selected']
    assert sum(i['draws'] for i in level['items'])==level['draws']<=manifest['draw_budget']
    assert sum(i['vertices'] for i in level['items'])==level['vertices']<=manifest['vertex_budget']
    for item in level['items']:
        path=args.art/item['file']
        if path not in files:
            raw=path.read_bytes()
            files[path]=(hashlib.sha256(raw).hexdigest(),{i['entity']:i for i in candidates(path)},
                {pstring(n.body,0)[0]:n for n in walk(nodes(raw)) if n.kind==0x3f00000})
        digest,pool,entities=files[path]
        assert digest==item['source_sha256']
        assert item==pool[item['entity']], 'Selection no longer satisfies geometry/material rules'
        entity=entities[item['entity']];_,off=pstring(entity.body,0)
        assert struct.unpack_from('<I',entity.body,off+4)[0]==0
        mesh=next(n for n in entity.children if n.kind==0x10000)
        parts={n.kind:n for n in mesh.children}
        bounds=parts[0x10004].body+parts[0x10003].body
        assert len(bounds)==40 and fnv(bounds)==item['geometry_key']
        expected.append((item['name_key'],item['geometry_key']))
    eligible_files={path.name for path in args.art.glob('*.p3d')
        if re.fullmatch('l'+level['level']+r'[rz].*\.p3d',path.name,re.I) and candidates(path)}
    selected_files={item['file'] for item in level['items']}
    if 'candidate_files' in level:assert level['candidate_files']==len(eligible_files)
    if 'selected_files' in level:assert level['selected_files']==len(selected_files)
    report={k:level[k] for k in ('level','selected','draws','vertices')}
    report.update(eligible_files=len(eligible_files),selected_files=len(selected_files),
                  uncovered_files=sorted(eligible_files-selected_files))
    levels.append(report)
assert sorted(expected)==records
print(json.dumps(dict(records=count,bytes=len(data),source_files=len(files),levels=levels,
    checks=['all seven levels have selected architecture','per-level draw/vertex budgets',
            'source hashes and opaque geometry criteria','runtime name and bounds keys',
            'exact bounded sorted index']),indent=2))

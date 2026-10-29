#!/usr/bin/env python3
"""Restore triangle strips after offline head refinement without changing vertices."""
import argparse
from collections import Counter,defaultdict
import hashlib
import json
from pathlib import Path
import struct

from prepare_character_detail import nodes,group,triangles


def oriented(face):
    a,b,c=face
    return min((a,b,c),(b,c,a),(c,a,b))


def stripify(faces):
    """Greedy oriented strips, joined with degenerates rather than restart indices."""
    edges=defaultdict(list)
    for index,(a,b,c) in enumerate(faces):
        for x,y,z in ((a,b,c),(b,c,a),(c,a,b)):edges[x,y].append((index,z))
    unused=set(range(len(faces)));output=[]

    def degree(index):
        a,b,c=faces[index]
        return sum(other in unused for edge in ((b,a),(c,b),(a,c)) for other,_ in edges[edge])

    while unused:
        seed=min(unused,key=lambda index:(degree(index),index))
        best=None
        for rotation in range(3):
            face=faces[seed];strip=list(face[rotation:]+face[:rotation]);used={seed}
            while True:
                a,b=strip[-2:]
                if (len(strip)-2)%2:a,b=b,a
                options=[(index,vertex) for index,vertex in edges[a,b] if index in unused and index not in used]
                if not options:break
                index,vertex=min(options,key=lambda item:(degree(item[0]),item[0]))
                strip.append(vertex);used.add(index)
            if best is None or len(used)>len(best[1]):best=strip,used
        strip,used=best;unused.difference_update(used)
        if output:
            if len(output)%2:output.append(output[-1])
            output.extend((output[-1],strip[0]))
        output.extend(strip)
    assert Counter(map(oriented,triangles(output,1)))==Counter(map(oriented,faces))
    return output


def prepare(data):
    tree=nodes(data);statistics=[]
    def walk(items):
        for node in items:
            g=group(node)
            if g and struct.unpack_from('<I',node.body,g['offset'])[0]==0 and len(g['triangles'])>=4:
                old_indices=struct.unpack_from('<I',g['parts'][0x1000A].body)[0]
                indices=stripify(g['triangles'])
                if len(indices)<old_indices:
                    g['parts'][0x1000A].body=struct.pack('<I',len(indices))+struct.pack(f'<{len(indices)}I',*indices)
                    body=bytearray(node.body)
                    struct.pack_into('<I',body,g['offset'],1)
                    struct.pack_into('<I',body,g['offset']+12,len(indices))
                    node.body=bytes(body)
                    statistics.append(dict(material=g['material'],vertices=len(g['position']),
                                           triangles=len(g['triangles']),before_indices=old_indices,after_indices=len(indices)))
            walk(node.children)
    walk(tree)
    return b''.join(node.encode() for node in tree),statistics


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prepared',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False);report=[]
    for record in json.loads((args.prepared/'manifest.json').read_text()):
        source=args.prepared/record['file'];data=source.read_bytes()
        assert hashlib.sha256(data).hexdigest()==record['output_sha256']
        output,statistics=prepare(data);(args.output/source.name).write_bytes(output)
        report.append(dict(record,input_sha256=hashlib.sha256(data).hexdigest(),
                           output_sha256=hashlib.sha256(output).hexdigest(),output_bytes=len(output),strip_groups=statistics))
    (args.output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    stats=[group for record in report for group in record['strip_groups']]
    print(json.dumps(dict(models=len(report),groups=len(stats),before_indices=sum(s['before_indices'] for s in stats),
                         after_indices=sum(s['after_indices'] for s in stats),vertices_changed=0,triangles_changed=0)))


if __name__=='__main__':main()

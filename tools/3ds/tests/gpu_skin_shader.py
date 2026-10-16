#!/usr/bin/env python3
"""Execute the production skinning preamble with float24-rounded scalar operations.
This checks source instruction semantics, not PICA cycle timing or rasterization.
"""
from pathlib import Path
import math
import random
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
shader = ROOT/'libs/pure3d/pddi/pica/shaders/skin.v.pica'
source = shader.read_text()
with tempfile.TemporaryDirectory() as td:
    header = Path(td)/'skin.h'
    subprocess.run(['/opt/devkitpro/tools/bin/picasso', '-o', str(Path(td)/'skin.shbin'),
                    '-h', str(header), str(shader)], check=True)
    binary = (Path(td)/'skin.shbin').read_bytes()
    dvlp = binary.index(b'DVLP')
    instruction_count = struct.unpack_from('<I', binary, dvlp+12)[0]
    descriptor_count = struct.unpack_from('<I', binary, dvlp+20)[0]
    assert instruction_count <= 512 and descriptor_count <= 128
    print(f'compiled shader: {instruction_count} instructions, {descriptor_count} operand descriptors')
    assert '#define VSH_FVEC_u_bones 0x1B' in header.read_text()
    assert '#define VSH_ULEN_u_bones 69' in header.read_text()
    standard = (ROOT/'libs/pure3d/pddi/pica/shaders/pddi.v.pica').read_text()
    assert source[source.index('\tdp4\tr8.x'):].replace('r5.xyz', 'in_normal.xyz') == standard[standard.index('\tdp4\tr8.x'):]

code = source.split('.proc vmain\n')[1].split('\tdp4\tr8.x')[0].strip().splitlines()
labels = {line.strip()[:-1]: i for i, line in enumerate(code) if line.strip().endswith(':')}

def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]

def f24(x):
    # Round to 16 explicit fraction bits. Tests keep exponents in the normal range.
    if not x:
        return x
    m, e = math.frexp(x)
    return math.ldexp(round(m*131072)/131072, e)

rng = random.Random(41296)
worst_position = worst_normal = 0.0
for case in range(4000):
    matrices = [[[f32(rng.uniform(-2,2)) for c in range(4)] for r in range(3)] for b in range(23)]
    pos = [f32(rng.uniform(-2,2)) for i in range(3)] + [1.0]
    normal = [f32(rng.uniform(-1,1)) for i in range(3)] + [0.0]
    ids = [22 if case%7==0 else rng.randrange(23) for i in range(4)]
    influences = 1+case%4
    w = [0.0]*4
    w[0] = 1.0 if influences==1 else f32(rng.uniform(.05,.4))
    if influences>1: w[1]=f32(1-w[0])
    if influences>2: w[1]=f32(w[1]*.5); w[2]=f32(1-f32(w[1]+w[0]))
    if influences>3: w[2]=f32(w[2]*.5)
    w[3]=0 if influences==1 else f32(1-f32(f32(w[2]+w[1])+w[0]))
    offsets=[3*i for i in ids]
    if influences==1: offsets[3]=-1
    regs={'in_pos':list(map(f24,pos)), 'in_normal':list(map(f24,normal)),
          'in_weights':list(map(f24,w)), 'in_offsets':offsets, 'a0':[0]*4}
    regs.update({f'r{i}':[0.0]*4 for i in range(16)})
    def read(operand):
        if operand=='ones': return [1.0]*4
        if operand=='zeros': return [0.0]*4
        if operand.startswith('u_bones['):
            off=int(operand.split('+')[1].split(']')[0])+regs['a0'][0]
            assert 0<=off<69
            return list(map(f24,matrices[off//3][off%3]))
        name, _, swizzle=operand.partition('.')
        val=regs[name]
        if not swizzle: return val[:]
        swizzle=(swizzle*4)[:4] if len(swizzle)==1 else swizzle
        return [val['xyzw'.index(c)] for c in swizzle]
    def put(dest, val):
        name, _, mask=dest.partition('.')
        mask=mask or 'xyzw'
        for c in mask:
            i='xyzw'.index(c); regs[name][i]=int(val[i]) if name=='a0' else f24(val[i])
    pc=0; condition=False
    while pc<len(code):
        line=code[pc].strip();pc+=1
        if line.endswith(':'):continue
        op, rest=line.split(None,1);args=[x.strip() for x in rest.split(',')]
        if op in ('mov','mova'):put(args[0],read(args[1]))
        elif op in ('dp3','dp4'):
            a,b=read(args[1]),read(args[2]);n=int(op[-1]);dot=f24(sum(f24(a[i]*b[i]) for i in range(n)));put(args[0],[dot]*4)
        elif op in ('add','mul'):
            a,b=read(args[1]),read(args[2]);put(args[0],[a[i]+b[i] if op=='add' else a[i]*b[i] for i in range(4)])
        elif op=='cmp':condition=read(args[0])[0]>read(args[3])[0]
        elif op=='jmpc':
            if condition:pc=labels[args[1]]
        else:raise AssertionError(op)
    expected=[sum(sum(matrices[ids[j]][axis][k]*pos[k] for k in range(4))*w[j] for j in range(1 if influences==1 else 4)) for axis in range(3)]
    expected_normal=[sum(matrices[ids[0]][axis][k]*normal[k] for k in range(3)) for axis in range(3)]
    worst_position=max(worst_position,max(abs(a-b) for a,b in zip(expected,regs['r0'])))
    worst_normal=max(worst_normal,max(abs(a-b) for a,b in zip(expected_normal,regs['r5'])))
assert worst_position<0.001 and worst_normal<0.001
print(f'4000 production-preamble cases; max position error={worst_position:.9g}, normal={worst_normal:.9g}; shader c27..c95 verified')

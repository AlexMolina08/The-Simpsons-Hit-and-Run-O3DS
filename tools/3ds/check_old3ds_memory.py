#!/usr/bin/env python3
"""Reject fixed-heap binaries exceeding the Old 3DS 64 MiB application budget."""
import argparse,json,os,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path);a=p.parse_args()
b=a.elf.read_bytes();assert b[:6]==b'\x7fELF\x01\x01','Expected little-endian ELF32'
assert struct.unpack_from('<H',b,18)[0]==40,'Expected ARM executable'
phoff=struct.unpack_from('<I',b,28)[0];entsize,num=struct.unpack_from('<HH',b,42)
loads=[struct.unpack_from('<8I',b,phoff+i*entsize) for i in range(num)]
loads=[x for x in loads if x[0]==1]
image=sum((x[5]+4095)&~4095 for x in loads)
symbols={s.split()[-1]:int(s.split()[0],16) for s in subprocess.check_output([str(Path(os.environ.get('DEVKITPRO', '/opt/devkitpro')) / 'devkitARM/bin/arm-none-eabi-nm'),str(a.elf)],text=True).splitlines() if len(s.split())==3 and s.split()[-1] in ['__ctru_heap_size','__ctru_linear_heap_size']}
def value(name):
 address=symbols[name]
 for x in loads:
  if x[2]<=address<x[2]+x[4]:return struct.unpack_from('<I',b,x[1]+address-x[2])[0]
 raise ValueError('Heap symbol not backed by file: '+name)
heap=value('__ctru_heap_size');linear=value('__ctru_linear_heap_size')
# Conservative allowance for loader/startup allocations and stack beyond load segments.
# This preflight is necessary, not proof that later heap allocations will succeed.
reserve=32768;budget=64*1024*1024;required=image+heap+linear+reserve
result=dict(application_budget=budget,page_aligned_load_segments=image,normal_heap=heap,linear_heap=linear,startup_reserve=reserve,required=required,headroom=budget-required,pass_budget=required<=budget,limitation='Fixed startup budget only; physical allocation and runtime free-memory evidence remain required.')
print(json.dumps(result,indent=2));raise SystemExit(0 if result['pass_budget'] and heap and linear else 1)

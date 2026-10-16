#!/usr/bin/env python3
"""Compare complete CPU/GPU intervals, preserving workload and scene distinctions."""
import argparse
import json
import re
from collections import defaultdict
from fractions import Fraction
from pathlib import Path
from statistics import median

def rows(path):
    return [{k: float(v) for k,v in re.findall(r'(\w+)=(-?\d+(?:\.\d+)?)', line)}
            for line in path.read_text().splitlines() if line.startswith('GPU_SKIN frame=')]

def summarize(selected):
    frames=sum(r['frames'] for r in selected)
    return dict(intervals=len(selected), frames=frames,
        **{k: sum(r.get(k,0)*r['frames'] for r in selected)/frames for k in ('work_ms','update_ms','render_ms')},
        **{k+'_per_frame':sum(r.get(k,0) for r in selected)/frames for k in
           ('palette_ms','cpu_reference_ms','vectors','switches','gpu_vertices','avoided_write_bytes','avoided_flush_bytes')},
        interval_work_median=median(r['work_ms'] for r in selected),
        peak_added_linear=max(r['peak'] for r in selected), minimum_linear=min(r['linear_min'] for r in selected))

def compare(data):
    buckets=defaultdict(lambda: {0:[],1:[]})
    for r in data:
        if r['frames']!=120 or not r.get('eligible_vertices') or not r.get('draws'):continue
        # Include context count and eligible skin count, not just scene draw totals.
        key=tuple(Fraction(int(r[k]),int(r['frames'])) for k in
                  ('draws','submitted_vertices','eligible_vertices','gameplay_frames'))
        buckets[key][int(r['enabled'])].append(r)
    matched=[]
    for key,modes in buckets.items():
        if not all(modes.values()):continue
        matched.append(dict(workload=dict(zip(('draws','submitted_vertices','eligible_vertices','gameplay_fraction'),map(str,key))),
            cpu=summarize(modes[0]),gpu=summarize(modes[1])))
    return matched

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('log',type=Path);p.add_argument('--source',required=True,choices=['emulator','physical'])
    a=p.parse_args();data=rows(a.log)
    result=dict(source=a.source,scope='Same-executable alternating intervals; equal counts do not prove equal animation/physics work.',
        matched=compare(data),allocations=sum(r['allocs'] for r in data),retirements=sum(r['frees'] for r in data),
        allocation_failures=sum(r['failures'] for r in data),mutations=sum(r['mutations'] for r in data))
    print(json.dumps(result,indent=2))

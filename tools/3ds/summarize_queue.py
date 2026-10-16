#!/usr/bin/env python3
"""Compare queue telemetry only where GPU draw and vertex workloads match exactly."""
import argparse
from collections import Counter
from fractions import Fraction
import json
from pathlib import Path
import re
from statistics import median

TICKS_PER_MS = 268123.480

def fields(line):
    return {key: float(value) if '.' in value else int(value)
            for key,value in re.findall(r'(\w+)=(-?\d+(?:\.\d+)?)',line)}

def queue_rows(path):
    return [fields(line) for line in Path(path).read_text().splitlines()
            if line.startswith('QUEUE ') and 'partial=0' in line]

def workload(row):
    return Fraction(row['draws'],row['frames']),Fraction(row['vertices'],row['frames'])

def frame_rows(path):
    rows=[]; row=None
    for line in Path(path).read_text().splitlines():
        if line.startswith('OLD3DS_FRAME '):
            if row: rows.append(row)
            row={'frame':fields(line)}
        elif row:
            for prefix,key in [('OLD3DS_PROFILE ','profile'),('OLD3DS_DRAW ','draw'),('OLD3DS_RPHASE ','phase')]:
                if line.startswith(prefix): row[key]=fields(line)
    if row: rows.append(row)
    return rows

def summarize_queue(rows):
    count=sum(r['frames'] for r in rows)
    result={'intervals':len(rows),'frames':count}
    for key in ['draws','vertices','queued','moved','material_passes','textures','uniform_blocks',
                'uniform_reuses','material_reuses','pipeline_reuses','uniform_bypasses','command_words',
                'original_changes','submitted_changes','barriers','capacity_flushes']:
        result[key+'_per_frame']=sum(r.get(key,0) for r in rows)/count
    calls=sum(r.get('backend_calls',0) for r in rows)
    samples=sum(r.get('backend_samples',0) for r in rows)
    result['buffered_backend_cpu_ms_estimate']=sum(r.get('backend_ticks',0) for r in rows)/max(samples,1)*calls/count/TICKS_PER_MS
    for name in ['opaque','translucent','traversal']:
        result[name+'_cpu_ms_median']=median(r.get(name+'_ticks',0)/r['frames']/TICKS_PER_MS for r in rows)
    result['world_scene_cpu_ms_median']=median(sum(r.get(name+'_ticks',0) for name in ['opaque','translucent','traversal'])/r['frames']/TICKS_PER_MS for r in rows)
    result['peak_entries']=max(r['peak_entries'] for r in rows)
    return result

def summarize_frame(rows,key):
    selected=[]
    for row in rows:
        if not all(x in row for x in ['frame','profile','draw','phase']): continue
        frame,profile,draw=row['frame'],row['profile'],row['draw']
        samples=profile['samples']
        if frame['context'] not in [9,10] or frame['paused'] or samples<100: continue
        if (Fraction(draw['calls'],samples),Fraction(draw['verts'],samples))!=key: continue
        if any(draw[x] for x in ['dropped_noframe','dropped_notdrawable','dropped_noroom']):
            raise ValueError('Dropped draws invalidate performance comparison')
        selected.append(row)
    result={'intervals':len(selected)}
    if selected:
        for name in ['render_ms','work_ms','update_ms','gpu_wait_ms']:
            result[name+'_median']=median(r['profile'][name] for r in selected)
        for name in ['opaque_ms','translucent_ms']:
            result[name+'_median']=median(r['phase'][name] for r in selected)
    return result

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',required=True,choices=['emulator','hardware'])
parser.add_argument('base_queue'); parser.add_argument('optimized_queue')
parser.add_argument('--split-modes',action='store_true',help='Split one A/B executable log by queue_enabled')
parser.add_argument('--base-frames'); parser.add_argument('--optimized-frames')
args=parser.parse_args()
base,optimized=queue_rows(args.base_queue),queue_rows(args.optimized_queue)
if args.split_modes:
    base=[row for row in base if row.get('queue_enabled')==0]
    optimized=[row for row in optimized if row.get('queue_enabled')==1]
b=Counter(workload(r) for r in base); q=Counter(workload(r) for r in optimized)
common=set(b)&set(q)
common={key for key in common if key[0]>150 and key[1]>10000 and key[0].denominator==key[1].denominator==1}
if not common: raise SystemExit('No exactly matched settled GPU workload. No speedup conclusion is valid.')
key=max(common,key=lambda key:(min(b[key],q[key]),b[key]+q[key]))
result={'source':args.source,'within_one_executable':args.split_modes,'matched_draws_per_frame':int(key[0]),'matched_vertices_per_frame':int(key[1]),
        'base':summarize_queue([r for r in base if workload(r)==key]),
        'optimized':summarize_queue([r for r in optimized if workload(r)==key])}
if args.base_frames and args.optimized_frames:
    result['base']['frame_work']=summarize_frame(frame_rows(args.base_frames),key)
    result['optimized']['frame_work']=summarize_frame(frame_rows(args.optimized_frames),key)
print(json.dumps(result,indent=2))

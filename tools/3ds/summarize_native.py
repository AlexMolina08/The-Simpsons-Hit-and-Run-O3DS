#!/usr/bin/env python3
"""Summarize bounded native benchmark captures without mixing work and pacing."""
import argparse,json,math
from pathlib import Path
from collections import Counter

def fields(line):return dict(s.split('=',1) for s in line.split()[1:] if '=' in s)
def percentile(values,p):
 a=sorted(values);return a[min(len(a)-1,max(0,math.ceil(p*len(a))-1))] if a else None
def read(path):
 lines=path.read_text().splitlines();header=fields(next(l for l in lines if l.startswith('NATIVE_BUILD ')))
 summary=fields(next(l for l in lines if l.startswith('NATIVE_SUMMARY ')))
 rows=[fields(l) for l in lines if l.startswith('NATIVE_FRAME ')]
 stat=lambda k:dict(mean=sum(float(r[k]) for r in rows)/len(rows),p50=percentile([float(r[k]) for r in rows],.5),p95=percentile([float(r[k]) for r in rows],.95),p99=percentile([float(r[k]) for r in rows],.99),maximum=max(float(r[k]) for r in rows)) if rows else None
 unavailable=['camera','spatial','palette','buffer_write']
 costs={k:stat(k) for k in rows[0] if k.endswith('_us') and not any(k.startswith(n+'_') for n in unavailable)} if rows else {}
 counts={k:sum(int(r[k]) for r in rows) for k in rows[0] if k.startswith('count_')} if rows else {}
 period=[float(r.get('period_us',0)) for r in rows if float(r.get('period_us',0))>0]
 automation=fields(next((l for l in lines if l.startswith('NATIVE_AUTOMATION ')),'NATIVE_AUTOMATION'))
 result=dict(automation=automation,uninstrumented_stages=unavailable,header=header,summary=summary,retained_frames=len(rows),complete_rows=not int(header['overwritten']),costs_us=costs,counters=counts,
 substeps=dict(Counter(r['substeps'] for r in rows)),contexts=dict(Counter(r['context'] for r in rows)),
 motion=dict(in_car_frames=sum(int(r['incar']) for r in rows),walking_frames=sum(y['incar']=='0' and x['incar']=='0' and sum((float(y[k])-float(x[k]))**2 for k in ['x','z'])**.5 > .5*int(y['elapsed'])/1000 for x,y in zip(rows,rows[1:])),moving_frames=sum(float(r['speed'])>1 for r in rows),streaming_frames=sum(int(r['loading'])>0 for r in rows)),
 loop_period_fps=1e6*len(period)/sum(period) if period else None,
 verification=fields(next((l for l in lines if l.startswith('NATIVE_VERIFY ')),'NATIVE_VERIFY calls=0 failures=0')))
 # Completion and route coverage are separate: button scripts can get stuck.
 driving_window_end=int(header['seconds'])*1000 if header.get('scenario')=='L2-road-v2' else 28000
 driving_ms=sum(int(r['elapsed']) for r in rows if int(r['route_ms'])<driving_window_end and r['incar']=='1' and float(r['speed'])>1)
 walking_ms=sum(int(y['elapsed']) for x,y in zip(rows,rows[1:]) if 31000<=int(y['route_ms'])<38000 and y['incar']=='0' and x['incar']=='0' and sum((float(y[k])-float(x[k]))**2 for k in ['x','z'])**.5 > .5*int(y['elapsed'])/1000)
 exited=any(r['incar']=='0' and int(r['route_ms'])>=28000 for r in rows)
 reentered=exited and any(r['incar']=='1' and int(r['route_ms'])>=42000 for r in rows)
 driving_only=header.get('scenario') in ('L2-drive-v1','L2-road-v2')
 intact=summary['complete']=='1' and not int(header.get('aborted',0)) and not int(header['overwritten']) and len(rows)==int(summary['frames'])
 minimum_driving_ms=max(4000,int(header['seconds'])*500) if driving_only else 4000
 coverage=intact and driving_ms>=minimum_driving_ms and (all(r['incar']=='1' for r in rows) if driving_only else walking_ms>=2000 and reentered)
 route=fields(next((l for l in lines if l.startswith('NATIVE_ROUTE ')),'NATIVE_ROUTE'))
 road_route=header.get('scenario')=='L2-road-v2'
 if road_route:
  coverage=coverage and driving_ms>=int(header['seconds'])*750 and float(route.get('distance',0))>=int(header['seconds'])*7.5 and int(route.get('recoveries',99))<=1 and int(route.get('point',0))>=3
 result['route_execution']=route
 result['streaming_window_observed']=any(int(r['loading'])>0 for r in rows)
 result['route_health']=dict(scenario=header.get('scenario'),driving_only=driving_only,driving_moving_ms=driving_ms,walking_moving_ms=walking_ms,observed_exit=exited,observed_late_in_car=reentered,clock_complete=summary['complete']=='1',coverage_pass=coverage)
 result['workload']={k:dict(total=sum(int(r[k]) for r in rows),minimum=min(int(r[k]) for r in rows),maximum=max(int(r[k]) for r in rows)) for k in ['draws','vertices','pairs','areas','entities','loading','substeps']} if rows else {}
 result['timing_note']='Loop periods include pacing; use associated O3 presentation records for presentation intervals. gpu_last_ms is unassociated, never a per-frame GPU measurement. Replay does not establish physical FPS.'
 result['uninstrumented_stages']=[k for k in ['physics','vehicle_prep','character','pair_prep','narrowphase','solve','volume','vehicle_sim','dynamic_sim','camera','spatial','suspension','pose','skin','particles','material','commands','flush','wait','static_submit','dynamic_submit','terrain','traversal','palette','buffer_write']]
 return result,rows
p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--source',required=True,choices=['emulator','physical']);p.add_argument('--compare',type=Path);a=p.parse_args()
r,rows=read(a.log);r['source']=a.source
if a.compare:
 b,other=read(a.compare);keys=['check','elapsed','route_ms','substeps','draws','vertices','pairs','areas','entities','loading']
 mismatches={k:sum(x.get(k)!=y.get(k) for x,y in zip(rows,other)) for k in keys}
 identical=len(rows)==len(other) and not any(mismatches.values()) and r['header']['mode']=='replay' and b['header']['mode']=='replay' and r['complete_rows'] and b['complete_rows'] and r['route_health']['coverage_pass'] and b['route_health']['coverage_pass']
 r['comparison']=dict(other=str(a.compare),equivalent_recorded_work=identical,mismatch_frames=mismatches,note='Matching hashes/counters are regression evidence, not an exhaustive mission-state proof. Different workloads cannot establish a causal speedup.')
 if identical and r['header'].get('detail')==b['header'].get('detail') and r['header'].get('validate')==b['header'].get('validate'):r['comparison']['mean_work_delta_us']=float(r['summary']['mean_us'])-float(b['summary']['mean_us'])
# Within-run context randomization is separate from unmatched route comparisons.
if r['automation'].get('context_mode')=='2' and rows:
 import random,statistics
 groups={value:[x for x in rows if x.get('native_context')==value] for value in ('0','1')}
 def describe(group):
  return {'frames':len(group),'mean':{key:statistics.mean(float(x[key]) for x in group) for key in ('work_us','render_us','update_us','draws','vertices','entities')}} if group else {'frames':0}
 used=set();pairs=[];keys=['draws','vertices','entities','loading','elapsed','substeps']
 for i,x in enumerate(rows):
  if x.get('native_context')!='0' or i in used or x.get('loading')!='0':continue
  matches=[j for j in range(max(0,i-3),min(len(rows),i+4)) if j not in used and rows[j].get('native_context')=='1' and all(x[k]==rows[j][k] for k in keys)]
  if not matches:continue
  j=min(matches,key=lambda j:abs(i-j));used.update((i,j));y=rows[j]
  pairs.append({'reference_frame':int(x['frame']),'candidate_frame':int(y['frame']),'block':min(i,j)//25,'render_saved_us':float(x['render_us'])-float(y['render_us']),'work_saved_us':float(x['work_us'])-float(y['work_us']),'reference_render_us':float(x['render_us'])})
 paired={'count':len(pairs),'equal_recorded_fields':keys,'maximum_frame_gap':3,'loading_excluded':True}
 if pairs:
  paired.update(mean_render_saved_us=statistics.mean(x['render_saved_us'] for x in pairs),median_render_saved_us=statistics.median(x['render_saved_us'] for x in pairs),mean_work_saved_us=statistics.mean(x['work_saved_us'] for x in pairs),reference_render_us=statistics.mean(x['reference_render_us'] for x in pairs))
  blocks={i:[x for x in pairs if x['block']==i] for i in sorted(set(x['block'] for x in pairs))};generator=random.Random(12345);samples=[]
  for trial in range(2000):
   draw=[x for i in generator.choices(list(blocks),k=len(blocks)) for x in blocks[i]]
   samples.append(statistics.mean(x['render_saved_us'] for x in draw))
  samples.sort();paired['render_saved_us_block_bootstrap_95']=[samples[49],samples[1949]];paired['blocks']=len(blocks)
 r['randomized_context_comparison']={'reference':describe(groups['0']),'candidate':describe(groups['1']),'matched_neighbor_frames':paired,'note':f"Pseudo-randomized render strategies within one {r['header']['mode']} run; positive differences mean lower CPU cost. Recorded-work matches are not exhaustive state or visual proof. Raw whole-frame means can contain differently assigned loading stalls. CPU-loop periods are not verified presentation intervals; replay is not a shipping FPS measurement.",'pairs':pairs}
print(json.dumps(r,indent=2))

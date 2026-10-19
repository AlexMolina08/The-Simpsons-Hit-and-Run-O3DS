#!/usr/bin/env python3
"""Summarize bounded causal windows. Missing geometry is never a speedup claim."""
import argparse,json,re,statistics
from pathlib import Path
names=['backend','material','submit','uniform','stream_flush','texture_upload','texture_conversion','physics','substep','world','render','gpu_wait']
modes=['reference','low_instrumentation','tiny_raster','coarse_mips','no_world_backend']
p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);args=p.parse_args()
rows=[]
for line in args.log.read_text(errors='replace').splitlines():
 if line.startswith('O3_FRAME '):
  row={k:float(v) for k,v in re.findall(r'(\w+)=(-?[\d.]+)',line)}
  rows.append(row)
def distribution(values):
 s=sorted(values)
 return {'mean':statistics.mean(s),'p50':statistics.median(s),'p90':s[min(len(s)-1,int(len(s)*.9))],'p95':s[min(len(s)-1,int(len(s)*.95))],'max':max(s)} if s else None
result={'source':str(args.log),'scope_names':dict(enumerate(names)),'notes':[
 'All timings are milliseconds. iN is inclusive; xN excludes instrumented child scopes.',
 'Do not sum nested inclusive scopes. World exclusive includes uninstrumented preparation and callback work.',
 'Low mode disables sampled operation/entity/UID and added detailed scopes. Raw counters, GPU metrics and cache guards remain.',
 'Mode 2 retains geometry/viewport and limits raster coverage to 4x4. Mode 3 forces existing mips; alpha/depth coverage can change.',
 'Mode 4 removes persistent material/backend work after native culling, and immediate submission after CPU stream generation.',
 'Fixed=1 supplies constant 50 ms elapsed, retaining existing adaptive substep rules. Scene state/input is not deterministic replay.',
 'Presentation records observe the next top-screen VBlank after the completion callback requests a swap; not photodiode scanout.',
 'Interventions estimate cost bounds. No intervention is a shipping performance improvement.'], 'routes':[]}
for route in sorted(set(int(r['route']) for r in rows)):
 rr=[r for r in rows if r['route']==route];windows=[]
 for window in range(9):
  raw=[r for r in rr if int(r['ordinal'])//90==window];wr=[r for r in raw if int(r['ordinal'])%90>=5]
  if not wr:continue
  visible=sorted(r['visible_tick'] for r in wr if r['visible_tick']>0)
  presentation=[(b-a)*1000/268111856 for a,b in zip(visible,visible[1:])]
  w={'window':window,'mode':modes[int(wr[0]['mode'])],'fixed':int(wr[0]['fixed']),'frames':len(raw),'analysed':len(wr),
     'work_ms':distribution([r['work'] for r in wr]),'loop_ms':distribution([r['period'] for r in wr if r['period']>0]),
     'presentation_ms':distribution(presentation),'presentation_records':len(visible),'gpu_ms':distribution([r['gpu_ms'] for r in wr if r['gpu_ms']>=0]),
     'mean':{k:statistics.mean(r[k] for r in wr) for k in ['draws','vertices','suppressed','mip','nomip','entities','substeps','pairs','areas','linear']},
     'phase_ms':dict(zip(['update','render','service','load'],[statistics.mean(r[f'phase{i}'] for r in wr) for i in range(4)])),
     'inclusive_ms':{n:statistics.mean(r.get(f'i{i}',0) for r in wr) for i,n in enumerate(names)},
     'exclusive_ms':{n:statistics.mean(r.get(f'x{i}',0) for r in wr) for i,n in enumerate(names)}}
  windows.append(w)
 result['routes'].append({'route':route,'windows':windows})
print(json.dumps(result,indent=2))

#!/usr/bin/env python3
"""Summarize an ordinary-play incident without equating dispatch with pixels."""
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
import statistics

from summarize_streaming import percentile, rows


def analyze(path, catalog=None):
    data = rows(path)
    config = data['STREAM_CONFIG'][-1] if data['STREAM_CONFIG'] else {}
    end = data['PASSIVE_CAPTURE_END'][-1] if data['PASSIVE_CAPTURE_END'] else {}
    window = data['STREAM_WINDOW'][-1] if data['STREAM_WINDOW'] else {}
    admission = data['STREAM_ADMISSION'][-1] if data['STREAM_ADMISSION'] else {}
    frames = data['PASSIVE_FRAME']
    snapshots = data['STREAM_SNAPSHOT']
    probes = {int(p['id']): p for p in data['STREAM_PROBE']}
    labels = {}
    if catalog:
        labels = {f"{p['zone']:08x}": p['file']
                  for p in json.loads(catalog.read_text())['probes']}
    observations = defaultdict(list)
    unknown_probes = 0
    for row in data['STREAM_OBSERVATION']:
        identity = int(row['probe'])
        if identity not in probes:
            unknown_probes += 1
            continue
        observations[identity].append((int(row['route_ms']), int(row['flags'])))
    candidates = []
    for identity, seen in observations.items():
        missing = [(time, flags) for time, flags in seen if flags & 32 and not flags & 1]
        if not missing:
            continue
        probe = probes[identity]
        nearest = None
        if snapshots:
            last = snapshots[-1]
            nearest = math.sqrt(sum((float(probe[axis]) - float(last[axis])) ** 2
                                    for axis in ('x', 'y', 'z'))) - float(probe['radius'])
        # Report each observed absence and its next resident observation. An
        # earlier residency must not mask a later unload/reload failure.
        episodes = []
        active = None
        for time, flags in seen:
            if flags & 1:
                if active is not None:
                    active['next_resident_ms'] = time
                    active = None
            elif flags & 32:
                if active is None:
                    active = dict(first_absent_ms=time, last_absent_ms=time,
                                  next_resident_ms=None)
                    episodes.append(active)
                active['last_absent_ms'] = time
        candidates.append(dict(probe=identity, key=probe['key'], zone=probe['zone'],
            file=labels.get(probe['zone']), radius=float(probe['radius']),
            marker_distance_to_sphere_m=nearest, absent_samples=len(missing),
            last_observation_ms=seen[-1][0], last_flags=seen[-1][1],
            observed_absence_episodes=episodes))
    periods = [float(f['period_ms']) for f in frames if float(f['period_ms']) > 0]
    work = [float(f['work_ms']) for f in frames]
    events = [dict(e, file=labels.get(e['zone'])) for e in data['STREAM_EVENT']]
    valid = (config.get('passive') == '1' and config.get('catalog_valid') == '1'
             and config.get('audit') == '1' and config.get('lost') == '0'
             and config.get('events_lost') == '0'
             and int(admission.get('probe_bindings', 0)) > 0
             and bool(observations) and not unknown_probes)
    return dict(path=str(path), passive_capture=end, config=config, window=window,
        audit_valid=valid, unknown_probes=unknown_probes,
        performance_comparison_valid=False, visual_success_proven=False,
        limitation='Instrumented ordinary gameplay. Geometric demand, residency, '
                   'draw dispatch and displayed pixels are separate. Absence times '
                   'are sampled observations; marker reaction delay is unknown.',
        frames=len(frames), retained_frame_span_ms=(int(frames[-1]['ms']) - int(frames[0]['ms'])) if frames else None,
        period_mean_ms=statistics.mean(periods) if periods else None,
        period_p95_ms=percentile(periods, .95), period_p99_ms=percentile(periods, .99),
        period_max_ms=max(periods, default=None),
        work_p95_ms=percentile(work, .95), work_p99_ms=percentile(work, .99),
        frames_over_50ms=sum(p > 50 for p in periods),
        maximum_speed_mps=max((float(f['speed']) for f in frames), default=None),
        minimum_linear_bytes=min((int(s['linear']) for s in snapshots), default=None),
        minimum_sampled_normal_bytes=min((int(s['normal']) for s in snapshots), default=None),
        mean_active_peds=statistics.mean(int(s['peds']) for s in snapshots) if snapshots else None,
        mean_frustum_peds=statistics.mean(int(s['visible_peds']) for s in snapshots) if snapshots else None,
        fresh_ground_miss_samples=sum(s['ground_query_hit'] == '0' for s in snapshots),
        snapshots=snapshots, images=data['STREAM_IMAGE'],
        events_by_stage=dict(Counter(e['stage'] for e in events)), events=events,
        late_candidate_count=len(candidates), late_candidates=sorted(candidates,
            key=lambda p: (p['marker_distance_to_sphere_m'] if p['marker_distance_to_sphere_m'] is not None else math.inf,
                           -p['absent_samples'])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--catalog', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(analyze(args.capture, args.catalog), indent=2) + '\n'
    if args.output:
        args.output.write_text(result)
    else:
        print(result, end='')

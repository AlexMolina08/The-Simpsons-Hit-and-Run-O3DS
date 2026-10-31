#!/usr/bin/env python3
"""Report streaming candidates without treating requests or draws as visible pixels."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re
import statistics


def rows(path):
    result = defaultdict(list)
    for line in path.read_text(errors='replace').splitlines():
        tag = line.split(' ', 1)[0]
        result[tag].append(dict(re.findall(r'(\w+)=([^\s]+)', line)))
    return result


def percentile(values, quantile):
    if not values:
        return None
    values = sorted(values)
    return values[min(len(values) - 1, math.ceil(quantile * len(values)) - 1)]


def analyze(path):
    data = rows(path)
    config = data['STREAM_CONFIG'][-1] if data['STREAM_CONFIG'] else {}
    frames = data['NATIVE_FRAME']
    periods = [float(f['period_us']) / 1000 for f in frames if float(f.get('period_us', 0)) > 0]
    work = [float(f['work_us']) / 1000 for f in frames]
    samples = {int(s['route_ms']): s for s in data['STREAM_SNAPSHOT']}
    probes = {int(p['id']): p for p in data['STREAM_PROBE']}
    observations = defaultdict(list)
    for o in data['STREAM_OBSERVATION']:
        observations[int(o['probe'])].append((int(o['route_ms']), int(o['flags'])))
    candidates = []
    sectors = [0, 0, 0, 0]
    for identity, seen in observations.items():
        expected = [(time, flags) for time, flags in seen if flags & 32]
        missing = [(time, flags) for time, flags in expected if not flags & 1]
        p = probes[identity]
        if missing:
            first = expected[0][0]
            resident = next((time for time, flags in seen if flags & 1), None)
            submitted = next((time for time, flags in seen if flags & 2), None)
            candidates.append(dict(probe=identity, key=p['key'], zone=p['zone'],
                first_geometric_demand_ms=first, first_observed_resident_ms=resident,
                first_observed_submission_ms=submitted, absent_samples=len(missing),
                availability_lead_ms=None if resident is None else first - resident))
        # Full surrounding coverage is counted even outside the current camera.
        for time, flags in seen:
            s = samples.get(time)
            if not s:
                continue
            dx, dz = float(p['x']) - float(s['x']), float(p['z']) - float(s['z'])
            hx, hz = float(s['heading_x']), float(s['heading_z'])
            angle = math.atan2(hz * dx - hx * dz, hx * dx + hz * dz)
            sectors[int((angle + math.pi / 4) % (2 * math.pi) / (math.pi / 2))] += 1
    route = data['NATIVE_ROUTE'][-1] if data['NATIVE_ROUTE'] else {}
    capture = data['NATIVE_CAPTURE_END'][-1] if data['NATIVE_CAPTURE_END'] else {}
    complete = capture.get('complete') == '1'
    route_valid = complete and route.get('road') == '1' and route.get('recoveries') == '0' and route.get('path_exhausted') == '0'
    admission = data['STREAM_ADMISSION'][-1] if data['STREAM_ADMISSION'] else {}
    flight = bool(data['STREAM_FLIGHT'] and data['STREAM_FLIGHT'][-1].get('enabled') == '1')
    audit_valid = int(admission.get('probe_bindings', 0)) > 0 and config.get('audit') == '1' and config.get('catalog_valid') == '1' and config.get('lost') == '0' and config.get('events_lost') == '0' and bool(observations)
    performance_valid = route_valid and not flight and config.get('audit') == '0' and len(periods) >= 100
    return dict(path=str(path), config=config, route=route,
        route_valid=route_valid, audit_valid=audit_valid, route_kind='focus_sweep' if flight else 'gameplay',
        performance_valid=performance_valid, visual_success_proven=False,
        limitation='Absent resident probes are candidates for video review. Occlusion, selection, GPU acceptance and displayed pixels remain distinct.',
        frames=len(frames), loop_fps=1000/statistics.mean(periods) if periods else None,
        period_p95_ms=percentile(periods, .95), period_p99_ms=percentile(periods, .99),
        work_mean_ms=statistics.mean(work) if work else None,
        work_p95_ms=percentile(work, .95), work_p99_ms=percentile(work, .99),
        frames_over_50ms=sum(v > 50 for v in periods),
        minimum_linear_bytes=min((int(f['linear']) for f in frames), default=None),
        mean_active_peds=statistics.mean(int(f['active_peds']) for f in frames if 'active_peds' in f) if any('active_peds' in f for f in frames) else None,
        mean_frustum_peds=statistics.mean(int(f['frustum_peds']) for f in frames if 'frustum_peds' in f) if any('frustum_peds' in f for f in frames) else None,
        surrounding_probe_samples=sectors, late_candidate_count=len(candidates),
        late_candidates=sorted(candidates, key=lambda p: -p['absent_samples'])[:40],
        admission=admission,
        events_by_stage={stage: sum(e['stage'] == stage for e in data['STREAM_EVENT']) for stage in sorted({e['stage'] for e in data['STREAM_EVENT']})})


def compare(control, candidate):
    a, b = rows(control), rows(candidate)
    # The placement car can settle by centimetres before the first frame.
    # Require the same indexed route within 10 cm, then check measured camera
    # and position coverage separately; do not accept a different branch.
    wa, wb = a['STREAM_WAYPOINT'], b['STREAM_WAYPOINT']
    waypoints_equal = bool(wa) and len(wa) == len(wb) and all(
        left['index'] == right['index'] and sum((float(left[k])-float(right[k]))**2
        for k in ('x','y','z')) <= .01 for left, right in zip(wa, wb))
    fa, fb = a['NATIVE_FRAME'], b['NATIVE_FRAME']
    def coverage(source, target):
        # Restrict by time to avoid counting a later lap as the same traversal.
        found = 0
        for row in source[::5]:
            near = [r for r in target[::5] if abs(int(r['route_ms']) - int(row['route_ms'])) < 1500]
            def camera_match(r):
                if not all(k in r and k in row for k in ('camera_x','camera_z','target_x','target_z')):
                    return False
                ax, az = float(row['target_x'])-float(row['camera_x']), float(row['target_z'])-float(row['camera_z'])
                bx, bz = float(r['target_x'])-float(r['camera_x']), float(r['target_z'])-float(r['camera_z'])
                denominator = math.hypot(ax, az)*math.hypot(bx, bz)
                return denominator > 0 and (ax*bx+az*bz)/denominator >= math.cos(math.radians(15))
            found += any((float(r['x'])-float(row['x']))**2 + (float(r['z'])-float(row['z']))**2 <= 25 and camera_match(r) for r in near)
        return found / len(source[::5]) if source else 0
    forward, reverse = coverage(fa, fb), coverage(fb, fa)
    ca, cb = analyze(control), analyze(candidate)
    return dict(planned_route_equal=waypoints_equal, control_position_coverage=forward,
                candidate_position_coverage=reverse,
                comparable_performance=waypoints_equal and min(forward, reverse) >= .9 and ca['performance_valid'] and cb['performance_valid'],
                camera_match_required=True,
                camera_match_proven=min(forward, reverse) >= .9,
                fps_change_percent=100*(cb['loop_fps']/ca['loop_fps'] - 1) if ca['loop_fps'] and cb['loop_fps'] else None)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--control', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = analyze(args.capture)
    if args.control:
        report['comparison'] = compare(args.control, args.capture)
    text = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')

#!/usr/bin/env python3
"""Gate a physical world-residency stress run using fresh logs and allocator bounds."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
from summarize_streaming import rows


def analyze(folder, catalog, normal_reserve=2*1024*1024, linear_reserve=2*1024*1024):
    folder, catalog = Path(folder), Path(catalog)
    manifest = json.loads((folder/'manifest.json').read_text())
    expected = json.loads((catalog/'manifest.json').read_text())['levels']
    data = rows(folder/'console-logs/native-graphics-modes.log')
    boot = rows(folder/'console-logs/old3ds.log')
    failures = []

    def check(value, message):
        if not value:
            failures.append(message)

    def last(tag):
        values = data[tag]
        check(bool(values), 'Missing '+tag)
        return values[-1] if values else {}

    def number(record, key, default=-1):
        try:
            return int(record.get(key, default))
        except (TypeError, ValueError):
            return default

    recovery = manifest.get('manual_recovery', {})
    recovery_file = folder/'manual-recovery.json'
    if recovery_file.is_file():
        recovery = json.loads(recovery_file.read_text())
    transport_complete = bool(manifest.get('complete') and manifest.get('fresh_trial_verified')
                              and not recovery.get('receiver_reopened_by_user'))
    check(manifest.get('personal_save_unchanged'), 'Personal Save1 hash changed or was not verified')
    before_save = manifest.get('before', {}).get('Save1')
    after_save = manifest.get('after', {}).get('Save1')
    save = folder/'console-logs/Save1'
    check(before_save and before_save == after_save and save.is_file()
          and hashlib.sha256(save.read_bytes()).hexdigest() == after_save.get('sha256'), 'Save1 evidence does not match its hashes')
    artifact = Path(manifest['artifact'])
    binary = artifact.read_bytes() if artifact.is_file() else b''
    check(bool(binary) and hashlib.sha256(binary).hexdigest() == manifest['sha256'], 'Frozen executable differs from launch manifest')
    check(any(r.get('id') == manifest['trial'] for r in boot['NATIVE_LAUNCH']), 'Startup log belongs to a different trial')
    check(any(r.get('model') == 'old' and number(r, 'loader_region') == 64*1024*1024 for r in boot['OLD3DS_BOOT']), 'Old 3DS 64 MiB boot not established')
    check(any(r.get('result') == '00000000' for r in boot['OLD3DS_DEV_RETURN']), 'No clean receiver-return marker')
    check(any(record.get('id') == manifest['trial'] for record in data['NATIVE_RUN']), 'Native log belongs to a different trial')
    check(number(last('NATIVE_BUILD'), 'aborted') == 0, 'Run aborted')
    check(number(last('NATIVE_CAPTURE_END'), 'complete') == 1, 'Capture incomplete')
    plan, replay = last('NATIVE_RELOAD_PLAN'), last('NATIVE_REPLAY')
    destinations = number(plan, 'destinations')
    check(destinations > 0, 'Empty or invalid destination plan')
    check(number(replay, 'reload_dispatches') == destinations and number(replay, 'reload_phase') == destinations+1, 'Not every transition completed')
    arguments = set(manifest['arguments'])
    driving = None
    seconds = int(next((argument.split('=', 1)[1] for argument in arguments
                        if argument.startswith('bench_seconds=')), '0'))
    if seconds >= 60 and 'bench_route=road' in arguments:
        route, frames = last('NATIVE_ROUTE'), data['NATIVE_FRAME']
        try:
            speeds = [float(frame['speed']) for frame in frames]
            positions = [(float(frame['x']), float(frame['z'])) for frame in frames]
            distance = float(route['distance'])
            maximum = max(speeds)
            fast_seconds = sum(number(frame, 'real_elapsed', 0) for frame in frames
                               if float(frame['speed']) >= 20) / 1000
            span = max(max(point[axis] for point in positions)-min(point[axis] for point in positions)
                       for axis in (0, 1))
            duration = max(number(frame, 'route_ms') for frame in frames)
            check(all(map(math.isfinite, speeds+[distance, span])), 'Nonfinite driving telemetry')
            check(number(route, 'input_only') == 1 and number(route, 'road') == 1,
                  'Driving route did not use normal vehicle inputs')
            check(distance >= 400 and span >= 200, 'Driving route did not cover enough scenery')
            check(maximum >= 25 and fast_seconds >= 5, 'Driving route did not sustain high speed')
            check(duration >= seconds*1000-500 and number(last('NATIVE_BUILD'), 'overwritten') == 0,
                  'Driving capture is short or overwritten')
            driving = {'distance_metres': distance, 'maximum_speed_mps': maximum,
                       'seconds_above_20_mps': fast_seconds, 'map_span_metres': span,
                       'captured_route_ms': duration, 'input_only': True}
        except (KeyError, ValueError, TypeError):
            check(False, 'Invalid or absent driving telemetry')
    active = 'bench_reload_active=1' in arguments
    requested = {key: int('bench_reload_'+argument+'=1' in arguments)
                 for key, argument in [('resets', 'reset'), ('teleports', 'teleport'), ('interiors', 'interior')]}
    custom = next((a.split('=', 1)[1] for a in arguments if a.startswith('bench_reload_plan=')), None)
    itinerary = [tuple(map(int, entry.split(':'))) for entry in custom.split(',')] if custom else [(1, 1), (2, 2)]
    check(len(itinerary) == destinations, 'Logged plan length differs from launch arguments')
    phases = data['NATIVE_RELOAD_FRAMES']
    check(len(phases) == destinations and len({number(r, 'phase') for r in phases}) == destinations, 'Missing or duplicate phase reports')
    for index, record in enumerate(phases, 1):
        prefix = 'Phase '+str(index)+': '
        check(number(record, 'phase') == index and number(record, 'entered') == 1 and number(record, 'frames') > 0, prefix+'no measured gameplay')
        level, mission = itinerary[index-1] if index <= len(itinerary) else (-1, -1)
        check((number(record, 'level'), number(record, 'mission')) == (level, mission), prefix+'wrong destination')
        count = expected.get(str(level), {}).get('resident_meshes', -1)
        check(count > 0 and number(record, 'resident_expected') == count and number(record, 'resident_bound') == count and number(record, 'resident_bad') == 0, prefix+'missing or invalid resident scenery')
        check(number(record, 'mission_active') == int(active), prefix+'wrong active/free-roam mission mode')
        if 'graphics_stereo=1' in arguments:
            check(number(record, 'stereo_frames') > 0 and number(record, 'stereo_segments') > 0, prefix+'no actual stereo frame and right-eye replay evidence')
        for key, value in requested.items():
            check(number(record, key) == value, prefix+'incomplete '+key)
        if requested['teleports']:
            actions = [r for r in boot['NATIVE_RELOAD_ACTION'] if number(r, 'phase') == index and r.get('action') == 'teleport']
            check(len(actions) == 1 and float(actions[0].get('distance', 0)) >= 150, prefix+'no recorded long-distance teleport')
        if requested['interiors']:
            ground = [r for r in boot['NATIVE_INTERIOR_GROUND'] if number(r, 'phase') == index and number(r, 'valid') == 1]
            check({r.get('location') for r in ground} == {'inside', 'outside'}, prefix+'interior ground round trip incomplete')
        entries = [r for r in boot['NATIVE_RELOAD_ENTER'] if number(r, 'phase') == index]
        check(len(entries) == 1 and number(entries[0], 'stages', 0) > 0, prefix+'mission scripts were not initialized')
        if active:
            check(bool(entries) and not entries[0].get('name', '').endswith('sd'), prefix+'loaded a free-roam setup instead of an active mission')
    residency = last('WORLD_RESIDENCY')
    check(residency.get('mask') == '7f' and number(residency, 'count') == number(residency, 'bound') and number(residency, 'bad') == 0, 'Final resident catalog invalid')
    check(all(number(r, 'bad') == 0 for r in data['WORLD_RESIDENCY_LEVEL']), 'A resident level reported invalid bindings')
    queues = data['WORLD_ZONE_QUEUE']
    if b'WORLD_ZONE_QUEUE' in binary:
        check(bool(queues), 'Missing checked zone queue diagnostics')
    for queue in queues:
        check(number(queue, 'failures') == 0 and number(queue, 'peak') <= number(queue, 'capacity'),
              'Zone demand queue overflow or allocation failure')
    placement = last('WORLD_RESIDENCY_PLACEMENT')
    check(number(placement, 'static_failures') == 0 and number(placement, 'ground_failures') == 0, 'Scene placement capacity exhausted')
    check(number(last('NATIVE_TEXTURES'), 'invalid') == 0, 'Native texture integrity failure')
    for report in boot['NATIVE_WRAPPER_INTEGRITY']:
        check(number(report, 'failures') == 0, 'Persistent loader identity or vtable corruption')
    admissions = [record for record in data['WORLD_RESIDENCY_SET'] if 'opened' in record]
    if b'WORLD_RESIDENCY_SET opened=' in binary:
        check(bool(admissions), 'Missing production asset admission diagnostics')
    if admissions:
        admission = admissions[-1]
        check(number(admission, 'opened') == 1 and number(admission, 'published') == 1
              and admission.get('rejected') == '00', 'Production asset admission failed')
        used = {level for level, _ in itinerary}
        admitted = int(admission.get('admitted', '0'), 16)
        check(all(admitted & (1 << (level-1)) for level in used), 'A tested level never passed production admission')
    memory, maxima = last('NATIVE_MEMORY'), last('NATIVE_MEMORY_ALLOCATOR_MAXIMA')
    check(number(memory, 'normal_failures') == 0 and number(memory, 'linear_failures') == 0, 'Allocation failure')
    normal_peak = number(maxima, 'newlib_total_peak')
    capacities = data['NATIVE_MEMORY_CAPACITY']
    # libctru carves the main stack and TLS from the declared normal region.
    # Fresh builds report the actual newlib range. Older evidence is corrected
    # conservatively for the 2 MiB stack plus at least one TLS/alignment page.
    normal_capacity = (number(capacities[-1], 'normal_allocator_bytes') if capacities else
                       manifest['preflight']['normal_heap']-2*1024*1024-4096)
    check(0 < normal_capacity <= manifest['preflight']['normal_heap']-2*1024*1024,
          'Invalid normal allocator capacity or main-stack accounting')
    if capacities:
        check(number(capacities[-1], 'excludes_main_stack_and_tls') == 1,
              'Normal allocator capacity includes main stack or TLS')
    normal_available = normal_capacity-normal_peak if normal_peak >= 0 else -1
    linear_available = number(memory, 'linear_free_min')
    check(0 < linear_available <= manifest['preflight']['linear_heap'], 'Invalid linear free-memory bound')
    check(normal_available >= normal_reserve, 'Normal allocator commitment leaves insufficient reserve')
    check(linear_available >= linear_reserve, 'Linear memory leaves insufficient reserve')
    runtime_checks_passed = not failures
    check(transport_complete, 'Transport or fresh final capture failed')
    return {'trial': manifest['trial'], 'passed': not failures, 'failures': failures,
            'runtime_checks_passed': runtime_checks_passed, 'transport_complete': transport_complete,
            'destinations': destinations, 'active_missions': active, 'phases': phases,
            'driving_coverage': driving,
            'normal_commitment_peak': normal_peak, 'normal_allocator_capacity': normal_capacity,
            'normal_capacity_source': 'runtime_newlib_range' if capacities else 'conservative_declared_region_minus_stack_and_tls', 'normal_reserve_bytes': normal_available,
            'linear_minimum_bytes': linear_available, 'memory': memory,
            'performance_acceptance': False,
            'limitations': ['Memory instrumentation changes frame costs.',
                            'Normal bound covers newlib commitment, not separate custom heaps or SVC allocations.',
                            'A passed run establishes only its recorded missions, actions and dwell windows.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--normal-reserve', type=int, default=2*1024*1024)
    parser.add_argument('--linear-reserve', type=int, default=2*1024*1024)
    args = parser.parse_args()
    result = analyze(args.run, args.catalog, args.normal_reserve, args.linear_reserve)
    (args.run/'world-stress-validation.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({key: value for key, value in result.items() if key not in ('phases', 'memory')}, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Run bounded physical-console suites through an explicitly supplied receiver CLI.

Requires a frozen diagnostic build with protected save fixtures and a receiver
return hook. Nothing is uploaded when --dry-run is used. Stop after any transport,
freshness, save, memory or integrity failure; never retry a failed launch.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from summarize_world_stress import analyze

LEVELS = (4, 1, 2, 3, 5, 6, 7)


def cases(suite):
    if suite == 'missions':
        # Interleave maps, rather than hiding repeated-load problems behind one
        # long stay on each map. Mission numbers match the public mission menu.
        plan = ','.join(f'{level}:{mission}' for mission in range(1, 8) for level in LEVELS)
        return [('all-story-missions', 2, plan, 6, True, True, False, False, 5)]
    if suite == 'interiors':
        return [('seven-interiors', 2, ','.join(f'{level}:7' for level in LEVELS),
                 6, False, False, False, True, 5)]
    if suite == 'warps':
        return [('seven-warps', 2, ','.join(f'{level}:7' for level in LEVELS),
                 6, False, True, True, False, 5)]
    return [(f'level{level}-drive', level, f'{level}:7', 3, False, False, False, False, 90)
            for level in LEVELS]


def arguments(case, trial, asset_host, stereo):
    _, initial, plan, dwell, active, reset, warp, interior, seconds = case
    return [f'bench_asset_host={asset_host}', 'graphics_quality=1',
            f'graphics_stereo={int(stereo)}', 'graphics_fps=30',
            'bench_stereo_slider=0.55', 'bench_route=road', f'bench_seconds={seconds}',
            'bench_warmup=3', 'bench_hold=0', 'bench_quality=7', 'bench_cadence=1',
            f'bench_portrait_level={initial}', 'bench_vram_compact=1', 'bench_linear_compact=1',
            'bench_native_cache=0', 'bench_loader_core=0', 'bench_idle_loader=1',
            'bench_world_priority=1', 'bench_stream_audit=0', 'bench_scene_audit=0',
            'bench_service_profile=0', 'bench_memory_stride=128', 'bench_reload=1',
            f'bench_reload_plan={plan}', f'bench_reload_dwell={dwell}',
            f'bench_reload_active={int(active)}', f'bench_reload_reset={int(reset)}',
            f'bench_reload_teleport={int(warp)}', f'bench_reload_interior={int(interior)}',
            'bench_cruise_speed=32', f'bench_trial={trial}']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=('missions', 'interiors', 'warps', 'driving'), required=True)
    parser.add_argument('--cli', type=Path, required=True, help='Receiver development CLI; not a game-data downloader')
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--asset-host', required=True, help='Verified read-only development asset container on SD')
    parser.add_argument('--host', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--stereo', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch('[A-Za-z0-9_-]{1,35}', args.prefix):
        parser.error('Use a unique trial prefix of at most 35 simple characters')
    itinerary = []
    for case in cases(args.suite):
        trial = args.prefix+'-'+case[0]
        run = args.output/trial
        if run.exists() and not args.dry_run:
            parser.error(f'Existing run is preserved: {run}. Select a fresh prefix.')
        timeout = 240+120*len(case[2].split(','))+case[-1]
        command = [sys.executable, str(args.cli.resolve()), '--host', args.host, 'run',
                   str(args.artifact.resolve()), '--elf', str(args.elf.resolve()),
                   '--output', str(run.resolve()), '--keep-candidate', '--timeout', str(timeout)]
        for argument in arguments(case, trial, args.asset_host, args.stereo):
            command += ['--arg', argument]
        itinerary.append({'trial': trial, 'output': str(run), 'command': command})
    if args.dry_run:
        print(json.dumps(itinerary, indent=2))
        return 0
    for path in (args.cli, args.artifact, args.elf):
        if not path.is_file():
            parser.error(f'Missing prerequisite: {path}')
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/(args.prefix+'-plan.json')).write_text(json.dumps(itinerary, indent=2)+'\n')
    for item in itinerary:
        run = Path(item['output'])
        print('START '+item['trial'], flush=True)
        with run.with_suffix('.transport.log').open('w') as log:
            result = subprocess.run(item['command'], stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print('STOP: transport or capture failed; preserve console state and collect diagnostics.', flush=True)
            return result.returncode
        report = analyze(run, args.catalog)
        (run/'world-stress-validation.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({key: report[key] for key in ('trial', 'passed', 'failures',
              'normal_reserve_bytes', 'linear_minimum_bytes')}), flush=True)
        if not report['passed']:
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

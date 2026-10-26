#!/usr/bin/env python3
"""Expose sustained drops that a passing average can hide."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def capture(periods, gap=False):
    rows = []
    clock = 0.0
    for index, period in enumerate(periods):
        frame = index + 1 + (1 if gap and index >= 20 else 0)
        rows.append(
            f"NATIVE_FRAME frame={frame} route_ms={int(clock/1000)} elapsed=33 "
            f"incar=1 speed=10 x={index} z=0 substeps=1 context=9 loading=0 "
            f"draws=200 vertices=50000 pairs=40 areas=60 entities=30 "
            f"period_us={period} work_us=30000")
        clock += max(0, period)
    return '\n'.join([
        'NATIVE_BUILD scenario=loaded-level-road-v3 seconds=6 overwritten=0 aborted=0 mode=realtime',
        *rows, f'NATIVE_SUMMARY complete=1 frames={len(rows)}',
        'NATIVE_ROUTE road=1 points=256 point=4 distance=250 recoveries=0'])

with tempfile.TemporaryDirectory(prefix='srr-frame-pacing-') as directory:
    log = Path(directory) / 'capture.log'
    def summarize(periods, **kwargs):
        log.write_text(capture(periods, **kwargs))
        return json.loads(subprocess.check_output([
            'python3', str(root/'tools/3ds/summarize_native.py'), str(log),
            '--source', 'physical'], text=True))
    result = summarize([33333]*60 + [47000]*30 + [33333]*90)
    assert 26 < result['loop_period_fps'] < 30
    window = result['sustained_loop_periods']
    assert window['valid_windows'] == 151
    worst = window['worst']
    assert abs(worst['fps'] - 1e6/47000) < 1e-9
    assert worst['start_frame'] == 61 and worst['end_frame'] == 90
    assert worst['duration_us'] == 1410000
    assert worst['start_route_ms'] == 1999
    assert summarize([33333]*29)['sustained_loop_periods']['worst'] is None
    assert summarize([33333]*20 + [0] + [33333]*20)['sustained_loop_periods']['worst'] is None
    assert summarize([33333]*40, gap=True)['sustained_loop_periods']['worst'] is None
print('PASS: sustained 30-frame drops remain visible despite passing averages; short, invalid and discontinuous windows stay unavailable.')

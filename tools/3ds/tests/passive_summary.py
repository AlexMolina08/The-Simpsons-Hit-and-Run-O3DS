#!/usr/bin/env python3
"""Incident reports must retain reload failures and reject invalid observers."""
import importlib.util
from pathlib import Path
import sys
import tempfile

tools = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(tools))
spec = importlib.util.spec_from_file_location('passive_summary', tools / 'summarize_passive.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

log = '''NATIVE_RUN id=test passive=1
STREAM_CONFIG passive=1 catalog_valid=1 audit=1 lost=0 events_lost=0
STREAM_WINDOW requested_ms=20000 retained_observation_span_ms=20000 truncated=0
STREAM_ADMISSION probe_bindings=3
STREAM_PROBE id=0 key=abcd zone=beef level=2 x=0 y=0 z=0 radius=20
STREAM_OBSERVATION route_ms=0 probe=0 flags=97
STREAM_OBSERVATION route_ms=1000 probe=0 flags=96
STREAM_OBSERVATION route_ms=1250 probe=0 flags=96
STREAM_OBSERVATION route_ms=1500 probe=0 flags=97
STREAM_OBSERVATION route_ms=2000 probe=0 flags=96
STREAM_SNAPSHOT route_ms=2000 linear=4000000 normal=9000000 peds=4 visible_peds=2 ground_query_hit=0 x=5 y=0 z=0
PASSIVE_FRAME ms=1000 period_ms=30 work_ms=20 speed=30
PASSIVE_FRAME ms=2000 period_ms=1000 work_ms=990 speed=32
PASSIVE_CAPTURE_END marked=1 time_ms=2000 frames=2
'''
with tempfile.TemporaryDirectory() as temporary:
    path = Path(temporary) / 'incident.log'
    path.write_text(log)
    result = module.analyze(path)
    assert result['audit_valid']
    assert not result['visual_success_proven']
    assert not result['performance_comparison_valid']
    assert result['maximum_speed_mps'] == 32
    assert result['frames_over_50ms'] == 1
    assert result['fresh_ground_miss_samples'] == 1
    episodes = result['late_candidates'][0]['observed_absence_episodes']
    assert episodes == [dict(first_absent_ms=1000, last_absent_ms=1250, next_resident_ms=1500),
                        dict(first_absent_ms=2000, last_absent_ms=2000, next_resident_ms=None)]
    for bad in (log.replace('probe_bindings=3', 'probe_bindings=0'),
                log.replace('catalog_valid=1', 'catalog_valid=0'),
                log.replace('lost=0', 'lost=1'),
                log + 'STREAM_OBSERVATION route_ms=2000 probe=999 flags=96\n'):
        path.write_text(bad)
        assert not module.analyze(path)['audit_valid']
print('PASS: passive incident metrics remain diagnostic; reload episodes survive; invalid observers fail.')

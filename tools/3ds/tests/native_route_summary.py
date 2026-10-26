#!/usr/bin/env python3
"""Keep route labels from weakening or falsely failing driving coverage gates."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def capture(scenario, *, moving=True, aborted=0, recoveries=0, point=4, exhausted=0):
    header = (f"NATIVE_BUILD scenario={scenario} seconds=25 overwritten=0 "
              f"aborted={aborted} mode=realtime")
    rows = []
    for frame in range(25):
        rows.append(
            f"NATIVE_FRAME frame={frame} route_ms={frame * 1000} elapsed=1000 "
            f"incar=1 speed={10 if moving else 0} x={frame * 10} z=0 substeps=1 "
            "context=9 loading=1 draws=200 vertices=50000 pairs=40 areas=60 "
            "entities=30 period_us=1000000 work_us=30000")
    return "\n".join([header, *rows,
        "NATIVE_SUMMARY complete=1 frames=25",
        f"NATIVE_ROUTE road=1 points=256 point={point} distance=250 recoveries={recoveries} path_exhausted={exhausted}"])

with tempfile.TemporaryDirectory(prefix="srr-route-summary-") as directory:
    log = Path(directory) / "capture.log"
    def check(scenario, expected, **kwargs):
        log.write_text(capture(scenario, **kwargs))
        result = json.loads(subprocess.check_output(
            ["python3", str(root / "tools/3ds/summarize_native.py"), str(log),
             "--source", "physical"], text=True))
        assert result["route_health"]["coverage_pass"] is expected, (scenario, kwargs)
    for scenario in ("L2-road-v2", "loaded-level-road-v3"):
        check(scenario, True)
        check(scenario, False, moving=False)
        check(scenario, False, aborted=1)
        check(scenario, False, recoveries=2)
        check(scenario, False, point=1)
        check(scenario, False, exhausted=1)
    for scenario in ("L2-drive-v1", "loaded-level-drive-v2"):
        check(scenario, True)
        check(scenario, False, moving=False)
    check("character-portrait", False)
print("PASS: old and current road/drive labels retain movement, completion and route-health gates; stationary and invalid routes fail. Scenery is not inferred.")

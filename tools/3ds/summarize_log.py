#!/usr/bin/env python3
"""Summarize Old 3DS telemetry without treating emulator timings as hardware."""

import argparse
import json
import re
import statistics
from pathlib import Path


def fields(line):
    return {
        key: float(value)
        for key, value in re.findall(r"(\w+)=(-?\d+(?:\.\d+)?)", line)
    }


def summarize(path, source):
    frames = []
    profiles = []
    simulation = []
    world = None
    context = None
    for line in path.read_text(errors="replace").splitlines():
        values = fields(line)
        if line.startswith("OLD3DS_WORLD "):
            world = values
        elif line.startswith("OLD3DS_FRAME "):
            context = values.get("context")
            if context == 9 and "fps" in values:
                frames.append(values)
        elif line.startswith("OLD3DS_PROFILE ") and context == 9:
            profiles.append(values)
        elif line.startswith("OLD3DS_SIM ") and context == 9:
            simulation.append(values)

    def weighted(rows, key):
        usable = [row for row in rows if key in row and row.get("samples", 0) > 0]
        count = sum(row["samples"] for row in usable)
        return sum(row[key] * row["samples"] for row in usable) / count if count else None

    fps = [row["fps"] for row in frames]
    return {
        "file": str(path),
        "source": source,
        "scope": "Intervals ending in gameplay; transitions and suspension may affect samples.",
        "world": world,
        "gameplay_samples": len(frames),
        "median_fps": statistics.median(fps) if fps else None,
        "minimum_fps": min(fps) if fps else None,
        "samples_at_least_25_fps": sum(value >= 25 for value in fps),
        "minimum_linear_free_bytes": min((row["linear_free"] for row in frames if "linear_free" in row), default=None),
        "mean_profile_ms": {
            key: weighted(profiles, key)
            for key in ("update_ms", "render_ms", "service_ms", "load_ms", "gpu_wait_ms", "submit_ms", "work_ms")
        },
        "peak_work_ms": max((row["peak_ms"] for row in profiles if "peak_ms" in row), default=None),
        "over_budget_frames": (
            int(sum(row["over_budget"] for row in profiles if "over_budget" in row))
            if any("over_budget" in row for row in profiles) else None
        ),
        "mean_simulation_ms": {
            key: weighted(simulation, key)
            for key in ("physics_ms", "pedestrians_ms", "traffic_ms", "effects_ms")
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--source", choices=("hardware", "emulator", "unknown"), default="unknown")
    args = parser.parse_args()
    print(json.dumps([summarize(path, args.source) for path in args.logs], indent=2))


if __name__ == "__main__":
    main()

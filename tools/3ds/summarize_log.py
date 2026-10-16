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
    entities, operations, backend, phase2, drops = [], [], [], [], []
    buffers, dumps, anim_queries, animations = [], [], [], []
    pending_queries, pending_animations, weights = None, [], None
    for line in path.read_text(errors="replace").splitlines():
        values = fields(line)
        if line.startswith("OLD3DS_BUFFER "):
            buffers.append(values)
        elif line.startswith("OLD3DS_DUMP "):
            dumps.append(values)
        elif line.startswith("OLD3DS_ANIMQUERY "):
            pending_queries = values
        elif line.startswith("OLD3DS_ANIM "):
            # Preserve 64-bit asset identifiers exactly, not as float.
            uid = re.search(r"uid=(\d+)", line)[1]
            pending_animations.append((uid, {key: value for key, value in values.items() if key != "uid"}))
        elif line.startswith("OLD3DS_WEIGHTS "):
            weights = values  # cumulative asset-load histogram, not frame work
        elif line.startswith("OLD3DS_WORLD "):
            world = values
        elif line.startswith("OLD3DS_FRAME "):
            context = values.get("context")
            if context == 9:
                if pending_queries is not None:
                    anim_queries.append(pending_queries)
                animations.extend(pending_animations)
            pending_queries, pending_animations = None, []
            if context == 9 and "fps" in values:
                frames.append(values)
        elif line.startswith("OLD3DS_PROFILE ") and context == 9:
            profiles.append(values)
        elif line.startswith("OLD3DS_SIM ") and context == 9:
            simulation.append(values)
        elif line.startswith("OLD3DS_ENTITY ") and context == 9:
            entities.append((re.search(r"category=(\S+)", line)[1], values))
        elif line.startswith("OLD3DS_BACKEND ") and context == 9:
            operations.append((re.search(r"op=(\S+)", line)[1], values))
        elif line.startswith("OLD3DS_BCOUNTS ") and context == 9:
            backend.append(values)
        elif line.startswith("OLD3DS_PHASE2 ") and context == 9:
            phase2.append(values)
        elif line.startswith("OLD3DS_DRAW "):
            drops.append((context, values))

    def weighted(rows, key):
        usable = [row for row in rows if key in row and row.get("samples", 0) > 0]
        count = sum(row["samples"] for row in usable)
        return sum(row[key] * row["samples"] for row in usable) / count if count else None

    profile_count = sum(row.get("samples", 0) for row in profiles)

    def sampled_groups(rows):
        groups = {}
        for name, row in rows:
            group = groups.setdefault(name, {"calls": 0, "samples": 0, "sampled_ms": 0,
                                             "estimated_total_ms": 0})
            for key, value in row.items():
                if key not in ("frames", "estimated_ms_per_frame"):
                    group[key] = group.get(key, 0) + value
            if row.get("samples", 0):
                group["estimated_total_ms"] += row["sampled_ms"] * row["calls"] / row["samples"]
        for group in groups.values():
            group["estimated_ms_per_frame"] = group["estimated_total_ms"] / profile_count if profile_count else None
        return groups or None

    def counter_totals(rows):
        if not rows:
            return None
        return {key: sum(row.get(key, 0) for row in rows) for key in rows[0] if key != "frames"}

    entity_detail = sampled_groups(entities)
    backend_detail = sampled_groups(operations)
    backend_totals = counter_totals(backend)
    reconciliation = None
    if entity_detail and phase2:
        reconciliation = {
            "entity_count_difference": sum(v["calls"] for v in entity_detail.values()) - sum(r["opaque"] + r["translucent"] for r in phase2),
            "entity_draw_difference": sum(v["draws"] for v in entity_detail.values()) - sum(r["display_draws"] for r in phase2),
        }
        if backend_totals:
            reconciliation["backend_draw_difference"] = backend_totals["indexed"] + backend_totals["arrays"] - sum(v["draws"] for v in entity_detail.values())

    fps = [row["fps"] for row in frames]
    return {
        "file": str(path),
        "source": source,
        "scope": "Intervals ending in gameplay; transitions and suspension may affect samples.",
        "world": world,
        "telemetry_buffers": buffers or None,
        "diagnostic_dump_costs": dumps or None,
        "loaded_skin_weight_histogram": weights,
        "animated_bounds_queries": counter_totals(anim_queries),
        "animated_asset_counters": {
            uid: {key: sum(row.get(key, 0) for name, row in animations if name == uid)
                  for key in ("calls", "draws", "particle_calls", "translated_calls", "translated_draws")}
            for uid in dict(animations)
        } or None,
        "entity_categories_sampled": entity_detail,
        "backend_operations_sampled_nested": backend_detail,
        "backend_counter_totals": backend_totals,
        "detail_reconciliation_differences": reconciliation,
        "draw_drops": {
            scope: {key: int(sum(row.get(key, 0) for context, row in drops if (context == 9) == gameplay))
                    for key in ("dropped_noframe", "dropped_notdrawable", "dropped_noroom")}
            for scope, gameplay in (("gameplay", True), ("other_contexts", False))
        } if drops else None,
        "sampling_note": "Entity timings sample 1/4 calls; backend timings sample 1/32. Estimates can be biased; nested operations overlap.",
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

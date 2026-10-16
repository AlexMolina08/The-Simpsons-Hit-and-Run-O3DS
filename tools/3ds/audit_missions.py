#!/usr/bin/env python3
"""Inventory mission dependencies. Static checks do not prove mission completion."""

import argparse
import collections
import json
import pathlib
import re


def source(path):
    text = path.read_text(errors="replace")
    # Preserve quoted paths while removing both forms of script comment.
    return re.sub(r'"[^"\n]*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else "",
                  text, flags=re.S)


def calls(text, name):
    return re.findall(r'\b' + name + r'\s*\((.*?)\)\s*;', text, re.S)


def strings(arguments):
    return re.findall(r'"([^"\n]*)"', arguments)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()
    root = args.game.resolve()
    files = {p.relative_to(root).as_posix().lower(): p
             for folder in (root / name for name in ("art", "scripts", "sound", "movies"))
             for p in folder.rglob("*") if p.is_file()}
    objective_header = pathlib.Path(__file__).resolve().parents[2] / "code/mission/objectives/missionobjective.h"
    known = set(re.findall(r'"([a-z]+)"', objective_header.read_text()))
    levels = []
    objectives = collections.Counter()
    missing_assets = set()
    movies = set()
    unknown = set()
    for level in range(1, 8):
        directory = root / "scripts/missions" / f"level{level:02}"
        declarations = []
        for kind in ("AddMission", "AddBonusMission"):
            for arguments in calls(source(directory / "level.mfk"), kind):
                name = strings(arguments)[0]
                paths = [directory / f"{name}{suffix}.mfk" for suffix in ("l", "i")]
                texts = [source(p) for p in paths if p.exists()]
                kinds = [strings(a)[0] for text in texts for a in calls(text, "AddObjective")]
                objectives.update(kinds)
                unknown.update(set(kinds) - known)
                declarations.append({"name": name, "bonus": kind == "AddBonusMission",
                                     "missing_scripts": [p.name for p in paths if not p.exists()],
                                     "objectives": kinds, "completion": "not played through"})
        # Include Sunday-drive scripts and level-transition movies, not just races.
        for path in directory.glob("*.mfk"):
            text = source(path)
            for command in ("LoadP3DFile", "LoadDisposableCar"):
                for arguments in calls(text, command):
                    names = strings(arguments)
                    if names:
                        asset = names[0].replace("\\", "/").lower()
                        if asset not in files:
                            missing_assets.add(asset)
            for command in ("SetFMVInfo", "SetPostLevelFMV", "GagPlayFMV"):
                for arguments in calls(text, command):
                    names = strings(arguments)
                    if names:
                        movies.add(pathlib.PurePosixPath(names[0]).stem.lower())
        levels.append({"level": level, "missions": declarations})
    report = {"scope": "Static dependency review only; no mission completion claim",
              "levels": levels, "objective_counts": dict(sorted(objectives.items())),
              "unknown_objectives": sorted(unknown), "checked_asset_commands": ["LoadP3DFile", "LoadDisposableCar"],
              "missing_direct_load_assets": sorted(missing_assets),
              "movies": [{"name": name, "converted_file_present": f"movies/{name}.3dm" in files}
                         for name in sorted(movies)]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{sum(len(x['missions']) for x in levels)} mission declarations; "
          f"{len(objectives)} objective types; {len(missing_assets)} missing loose asset references")
    print(f"Report: {args.out}")
    return bool(unknown)


if __name__ == "__main__":
    raise SystemExit(main())

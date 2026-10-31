# World loading on Old 3DS

The original streaming path can load exterior scenery too late during fast
travel. The v1.2 preparation step moves eligible static scenery and exterior
ground into each level's permanent terrain section. Dynamic objects, missions,
interiors and interactive content keep their existing loading paths.

## Prepared assets

`tools/3ds/prepare_world_residency.py` prepares all seven levels after the normal
texture and visual conversion. The native packager and browser builder use the
same preparation rules.

A complete set contains 79 prepared exterior files, seven visibility catalogs,
landmark metadata and an admission manifest. Original files remain available.
The damaged-dam files for Levels 3 and 6 remain demand-loaded supplements.
Geometry, collision and image payloads are preserved. Private names isolate
moved resources from equal names in other source files; ambiguous dependencies
are rejected rather than rebound.

| Level | Resident static objects |
| --- | ---: |
| 1 | 733 |
| 2 | 539 |
| 3 | 590 |
| 4 | 739 |
| 5 | 547 |
| 6 | 604 |
| 7 | 519 |

## Runtime admission and visibility

`WorldResidencySet` validates the prepared set against its manifest before a
level opens its prepared exterior assets. An invalid set selects the complete
original path for that level. Once admitted, a prepared read failure is reported;
the loader does not mix prepared and original versions of that level.

`WorldResidency` binds catalog entries to loaded static entities. Authored zone
triggers still control visibility. Keeping the resources resident avoids a disk
load on approach without drawing every part of the map at once. This does not
remove all distance culling, object LOD changes or dynamic-object pop-in.

The entity lookup caches its catalog index without increasing the entity's
size. Unloading terrain clears its bindings. Pending zone requests use an owned,
bounded queue with duplicate coalescing; locators are not kept as borrowed
pointers across loading callbacks.

The normal release enables `SRR2_OLD3DS_WORLD_RESIDENCY` with
`SRR2_OLD3DS_RESIDENCY_ROOT=romfs:/world-residency`. Benchmarking, memory diagnostics,
profiling and development receiver hooks remain disabled in the release preset.

## Related lifetime fixes

Active mission characters retain their real models while distant, preventing
an invisible placeholder from needing a model reload as the player approaches.
The existing stage cleanup still releases those characters.

Parked-car locator cleanup first removes the exact object from its owning
inventory section, with the original global search as fallback. Nearby, upright
cars at rest use a smaller physics step to prevent suspension oscillation.
Fast-moving cars retain the existing step policy.

## Validation

Physical Old 3DS runs cover all seven catalogs, stereo interior round trips,
49 active story-mission loads and restarts, repeated level changes and fast
input-driven routes. Loading and restarting a mission is not a full playthrough.
Focused checks cover mission-model arrival and cleanup, parked suspension,
allocation failures, resource integrity and save preservation.

Performance remains a separate limit. Recent 45-second stereo driving stress
runs averaged about 21–25 FPS across the seven levels. An earlier normal Level 2
manual drive measured about 26 FPS. These routes differ and do not establish a
performance comparison or a game-wide 26–30 FPS result.

Host regression checks under `tools/3ds/tests/` cover admission, native/browser
asset parity, resource identity, visibility, queue ownership, level teardown,
mission-character lifetime and physics scheduling. Emulator and package checks
complement physical-console testing; they do not establish console performance.

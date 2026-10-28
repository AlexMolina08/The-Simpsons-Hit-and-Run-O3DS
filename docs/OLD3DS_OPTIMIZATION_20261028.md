# October 28 Old 3DS optimization closeout

## Promoted change

Original-model stereoscopic rendering now starts the GPU after the opaque world
pass instead of waiting for the rest of the CPU frame. This is the measured
phase-2 candidate. Mono and New-model presentation keep their existing policy.
If the opaque split cannot be submitted, the existing end-of-world boundary can
submit the available world commands instead.

The command sequence is unchanged. The renderer flushes the recorded prefix,
then replays that exact prefix followed by its original tail for the right eye.
There is no second scene traversal, geometry simplification, draw reordering or
new CPU wait. The queue completion gate waits to publish the SDK callback until
FrameEnd has armed its swap state. Stale intermediate callbacks cannot complete
an unfinished queue, and the original callback runs once.

The accepted adjacent fog/combiner render barriers remain in both native and SDK
paths. The accepted visual mask remains 255. Prepared assets, audio, gameplay,
saves, title identity and version are unchanged.

## Physical measurements

These are automated driving captures on the physical Old 3DS, not emulator
measurements. Each ABBA comparison contains two 25-second Level 2 stereo drives
per mode, with fixed route direction, matching planned route, no recoveries,
valid coverage and unchanged personal-save hashes. Compare modes within each
experiment; the two experiments use different executables.

| Submission boundary | Control FPS | Candidate FPS | Change | Control work p99 | Candidate work p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| End of world, phase 1 | 24.3145 | 25.0896 | +3.19% | 64.2784 ms | 59.2079 ms |
| After opaque world, phase 2 | 24.8215 | 25.3947 | +2.31% | 63.9070 ms | 56.3058 ms |

The phase-2 policy is enabled in normal release builds. Its benchmark override
exists only in explicitly instrumented builds, where it allows matched controls.
The SDK GPU timer excludes the early portion of GPU work; it must not be used
as total-frame GPU time. FPS above uses whole-loop periods, including pacing.

A fresh 60-second Level 2 stereo drive exercised the promoted default without
an early-queue argument: 24.7743 FPS, 58.6485 ms work p99, 993.286 m driven,
no recoveries, valid coverage and unchanged save. It recorded 1,888 queue starts
and exactly 1,888 final callbacks across the session. Its decoded left/right
post-route buffers contain the world and distinct eye views. These buffers do
not prove temporal flicker absence or full visual acceptance.

A 25-second Level 2 mono compatibility drive completed at 26.9585 loop FPS,
with valid coverage, no recoveries, unchanged save and zero early queue starts.
This verifies the mono exclusion; it is not an A/B performance improvement.

This longer drive is functional and coverage evidence. It is not a new matched
A/B comparison, and it must not be compared directly with the 25-second averages.

The additional Level 7 check aborted during benchmark placement before capturing
any route frames. The same executable with early submission explicitly disabled
also aborted there with zero route frames. Both returned cleanly with unchanged
saves. Neither run establishes Level 7 performance or route coverage; this
benchmark-setup limitation remains open.

## Release hardening

The release configuration disables NativeBench, optional/causal/memory profiles,
coarse pools, recurring/main/submission/vehicle profiles, collision verification
and GPU-skin metrics/A-B modes. Optional sampled diagnostics now default OFF.

Normal builds also compile out the formerly unconditional five-second report
ring, pause/exit report dumping, update/render stage timers, collision-volume
timers and GPU-skin palette timers. Frame pacing, HUD cadence, GPU-skin frame
numbering and allocation/retirement policies remain active. Early-queue counters
are absent from the production callback gate.

The actual release ELF and ARM preprocessed gameplay paths are checked by
`tools/3ds/tests/stable_runtime.py`. The check rejects benchmark/receiver/report
markers and confirms that accepted visual/asset features and the production
queue policy remain enabled. The startup budget passes with 507,904 bytes of
headroom, versus 364,544 bytes in the handoff runtime. This is fixed startup
budget headroom, not a measurement of runtime free heap or an FPS gain.

## Other investigations

- Increasing draw distance to 160 m, small-object distance to 100 m and ambient
  traffic to three more than doubled mean visible traffic, but reduced the
  short stereo route to 21.59 FPS. Those settings are not promoted.
- Stereo bounds culling and sparse stereo projection did not establish a useful
  whole-route gain. The projection prototype was removed.
- Exact static/fence query reuse had a low hit rate and negligible gain. Its
  implementation was removed; local experiment evidence remains archived.
- Fog setter reuse reduced flushes by about 59%, but needs repeated performance
  controls. It remains a benchmark-only, disabled probe.
- Texture source ordering improved one four-run Level 1 average by 0.54%, with
  mixed streaming tails. It is not a new global bundle layout.
- The exact legacy-format world cache removed measured fallback misses but did
  not improve the four-run Level 2 average. The accepted ETC1 index is restored.
- Compact ETC1 VRAM placement recovered approximately 1 MiB of linear memory in
  one probe, without a demonstrated FPS win. Unbuffered reads also need repeats.
  Both remain disabled benchmark-only controls.
- The untested seek I/O mutex-lending and larger VRAM-budget prototypes were
  archived and removed from normal source.

Normal gameplay keeps the accepted 120 m world distance, existing small-object
culling and one ambient-traffic limit. The simultaneous 26–30 FPS, much longer
viewing distances and denser traffic goal is not yet achieved across seven maps.

## Verification and provenance

Focused host checks cover 20,000 adversarial callback schedules in both release
and instrumented gates, first-use stereo uniforms, exact prefix replay, GPU
command visibility, world/HUD/bottom-screen scope, fog state parity, native
texture validation/reupload/retirement, collision preparation, pacing and
profile removal. The real ARM renderer, vehicle and character layouts agree:
context size 11,216 bytes; world/bottom/wheel offsets 8,020/8,028/8,032.

Private raw captures, frozen executables, source patches, build caches and
summaries remain under `out/local-dev/optimization-20261028/`. The refreshed
release runtime and its manifest are separate from the instrumented hardware
validation executable. No retail assets or generated installers are committed.
No CIA was created or installed, and no remote Git state was changed.

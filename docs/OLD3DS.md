# Original Nintendo 3DS

This experimental build targets the original 3DS and 2DS. It has been exercised
in Azahar 2126.2 with New 3DS mode disabled and CPU clock at 100%. The first physical Old 3DS log confirms a 64 MiB application region and
successful gameplay, but busy scenes miss the performance target. Across 146
samples ending in gameplay, median submission rate was 18.22 fps. The 25–30 fps
hardware target remains an acceptance requirement, not a verified result.

## Build and install

Install devkitPro's ARM toolchain, libctru, Citro3D, CMake, Ninja, Git and Python 3.
The build script compiles pinned SDL2, zlib and libpng sources locally; it does
not require administrator access or modify the installed toolchain.

```sh
bash tools/3ds/build_old3ds.sh
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds \
    --3dsx out/old3ds/SimpsonsNTSC.3dsx
```

Copy `sdcard/3ds` to the SD card root. The executable and game data belong in
`sd:/3ds/simpsons/`. Audio requires the console's own DSP firmware at
`sd:/3ds/dspfirm.cdc`, dumped using DSP1. No DSP firmware is bundled.

The executable uses a 30 MiB ordinary heap and a 28 MiB linear heap. Executable,
static data and runtime overhead also consume application memory. Azahar's
homebrew loader reports a 96 MiB region even in Old 3DS mode; the explicit heap
limits prevent allocation of that entire region. This does not establish that
every physical Homebrew Launcher entry method supplies sufficient memory.

## Controls

| Input | Action |
| --- | --- |
| Circle Pad | Move or steer |
| Hold L + Circle Pad | Camera; movement from Circle Pad is neutral while held |
| D-pad | Move or steer, including while L is held |
| L + Select | Cycle camera modes |
| Select | Reset vehicle |
| A | Jump; accelerate in a car |
| B | Run; brake/reverse in a car |
| X | Interact; enter or exit a vehicle |
| Y | Attack; handbrake in a car |
| R | Handbrake |
| L + R | Horn |
| Start | Pause |

The camera-cycle chord latches L when Select is pressed. Releasing L before
Select cannot turn the chord into a vehicle reset. An optional right stick
remains usable when L is not held. Original PC assets still show PC keyboard
instructions in some screens.

## Changes

- Correct the native thread stack unit error: the loader requested 64 MiB
  instead of 64 KiB. Native worker stacks now have a 128 KiB minimum and thread
  creation failure stops with an error instead of leaving an endless load.
- Use ARMv6 CP15 barrier specs and verify ARMv6K/VFPv2 ELF attributes. The local
  toolchain's default specs pulled ARMv7 attributes from an unused barrier
  helper; correcting the specs did not change the generated 3DSX bytes.
- Disable New 3DS CPU acceleration, hardware movie decoding and the second-eye
  render target. Preserve the native 400×240 image and existing world assets.
- Pace the loop to 30 Hz using the ARM11 system tick. Work exceeding the frame
  budget is not hidden; the frame finishes without an additional sleep.
- Log completed render submissions, memory usage and renderer timings every
  five seconds to `old3ds.log` beside the game data. A log sample's context is the
  context at the end of its interval; samples spanning transitions are mixed.

## Evidence and remaining checks

The local validation directory contains logs, screenshots, a binary checksum
and emulator performance samples. Verified paths include normal copyright and
frontend startup, Level 1 loading, walking, car entry, acceleration, traffic and
camera rotation on foot and in a car. The final binary was also booted with the
existing `skipfe notutorial` QA options in a separate emulator SD directory.
Those options are not present in the SD installation folder.

All seven level starts reached gameplay with the expected player and scene.
Screenshots and separate logs are stored as `out/validation/level2.png` through
`level7.png` and `level1.log` through `level7.log`. The summary in
`out/validation/seven-level-smoke.json` records the samples and memory headroom.
Levels 4 and 7 start inside the Simpsons' house; their startup checks do not
exercise the outdoor world. None of these checks establish complete missions
or sustained traversal through every streaming zone.

Settled emulator scenes reached approximately 29–30 fps with the 30 Hz limit.
The first 19 settled five-second Level 1 samples in
`out/validation/final-driving-summary.json` range from 28.88 to 29.99 fps,
with a mean of 29.82 fps. Minimum sampled linear headroom is 2,449,536 bytes;
maximum sampled ordinary heap usage is 13,841,112 bytes.
This is a functional and frame-pacing check, not a physical CPU/GPU benchmark.
Azahar's GPU timing figures must not be interpreted as PICA200 measurements.
Loading intervals run below 25 fps. No textures or world geometry were reduced
beyond the upstream renderer's existing conversions.

Host camera tests sweep all signed 16-bit axis values and cover modifier release
and the camera/reset chord. Run them with:

```sh
clang++ -std=c++11 -Wall -Wextra -Werror -I . \
    tools/3ds/tests/camera_input.cpp -o out/validation/camera_input
out/validation/camera_input
```

Still required: validation of the updated build on physical Old 3DS; sustained
on-foot and driving routes through busy streaming boundaries in every level;
complete missions, save/reload, suspend/resume, and audible sound with DSP firmware.
Movies are skipped. Audible output has not been verified in the local setup.

For hardware acceptance, collect `old3ds.log` while traversing representative
routes with traffic, pedestrians and collisions. Require 25–30 fps during
settled gameplay and inspect streaming stalls and memory headroom separately.
Record the console model, launcher method and executable SHA-256 with results.

## Local Mac launcher

`Play Old 3DS.command` opens the isolated installation under
`~/Applications/hitrun-old3ds/`. Its SD directory is separate from `sdcard/` and
contains the QA options described above. Keyboard controls are arrows for the
Circle Pad, Shift for L, Command for A, Option for B, Z for X, X for Y, M for
Start and N for Select. The emulator profile uses original 3DS mode and 100%
CPU clock. Hardware controls remain those in the table above.

## October 16 update

The pause Settings screen used console-only camera inversion and vibration
indices against PC menu assets. Missing rows compacted the menu, so entering
Settings accessed an absent item. The 3DS now uses the PC row layout. The crash
was reproduced in Azahar before the fix; the updated screen opens and accepts
changes.

An optional `old3ds-world-scale.txt` beside the executable selects `100`, `80`,
or `75` (a single integer). Missing or invalid settings select native 400×240.
The lower modes render the world at 320×192 or 300×180, then bilinearly upscale
it before rendering the GUI at native resolution. Frontend screens stay native.
Render targets are allocated once in VRAM, with native fallback on allocation
failure. These fixed modes are for hardware comparison; automatic resolution
scaling is not enabled. The architecture follows the intermediate texture and
presentation approach in Mario Kart 64 3DS commit
`3a5de70ddab1a4970f0b54a6240a898a13a639ba`, adapted to this backend's rotated
coordinates and GUI layer boundary.

Font texture chunks bypass the frontend atlas size reduction. This does not
increase glyph size; small PC text needs a separate readability pass.

The Old 3DS renderer now flushes command lists with `GX_CMDLIST_FLUSH`, avoiding
a second flush of the entire 28 MiB linear heap. Texture upload, persistent
vertex/index updates, expanded geometry and immediate vertex/index rings flush
their modified ranges explicitly. The scaled-world split also flushes immediate
ranges before submitting world commands. The native New 3DS path is unchanged.

`OLD3DS_PROFILE` logs interval averages for game update, render, services and
loading, plus GPU queue waits and frame submission. Wait and submission times
are subsets of render time, not additional costs. `cpu_ms` remains Citro3D's
renderer processing sample, not whole-frame CPU time. Emulator timing does not
predict physical ARM11/PICA200 throughput.

For an existing installation, replace only `SimpsonsNTSC.3dsx`; optionally add
the tiny scale file. Keep the installed game assets and saves. No bulk asset
transfer is required.

# Contributing

Small fixes, reproducible bug reports and hardware test results are welcome.
Check existing issues before starting a larger change so the scope can be discussed.

## Build and run

Use the [build guide](docs/OLD3DS.md) for prerequisites, data preparation, controls
and CIA packaging. You need your own legal PC copy of the game. Keep retail data,
DSP firmware, saves, generated binaries and personal logs out of source commits.

Original 3DS and 2DS remain the baseline: native 400×240 gameplay, ARMv6K/VFPv2
and the original-model application memory budget. Changes should also retain
New 3DS support and normal gameplay, missions, controls and save compatibility.

## Run the host checks

These checks need Python 3.11 or later and a C++ compiler on `PATH`. They use
synthetic fixtures and source-level harnesses; no game data or 3DS toolchain is needed.

```sh
python3 tools/3ds/tests/release_workflow.py
python3 tools/3ds/tests/movie_setup.py
python3 tools/3ds/tests/bundled_assets.py
```

GitHub Actions runs these checks for pushes to `main` and pull requests. Other
focused regression checks live in `tools/3ds/tests/`; run the ones relevant to
your change. Some require Clang, the 3DS toolchain or locally prepared game data.
A passing host check is not a substitute for building and testing the game.

For interface changes, run `ui_assets.py`, `context_text.py` and
`handheld_text.py` in `tools/3ds/tests/`. The first two require prepared game
data; the last also runs without it. Check the rendered screens and physical
hardware as well.

## Submit a change

- Keep each pull request focused on one fix or feature. Describe the problem,
  the resulting behavior and the checks you ran.
- Follow the surrounding code style and include a regression check when it can
  catch the reported failure.
- For performance work, name the console model, build revision, scene and route.
  Compare the same conditions and include frame-time measurements. Label emulator
  results separately from physical-console results.
- For gameplay or platform changes, report the hardware or emulator used and
  test the affected behavior, saving/loading and HOME/resume where relevant.
- Credit upstream work and retain existing notices. Do not add a license to
  inherited code or assets without permission from the relevant rights holders.

Use concise commit subjects that describe what changed. Include the reason and
validation in the body when they help a future maintainer understand the patch.

## Report a problem

Open a [bug report](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/issues/new?template=bug_report.yml)
with the release or commit, console model, installation method and reproduction
steps. Include expected and actual behavior. Logs or screenshots help, but do
not attach retail game data or personal saves unless specifically requested.

# The Simpsons: Hit & Run for Nintendo 3DS

A homebrew port for Nintendo 3DS, optimized for the original Nintendo 3DS and 2DS.
Based on x2apic's New 3DS port, with original-model rendering, memory, controls
and software movie playback work.
Building from source requires your own copy of the PC game. Game assets and DSP
firmware are not included in this source repository.

<img src="images/game.jpg" width="320" alt="The Simpsons: Hit & Run on Nintendo 3DS">

## Install the CIA

The complete CIA test build is available under
[Releases](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases).
Install it with FBI on a console configured to run homebrew CIAs. Game data and
all 17 converted movies are bundled; no separate game-data copy is needed.
Saves and settings use `sd:/3ds/simpsons/`. Audio requires your console's DSP
firmware at `sd:/3ds/dspfirm.cdc`, dumped with DSP1.

This is a test release. Check its release notes for the current validation status
and known limits. Performance varies by scene; demanding areas do not maintain
30 FPS.

## Original 3DS and 2DS

See the [complete source build guide and controls](docs/OLD3DS.md).
The guide covers toolchain installation, PC game data, movie conversion and
building an executable for the original hardware.

The original models use a single 400×240 gameplay view. The lower screen shows a
TV menu with touch selection, a frame-synced gameplay map, and the original newspaper loading art.
Performance varies by scene; demanding areas do not maintain 30 FPS.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install imageio-ffmpeg
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds
bash tools/3ds/build_old3ds.sh
cp out/old3ds/SimpsonsNTSC.3dsx sdcard/3ds/simpsons/
```

Install the prerequisites in the build guide before running these commands.
Keep `--old3ds`: it selects the software movie format required by original models.
These commands produce a Homebrew Launcher build. The guide also explains
[complete CIA packaging](docs/OLD3DS.md#build-a-complete-cia).

Copy `sdcard/3ds` to the SD card root. Launch through the Homebrew Launcher.
Sound requires your console's DSP firmware at `sd:/3ds/dspfirm.cdc`, dumped with DSP1.

## New 3DS and New 2DS XL

Install devkitPro with `3ds-dev`, `3ds-sdl` and `3ds-libpng`, plus Python and
`imageio-ffmpeg`. On Windows:

```bat
build.bat "C:\Program Files (x86)\Vivendi Universal Games\The Simpsons Hit & Run"
```

This builds the game and prepares its data under `sdcard/`.
To prepare data without building:

```sh
python3 tools/3ds/setup_sd.py "/path/to/PC game"
```

## Credits

Based on [x2apic's New 3DS port](https://github.com/x2apic/The-Simpsons-Hit-and-Run-N3DS)
and [ZenoArrows' Switch and PS Vita port](https://github.com/ZenoArrows/The-Simpsons-Hit-and-Run).
Movie playback follows [REGTA-3DSPort-Complete](https://github.com/Epic0522/REGTA-3DSPort-Complete) by Epic0522.
Built with devkitPro, libctru and Citro3D.

# Original Nintendo 3DS and 2DS

## Build from source

These commands build the original-model version on macOS or Linux using Bash.
On Windows, use a Linux environment such as WSL with the Linux toolchain installed
inside it. The root `build.bat` follows the separate New 3DS build path.

1. Install devkitPro using its [official setup instructions](https://devkitpro.org/wiki/Getting_Started)
   and install the `3ds-dev` package group. This supplies devkitARM, libctru,
   Citro3D and the 3DS asset tools. Keep the default `/opt/devkitpro` location,
   or export `DEVKITPRO` with your installation path.
2. Install host tools: Git, Bash, CMake, Ninja, GNU Make, Python 3 with `venv`
   and pip, and a host C compiler. On macOS, install the Xcode command-line
   tools with `xcode-select --install`. On Debian/Ubuntu, the host packages are
   `git build-essential cmake ninja-build python3 python3-venv python3-pip`.
3. Clone this port and enter its folder:

```sh
git clone https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS.git
cd The-Simpsons-Hit-and-Run-O3DS
```

Use your own installed retail PC game. Point setup at the folder containing
`art/`, `scripts/`, `sound/`, `movies/` and the `.rcf` archives. The build needs
internet access on its first run to download its source dependencies.

Prepare the data and movies before compiling:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install imageio-ffmpeg
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds
bash tools/3ds/build_old3ds.sh
cp out/old3ds/SimpsonsNTSC.3dsx sdcard/3ds/simpsons/
```

Alternatively, install FFmpeg on your host and pass `--ffmpeg /path/to/ffmpeg`
to setup instead of installing `imageio-ffmpeg`.

The setup step prepares the SD data and boot-menu assets embedded in the executable.
It converts PC movies to the MPEG-1 software format used on original models.
Do not omit `--old3ds`: the default movie format uses the New 3DS decoder.
Movie conversion can take several minutes; subsequent runs reuse unchanged movies.
The build script stops if those boot assets are missing. It builds the required SDL2,
zlib, libpng and software movie decoder locally, using `cmake/old3ds-release.cmake`.
No separately installed SDL or libpng package is required for this build path.
Automated routes, diagnostic rendering modes and experimental workers are disabled.
The executable is `out/old3ds/SimpsonsNTSC.3dsx`.

Copy `sdcard/3ds` to the SD card root. The game belongs in `sd:/3ds/simpsons/`.
Sound requires `sd:/3ds/dspfirm.cdc` from your console, dumped with DSP1.
Launch `SimpsonsNTSC.3dsx` through the Homebrew Launcher. This section describes
building and running from source; it does not produce an installable CIA.

To rebuild after a source change, run `bash tools/3ds/build_old3ds.sh` again.
Use `JOBS=2 bash tools/3ds/build_old3ds.sh` to limit parallel build jobs on a
host with less memory. Generated dependencies stay under `out/deps/`.

## Build a complete CIA

Prepare the retail data and original-model movies, then build the executable as
above. Use Python 3.11 or later for packaging. Obtain
[makerom](https://github.com/3DSGuy/Project_CTR/releases) and
[bannertool](https://github.com/carstene1ns/3ds-bannertool), then run:

```sh
python3 tools/3ds/build_cia.py \
  --makerom /path/to/makerom \
  --bannertool /path/to/bannertool
```

The output is `out/release/Simpsons-Hit-and-Run-3DS.cia`, with an asset manifest
and SHA-256 checksum beside it. It contains the prepared game assets and all 17
software-decoded movies. The packager excludes loose executables, personal saves,
logs and QA command files. It uses title ID `000400000f3a1100`, the original-model
64 MiB application mode, and region-free HOME Menu metadata.

Install the CIA with FBI on a console configured to run homebrew CIAs. No separate
game-data copy is needed. Saves and settings stay in `sd:/3ds/simpsons/`; keep
your console's DSP firmware at `sd:/3ds/dspfirm.cdc` for audio. Installing from a
CIA stored on SD requires space for both that file and the installed title.

Successful packaging does not establish release readiness. Test the installed
CIA with no loose game assets, including movies, saving/loading and HOME/resume,
on original hardware before distributing a release.

## Controls

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Move or steer |
| Hold L + Circle Pad | Move camera |
| L + Select | Cycle camera |
| Select | Reset vehicle |
| A | Jump / accelerate |
| B | Run / brake and reverse |
| X | Interact / enter or exit vehicle |
| Y | Attack / handbrake |
| R | Handbrake |
| L + R | Horn |
| Start | Pause |

The D-pad remains available for movement while L controls the camera.
Camera and map settings are available from the game menus.

## Screens

Gameplay uses the native 400×240 top screen. The lower screen shows a live map,
with tutorials moved below to keep the world visible. The map refreshes with each
rendered frame; its refresh rate follows the game's frame rate.

The main menu uses a purple TV surround and animated static behind the menu choices.
Use the controller as usual, or tap a choice to highlight it and tap again to select.
Menu artwork and messages remain above; menu actions use a separate lower-screen
layout. Startup loading shows the logo above and a loading label below. Level
loading uses the original newspaper art below, without static. Cinematics play on
the top screen only. Small interface text is enlarged for the native display and
uses smooth alpha coverage.

Original-model builds disable stereoscopic rendering and New 3DS CPU acceleration.
The loop targets 30 FPS, but busy scenes and streaming can run slower. An emulator's
frame rate does not predict performance on the original console.

## Development

The build script checks ARMv6K/VFPv2 executable attributes and the fixed 64 MiB startup
budget. Runtime memory use also depends on loaded assets. Source checks live under
`tools/3ds/tests/`; generated builds and local logs are excluded from Git.

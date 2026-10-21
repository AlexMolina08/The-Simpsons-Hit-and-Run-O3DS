# Original Nintendo 3DS and 2DS

## Prerequisites

Use Bash on macOS or Linux. On Windows, use WSL and install the Linux toolchain
inside it. The original `build.bat` follows the separate New 3DS build path.

- Install devkitPro's [3ds-dev package group](https://devkitpro.org/wiki/Getting_Started).
  Keep `/opt/devkitpro`, or export `DEVKITPRO` with your installation path.
- Install Git, Bash, CMake 3.28+, Ninja, GNU Make, Python 3.11+ with Pillow and a host C compiler.
  On Debian/Ubuntu: `sudo apt install git build-essential cmake ninja-build python3 ffmpeg`.
  Check `cmake --version` and `python3 --version`; older distributions need newer packages.
- Install FFmpeg, or use `python3 -m pip install imageio-ffmpeg` in a Python virtual environment.
- Use your installed retail PC game, including all 17 `.rmv` movies. Earlier
  development/leaked data is not supported.

## macOS

Install the Xcode command-line tools, [Homebrew](https://brew.sh) and devkitPro's
macOS installer with **3ds-dev**. Install host tools:

```sh
xcode-select --install
brew install cmake ninja python ffmpeg
```

## Linux

Ubuntu 24.04 LTS provides the required Python and CMake versions:

```sh
sudo apt update
sudo apt install git build-essential cmake ninja-build python3 python3-venv ffmpeg
```

Install devkitPro's package manager using its [Linux setup instructions](https://devkitpro.org/wiki/devkitPro_pacman),
then install the **3ds-dev** package group with `sudo dkp-pacman -S 3ds-dev`.
The build uses `/opt/devkitpro` by default.

## Windows (WSL 2)

Use [Microsoft's WSL installation guide](https://learn.microsoft.com/en-us/windows/wsl/install).
In an administrator PowerShell terminal:

```powershell
wsl --install -d Ubuntu-24.04
```

Restart if requested and open Ubuntu. Follow the Linux steps above inside Ubuntu,
including the Linux devkitPro installation. Clone the repository inside your Linux
home directory (`cd ~`), rather than under `/mnt/c`, for faster compilation and
reliable file permissions. Your PC game can remain on Windows: for example,
`C:\Games\Hit and Run` becomes `/mnt/c/Games/Hit and Run`.
All remaining commands run in Ubuntu, not PowerShell. To retrieve the result,
run `explorer.exe out/release` from the repository root.

## Clone and prepare Python

On every supported host:

```sh
git clone https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS.git
cd The-Simpsons-Hit-and-Run-O3DS
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install Pillow
```

Reactivate this environment in a new terminal with `source .venv/bin/activate`.
FFmpeg can also come from `python3 -m pip install imageio-ffmpeg` in this environment.

## Build a complete CIA

From the repository root:

```sh
python3 tools/3ds/make_cia.py "/path/to/PC game"
```

The command validates prerequisites, prepares game data, converts the movies,
fully decodes all 17 conversions, compares frame counts and complete PCM against
the originals, rebuilds the accepted native texture bundle, compiles dependencies
and the game, checks ARMv6K/VFPv2 and the fixed 64 MiB startup
budget, then packages a complete CIA. Normal play is enabled. The release profile preserves the accepted v104 rendering
options, including geometry input and prepared ETC1 world textures. It disables
benchmark routes and profiling.

Outputs:

- `out/release/Simpsons-Hit-and-Run-3DS.cia`
- `out/release/Simpsons-Hit-and-Run-3DS.sha256`
- `out/release/Simpsons-Hit-and-Run-3DS.manifest.json`
- `out/release/movie-validation.json`: full decode, frame count and exact source PCM checks.
- `out/release/Simpsons-Hit-and-Run-3DS.home-menu/`: generated banner, icon and audio provenance.

The HOME Menu sound is a short excerpt of the retail game's theme, with fades.
The music excerpt is generated from your files rather than stored in Git.
The packager includes only game assets and converted movies; it excludes saves,
settings, logs and loose executables. The title ID is `000400000f3a1100`.

Use `--clean` to move `out/deps`, `out/old3ds-build`, `out/old3ds` and `out/native-textures` to the system
Trash before a fresh compilation. The command checks the PC game and tools first.
It preserves original game files, saves, previous releases and local test evidence.
Use `JOBS=2 python3 tools/3ds/make_cia.py "/path/to/PC game"` to reduce parallel build memory use.

On macOS (Apple Silicon and Intel), Linux x86-64 and WSL, packaging tools are
prepared automatically from pinned upstream sources with verified download hashes.
On other architectures, install [makerom](https://github.com/3DSGuy/Project_CTR/releases)
and [bannertool](https://github.com/carstene1ns/3ds-bannertool), or pass their paths:

```sh
python3 tools/3ds/make_cia.py "/path/to/PC game" \
  --makerom /path/to/makerom --bannertool /path/to/bannertool
```

On macOS, double-click **Build CIA.command** for the same local workflow.
This does not require GitHub authentication and never publishes anything.

## Install

Copy the CIA to SD and install it with **FBI → SD → Install CIA**. A complete CIA
needs no separate game-data copy. Keep room for both the CIA and the installed
title. After installation, launch the game from HOME Menu.

Dump your console's DSP firmware with DSP1 and keep it at `sd:/3ds/dspfirm.cdc`.
Saves and settings remain in `sd:/3ds/simpsons/`; preserve that folder when updating.
Test movies, save/load, HOME/resume and demanding gameplay on an original console.
A successful build or emulator run does not prove hardware performance or stability.

## Homebrew Launcher build

To prepare data and compile without CIA packaging:

```sh
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds
bash tools/3ds/build_old3ds.sh
cp out/old3ds/SimpsonsNTSC.3dsx sdcard/3ds/simpsons/
```

Copy `sdcard/3ds` to SD root and launch `SimpsonsNTSC.3dsx` through the Homebrew
Launcher. Keep `--old3ds`: it selects the software movie format. Incremental
setup reuses unchanged movies. Compilation dependencies live under `out/deps/`.
Rebuild with `bash tools/3ds/build_old3ds.sh` after source changes.

## Screens and controls

See the [README](../README.md#interface-and-controls) for the control mapping.
Menus accept touch, D-pad and Circle Pad input. Selection commits on release;
dragging away cancels. The main menu scrolls while preserving neighboring choices.
Gameplay keeps contextual information and the original radar on the lower screen.
The radar refreshes at up to 15 Hz. Mission timing and driving overlays retain
their existing update paths. Original-model builds use one 400×240 gameplay view.
New models automatically enable CPU/L2 acceleration with the same memory budget.

## Publish a release

Publishing is separate from local building. **Release CIA.command** invokes the
versioned release launcher. Commit source changes on `main` first. Install GitHub
CLI and sign in using `gh auth login`. The launcher prepares packaging tools,
rebuilds, packages, tags and uploads a test release. It verifies server checksums
before publishing and does not overwrite an existing public version.

```sh
python3 tools/3ds/release_cia.py --version 0.1.3-rc.1 --no-upload
python3 tools/3ds/release_cia.py --version 0.1.3-rc.1 --resume
```

The first command builds a versioned local package. The second resumes an
interrupted upload after verifying source/tag/artifact integrity. Use `--dry-run`
to inspect the plan. Use `--stable` only after the intended hardware validation.
Builds live under `out/releases/vVERSION/`; this workflow preserves earlier packages.

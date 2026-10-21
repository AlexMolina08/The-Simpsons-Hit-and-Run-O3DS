# Original Nintendo 3DS and 2DS

## Prerequisites

Use Bash on macOS or Linux. On Windows, use WSL and install the Linux toolchain
inside it. The original `build.bat` follows the separate New 3DS build path.

- Install devkitPro's [3ds-dev package group](https://devkitpro.org/wiki/Getting_Started).
  Keep `/opt/devkitpro`, or export `DEVKITPRO` with your installation path.
- Install Git, Bash, CMake 3.28+, Ninja, GNU Make, Python 3.11+ with Pillow and host C and C++ compilers.
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
run `explorer.exe out/releases/v1.0.0` from the repository root.

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
the originals, preserves video proportions with letterboxing where needed, rebuilds the accepted native texture bundle, compiles dependencies
and the game, checks ARMv6K/VFPv2 and the fixed 64 MiB startup
budget, then packages a complete CIA. Normal play is enabled. The release profile preserves the accepted v104 rendering
options, including geometry input and prepared ETC1 world textures. It disables
benchmark routes and profiling.

Outputs:

- `out/releases/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0.cia`
- `out/releases/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0.sha256`
- `out/releases/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0.manifest.json`
- `out/releases/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0.movies.json`: full decode, frame count and exact source PCM checks.
- `out/releases/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0.home-menu/`: generated banner, icon and audio provenance.

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

### Update or start fresh

[Follow the README update steps](../README.md#updating-an-existing-installation).
Install the new CIA over the existing title with FBI. Copying a CIA to SD alone
does not update the installed game. Preserve `sd:/3ds/simpsons/` for saves and
settings, or back it up and rename it for a fresh start. The title ID and game
save format are unchanged. Old `maptop` settings are safely ignored.

### Console checks for this build

After installing, verify the following on your console model:

1. Launch from HOME Menu and check the banner sound, icon and boot movies.
2. Load an existing save, move and drive, collect coins, and open the pause menu.
3. Navigate menus with D-pad, Circle Pad and touch. Drag each sound slider and
   tap its label; a label tap should not change the value.
4. Start a mission, inspect its briefing, play a conversation and watch a movie.
5. Close the lid during gameplay, pause and movie playback. Reopen it: expect
   one short greeting, then normal game audio. Repeat to check variety. Set
   Voice to zero and repeat; the greeting should be silent. HOME return alone
   should not play a greeting.
6. Open **Main Menu → Options → View Credits**. Check scrolling, then leave with
   A, B or the touch-screen Back control. Reopen credits to check the return path.
7. Save, exit and reopen the game. Check progress and sound settings survived.

Report console model and any error. Host checks and emulator screenshots do not
establish physical-console sleep behavior, movie smoothness or performance.

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

## Lid-open greetings

A confirmed sleep/wake cycle triggers one complete character greeting. Ten
original English retail lines cover Homer, Bart, Lisa, Marge and Apu equally.
A shuffled bag plays every line before repeating, including a repeat guard at
bag boundaries. The random generator is separate from gameplay randomness.
HOME suspension/restoration alone does not play a greeting.

Setup extracts these lines from your `dialog.rcf`, matches their volume gently
and prepares mono 24 kHz PCM. Source archives remain unchanged. No voice assets
are committed to Git. CIA packaging includes the prepared clips automatically.
The game reserves one of the 24 DSP channels, reads only the chosen clip after
wake and frees its PCM when playback ends. The bounded buffer is at most 144 KB;
no polling file reads or audio decoding run during normal gameplay.

The cue respects master and Voice volume, and remains silent if DSP firmware is
unavailable. It does not change music playback positions or game settings. This
feature runs while the game is active; it cannot add sounds to the system HOME
Menu or other applications. Actual sleep/wake audio needs a physical console check.

## Publish a release

Publishing is separate from local building. **Release CIA.command** invokes the
versioned release launcher. Commit source changes on `main` first. Install GitHub
CLI and sign in using `gh auth login`. The launcher prepares packaging tools,
rebuilds, packages, tags and uploads a test release. It verifies server checksums
before publishing and does not overwrite an existing public version.

```sh
python3 tools/3ds/release_cia.py --version 1.0.0 --stable --no-upload
python3 tools/3ds/release_cia.py --version 1.0.0 --stable --resume
```

The first command builds a versioned local package. The second resumes an
interrupted upload after verifying source/tag/artifact integrity. Use `--dry-run`
to inspect the plan. Use `--stable` only after the intended hardware validation.
Builds live under `out/releases/vVERSION/`; this workflow preserves earlier packages.

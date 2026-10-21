# The Simpsons: Hit & Run for Nintendo 3DS

A homebrew port for original Nintendo 3DS and 2DS. The same build also runs on
New 3DS and New 2DS XL, enabling their faster CPU and L2 cache automatically.
You need your own retail PC copy of the game to build from source.
Game data and DSP firmware are not distributed in this source repository.

<img src="images/game.jpg" width="320" alt="The Simpsons: Hit & Run on Nintendo 3DS">

## Install

1. Download a CIA from [Releases](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases),
   or build one below. Read that version's release notes; published packages may
   predate the current source.
2. Copy the CIA to your console's SD card. Keep enough free space for both the
   CIA and the installed game.
3. In **FBI → SD**, select the CIA and choose **Install CIA**. Once installed,
   launch it from the HOME Menu. The complete CIA includes the game data and movies.
4. Sound needs your console's DSP firmware at `sd:/3ds/dspfirm.cdc`, dumped with DSP1.

Saves and settings live in `sd:/3ds/simpsons/`. Keep that folder when updating.
Performance varies by scene; demanding areas can run below 30 FPS. Emulator
results do not establish performance on an original console.

## Build a CIA

macOS and Linux need Git, Bash, CMake 3.28+, Ninja, GNU Make, a host C compiler,
Python 3.11+, Pillow and FFmpeg. Install devkitPro's `3ds-dev` package group using its
[setup guide](https://devkitpro.org/wiki/Getting_Started).
Windows users use WSL 2 with Ubuntu 24.04. Install its Linux devkitPro toolchain,
then follow the [Windows setup steps](docs/OLD3DS.md#windows-wsl-2).

On macOS, install the Xcode command-line tools and host dependencies:

```sh
xcode-select --install
brew install cmake ninja python ffmpeg
```

After installing devkitPro, clone the source and build:

```sh
git clone https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS.git
cd The-Simpsons-Hit-and-Run-O3DS
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install Pillow
python3 tools/3ds/make_cia.py "/path/to/PC game"
```

Point at the PC folder containing `art/`, `scripts/`, `sound/`, `movies/` and the
`.rcf` archives. The command prepares retail data, converts and fully validates all 17 movies, rebuilds the accepted native texture bundle, builds
the original-model executable and packages **`out/release/Simpsons-Hit-and-Run-3DS.cia`**.
Its checksum, movie validation report and asset manifest appear alongside it. The first build downloads
source dependencies and can take several minutes. Later builds reuse them.

On macOS, **Build CIA.command** runs the same local build and asks for the PC
folder. It does not upload, tag or publish anything. macOS (Apple Silicon and Intel), Linux x86-64 and WSL can
prepare makerom and bannertool automatically. Other architectures must install them or
pass `--makerom /path/to/makerom --bannertool /path/to/bannertool`.

Use `--clean` for a fresh dependency and game compilation. It moves previous
compilation outputs and generated texture cache to the system Trash after checking prerequisites. Retail
files, saves, screenshots and existing release packages are preserved.
See the [build guide](docs/OLD3DS.md) for Linux prerequisites, Homebrew Launcher
builds, controls and publishing a separate release.

## Interface and controls

Gameplay uses the native 400×240 top screen. The touch screen carries the original
radar, coin and wasp counters, current objective and a contextual interaction control.
Menus use original game artwork and fonts, with touch, D-pad and Circle Pad navigation.
Tap a menu item to select it; drag away before releasing to cancel. Slider labels
focus a row; their tracks adjust the value. The map always uses the lower screen.

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Move, steer or navigate menus |
| A | Jump / accelerate / confirm |
| B | Run / brake and reverse / back |
| X | Interact / enter or exit vehicle |
| Y | Attack / handbrake |
| Hold L + Circle Pad | Move camera |
| R | Handbrake |
| L + R | Horn |
| Select | Reset vehicle |
| L + Select | Cycle camera |
| Start | Pause / continue |

Port credits are under **Pause → Options → Settings**. Original game credits
remain available through the game's existing credits menu.

Reopening the lid while the game is running plays a short greeting from Homer,
Bart, Lisa, Marge or Apu. Ten complete retail lines play in a shuffled order,
with no consecutive repeats. Greetings follow the **Voice** volume setting;
setting Voice to zero silences them. Returning from HOME alone does not trigger one.

## Development

`cmake/old3ds-release.cmake` defines the normal-play build profile. It preserves the accepted v104 rendering options and
original-model memory limits, with benchmark automation and profiling disabled. The local `old3ds-ui-baseline` tag marks the
UI baseline. Build and package checks do not replace installed-console testing.

See [CONTRIBUTING.md](CONTRIBUTING.md) for regression checks and bug reports.

## Credits

The original game was developed by Radical Entertainment.
This port builds on [x2apic's New 3DS port](https://github.com/x2apic/The-Simpsons-Hit-and-Run-N3DS)
and [ZenoArrows' Switch and PS Vita port](https://github.com/ZenoArrows/The-Simpsons-Hit-and-Run).
Old 3DS work is maintained by [AlexMolina08](https://github.com/AlexMolina08).
Movie playback follows [REGTA-3DSPort-Complete](https://github.com/Epic0522/REGTA-3DSPort-Complete) by Epic0522.
Built with devkitPro, libctru and Citro3D.

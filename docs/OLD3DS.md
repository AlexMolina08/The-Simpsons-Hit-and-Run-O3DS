# Original Nintendo 3DS and 2DS

## Build

Install devkitPro's ARM toolchain, libctru, Citro3D, CMake, Ninja, Git and Python 3.
Prepare assets from your own PC installation, then build:

```sh
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds
bash tools/3ds/build_old3ds.sh
cp out/old3ds/SimpsonsNTSC.3dsx sdcard/3ds/simpsons/
```

The setup step prepares the SD data and boot-menu assets embedded in the executable.
The build script stops if those boot assets are missing. It builds the required SDL2,
zlib, libpng and software movie decoder locally, using `cmake/old3ds-release.cmake`.
Automated routes, diagnostic rendering modes and experimental workers are disabled.
The executable is `out/old3ds/SimpsonsNTSC.3dsx`.

Copy `sdcard/3ds` to the SD card root. The game belongs in `sd:/3ds/simpsons/`.
Sound requires `sd:/3ds/dspfirm.cdc` from your console, dumped with DSP1.

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
Loading screens use the original newspaper art, without static. Small interface
text is enlarged for the native display and uses smooth alpha coverage.

Original-model builds disable stereoscopic rendering and New 3DS CPU acceleration.
The loop targets 30 FPS, but busy scenes and streaming can run slower. An emulator's
frame rate does not predict performance on the original console.

## Development

The build script checks ARMv6K/VFPv2 executable attributes and the fixed 64 MiB startup
budget. Runtime memory use also depends on loaded assets. Source checks live under
`tools/3ds/tests/`; generated builds and local logs are excluded from Git.

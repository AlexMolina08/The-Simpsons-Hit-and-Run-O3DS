# The Simpsons: Hit & Run for Nintendo 3DS

A homebrew port for Nintendo 3DS, including the original 3DS and 2DS.
You need your own copy of the PC game. Game assets and DSP firmware are not included.

<img src="images/game.jpg" width="320" alt="The Simpsons: Hit & Run on Nintendo 3DS">

## Original 3DS and 2DS

See [building and controls](docs/OLD3DS.md).

The original models use a single 400×240 gameplay view. The lower screen shows a
TV-framed map during gameplay and animated TV static in menus and loading screens.
Performance varies by scene; demanding areas do not maintain 30 FPS.

```sh
python3 tools/3ds/setup_sd.py "/path/to/PC game" --old3ds
bash tools/3ds/build_old3ds.sh
cp out/old3ds/SimpsonsNTSC.3dsx sdcard/3ds/simpsons/
```

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

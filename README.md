# The Simpsons: Hit & Run for Nintendo 3DS

Version **1.1.1**. A homebrew port for original Nintendo 3DS and 2DS. The same build also runs on
New 3DS and New 2DS XL, enabling their faster CPU and L2 cache automatically.
You need your own retail PC copy of the game to build from source.
Complete game-data installers are not provided for download. Build your CIA locally
from your own retail PC copy. DSP firmware must come from your own console.

[Build your CIA](#build-your-cia) · [Install](#install) · [Update](#updating-an-existing-installation) · [Controls](#interface-and-controls) · [Release notes](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/tag/v1.1.1)

## Screenshots and menu tour

Original artwork and fonts, a touch-screen HUD, television menus and native button navigation.

<a href="https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/download/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0-menu-tour.mp4"><img src="images/menu-tour.gif" width="400" alt="Main menu navigation, Options and Sound menus in the 3DS port"></a>

[Watch or download the 25-second menu tour](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/download/v1.0.0/Simpsons-Hit-and-Run-3DS-v1.0.0-menu-tour.mp4).
Silent footage captured in Azahar at native resolution, with cuts between sections.

<details>
<summary>View gameplay and menu screenshots</summary>

Captures use the native 400×240 top screen and 320×240 touch screen. Click an image for full size.
These are emulator captures, not measurements of console performance.

<table>
<tr><th>Gameplay HUD</th><th>Pause menu</th></tr>
<tr>
<td><img src="images/screenshots/gameplay.png" width="400" alt="Gameplay above; radar, wasp count, coins and current objective below"></td>
<td><img src="images/screenshots/pause.png" width="400" alt="Centered mission objective and touch-screen pause menu"></td>
</tr>
<tr><th>Main menu</th><th>Sound</th></tr>
<tr>
<td><img src="images/screenshots/main-menu.png" width="400" alt="Original living room artwork above a scrolling television menu"></td>
<td><img src="images/screenshots/sound.png" width="400" alt="Original Krusty artwork and four sound sliders"></td>
</tr>
<tr><th>Mission briefing</th><th>Scrapbook</th></tr>
<tr>
<td><img src="images/screenshots/mission-briefing.png" width="400" alt="The Cola Caper mission artwork and actual mission instructions"></td>
<td><img src="images/screenshots/scrapbook.png" width="400" alt="Level artwork above per-level completion counters"></td>
</tr>
</table>

</details>

## Build your CIA

Use your own **complete retail PC game folder**. The builder prepares textures,
movies, sound, and HOME Menu artwork from those local files, then creates your CIA.
Your game folder is read only; neither it nor the finished installer is uploaded.

The browser-only builder is not published yet. Until it is available, follow the
[local source-build guide](docs/INSTALL.md#build-from-your-own-pc-copy) for Windows
through WSL 2, Linux, or macOS. It includes PC-folder and disc-image preparation.

The source builder checks the installation, converts and validates all game cinematics,
compiles the port, and writes:

```text
out/releases/v1.1.1/Simpsons-Hit-and-Run-3DS-v1.1.1.cia
```

## Install

A console with custom firmware and FBI is required.

1. Create the CIA from your PC copy using the build instructions above.
2. Keep at least **4 GB free on the SD card before copying** the CIA.
3. Copy your CIA to `sd:/cias/`.
4. In **FBI → SD → cias**, select it and choose **Install CIA**.
5. Wait for success, then launch the game from the **HOME Menu**.

Your locally created CIA includes its prepared game data and all cinematics from your PC copy.
Sound needs your console's DSP firmware at `sd:/3ds/dspfirm.cdc`, dumped with DSP1.
No separate PC-data copy is needed on the SD card after creating the complete CIA.

**[Full installation and source-build guide](docs/INSTALL.md)**

### Updating an existing installation

1. Close the game and turn off the console.
2. **Back up the entire `sd:/3ds/simpsons/` folder to your computer.** Keep your DSP firmware on SD.
3. Create the new version's CIA from your PC copy, then copy it to `sd:/cias/`.
4. Install it with **FBI → SD → cias → select the CIA → Install CIA**. Confirm replacement if asked.
5. Launch the game from the **HOME Menu**, then check your existing save and settings.

**Do not uninstall the old game or delete its save folder.** Copying the CIA to
SD alone does not update the installed title. The title ID and save format stay
the same; your saves and settings remain. After checking the update, you can
delete your local installer from `cias/`.

If you used an old `.3dsx`, launch the newly installed HOME Menu icon to use this
release. [Full update and recovery instructions](docs/INSTALL.md#update-without-losing-your-progress).

## Interface and controls

The top screen uses 400×240 in Normal Mode or 400×240 per eye in Stereo 3D.
Quality Mode defaults to On and uses 800×240 wide output on 3DS models and New
2DS XL. Original 2DS keeps 400×240 with richer lighting. Open **Options → Graphics**
to change Quality Mode. Raise the 3D slider to enable Stereo 3D at 400×240 per eye;
lower it fully to restore your Quality preference. 2DS models keep stereo disabled.
See [Graphics modes](docs/GRAPHICS_MODES.md) for frame-rate targets and details.

The touch screen carries the original
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

Port attribution appears above the original game credits under
**Main Menu → Options → View Credits**.

Reopening the lid while the game is running plays a short greeting from Homer,
Bart, Lisa, Marge or Apu. Ten complete retail lines play in a shuffled order,
with no consecutive repeats. Greetings follow the **Voice** volume setting;
setting Voice to zero silences them. Returning from HOME alone does not trigger one.

Performance varies by scene; demanding areas can run below 30 FPS. Emulator results do not establish performance on an original console.

## Development

`cmake/old3ds-release.cmake` defines the normal-play build profile. It preserves the accepted v104 rendering options and
original-model memory limits, with benchmark automation and profiling disabled. The `v1.0.0` and `old3ds-ui-baseline` tags mark this
baseline. Build and package checks do not replace installed-console testing.

See [CONTRIBUTING.md](CONTRIBUTING.md) for regression checks and bug reports.

AI helped with repetitive testing work, such as checking mission icons at different sizes
and making sure text still fits the handheld interface. The [testing notes](docs/AI_TESTING.md)
explain what those checks cover, with examples from the game.

## Credits

The original game was developed by Radical Entertainment.
This port builds on [x2apic's New 3DS port](https://github.com/x2apic/The-Simpsons-Hit-and-Run-N3DS)
and [ZenoArrows' Switch and PS Vita port](https://github.com/ZenoArrows/The-Simpsons-Hit-and-Run).
Old 3DS work is maintained by [AlexMolina08](https://github.com/AlexMolina08).
Movie playback follows [REGTA-3DSPort-Complete](https://github.com/Epic0522/REGTA-3DSPort-Complete) by Epic0522.
Built with devkitPro, libctru and Citro3D.

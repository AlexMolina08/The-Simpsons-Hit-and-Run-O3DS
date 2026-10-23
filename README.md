# The Simpsons: Hit & Run for Nintendo 3DS

Version **1.1.1**. A homebrew port for original Nintendo 3DS and 2DS. The same build also runs on
New 3DS and New 2DS XL, enabling their faster CPU and L2 cache automatically.
You need your own retail PC copy of the game to build from source.
Game data and DSP firmware are not distributed in this source repository.

[Install](#install) · [Update](#updating-an-existing-installation) · [Build from your PC copy](docs/INSTALL.md#build-from-your-own-pc-copy) · [Controls](#interface-and-controls) · [Release notes](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/tag/v1.1.1)

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

## Install

Download the [complete v1.1.1 CIA](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/download/v1.1.1/Simpsons-Hit-and-Run-3DS-v1.1.1.cia).
It includes game data and all 17 cinematics. A console with custom firmware and FBI is required.

1. Keep at least **4 GB free on the SD card before copying** the CIA.
2. Download it on your computer and copy it to `sd:/cias/`.
3. In **FBI → SD → cias**, select the CIA and choose **Install CIA**.
4. Wait for success, then launch the game from the **HOME Menu**.

Sound needs your console's DSP firmware at `sd:/3ds/dspfirm.cdc`, dumped with DSP1.
No separate PC-data copy is needed for the complete CIA.

**[Full installation guide: release CIA, PC game folder, or PC ISO — Windows, Linux and macOS](docs/INSTALL.md)**

### Updating an existing installation

1. Close the game and turn off the console.
2. **Back up the entire `sd:/3ds/simpsons/` folder to your computer.** Keep your DSP firmware on SD.
3. Download the new **v1.1.1 CIA**, copy it to `sd:/cias/`, and install it with
   **FBI → SD → cias → select the CIA → Install CIA**. Confirm replacement if asked.
4. Launch the game from the **HOME Menu** and load your existing save.

**Do not uninstall the old game or delete its save folder.** Copying the CIA to
SD alone does not update the installed title. The title ID and save format stay
the same; your saves and settings remain. After checking the update, you can
delete the installer from `cias/`.

If you used an old `.3dsx`, launch the newly installed HOME Menu icon to use this
release. [Full update and recovery instructions](docs/INSTALL.md#update-without-losing-your-progress).

### Install with a QR code

Open **FBI → Remote Install → Scan QR Code** and scan this from another screen:

<a href="images/cia-install-qr.png"><img src="images/cia-install-qr.png" width="424" alt="Scan with FBI to install the complete version 1.1.1 CIA"></a>

[Open the QR at full size](images/cia-install-qr.png). Keep its white border visible.
It points directly to the complete v1.1.1 CIA. For this large download, the computer/SD
method above is recommended. If FBI fails, report its version, exact error code
and the percentage reached.

## Build a CIA

Have an installed **retail PC game folder**, or your **PC disc ISO(s)**?
Follow the [step-by-step PC-copy installation guide](docs/INSTALL.md#build-from-your-own-pc-copy).
It explains how to prepare the game files, install the tools and build on
**Windows through WSL 2, Linux, and macOS**.

The builder takes the installed game directory, not an ISO or installer CAB.
The guide covers that preparation step. For build flags, Homebrew Launcher and
packaging details, see the [technical build guide](docs/OLD3DS.md).

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

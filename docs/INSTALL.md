# Install or update The Simpsons: Hit & Run for Nintendo 3DS

These steps cover Nintendo 3DS, 3DS XL, 2DS, New 3DS, New 3DS XL and New 2DS XL.
The same CIA supports all models. You need a console with custom firmware and FBI.

- **Already have the port?** Follow [Update without losing your progress](#update-without-losing-your-progress).
- **Have created your CIA?** Follow [Install your locally created CIA](#install-your-locally-created-cia).
- **Have the PC game folder or a PC disc ISO?** Follow [Build from your own PC copy](#build-from-your-own-pc-copy).

Complete game-data installers are not provided for download. The v1.2 browser
builder is under validation; its final public host will be GitHub Pages. The
public link will be added to the project README after testing and publication.
It creates a CIA locally from your PC copy without desktop tools or game uploads.

## Install your locally created CIA

1. Turn off the console and connect its SD card to your computer.
2. Create **Simpsons-Hit-and-Run-3DS-v1.2.cia** from your PC copy using
   [Build from your own PC copy](#build-from-your-own-pc-copy).
3. Make sure the SD card has at least **4 GB free before copying**. It needs room
   for both the locally created CIA and the installed game. Copy the CIA into `cias`
   at the root of the card. Create that folder if necessary.
4. Safely eject the card, put it back in the console and turn the console on.
5. Open **FBI → SD → cias → Simpsons-Hit-and-Run-3DS-v1.2.cia → Install CIA**.
   Wait until FBI confirms installation has finished.
6. Exit FBI and launch **The Simpsons: Hit & Run** from the HOME Menu.
7. After checking the game works, you may delete the installer from `sd:/cias/`.
   Keep the installed title and the `sd:/3ds/simpsons/` folder.

Your locally created CIA includes the prepared game data and all game cinematics.
It needs no separate PC-data copy on the SD card.

**Sound:** use DSP1 on your console to dump its DSP firmware. The resulting file
must be `sd:/3ds/dspfirm.cdc`. Keep it when installing or updating the game.

## Update without losing your progress

You do **not** need to uninstall the old version, replay the game, rebuild the PC
files or replace your save folder.

1. Close the game and turn off the console.
2. Copy the whole **`3ds/simpsons` folder from your SD card to your computer** as a
   backup. It holds `Save1`–`Save4` and `srr2_settings.ini` when present. Keep this
   backup outside the SD card. Also keep `3ds/dspfirm.cdc` on the card.
3. Create the new version's CIA from your own PC copy, then copy it to `sd:/cias/`. Keep at least **4 GB free before copying** to allow room
   for the installer and installation work.
4. Put the SD card back in the console. Open **FBI → SD → cias**, select the new
   CIA and choose **Install CIA**. If FBI asks to replace the existing title,
   confirm the replacement. Wait for the success message.
5. Launch the game from the **HOME Menu**, then select your existing save. Check
   your progress and Graphics settings before deleting the downloaded installer.

**Copying the CIA onto the card alone does not update the installed game.** FBI
must install it. The port keeps title ID `000400000f3a1100` and the existing save
format. New bundled assets take priority over older loose game assets on SD.
Do not delete or overwrite `sd:/3ds/simpsons/` while updating.

If you previously launched a `.3dsx` through Homebrew Launcher, installing the CIA
adds a HOME Menu version. Launch that icon for this release; an old `.3dsx` remains
an old executable. Both use the same `sd:/3ds/simpsons/` save folder.

If your progress is missing, stop before creating a new save. Check that you used
the same SD card and that the original `Save1`–`Save4` files remain in that folder.
Your computer backup provides a recovery copy.

## Build from your own PC copy

1. Once published, open the browser builder using the link in the project README
   in a current Chrome, Edge or Safari browser. Allow at least **8 GB free** for
   temporary files. Keep the page open and the device awake during the build.
2. Choose your **PC ISO** or **complete installed PC game folder**. The folder
   should contain `art`, `scripts`, `sound`, `movies` and the retail `.rcf` archives.
   Your originals are read only. The website downloads its processing tools
   automatically; you do not install or run a desktop program.
3. Select **Build CIA**. The builder prepares the accepted v1.2 textures, car and
   character detail, HUD artwork, lighting across all seven maps, distant
   landmarks, audio and complete cinematics, then packages the frozen v1.2 game.
4. Select **Save CIA** and wait for the save or download to finish. You can also
   save a build report. Keep the completed installer private because it contains
   data from your PC copy.
5. Follow [Install your locally created CIA](#install-your-locally-created-cia).

The builder detects English, French and Spanish copies and offers a language choice
when the selected copy includes more than one. Supported images are read locally;
installer programs are never executed. If an image is unsupported, select its
complete installed PC folder. Physical-console language checks are still in progress.

**Phones:** use an ISO when the browser cannot select a folder. iPhone and Android
end-to-end builds remain experimental until physical-device testing is complete.
Mac Safari and a phone-sized desktop layout do not establish phone compatibility.

Completed stages remain in this browser's local storage. If a build is interrupted,
select the same copy and build again to reuse verified stages. Changing browsers,
clearing website data or receiving a different builder recipe can require a fresh
build. **Clear this builder’s temporary files** removes only its local build data.

## Advanced: build from source

For the toolchain, platform-specific setup and build commands, see the
[technical build guide](OLD3DS.md). You need a complete installed PC game folder,
including its archives and movies.

For a disc set, install the game on Windows and copy its installed folder, or use
[unshield](https://github.com/twogood/unshield) to extract compatible
InstallShield cabinets. Split installers need the matching CAB and HDR parts
from every disc. Select the extracted game folder containing `art`, `scripts`,
`sound`, `movies` and the `.rcf` archives, not the installer folder.

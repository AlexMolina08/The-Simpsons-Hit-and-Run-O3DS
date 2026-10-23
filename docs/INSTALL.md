# Install or update The Simpsons: Hit & Run for Nintendo 3DS

These steps cover Nintendo 3DS, 3DS XL, 2DS, New 3DS, New 3DS XL and New 2DS XL.
The same CIA supports all models. You need a console with custom firmware and FBI.

- **Already have the port?** Follow [Update without losing your progress](#update-without-losing-your-progress).
- **Want to install the ready-made release?** Follow [Install the release CIA](#install-the-release-cia).
- **Have the PC game folder or a PC disc ISO?** Follow [Build from your own PC copy](#build-from-your-own-pc-copy).

## Install the release CIA

1. Turn off the console and connect its SD card to your computer.
2. Download **Simpsons-Hit-and-Run-3DS-v1.1.1.cia** from the
   [v1.1.1 release](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/tag/v1.1.1).
   Choose the `.cia` file under **Assets**, not GitHub's **Source code** archives.
3. Make sure the SD card has at least **4 GB free before copying**. It needs room
   for both the downloaded CIA and the installed game. Copy the CIA into `cias`
   at the root of the card. Create that folder if necessary.
4. Safely eject the card, put it back in the console and turn the console on.
5. Open **FBI → SD → cias → Simpsons-Hit-and-Run-3DS-v1.1.1.cia → Install CIA**.
   Wait until FBI confirms installation has finished.
6. Exit FBI and launch **The Simpsons: Hit & Run** from the HOME Menu.
7. After checking the game works, you may delete the installer from `sd:/cias/`.
   Keep the installed title and the `sd:/3ds/simpsons/` folder.

The complete CIA includes the game data and all 17 cinematics. It needs no separate
PC-data copy. A source build produces the same type of complete installer.

**Sound:** use DSP1 on your console to dump its DSP firmware. The resulting file
must be `sd:/3ds/dspfirm.cdc`. Keep it when installing or updating the game.

**QR alternative:** open **FBI → Remote Install → Scan QR Code**, then scan the
[release QR](../images/cia-install-qr.png) from another screen. It installs the same CIA.
For this large download, copying the CIA from a computer is more reliable than
console Wi-Fi. If QR download fails, use the SD steps above.

## Update without losing your progress

You do **not** need to uninstall the old version, replay the game, rebuild the PC
files or replace your save folder.

1. Close the game and turn off the console.
2. Copy the whole **`3ds/simpsons` folder from your SD card to your computer** as a
   backup. It holds `Save1`–`Save4` and `srr2_settings.ini` when present. Keep this
   backup outside the SD card. Also keep `3ds/dspfirm.cdc` on the card.
3. Download **Simpsons-Hit-and-Run-3DS-v1.1.1.cia** from the
   [v1.1.1 release](https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS/releases/tag/v1.1.1).
   Copy it to `sd:/cias/`. Keep at least **4 GB free before copying** to allow room
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

This route runs on **Windows through WSL 2**, **Linux**, and **macOS**. It converts
retail PC data and compiles a complete CIA. The PC game does not run on the 3DS;
copying an ISO or `Simpsons.exe` to the console will not install the port.

Allow several GB of free space on the computer for the source, toolchain, copied
game data, converted movies and CIA. The first build takes longer than later builds.

### 1. Find or prepare the PC game folder

**If the PC game is already installed:** find the folder containing `art`,
`scripts`, `sound`, `movies`, `scripts.rcf` and the other `.rcf` archives. It is often
beside `Simpsons.exe`. Use that folder itself, not its parent directory or a shortcut.
You can copy this folder from your Windows installation to Linux or macOS.

A complete retail installation should contain:

```text
Hit and Run/
  art/frontend/scrooby/bootup.p3d
  scripts/
  sound/
  movies/                  (all 17 .rmv files)
  scripts.rcf
  dialog.rcf
  music00.rcf
  music01.rcf
  music02.rcf
  music03.rcf
  ...other retail .rcf archives
```

**If you have PC disc ISOs on Windows:**

1. Mount the first ISO in Windows, or extract it with [7-Zip](https://www.7-zip.org/).
2. Run the retail installer from that disc. Choose a full installation, including
   movies. When asked for another disc, mount the corresponding ISO and select
   its drive. Keep all discs available until installation finishes.
3. Locate the installed game folder shown above. An ISO extraction containing
   only `Setup.exe`, `data1.cab` and similar installer files is **not** that folder.
4. Continue with Windows/WSL setup below. The installed PC game does not need to
   launch successfully for the build tool to read its data.

**If you have PC disc ISOs on Linux or macOS:** an installed PC folder copied from
Windows is the most direct route. For InstallShield-based retail discs, you can
also extract their data locally with [unshield](https://github.com/twogood/unshield):

1. Install the extraction tools:

   ```sh
   # Ubuntu 24.04
   sudo apt update
   sudo apt install p7zip-full unshield

   # macOS with Homebrew
   brew install sevenzip unshield
   ```

2. Extract each ISO into its own directory. Replace the example ISO path.
   On Linux use `7z`; on macOS use `7zz`:

   ```sh
   # Linux
   7z x "/path/to/disc1.iso" -o"$HOME/HitRun-Disc1"

   # macOS
   7zz x "/path/to/disc1.iso" -o"$HOME/HitRun-Disc1"
   ```

3. Find the InstallShield `data1.cab` and its matching `.hdr`/additional CAB parts.
   For a split installer, collect its matching parts from every disc into one
   working folder, keeping their original names. Do not overwrite conflicting
   files from different installers. Then extract, for example:

   ```sh
   unshield -d "$HOME/HitRun-PC" x "$HOME/HitRun-Disc1/data1.cab"
   ```

4. Inside the extracted output, find the directory matching the game-folder
   layout above. Unshield may place it inside a component subfolder. Use that
   directory as the input to the build, rather than the folder containing the CAB.

Disc layouts vary. If unshield reports a missing part, extraction fails, or the
result lacks the retail archives or movies, it is not ready to build. Finish the
retail installation on Windows and copy the installed folder instead. These steps
do not promise support for every disc image or repack. Development/leaked game
files and incomplete installations are unsupported.

### 2. Set up your computer

Choose your operating system. Run the commands for that system only.

#### Windows 10/11: WSL 2 with Ubuntu 24.04

1. Open **PowerShell as administrator** and run:

   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

2. Restart if requested. Open **Ubuntu 24.04** from Start, finish creating its
   Linux user, then run all remaining commands in that Ubuntu terminal.
3. Follow the **Linux** dependency steps below, including devkitPro's Linux
   toolchain. Do not use the Windows devkitPro toolchain inside WSL.
4. Keep the source checkout in your Linux home directory (`cd ~`). The PC game
   can stay on Windows: `C:\Games\Hit and Run` becomes `/mnt/c/Games/Hit and Run`.

See [Microsoft's WSL guide](https://learn.microsoft.com/windows/wsl/install) if
Windows reports that WSL or virtualization is unavailable.

#### Linux: Ubuntu 24.04 or Ubuntu inside WSL

```sh
sudo apt update
sudo apt install git build-essential cmake ninja-build python3 python3-venv ffmpeg
```

Install devkitPro's package manager using its official
[Linux instructions](https://devkitpro.org/wiki/devkitPro_pacman), then install:

```sh
sudo dkp-pacman -S 3ds-dev
```

Accept the package group's default selection. Other distributions need equivalent
packages, **CMake 3.28+** and **Python 3.11+**. Ubuntu 24.04 supplies both.

#### macOS: Apple Silicon or Intel

Install the [Xcode command-line tools](https://developer.apple.com/xcode/resources/),
[Homebrew](https://brew.sh), and devkitPro's macOS installer from its
[getting-started guide](https://devkitpro.org/wiki/Getting_Started). Select **3ds-dev**.

```sh
xcode-select --install
brew install git cmake ninja python ffmpeg
```

Skip `xcode-select --install` if the command-line tools are already installed.

### 3. Download the source and prepare Python

Run in **Ubuntu/WSL, Linux Terminal, or macOS Terminal**:

```sh
cd ~
git clone --branch v1.1.1 https://github.com/AlexMolina08/The-Simpsons-Hit-and-Run-O3DS.git
cd The-Simpsons-Hit-and-Run-O3DS
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install Pillow
```

The devkitPro toolchain should be at `/opt/devkitpro`. If you chose another
location, set `DEVKITPRO` to that location before building. Packaging tools
`makerom` and `bannertool` are prepared automatically on macOS and Linux x86-64,
including WSL. See the [technical build guide](OLD3DS.md) for other architectures.

### 4. Build the CIA

From the source folder, with the Python environment active, run **one** example
below. Replace its PC-folder path with your actual path and keep the quotes.

```sh
# Windows through WSL, with the PC game in C:\Games\Hit and Run
python3 tools/3ds/make_cia.py "/mnt/c/Games/Hit and Run" --version 1.1.1

# Linux example
python3 tools/3ds/make_cia.py "$HOME/Games/Hit and Run" --version 1.1.1

# macOS example
python3 tools/3ds/make_cia.py "$HOME/Games/Hit and Run" --version 1.1.1
```

The tool checks the installation, copies assets, converts and validates all 17
movies, compiles the game and creates the CIA. Keep the terminal open until it
prints **Ready to install**. It does not upload or publish your files.

Your installer is:

```text
out/releases/v1.1.1/Simpsons-Hit-and-Run-3DS-v1.1.1.cia
```

On WSL, run `explorer.exe out/releases/v1.1.1` to open the output in Windows.
On Linux/macOS, open that folder with your file manager. Install the CIA using the
[SD steps above](#install-the-release-cia). No extra game-data copy is needed.

### Common build problems

- **Does not look like the PC game folder:** select the directory containing
  `art`, `scripts` and `scripts.rcf`, not the ISO, disc installer or parent folder.
- **Missing boot artwork, movies or archive members:** use a complete retail PC
  installation. Empty folders or placeholder files cannot replace missing data.
- **Missing devkitPro tools:** install the entire `3ds-dev` group, not only devkitARM.
- **Missing Pillow:** from the source folder, run `source .venv/bin/activate`,
  then `python3 -m pip install Pillow`. Do not install into the system Python.
- **Build stops because memory runs out:** retry with
  `JOBS=2 python3 tools/3ds/make_cia.py "/path/to/PC game" --version 1.1.1`.
- **Opening a new terminal:** return to the source folder and reactivate
  `.venv` before running the build command again.

For a console install failure, report the CIA filename, FBI version, full error
code and percentage reached. A valid download alone does not prove installation
or gameplay on your particular console.

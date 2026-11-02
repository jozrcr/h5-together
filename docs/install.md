# Installing H5 Together

How to install the add-on on a player's PC. What it does: [the main README](../README.md); every setting: [src/mods/README.md](../src/mods/README.md).

## What you need

- Heroes of Might and Magic V: Tribes of the East (Steam) with **MMH5.5 RC19d or RC19k already installed**. The add-on does not install MMH5.5.
- The same add-on version as the friends you play with: **everyone in the same co-op game should install the same version.**

## Install

1. Download the latest `h5-together-<version>.zip` from the [releases page](https://github.com/jozrcr/h5-together/releases/latest).
2. Close the game. Extract the whole zip (Windows: right-click it, **Extract All**).
3. Run the installer from the extracted folder:
   - **Windows:** double-click `install.bat`. It finds the game through Steam, or asks you for the folder, and tells you what it did.
   - **Linux (Steam / Proton):** run `bash install.sh`. At the end it prints one line to paste into the game's Steam launch options. The full walkthrough, with ZeroTier to play without GameRanger, is [linux.md](linux.md).

The installer never changes the game's own files, and never replaces a `dinput8.dll` that is not ours.

## What gets installed

Everything goes into the game's `bin` folder (on Steam usually `C:\Program Files (x86)\Steam\steamapps\common\Heroes of Might and Magic 5 Tribes of the East\bin`):

| File | What it is |
|---|---|
| `dinput8.dll` | the add-on; the game loads it when it starts |
| `h5-together.ini` | the settings. Every line is commented out and shows its default, so you can leave it alone |
| `h5-together\templates\OH-*.xdb` | 24 ready random map templates for the game's list. Every player of a multiplayer game needs the same ones |

## Check that it works

Start the game as you do for co-op games (GameRanger, or `MMH55_Utility_64.exe`). A file **`h5-together.log`** appears in the same `bin` folder; its first line says `H5 Together hook DLL <version> loaded`. If something goes wrong (a freeze, a crash, an "out of sync"), make a bug report (below). After a freeze, wait 30 seconds before closing the game, so the log can say where it was stuck. The log does not grow for ever: when it is over 5 MB at the next start, the game keeps it as `h5-together.old.log` (one older log is kept) and starts a new `h5-together.log`.

## Updating

Install the new version the same way, over the old one. Your settings are kept, and the new ones are added: `h5-together.ini` becomes the new version's file (every setting listed and commented out) with your values in it. The installer says `settings file refreshed: N of your settings kept, M new settings available (see the comments)`. A setting the new version no longer lists stays at the end of its section, under `; kept from your previous file:`. Your old file is saved as `h5-together.ini.bak` (the last three are kept: `.bak`, `.bak2`, `.bak3`). Our templates are replaced by the new ones; templates you added yourself stay.

### From open-homm5 (0.2.7 and older)

The add-on was called **open-homm5** until 0.2.7, and so were its files. Install the new version over it as usual: the installer gives your files the new names first and says so in one line (`open-homm5 is now called H5 Together: your files got the new name, h5-together (...)`):

| Before (0.2.7) | After |
|---|---|
| `open-homm5.ini` (and its `.bak` copies) | `h5-together.ini`, refreshed with your values; your 0.2.7 file is `h5-together.ini.bak` |
| `open-homm5-skins.ini` (your hero looks) | `h5-together-skins.ini` |
| `open-homm5.log` | `h5-together.old.log` (the new log starts at the next game) |
| the folder `open-homm5` (templates, `lang\fr` French packs, your own files) | the folder `h5-together`, with everything in it |

Nothing you set or added is lost. If you only copy the new `dinput8.dll` by hand, the add-on moves the settings and hero looks files itself and still finds its folder under the old name; run the installer when you can, so the folder moves too. To uninstall an old version, the new `uninstall.bat` / `uninstall.sh` removes the old names as well.

## Reporting a problem

Run `bug-report.bat` (Windows) or `bash bug-report.sh` (Linux) from the extracted folder. It finds the game the way the installer does and makes **`h5-together-bug-report-<date>.zip`** on your Desktop (on Linux, in the folder you run it from). Then [open a bug report](https://github.com/jozrcr/h5-together/issues/new/choose) and drag the zip into it.

The zip holds:
- `h5-together.log`, and `h5-together.old.log` if there is one;
- your settings, `h5-together.ini` and `h5-together-skins.ini`;
- `system.txt`: the add-on's version, MMH5.5's (its `H55_Version` and which `55u.dll` you have), Windows or Linux, the sizes and hashes of the game's exes, and the names of the other `.dll` files in the game's `bin` folder (another mod's `dinput8.dll` shows up there). Your user name is left out.

No saves, no maps, no game files. The script sends nothing anywhere: the zip is a normal zip, so you can open it and look before you send it. The log lists player names and LAN addresses. It works when the game does not start, and when the add-on is not installed (then the report only tells about your installation).

On Linux the zip is made with `zip`, or else with Python 3; with neither, you get a `.tar.gz` (`--tar` asks for one). `--out-dir <folder>` puts the file elsewhere.

## Uninstall

Run `uninstall.bat` (Windows) or `bash uninstall.sh` (Linux) from the extracted folder. Only the add-on's own files are removed (the DLL, the logs, our templates); it asks before deleting your settings file and its `.bak` copies, and templates you added yourself stay.

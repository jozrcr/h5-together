# Installing open-homm5

How to install the add-on on a player's PC. What it does: [the main README](../README.md); every setting: [src/mods/README.md](../src/mods/README.md).

## What you need

- Heroes of Might and Magic V: Tribes of the East (Steam) with **MMH5.5 RC19d or RC19k already installed**. The add-on does not install MMH5.5.
- The same add-on version as the friends you play with: **everyone in the same co-op game should install the same version.**

## Install

1. Download the latest `open-homm5-<version>.zip` from the [releases page](https://github.com/jozrcr/open-homm5/releases/latest).
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
| `open-homm5.ini` | the settings. Every line is commented out and shows its default, so you can leave it alone |
| `open-homm5\templates\OH-*.xdb` | 24 ready random map templates for the game's list. Every player of a multiplayer game needs the same ones |

## Check that it works

Start the game as you do for co-op games (GameRanger, or `MMH55_Utility_64.exe`). A file **`open-homm5.log`** appears in the same `bin` folder; its first line says `open-homm5 hook DLL <version> loaded`. If something goes wrong (a freeze, a crash, an "out of sync"), make a bug report (below). After a freeze, wait 30 seconds before closing the game, so the log can say where it was stuck. The log does not grow for ever: when it is over 5 MB at the next start, the game keeps it as `open-homm5.old.log` (one older log is kept) and starts a new `open-homm5.log`.

## Updating

Install the new version the same way, over the old one. Your settings are kept, and the new ones are added: `open-homm5.ini` becomes the new version's file (every setting listed and commented out) with your values in it. The installer says `settings file refreshed: N of your settings kept, M new settings available (see the comments)`. A setting the new version no longer lists stays at the end of its section, under `; kept from your previous file:`. Your old file is saved as `open-homm5.ini.bak` (the last three are kept: `.bak`, `.bak2`, `.bak3`). Our templates are replaced by the new ones; templates you added yourself stay.

## Reporting a problem

Run `bug-report.bat` (Windows) or `bash bug-report.sh` (Linux) from the extracted folder. It finds the game the way the installer does and makes **`open-homm5-bug-report-<date>.zip`** on your Desktop (on Linux, in the folder you run it from). Then [open a bug report](https://github.com/jozrcr/open-homm5/issues/new/choose) and drag the zip into it.

The zip holds:
- `open-homm5.log`, and `open-homm5.old.log` if there is one;
- your settings, `open-homm5.ini` and `open-homm5-skins.ini`;
- `system.txt`: the add-on's version, MMH5.5's (its `H55_Version` and which `55u.dll` you have), Windows or Linux, the sizes and hashes of the game's exes, and the names of the other `.dll` files in the game's `bin` folder (another mod's `dinput8.dll` shows up there). Your user name is left out.

No saves, no maps, no game files. The script sends nothing anywhere: the zip is a normal zip, so you can open it and look before you send it. The log lists player names and LAN addresses. It works when the game does not start, and when the add-on is not installed (then the report only tells about your installation).

On Linux the zip is made with `zip`, or else with Python 3; with neither, you get a `.tar.gz` (`--tar` asks for one). `--out-dir <folder>` puts the file elsewhere.

## Uninstall

Run `uninstall.bat` (Windows) or `bash uninstall.sh` (Linux) from the extracted folder. Only the add-on's own files are removed (the DLL, the logs, our templates); it asks before deleting your settings file and its `.bak` copies, and templates you added yourself stay.

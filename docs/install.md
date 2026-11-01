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

Start the game as you do for co-op games (GameRanger, or `MMH55_Utility_64.exe`). A file **`open-homm5.log`** appears in the same `bin` folder; its first line says `open-homm5 hook DLL <version> loaded`. If something goes wrong (a freeze, a crash, an "out of sync"), send that log. After a freeze, wait 30 seconds before closing the game, so the log can say where it was stuck.

## Updating

Install the new version the same way, over the old one. Your settings are kept. Our templates are replaced by the new ones; templates you added yourself stay.

## Uninstall

Run `uninstall.bat` (Windows) or `bash uninstall.sh` (Linux) from the extracted folder. Only the add-on's own files are removed (the DLL, the log, our templates); it asks before deleting your settings file, and templates you added yourself stay.

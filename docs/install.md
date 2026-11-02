# Installing H5 Together

How to install the add-on on a player's PC. What it does: [the main README](../README.md); every setting: [src/mods/README.md](../src/mods/README.md).

## What you need

- Heroes of Might and Magic V: Tribes of the East (Steam) with **MMH5.5 RC19d or RC19k already installed**. The add-on does not install MMH5.5.
- The same add-on version as the friends you play with: **everyone in the same co-op game should install the same version.**

## Install

1. Download the latest `h5-together-<version>.zip` from the [releases page](https://github.com/jozrcr/h5-together/releases/latest).
2. Close the game. Extract the whole zip (Windows: right-click it, **Extract All**).
3. Run the installer from the extracted folder:
   - **Windows:** double-click `install.bat`. It looks for the game (Steam, GOG, or the folder that MMH5.5's own installer remembers), shows you what it found, copies the files in and tells you what to do next. The window waits for Enter at the end, so you can read it.
   - **Linux (Steam / Proton):** run `bash install.sh`. Same screens, and at the end it prints one line to paste into the game's Steam launch options. The full walkthrough, with ZeroTier to play without GameRanger, is [linux.md](linux.md).

The installer never changes the game's own files, and never replaces a `dinput8.dll` that is not ours (to keep another mod that uses that name, see [below](#another-mod-that-is-also-a-dinput8dll)).

### What the installer shows

Before it copies anything, it shows your game, so you can see it is the right one:

```
Your game
  Folder:     C:\Program Files (x86)\Steam\steamapps\common\Heroes of Might and Magic 5 Tribes of the East
  Found in:   Steam
  Language:   French (Steam's setting for the game)
  MMH5.5:     RC19k (tested with H5 Together)
  Game exes:  MMH55_Utility_64.exe  MMH5.5's co-op exe: H5 Together works with it
              H5_Game.exe           a copy of the co-op exe (made for GameRanger): works too
  H5 Together has its panels in French, so that is what you will see.
```

- **Found in** says Steam or GOG when it can tell. **Language** is Steam's language setting for the game (not shown for other copies); the add-on's own panels use it when we have that language.
- **MMH5.5** is named from the `55u.dll` file (RC19d and RC19k are the ones we tested). Any other `55u.dll` gives "a version we have not tested" with a yellow warning: the add-on may switch itself off (the game then runs as it always did, and `h5-together.log` says why). With no MMH5.5 at all the warning says that H5 Together needs it for now; the files are still copied, and start working when MMH5.5 is installed.
- **Game exes:** the add-on works with MMH5.5's co-op exe, `MMH55_Utility_64.exe`. GameRanger starts `H5_Game.exe`, so the GameRanger guide has you put a copy of the co-op exe under that name: the installer recognises that copy. An `H5_Game.exe` that is the game's own exe does nothing with the add-on; start `MMH55_Utility_64.exe` then.
- **No game found, or several:** with none it asks where the game is. Type **B** to choose the folder in a window (Windows), or paste the folder (the one that holds `bin`, or `bin` itself). With several it lists them (each with where it comes from and its MMH5.5) and you type a number. A folder is accepted only if its `bin` has `H5_Game.exe` or `MMH55_Utility_64.exe`; otherwise it explains and asks again. Press Enter alone to cancel (nothing is installed). GOG copies are looked for in GOG's list in the registry, and any copy through the entry that MMH5.5's own installer leaves; we have not tried a GOG copy of Heroes V yet, so tell us if yours is not found.
- **The game is running:** the add-on's file cannot be replaced while the game runs, so the installer says so and waits: close the game, then press Enter. Type `S` to go on anyway, `Q` to stop (exit code 8, nothing changed).
- **At the end** it says "All done" and what to do next: start the game, then look for `h5-together.log` in the `bin` folder. The first line says `H5 Together hook DLL <version> loaded` when the add-on runs.

## What gets installed

Everything goes into the game's `bin` folder (on Steam usually `C:\Program Files (x86)\Steam\steamapps\common\Heroes of Might and Magic 5 Tribes of the East\bin`):

| File | What it is |
|---|---|
| `dinput8.dll` | the add-on; the game loads it when it starts |
| `h5-together.ini` | the settings. Change them in the game (main menu, **Options**, **H5 Together**, or F3), or in this file with Notepad. Every line is commented out and shows its default, so you can leave it alone |
| `h5-together\templates\OH-*.xdb` | 24 ready random map templates for the game's list. Every player of a multiplayer game needs the same ones |

## Check that it works

Start the game as you do for co-op games (GameRanger, or `MMH55_Utility_64.exe`). A file **`h5-together.log`** appears in the same `bin` folder; its first line says `H5 Together hook DLL <version> loaded`. In the waiting room of a multiplayer game, a line at the bottom says whether every player has the same H5 Together version (and who has another one, or none). If something goes wrong (a freeze, a crash, an "out of sync"), make a bug report (below). If the game crashes, the add-on writes `h5-together-crash-<date>-<time>.txt` (what crashed) and a small `.dmp` next to the log, and the next start's log says so; the 3 newest are kept, nothing is sent anywhere. After a freeze, wait 30 seconds before closing the game, so the log can say where it was stuck. The log does not grow for ever: when it is over 5 MB at the next start, the game keeps it as `h5-together.old.log` (one older log is kept) and starts a new `h5-together.log`.

## Updating

Install the new version the same way, over the old one. To hear about new versions, say Yes to the question the first time the game starts with H5 Together (or tick it in Options, H5 Together, Help; or set `check=1` under `[update]` in `h5-together.ini`): then, at most once a day, the add-on asks GitHub for the number of the latest version and says in the main menu when there is a newer one. It is off unless you turn it on; nothing is ever downloaded by the add-on. Your settings are kept, and the new ones are added: `h5-together.ini` becomes the new version's file (every setting listed and commented out) with your values in it. The installer says `settings file refreshed: N of your settings kept, M new settings available (see the comments)`. A setting the new version no longer lists stays at the end of its section, under `; kept from your previous file:`. Your old file is saved as `h5-together.ini.bak` (the last three are kept: `.bak`, `.bak2`, `.bak3`). Our templates are replaced by the new ones; templates you added yourself stay.

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
- the newest crash report, if the game crashed: `h5-together-crash-<date>-<time>.txt` and its `.dmp` (a small "minidump": part of the game's memory at the crash, which holds player and folder names; `bug-report.bat -NoDump` or `bash bug-report.sh --no-dump` leaves the `.dmp` out);
- `system.txt`: the add-on's version, MMH5.5's (its `H55_Version` and which `55u.dll` you have), Windows or Linux, the sizes and hashes of the game's exes, and the names of the other `.dll` files in the game's `bin` folder (another mod's `dinput8.dll` shows up there). Your user name is left out.

No saves, no maps, no game files. The script sends nothing anywhere: the zip is a normal zip, so you can open it and look before you send it. The log lists player names and LAN addresses. It works when the game does not start, and when the add-on is not installed (then the report only tells about your installation).

On Linux the zip is made with `zip`, or else with Python 3; with neither, you get a `.tar.gz` (`--tar` asks for one). `--out-dir <folder>` puts the file elsewhere.

## Another mod that is also a `dinput8.dll`

Some other mods for the game also come as a file called `dinput8.dll` in the `bin` folder: "ASI loaders", which load widescreen fixes and the like. Only one file can have that name. To use H5 Together **and** that mod:

1. Rename the other mod's `dinput8.dll` to **`dinput8_next.dll`** (same folder).
2. Install H5 Together as usual.

When the game starts, H5 Together loads `dinput8_next.dll` and passes the game's DirectInput on to it (the other mod then passes it on to Windows). The log says `chained: dinput8_next.dll (another dinput8.dll mod) is loaded`. If that file cannot be loaded, or is a copy of H5 Together itself, the log says why and Windows' own `dinput8.dll` is used, as without it.

- Another file name or folder: `dll=<file>` under `[chain]` in `h5-together.ini` (next to the game's exe, or a full path). `dll=none` never loads one.
- To undo: delete or rename `dinput8_next.dll`. Uninstalling H5 Together leaves it there and reminds you to rename it back to `dinput8.dll`.
- Two mods that change the same part of the game can clash, and we cannot test every mod. If the game misbehaves, try once without `dinput8_next.dll`, and tell us in the bug report which mod it is.

## Uninstall

Run `uninstall.bat` (Windows) or `bash uninstall.sh` (Linux) from the extracted folder. It finds and shows the game the same way, asks you to close the game if it runs, and removes only the add-on's own files (the DLL, the logs, our templates and language files); it asks before deleting your settings file and its `.bak` copies, and templates you added yourself stay.

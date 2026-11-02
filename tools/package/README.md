# Package: the easy install

Builds `open-homm5-<version>.zip`, the file a friend downloads to install the hook DLL (see [docs/install.md](../../docs/install.md)). It stays local: this tool does not upload or publish anything.

## Build it

```
powershell -File src\hook\build.ps1            # builds build\hook\dinput8.dll (done for you if it is missing)
python tools\package\make_package.py            # -> build\package\open-homm5-0.1.0-dev.zip
python tools\package\make_package.py --version 0.1.0
python tools\package\make_package.py --rebuild  # build the DLL again first
```

`build\` is not committed. The script lists the zip's contents and its SHA-256 at the end.

## What is in the zip

| File | What it is |
|---|---|
| `dinput8.dll` | the hook DLL, built from `src/hook` and `src/mods` |
| `open-homm5.ini` | the settings, every one commented out and shown at its default (the DLL's built-in defaults apply until a line is un-commented) |
| `install.bat`, `install.ps1` | Windows installer (the `.bat` only starts the `.ps1`) |
| `uninstall.bat`, `uninstall.ps1` | Windows uninstaller |
| `install.sh`, `uninstall.sh` | Linux (Steam / Proton) installer and uninstaller |
| `bug-report.bat`, `bug-report.ps1`, `bug-report.sh` | the "make a bug report" helper, Windows and Linux (see "The bug report script" below) |
| `README.txt` | a short read-me for players (install, uninstall, settings, what's new; plain ASCII); `@VERSION@` is filled in |
| `LICENSE.txt` | our MIT licence: the repo's `LICENSE`, copied word for word |
| `THIRD-PARTY-NOTICES.txt` | the licence notices of what else is inside `dinput8.dll`: Dear ImGui (its `LICENSE.txt` is pasted in word for word, with the version from `imgui.h`), the stb files it carries (Sean Barrett) and its built-in ProggyClean font (Tristan Grimmer). The MIT licences ask for the notice to go along with copies. The text is the template `files/THIRD-PARTY-NOTICES.txt`; if third-party code is added to the DLL, add it there (and see `licence_checks` in `test_scripts.py`) |
| `open-homm5/templates/OH-*.xdb` | the ready random map templates (see below) |

The scripts and text files are in `tools/package/files/`. Everything is in one top folder, `open-homm5-<version>/`, so that unzipping on Linux does not spread the files around. Line endings are set when the zip is written: CRLF for `.bat`, `.ps1`, `.txt` and `.ini` (Notepad, cmd.exe), LF for `.sh`, which also get the executable bit. In the repo, `.gitattributes` keeps `.bat` as CRLF and `.sh` as LF.

## The random map templates

`make_package.py` packs every `src/mods/rmg/templates/*.xdb` as `open-homm5/templates/<name>.xdb` (inside the top folder, the same layout as in the game's `bin` folder). Those files are written by `node tools/rmg/make_templates.js`; the list of ready templates is in [../rmg/README.md](../rmg/README.md).

- **Byte for byte:** the `.xdb` files are UTF-8 with a BOM and CRLF, and they are not converted (`.gitattributes` has `*.xdb -text`). The script stops if a file lost its BOM or got LF line endings.
- **Names:** `OH-<letters, digits, . or ->.xdb` only. The scripts rely on that.
- **Every player of a multiplayer game needs the same template files** (each PC builds the map from the template). So a name that has shipped must keep building the same file; add a new entry for a changed template, and everyone installs the same version.
- `test_template_maker.js` fails if the files in `src/mods/rmg/templates` are not what `make_templates.js` builds now, so the package never carries a stale file.

## What the scripts do

- Windows (`install.ps1`, Windows PowerShell 5.1, French Windows included):
  - finds the game: Steam's folder from `HKCU:\Software\Valve\Steam`, the libraries from `steamapps\libraryfolders.vdf`, then `steamapps\common\Heroes of Might and Magic 5 Tribes of the East`, whose `bin` must hold `MMH55_Utility_64.exe` or `H5_Game.exe`. With none or several found it asks (a number, or a pasted folder: the game folder or its `bin`);
  - copies `dinput8.dll`; refuses (exit code 3) if the game folder already has a `dinput8.dll` that is not ours;
  - copies `open-homm5.ini` if there is none, and otherwise **refreshes** the player's file (see "The settings file" below);
  - copies the templates into `bin\open-homm5\templates\` (making the folder). It overwrites its own `OH-*.xdb` files there (by name) and touches no other file, so a player's own templates stay. The summary says how many templates were installed (and how many replaced older copies of ours, and how many other files it left alone);
  - looks at the network profiles and explains how to make a Public ZeroTier network Private. It changes no Windows setting and no firewall rule;
  - ends with "Press Enter to close".
- Linux (`install.sh`): the same, looking for Steam in `~/.steam/steam`, `~/.local/share/Steam` and the Flatpak folder. It prints the Steam launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%`, and which exe to start. It does not edit Steam's files.
- "Ours" means the file contains the text `open-homm5 hook DLL` (a string in `src/hook/dllmain.cpp`). `make_package.py` refuses to pack a DLL without it.
- The Linux installer also reuses an existing `open-homm5\templates` folder and existing template files under another upper/lower case spelling (Wine reads them either way), so it never makes a second copy.
- Uninstall removes our `dinput8.dll`, `open-homm5.log` (and `open-homm5.old.log`, the older log the DLL keeps once the log passes 5 MB), and `open-homm5.ini` after asking. The same question covers the installer's backups of it (`open-homm5.ini.bak`, `.bak2`, `.bak3`): "yes" removes the file and its backups (they are copies of the settings the player asked to delete), "no" keeps them all. It also removes the templates: only the `OH-*.xdb` names that are in the package it runs from, never another file. If the `templates` folder is then empty it is removed, and then `open-homm5` too if it is empty. Nothing else.
  - A template that an older version of the package installed, and a newer one no longer has, is not in the newer package's list: uninstalling from the newer package leaves it. This is on purpose (the alternative is deleting every `OH-*.xdb`, which would also delete a player's own template made with the template maker page).

## The bug report script

`bug-report.bat` (starts `bug-report.ps1`) and `bug-report.sh` make `open-homm5-bug-report-<yyyy-MM-dd-HHmm>.zip` for the issue form (`.github/ISSUE_TEMPLATE/bug_report.yml` asks for it). They find the game with the same code as the installer (the shared block below), read it only, and write the zip on the Desktop (Windows; `[Environment]::GetFolderPath('Desktop')`, so OneDrive's moved Desktop works) or in the current folder (Linux).

- **In the zip:** `open-homm5.log`, `open-homm5.old.log`, `open-homm5.ini`, `open-homm5-skins.ini` (the ones that exist, byte for byte) and `system.txt`. Nothing else: no saves, no `.bak` copies, no game files. A new file type must be added to the list in both scripts, with a thought for what it holds about the player.
- **`system.txt`:** the add-on's version (read from the text in `dinput8.dll`, so an old build works too; Windows also shows the file properties), the version of the package the script came from, Windows or Linux, the game folder (with the home folder written as `~`: no user name), MMH5.5's `H55_Version` (from `scripts/H55-Version.lua` in `data\MMH55-Index.pak`, a zip), `55u.dll` with its SHA-256 and the MMH5.5 version it belongs to (RC19d, RC19k, else "a version we have not tested"; the two hashes come from [docs/plans/mmh55-rc19k.md](../../docs/plans/mmh55-rc19k.md) and `test_scripts.py` compares them with that file), the game exes with size, file version (Windows) and SHA-256, and the names and sizes of the other `.dll` and `.asi` files in `bin`.
- **The game may be running:** the log is read with shared access, so a log that the game holds open is fine (tested).
- **No log, no add-on, no MMH5.5:** it still makes the zip (with `system.txt` only) and says what is missing.
- **Linux:** the zip is made with `zip`, else Python 3's `zipfile` (`unzip` or Python 3 reads the MMH5.5 pak), else a `.tar.gz`. `--tar` forces the last one (the tests use it).
- **Test options:** `-GameDir`, `-SteamDir`, `-OutDir <folder>`, `-NoPause` on Windows (without `-NoPause` it opens Explorer on the file at the end); `--game-dir`, `--steam-dir`, `--out-dir`, `--tar` on Linux.
- Exit codes: 0 done, 4 game not found or cancelled, 5 could not write the report, 6 unexpected error (Windows).

## The settings file

Every new version adds settings to `open-homm5.ini`. Earlier installers left an existing file as it was, so players never saw the new settings. Now it rebuilds the file from the package's `open-homm5.ini` and puts the player's values back in:

1. The result is the package's file (every setting documented and commented out with `;`), with its line endings (CRLF, as in the zip).
2. Each active line `key=value` of the player's file (not starting with `;` or `#`) fills the line `;key=...` of the **same `[section]`**: it becomes `key=value`, with the player's value. Section and key names ignore upper/lower case, as the game's ini reader does; spaces around the key and the value are dropped. Values the DLL writes itself (`builder_last=`, the host's `[rules]`) are ordinary active lines, so they are kept the same way.
3. Anything else with content in the player's file (a setting this version does not list, for example a renamed one; a second copy of a setting; a line without `=`) is kept as it was, at the end of its section, under `; kept from your previous file:`. A section the package's file does not have goes at the end of the file. Nothing active is lost.
4. The player's own comments and blank lines are not carried over; the backup has them.
5. If the result is byte for byte the file already there (a second install of the same version), nothing is written and no backup is made.
6. Otherwise the old file is first copied to `open-homm5.ini.bak`. The last three are kept: an older `.bak` becomes `.bak2`, `.bak2` becomes `.bak3`, and the oldest `.bak3` goes.
7. A file that is not plain text (it holds zero bytes: saved as "Unicode" by Notepad) is left exactly as it is, and the installer says so. A UTF-8 byte-order mark at the start is dropped.
8. The installer prints `settings file refreshed: N of your settings kept, M new settings available (see the comments)`, where M counts the package's settings that the old file did not mention at all (neither active nor commented).

Files are handled byte for byte (no re-encoding), so an ASCII file stays ASCII. No other file of the game folder is touched.

Windows (`Merge-Settings` in `install.ps1`, PowerShell 5.1) and Linux (`merge_ini` in `install.sh`, POSIX awk, so mawk works too) implement the same rules, and `test_scripts.py` checks that they write the same bytes. Change both together.

Exit codes of the scripts: 0 done, 3 refused (not our `dinput8.dll`), 4 game not found or cancelled, 5 copy/delete failed, 6 unexpected error, 7 package incomplete (for example `install.bat` started from inside the zip without extracting it, or a package without its `open-homm5\templates` folder).

Test options (also handy for odd setups): `-GameDir <folder>`, `-SteamDir <folder>`, `-NoPause` on Windows; `--game-dir <folder>`, `--steam-dir <folder>` on Linux (which otherwise reads `$HOME`). `install.ps1 -TestProfiles "name|category;..."` fakes the network profiles, to test the ZeroTier hint.

## Rules for editing the scripts

- **Plain ASCII only** in `files/*` (`make_package.py` checks). Windows PowerShell 5.1 reads a script without a byte-order mark as ANSI, so an accent would be garbled on a French Windows.
- The code that finds the game is written three times, in the install, uninstall and bug report scripts, between the lines `# ---- shared with uninstall...` and `# ---- end of the shared block ----`. Edit all copies; `make_package.py` stops if they differ.
- Keep the messages plain and short: the readers are not programmers.

## Test it

```
python tools\package\test_scripts.py --bash "C:\Program Files\Git\bin\bash.exe"
```

It builds the zip, unzips it, makes **fake** Steam and game folders in a temp folder (empty files named like the game's exes), and runs the scripts against them: fresh install, re-install over our DLL, a foreign `dinput8.dll` (refused), an existing ini (refreshed), uninstall, several or no games found, and more. For the settings file it checks that:
- a player's 0.2.3 file (`test_data/open-homm5-0.2.3.ini`: that version's file with `view_delay_ms=50`, `net_sleep_ms=1`, `original_music=1`, `language = fr`, `builder_last=OH-X1-...`, plus `texts=fr`, an unknown `fast_turns=1` and a section `[mine]` added by hand) becomes exactly this version's file with those values in their lines and the unknown ones kept, and the summary counts are right;
- a second install changes nothing and makes no backup; a fresh install gets the package's file;
- an odd file (byte-order mark, LF, a setting before `[net]`, `[NET]` and `Tuning = 0`, a second copy, `#` and `; ` comments, a line without `=`) comes out as expected;
- the backups rotate (`.bak`, `.bak2`, `.bak3`, never a fourth), a UTF-16 file is left alone, and uninstall removes the file and its backups only on "yes";
- **Windows and Linux write the same bytes** for every case.

For the templates it checks that the zip holds every source file byte for byte, then (Windows and Linux scripts alike) that:
- an install puts exactly our templates in `bin/open-homm5/templates`, and the summary counts them;
- a player's own files there (`my-own.xdb`, `notes.txt`, even an `OH-mine.xdb` that is not in the package) are kept on install and on uninstall;
- a re-install overwrites our changed file and brings back a missing one;
- an uninstall removes only our names, then the empty folders, but not an `open-homm5` folder that holds something else;
- a package without its `templates` folder is refused (exit 7);
- on Linux, a folder and a file that exist under another spelling (`Open-Homm5/Templates`) are reused.

For the licence files it checks that `LICENSE.txt` is the repo's `LICENSE`, that `THIRD-PARTY-NOTICES.txt` carries Dear ImGui's licence file word for word and names the stb files and the font, and that Dear ImGui still bundles exactly the three stb files the notices list.

For the bug report script (Windows and Linux alike, on fake folders with a log, an older log, the settings, a skins file, a version pak, and things that must stay out: a save, a map, an old `.bak`) it checks that:
- the zip holds exactly those four files, byte for byte, and `system.txt`, and nothing else;
- `system.txt` has the add-on version, the version of the package, MMH5.5's `H55_Version`, `55u.dll` with its hash, the exes with size and hash, the other DLLs, no user name, and the home folder as `~` (Linux);
- without the add-on or a log it still makes a report and says so; another mod's `dinput8.dll` is named as such and left alone; a pak without a version script or a missing pak is reported, not an error; a wrong game folder is exit 4 and no file;
- on Windows: a log that the running game holds open (opened for appending, others may only read) is packed; without `-OutDir` the file lands on the Desktop (it is deleted again); an output folder that cannot be made is exit 5;
- on Linux: the `.zip` (or `.tar.gz`, then also with `--tar`) comes out the same way; without `--out-dir` the file is made in the current folder; an output folder that cannot be made is exit 5.

It never touches a real game folder. On Linux, leave out `--bash`; on Windows without Git Bash the Linux part is skipped.

## Built by GitHub

`.github/workflows/package.yml` runs on GitHub's Windows machines, which are free for public repositories.

**On every push to `main` and every pull request**, it:
1. builds the DLL and runs its tests;
2. builds the zip (version `0.0.0-dev.<run number>`);
3. runs `test_scripts.py`.

The zip is then a download on the run's page (Actions tab).

**On a version tag** (`git tag v0.1.0`, then `git push origin v0.1.0`), it does the same with that version and creates a **draft** release with the zip attached. The owner edits the notes and publishes it; the workflow never publishes anything itself.

## Making a release

1. Set the version in `src/hook/version.h` (`OPEN_HOMM5_VERSION`). The DLL writes it on the first line of `open-homm5.log`, and the package takes it from there.
2. Write the release notes in `docs/releases/<version>.md`, in plain words for players.
3. Merge to `main`.
4. Tag that commit `v<version>` and push the tag (`git tag v0.1.0`, then `git push origin v0.1.0`). GitHub then builds and tests everything, and creates a **draft** release with the zip and the notes.
5. The owner reads the draft on the repo's Releases page and publishes it.

`make_package.py` refuses a release version that is not the one in `version.h`, and a DLL built from another version. Test builds use a version with a `-`, like `0.1.0-test2`.


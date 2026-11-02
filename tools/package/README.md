# Package: the easy install

Builds `h5-together-<version>.zip`, the file a friend downloads to install the hook DLL (see [docs/install.md](../../docs/install.md)). It stays local: this tool does not upload or publish anything.

## Build it

```
powershell -File src\hook\build.ps1            # builds build\hook\dinput8.dll (done for you if it is missing)
python tools\package\make_package.py            # -> build\package\h5-together-0.1.0-dev.zip
python tools\package\make_package.py --version 0.1.0
python tools\package\make_package.py --rebuild  # build the DLL again first
```

`build\` is not committed. The script lists the zip's contents and its SHA-256 at the end.

## What is in the zip

| File | What it is |
|---|---|
| `dinput8.dll` | the hook DLL, built from `src/hook` and `src/mods` |
| `h5-together.ini` | the settings, every one commented out and shown at its default (the DLL's built-in defaults apply until a line is un-commented) |
| `install.bat`, `install.ps1` | Windows installer (the `.bat` only starts the `.ps1`) |
| `uninstall.bat`, `uninstall.ps1` | Windows uninstaller |
| `install.sh`, `uninstall.sh` | Linux (Steam / Proton) installer and uninstaller |
| `bug-report.bat`, `bug-report.ps1`, `bug-report.sh` | the "make a bug report" helper, Windows and Linux (see "The bug report script" below) |
| `README.txt` | a short read-me for players (install, uninstall, settings, what's new; plain ASCII); `@VERSION@` is filled in |
| `LICENSE.txt` | our MIT licence: the repo's `LICENSE`, copied word for word |
| `THIRD-PARTY-NOTICES.txt` | the licence notices of what else is inside `dinput8.dll`: Dear ImGui (its `LICENSE.txt` is pasted in word for word, with the version from `imgui.h`), the stb files it carries (Sean Barrett) and its built-in ProggyClean font (Tristan Grimmer). The MIT licences ask for the notice to go along with copies. The text is the template `files/THIRD-PARTY-NOTICES.txt`; if third-party code is added to the DLL, add it there (and see `licence_checks` in `test_scripts.py`) |
| `h5-together/templates/OH-*.xdb` | the ready random map templates (see below) |
| `h5-together/texts/<code>.txt` | our language files, the words of the add-on's panels: `en.txt`, `fr.txt` (see below) |

The scripts and text files are in `tools/package/files/`. Everything is in one top folder, `h5-together-<version>/`, so that unzipping on Linux does not spread the files around. Line endings are set when the zip is written: CRLF for `.bat`, `.ps1`, `.txt` and `.ini` (Notepad, cmd.exe), LF for `.sh`, which also get the executable bit. In the repo, `.gitattributes` keeps `.bat` as CRLF and `.sh` as LF.

## The random map templates

`make_package.py` packs every `src/mods/rmg/templates/*.xdb` as `h5-together/templates/<name>.xdb` (inside the top folder, the same layout as in the game's `bin` folder). Those files are written by `node tools/rmg/make_templates.js`; the list of ready templates is in [../rmg/README.md](../rmg/README.md).

- **Byte for byte:** the `.xdb` files are UTF-8 with a BOM and CRLF, and they are not converted (`.gitattributes` has `*.xdb -text`). The script stops if a file lost its BOM or got LF line endings.
- **Names:** `OH-<letters, digits, . or ->.xdb` only. The scripts rely on that.
- **Every player of a multiplayer game needs the same template files** (each PC builds the map from the template). So a name that has shipped must keep building the same file; add a new entry for a changed template, and everyone installs the same version.
- `test_template_maker.js` fails if the files in `src/mods/rmg/templates` are not what `make_templates.js` builds now, so the package never carries a stale file.

## The language files

`make_package.py` packs every `src/mods/texts/*.txt` (named `<code>.txt`: `en.txt`, `fr.txt`; not `TRANSLATING.md`) as `h5-together/texts/<code>.txt`. They hold the words of our panels and messages ([src/mods/README.md](../../src/mods/README.md#our-panels-in-your-language-srcmodstextstxt-srchooktextscpp)).

- **UTF-8, by design.** They are the only files of the package that are not plain ASCII (accents, and later Cyrillic...): the scripts, `README.txt`, the notices and `h5-together.ini` stay ASCII (`test_scripts.py` checks every other file). In the zip they get a byte-order mark and CRLF line ends, so that every Notepad opens and saves them as UTF-8; the DLL reads them with or without the mark, CRLF or LF.
- The English in the DLL is built in from `en.txt` (`src/hook/build.ps1`): a missing or damaged texts folder only loses the other languages.
- `python tools/lang/ui_texts.py check` (run by `build.ps1 -Test`) checks the files against the code before they are packed.

## What the scripts do

- Windows (`install.ps1`, Windows PowerShell 5.1, French Windows included):
  - finds the game (see "Finding the game" below), shows it ("Your game": folder, store, Steam's language, MMH5.5 and its version, the exes, warnings), and asks the player to close the game if it runs;
  - copies `dinput8.dll`; refuses (exit code 3) if the game folder already has a `dinput8.dll` that is not ours;
  - gives the files of the add-on's old name their new name first (see "The old name" below);
  - copies `h5-together.ini` if there is none, and otherwise **refreshes** the player's file (see "The settings file" below);
  - copies the templates into `bin\h5-together\templates\` (making the folder). It overwrites its own `OH-*.xdb` files there (by name) and touches no other file, so a player's own templates stay. The summary says how many templates were installed (and how many replaced older copies of ours, and how many other files it left alone);
  - copies the language files into `bin\h5-together\texts\` the same way: ours are overwritten, a translation of the player's own (`de.txt`) stays; the summary names them (`en, fr`);
  - looks at the network profiles and explains how to make a Public ZeroTier network Private. It changes no Windows setting and no firewall rule;
  - ends with "All done", what to do next (start the game, look for `h5-together.log`, its first line) and "Press Enter to close this window".
- Linux (`install.sh`): the same screens, looking for Steam in `~/.steam/steam`, `~/.local/share/Steam`, the Flatpak and the Snap folders (no GOG, no folder window; a number or a pasted folder). It prints the Steam launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%`, and which exe to start. It does not edit Steam's files.
- "Ours" means the file contains the text `H5 Together hook DLL` (a string in `src/hook/dllmain.cpp`), or `open-homm5 hook DLL` (the DLLs of 0.2.7 and older). `make_package.py` refuses to pack a DLL without the first.
- The Linux installer also reuses an existing `h5-together\templates` folder and existing template files under another upper/lower case spelling (Wine reads them either way), so it never makes a second copy.
- Uninstall finds and shows the game the same way (without the MMH5.5 warnings), asks to close the game if it runs, then removes our `dinput8.dll`, `h5-together.log` (and `h5-together.old.log`, the older log the DLL keeps once the log passes 5 MB), and `h5-together.ini` after asking. The same question covers the installer's backups of it (`h5-together.ini.bak`, `.bak2`, `.bak3`): "yes" removes the file and its backups (they are copies of the settings the player asked to delete), "no" keeps them all. It also removes the language files and the templates: only the `<code>.txt` and `OH-*.xdb` names that are in the package it runs from, never another file. If the `texts` and `templates` folders are then empty they are removed, and then `h5-together` too if it is empty. The same files under the old name (`open-homm5.log`, `open-homm5.ini` and its copies, `open-homm5\templates`) go the same way. Nothing else: the hero looks' file (`h5-together-skins.ini`) and the French packs (`lang\fr`) stay.
  - A template that an older version of the package installed, and a newer one no longer has, is not in the newer package's list: uninstalling from the newer package leaves it. This is on purpose (the alternative is deleting every `OH-*.xdb`, which would also delete a player's own template made with the template maker page).

## Finding the game, and what the scripts say about it

The same code, in the shared block of `install`, `uninstall` and `bug-report` (the lines between `# ---- shared with uninstall...` and `# ---- end of the shared block ----`; `make_package.py` stops if the copies differ, separately for Windows and for Linux). It only looks; nothing here writes.

**Where it looks**, in this order, no duplicates (the same folder written two ways counts once):
1. **Steam** (Windows: `HKCU:\Software\Valve\Steam`, Linux: `~/.steam/steam`, `~/.local/share/Steam`, the Flatpak and the Snap folders), and every library in `steamapps\libraryfolders.vdf`. In each library, `steamapps\appmanifest_15370.acf` (the game's Steam file; 15370 is Tribes of the East) names the folder in its `installdir` line; the usual name `Heroes of Might and Magic 5 Tribes of the East` is the fallback.
2. **GOG** (Windows only): every game under `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<id>` (and `HKLM\SOFTWARE\GOG.com\Games`), value `path`. We do not match on GOG's game ids (not known for Heroes V): an entry only counts if its folder passes the check below, so Heroes III in the same list is ignored. Not tried with a real GOG copy of Heroes V yet.
3. **MMH5.5's own installer** (Windows only): its uninstall entry `{CEC93C99-EB3D-4EBE-B0BE-AE228E5B47AE}_is1`, value `InstallLocation` (it names the folder whatever the copy: CD, GOG, Steam).
4. **Ask**: with none found a prompt; with several a numbered list (each with its store and MMH5.5). The player types a number, **B** (Windows: a "choose a folder" window, `FolderBrowserDialog`; nothing opens unless the player types B), or a pasted folder (quotes and `~` are accepted). Enter alone cancels (exit 4). Five tries.

**The check**: a folder is the game when its `bin` (or the folder itself) holds `H5_Game.exe` or `MMH55_Utility_64.exe`. Anything else is refused with an explanation. `-GameDir` / `--game-dir` skips the search, with the same check.

**"Your game"** (`Write-GameInfo` / `write_game_info`) shows:
- *Folder*, and *Found in* (Steam when the folder sits in `steamapps\common` next to an `appmanifest_15370.acf`; GOG when the game folder holds a `goggame-*.info` file). Nothing for other copies.
- *Language*: the `language` line of that Steam file ("french" becomes French). Not shown when there is none. The installer adds whether the package has that language in `h5-together\texts` (the codes: english en, french fr, german de, russian ru, polish pl, spanish es, italian it, czech cs, hungarian hu).
- *MMH5.5*: **ok** when the co-op exe is there and `bin\55u.dll` has a known hash (RC19d, RC19k: the two hashes of [mmh55-rc19k.md](../../docs/plans/mmh55-rc19k.md), which `test_scripts.py` compares with the scripts); **untested** for the co-op exe with any other `55u.dll` (or none), with `H55_Version` from `data\MMH55-Index.pak` in brackets when it can be read (a zip; `scripts/H55-Version.lua`); **missing** without `MMH55_Utility_64.exe` / `MMH55_Utility.exe`. The version file says "19e" in RC19k too, so only `55u.dll` tells the versions apart. Untested and missing print a yellow warning (and again at the end of the install); the install goes on.
- *Game exes*: `MMH55_Utility_64.exe` and `MMH55_Utility.exe` are the co-op exes; `H5_Game.exe` is "a copy of the co-op exe" when it is byte for byte one of them (the GameRanger guide), else "the game's own exe", which the add-on does nothing with.

**The game is running** (`Confirm-GameClosed` / `confirm_game_closed`, install and uninstall only): Windows lists the processes `H5_Game`, `H5_GameOLD`, `MMH55_Utility_64`, `MMH55_Utility`, `MMH55_64` and `MMH55` whose program sits in the chosen `bin` (one whose folder Windows does not tell counts too); Linux reads `/proc/*/cmdline` for those exe names (Wine's process names its exe; any copy of the game that runs counts, since Wine's paths are not the folder's). While one is found: "close the game, then press Enter", `S` goes on anyway, `Q` stops. Nothing answering (no input) stops too. Exit code 8, nothing changed. Why: Windows will not replace a DLL that is loaded, and on Linux copying over a loaded file can crash the game.

**Test options** (all three scripts; the real registry, the real pickers and real 55u.dll hashes cannot be faked otherwise):
- `-TestRegistry <key>` / (Linux has none): read the GOG and MMH5.5-installer entries from this key instead of `HKLM:\SOFTWARE\WOW6432Node` and `HKLM:\SOFTWARE`; same layout below it (`GOG.com\Games\<id>`, `Microsoft\Windows\CurrentVersion\Uninstall\{...}_is1`). The tests write a key under `HKCU:\Software\H5TogetherTest-...` and delete it. They also point it at a key that does not exist for every other run, so that this PC's own GOG or MMH5.5 entries do not leak in.
- `-TestPicker <folder>`: the answer of the "B" window (a folder, or `cancel` for a closed window). No window opens.
- `-TestKnown55u "<sha256>=<name>;..."` / `--test-known-55u`: count these `55u.dll` files as known versions (a fake file cannot have RC19k's hash).
- The running game. Windows: no hook; the tests copy `cmd.exe` to `H5_Game.exe` in the fake folder and start it. Linux: `H5_TOGETHER_PROC_DIR=<folder>` (an environment variable, not an option) is read instead of `/proc`; the tests point it at a folder of their own (so a real game on the PC does not matter) and put a `4242/cmdline` file in it whose text ends with `...\H5_Game.exe`.

## The old name

Until 0.2.7 the add-on was called **open-homm5**, and so were its files in `bin`: `open-homm5.ini` (and the installer's `.bak` copies of it), `open-homm5-skins.ini`, `open-homm5.log` and `open-homm5.old.log`, and the folder `open-homm5\` (our templates, the French packs in `lang\fr`, anything the player put there). The installer (`Move-OldNames` in `install.ps1`, `move_old_names` in `install.sh`, the same rules) gives them the new names before it does anything else, so nothing a player set or added is lost:

- a file moves only if its new name is not there yet. Otherwise the old one stays as it is, and the summary says that the new one is the one used. The `.bak` copies follow the settings file (not when it stayed);
- the settings refresh then runs on the moved file, so `h5-together.ini.bak` is the player's old file, byte for byte;
- the last log becomes `h5-together.old.log` (the older log) while there is no log of the new name yet; the other old logs are removed (they are ours, and older);
- the folder is renamed. If an `h5-together` folder is there too, what it lacks is moved into it, our templates in the old folder go (the install puts this version's in the new one), and a file both have stays in the old folder (the summary counts them);
- on Linux the old names are found in any upper/lower case (`Open-Homm5`);
- the summary has one line: `open-homm5 is now called H5 Together: your files got the new name, h5-together (the settings, the hero looks, the log, the folder)`. If a move fails (the game is running), the installer stops with exit code 5 before it copies anything.

The DLL does part of this itself for a player who only copies the new `dinput8.dll` (see [src/hook/README.md](../../src/hook/README.md#the-old-name-old_namescpp)). The bug report script packs the four files under both names, so it also works on a 0.2.7 install that was not updated.

## The bug report script

`bug-report.bat` (starts `bug-report.ps1`) and `bug-report.sh` make `h5-together-bug-report-<yyyy-MM-dd-HHmm>.zip` for the issue form (`.github/ISSUE_TEMPLATE/bug_report.yml` asks for it). They find the game with the same code as the installer (the shared block below), read it only, and write the zip on the Desktop (Windows; `[Environment]::GetFolderPath('Desktop')`, so OneDrive's moved Desktop works) or in the current folder (Linux).

- **In the zip:** `h5-together.log`, `h5-together.old.log`, `h5-together.ini`, `h5-together-skins.ini` (the ones that exist, byte for byte), the same four under the old name (`open-homm5.log`...: a 0.2.7 install that was not updated) and `system.txt`. Nothing else: no saves, no `.bak` copies, no game files. A new file type must be added to the list in both scripts, with a thought for what it holds about the player.
- **`system.txt`:** the add-on's name and version (read from the text in `dinput8.dll`, so an old build works too: `H5 Together <version>`, or `open-homm5 0.2.7`; Windows also shows the file properties), the version of the package the script came from, Windows or Linux, the game folder (with the home folder written as `~`: no user name), MMH5.5's `H55_Version` (from `scripts/H55-Version.lua` in `data\MMH55-Index.pak`, a zip), `55u.dll` with its SHA-256 and the MMH5.5 version it belongs to (RC19d, RC19k, else "a version we have not tested"; the two hashes come from [docs/plans/mmh55-rc19k.md](../../docs/plans/mmh55-rc19k.md) and `test_scripts.py` compares them with that file), the game exes with size, file version (Windows) and SHA-256, and the names and sizes of the other `.dll` and `.asi` files in `bin`.
- **The game may be running:** the log is read with shared access, so a log that the game holds open is fine (tested).
- **No log, no add-on, no MMH5.5:** it still makes the zip (with `system.txt` only) and says what is missing.
- **Linux:** the zip is made with `zip`, else Python 3's `zipfile` (`unzip` or Python 3 reads the MMH5.5 pak), else a `.tar.gz`. `--tar` forces the last one (the tests use it).
- **Test options:** `-GameDir`, `-SteamDir`, `-OutDir <folder>`, `-NoPause` on Windows (without `-NoPause` it opens Explorer on the file at the end); `--game-dir`, `--steam-dir`, `--out-dir`, `--tar` on Linux.
- Exit codes: 0 done, 4 game not found or cancelled, 5 could not write the report, 6 unexpected error (Windows).

## The settings file

Every new version adds settings to `h5-together.ini`. Earlier installers left an existing file as it was, so players never saw the new settings. Now it rebuilds the file from the package's `h5-together.ini` and puts the player's values back in:

1. The result is the package's file (every setting documented and commented out with `;`), with its line endings (CRLF, as in the zip).
2. Each active line `key=value` of the player's file (not starting with `;` or `#`) fills the line `;key=...` of the **same `[section]`**: it becomes `key=value`, with the player's value. Section and key names ignore upper/lower case, as the game's ini reader does; spaces around the key and the value are dropped. Values the DLL writes itself (`builder_last=`, the host's `[rules]`) are ordinary active lines, so they are kept the same way.
3. Anything else with content in the player's file (a setting this version does not list, for example a renamed one; a second copy of a setting; a line without `=`) is kept as it was, at the end of its section, under `; kept from your previous file:`. A section the package's file does not have goes at the end of the file. Nothing active is lost.
4. The player's own comments and blank lines are not carried over; the backup has them.
5. If the result is byte for byte the file already there (a second install of the same version), nothing is written and no backup is made.
6. Otherwise the old file is first copied to `h5-together.ini.bak`. The last three are kept: an older `.bak` becomes `.bak2`, `.bak2` becomes `.bak3`, and the oldest `.bak3` goes.
7. A file that is not plain text (it holds zero bytes: saved as "Unicode" by Notepad) is left exactly as it is, and the installer says so. A UTF-8 byte-order mark at the start is dropped.
8. The installer prints `settings file refreshed: N of your settings kept, M new settings available (see the comments)`, where M counts the package's settings that the old file did not mention at all (neither active nor commented).

Files are handled byte for byte (no re-encoding), so an ASCII file stays ASCII. No other file of the game folder is touched.

Windows (`Merge-Settings` in `install.ps1`, PowerShell 5.1) and Linux (`merge_ini` in `install.sh`, POSIX awk, so mawk works too) implement the same rules, and `test_scripts.py` checks that they write the same bytes. Change both together.

Exit codes of the scripts: 0 done, 3 refused (not our `dinput8.dll`), 4 game not found or cancelled, 5 copy/delete failed, 6 unexpected error, 7 package incomplete (for example `install.bat` started from inside the zip without extracting it, or a package without its `h5-together\templates` or `h5-together\texts` folder), **8 the game is running and was not closed** (install and uninstall).

Test options (also handy for odd setups): `-GameDir <folder>`, `-SteamDir <folder>`, `-NoPause` on Windows; `--game-dir <folder>`, `--steam-dir <folder>` on Linux (which otherwise reads `$HOME`). `-TestRegistry`, `-TestPicker`, `-TestKnown55u` (Windows) and `--test-known-55u` (Linux) are described under "Finding the game". `install.ps1 -TestProfiles "name|category;..."` fakes the network profiles, to test the ZeroTier hint.

## Rules for editing the scripts

- **Plain ASCII only** in `files/*` (`make_package.py` checks; the language files are not in `files/` and are UTF-8 on purpose). Windows PowerShell 5.1 reads a script without a byte-order mark as ANSI, so an accent would be garbled on a French Windows.
- The code that finds and describes the game is written three times, in the install, uninstall and bug report scripts, between the lines `# ---- shared with uninstall...` and `# ---- end of the shared block ----`. Edit all copies (the block for Windows and the one for Linux are separate pairs); `make_package.py` stops if they differ. Handy: write the block once in a scratch file and paste it into the three scripts.
- Keep the messages plain and short: the readers are not programmers.

## Test it

```
python tools\package\test_scripts.py --bash "C:\Program Files\Git\bin\bash.exe"
```

It builds the zip, unzips it, makes **fake** Steam and game folders in a temp folder (empty files named like the game's exes), and runs the scripts against them: fresh install, re-install over our DLL, a foreign `dinput8.dll` (refused), an existing ini (refreshed), uninstall, several or no games found, and more. `--only windows` or `--only linux` runs one half (faster while working). For finding the game and what the scripts say (Windows and Linux alike, `game_tests`) it checks that:
- a Steam game is found with no question, its `appmanifest_15370.acf` gives the folder name (`installdir`) and the language; the found screen names the store, the language, MMH5.5 (a known `55u.dll`: `RC19k`; an unknown one: "a version we have not tested" with the version file's value and a warning, again at the end; none: "not found" and a warning, and still installs) and explains each exe (the co-op exe, an `H5_Game.exe` that is a copy of it, the game's own exe);
- the add-on's language note is right (French: "has its panels in French"; German: no texts yet; no language line: nothing);
- with several games the list says where each comes from and its MMH5.5; a wrong number and a wrong folder are explained; a pasted folder and a number both work; with none found a wrong folder is explained and Enter cancels (exit 4);
- the game running (a fake process): install and uninstall stop with exit 8 on Q and on no answer, change nothing, and go on with S; once it is closed they work; on Windows a game running from another folder does not stop an install;
- the uninstaller and the bug report script show the same game screen; the bug report names a known `55u.dll`.

On Windows only (`windows_only_game_tests`): GOG's registry list (a path with a trailing backslash found and marked GOG; Heroes III in the same list and a folder that is gone ignored), MMH5.5's installer entry, Steam and GOG both found (listed, the same game named twice counted once), the B window answered by `-TestPicker` (a folder; a closed window asks again; a folder that is not the game is refused; also from the list), the window can be created in this PowerShell, and the pause (the scripts wait for Enter at the end, also when they stop early, and exit with the right code after it). For the settings file it checks that:
- a player's 0.2.3 file (`test_data/open-homm5-0.2.3.ini`: that version's file with `view_delay_ms=50`, `net_sleep_ms=1`, `original_music=1`, `language = fr`, `builder_last=OH-X1-...`, plus `texts=fr`, an unknown `fast_turns=1` and a section `[mine]` added by hand) becomes exactly this version's file with those values in their lines and the unknown ones kept, and the summary counts are right;
- a second install changes nothing and makes no backup; a fresh install gets the package's file;
- an odd file (byte-order mark, LF, a setting before `[net]`, `[NET]` and `Tuning = 0`, a second copy, `#` and `; ` comments, a line without `=`) comes out as expected;
- the backups rotate (`.bak`, `.bak2`, `.bak3`, never a fourth), a UTF-16 file is left alone, and uninstall removes the file and its backups only on "yes";
- **Windows and Linux write the same bytes** for every case.

For the old name (open-homm5, 0.2.7 and older) it checks, Windows and Linux alike, that:
- an install over a 0.2.7-style folder (a 0.2.7 `dinput8.dll`, `open-homm5.ini` with a changed setting and an older `.bak`, `open-homm5-skins.ini`, two logs, `open-homm5\lang\fr\x.pak`, an old copy of one of our templates and a player's own `OH-mine.xdb`) leaves only the new names: the DLL replaced, `h5-together.ini` refreshed with the value kept and the old file as `.bak` (its older copy as `.bak2`), the hero looks and the French pack byte for byte, `h5-together.old.log`, this version's templates plus the player's own, and one summary line;
- a second install moves nothing and changes nothing;
- with both names there, the new files are kept and used, the old settings file stays, what the new folder lacks moves into it, and a file both folders have stays in the old one;
- uninstall removes our files under both names, and a 0.2.7 install that was never updated (answers "n" and "y"); the hero looks and the French pack stay;
- the bug report of a 0.2.7 install packs the old names and names the DLL `open-homm5 0.2.7`;
- on Linux, `Open-Homm5` and `Open-Homm5.ini` under another spelling get the new names too.

For the templates it checks that the zip holds every source file byte for byte, then (Windows and Linux scripts alike) that:
- an install puts exactly our templates in `bin/h5-together/templates`, and the summary counts them;
- a player's own files there (`my-own.xdb`, `notes.txt`, even an `OH-mine.xdb` that is not in the package) are kept on install and on uninstall;
- a re-install overwrites our changed file and brings back a missing one;
- an uninstall removes only our names, then the empty folders, but not an `h5-together` folder that holds something else;
- a package without its `templates` folder is refused (exit 7);
- on Linux, a folder and a file that exist under another spelling (`H5-Together/Templates`) are reused.

For the language files it checks that the zip holds every `src/mods/texts/*.txt` and nothing else, each as its source with a byte-order mark and CRLF, that `fr.txt` keeps its accents, and that every other file of the package is plain ASCII. Then (Windows and Linux alike): an install puts exactly ours in `bin/h5-together/texts` and names them; a re-install overwrites our `fr.txt` changed by hand and keeps a player's own `de.txt` and notes; an uninstall removes only ours, and install then uninstall leaves the game folder as it was; a package without its `texts` folder is refused (exit 7).

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

1. Set the version in `src/hook/version.h` (`OPEN_HOMM5_VERSION`). The DLL writes it on the first line of `h5-together.log`, and the package takes it from there.
2. Write the release notes in `docs/releases/<version>.md`, in plain words for players. Their "Check that it works" says: the first line of `h5-together.log` (in the game's `bin` folder) is `H5 Together hook DLL <version> loaded`. The first release with the new name also tells players that open-homm5 is now H5 Together, and that the installer moves their files (`open-homm5.ini` becomes `h5-together.ini`, the folder `open-homm5` becomes `h5-together`).
3. Merge to `main`.
4. Tag that commit `v<version>` and push the tag (`git tag v0.1.0`, then `git push origin v0.1.0`). GitHub then builds and tests everything, and creates a **draft** release with the zip and the notes.
5. The owner reads the draft on the repo's Releases page and publishes it.

`make_package.py` refuses a release version that is not the one in `version.h`, and a DLL built from another version. Test builds use a version with a `-`, like `0.1.0-test2`.


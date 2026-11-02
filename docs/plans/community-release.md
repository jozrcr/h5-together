# Plan: v0.3, the community release

Status: proposal for the owner (2026-11-02), being built. In `main`: block 1 (quick wins), block 2 (texts in files, part 3), **the friendlier install scripts** (part 1, without a setup.exe: see "Built" at the start of part 1) and **block 5, the safety nets** (crash reports, the version check in the lobby, the opt-in update check, chain-loading: see "Built" at the start of part 4). The welcome panel that asks about the update check waits for the settings menu (block 3).

The goal: share the add-on with the wider Heroes V / MMH5.5 community. Someone who has never heard of us should be able to:
1. download one file and double-click it;
2. read "found your game, MMH5.5 RC19k" and click Install;
3. start the game and change settings in a menu, in their own language;
4. when something goes wrong, click one button to make a bug report.

A "session" below means one focused working block of a few hours.

## The owner's decisions (2026-11-02)

- **The full plan**, except the installer exe: **no `setup.exe` for v0.3.** The zip with `install.bat` / `install.sh` stays and gets better instead (about 1 session): clearer messages, the MMH5.5 version and the game's language shown, GOG folders found, a folder picker when the game isn't found, the licence notices. Why: the scripts already find the game and refresh the settings safely; an exe would bring the "unsigned" warning, the `unins000.exe` clash and about 3 more sessions, for a nicer look and an "Apps" entry. **Total: about 16 sessions.**
- **The exe comes later** (after online play), with Inno Setup on GitHub's machines (OK'd), signed through **SignPath** (the owner applies then).
- **Update check:** yes, opt-in, contacting only GitHub.
- **Still to ask:** the settings menu sketches (block 3), the 7 pages and the hotkey, translators on the MMH5.5 Discord, announcements. Decided by Claude unless the owner objects: crash dumps kept on the PC; no Start-menu folder.
- **The add-on's name:** **H5 Together** (the owner's choice, 2026-11-02; it was open-homm5 until 0.2.7). Done in the branch `rename-h5-together`: the DLL's log and ini names (`h5-together.ini`, `bin\h5-together\`), the package and the installer, which moves an existing install's files to the new names (and the DLL does the same for a DLL copied by hand). The owner renames the GitHub repo to `jozrcr/h5-together`; GitHub redirects the old links. The plans below use the new names.

## Short version

- **Installer:** use **Inno Setup** (free, the tool MMH5.5's own installer is made with), built by GitHub's machines, so nothing is installed on your PC. One `h5-together-setup-0.3.0.exe`: it finds the game, checks MMH5.5 and its version, installs, and shows up in Windows "Apps" for uninstalling. The file work stays in our tested `install.ps1`. Linux keeps `install.sh`, made friendlier. **About 4 sessions.**
- **Unsigned exe warnings:** Windows will say "Windows protected your PC" for a while whatever we do, unless the setup is signed. **Free option:** apply to the SignPath Foundation (free code signing for open source projects, if they accept us). **Paid options** cost about €70 to €500 a year. You decide.
- **Settings menu:** a new panel in the game's look, with 7 pages. It shows the 28 settings a player cares about, plus 16 more on an "Advanced" page. The other 59 stay out of the menu: the 20 game rules, which already have their own panel (F2), and 39 that stay in the file only (35 for testing, the builder's memory, and 3 rarely needed). It saves into `h5-together.ini` as today. It opens from a button in the game's Options screen, a small button in the main menu (which also shows our version), and a hotkey. About 20 settings can apply at once; the others say "restart the game" or "from the next game". **About 5 sessions.**
- **Languages:** move our **~215 panel texts** (×2 today: English and French) out of the C++ code into one text file per language (`h5-together\texts\en.txt`, `fr.txt`, ...). English is built into the DLL as the fallback. A translator copies `en.txt`, translates the right-hand sides, and sends it to us. The DLL picks the language from Steam's own setting for the game. **About 4 sessions.**
- **Language audit:** nothing in our code finds a game widget by its text. Screens are recognised by their internal class names, which are the same in every language. The weak spots are a French-only "is the game French?" check, a French translation done by matching English sentences, two buttons placed using French/English screenshots, and panels that cut long words. All are listed in part 3.
- **Community kit:** an English-first README and FAQ, a compatibility table, GitHub bug-report forms that ask for `h5-together.log`, a "make a bug report" button (and script), crash dumps, a warning in the lobby when friends have another version, an opt-in "new version available" check, and working alongside another `dinput8.dll` mod. **About 5 sessions.**
- **Licences:** our code is MIT. The only bundled third-party code is Dear ImGui (MIT, with stb parts that are MIT or public domain). The zip must carry its notice (it doesn't today).
- **Order:** quick wins → texts in files → settings menu → installer → safety nets → docs and release. **About 19 sessions in total**, or about 14 for a smaller cut (see the end).
- **Left out on purpose (your choice):** plain ToE without MMH5.5 and Steam's 2025 exe. The plan leaves room for both (see "Leaving room for later").

---

## Part 1: one-click installer

### Built for v0.3 (2026-11-02, branch `v03-install`): the scripts, friendlier (no setup.exe)

The owner decided against the exe for v0.3, so the zip's scripts got the parts of this plan that do not need one. Everything below the next heading (Inno Setup, SmartScreen, "Apps" entry, signing) stays as the plan for later, after online play.

How it works now: [tools/package/README.md, "Finding the game, and what the scripts say about it"](../../tools/package/README.md#finding-the-game-and-what-the-scripts-say-about-it); for players: [docs/install.md](../install.md).

- **Messages:** one "Your game" screen before anything is copied (folder, Steam or GOG, Steam's language, MMH5.5 and its version, the exes, yellow warnings), "Installing ...", the existing "Done. What I did" list, then "All done" with a short "What to do next" (start the game; the first line of `h5-together.log` says `H5 Together hook DLL <version> loaded`). The Windows window waits for Enter at the end, also when it stops early (tested by watching the script wait).
- **MMH5.5:** `55u.dll`'s hash names RC19d or RC19k (the same two hashes as the bug report, compared with [mmh55-rc19k.md](mmh55-rc19k.md) by the tests); any other `55u.dll` is "a version we have not tested" with `H55_Version` from `MMH55-Index.pak` (the code moved from the bug report script into the shared block). Missing or untested gives a plain warning at the start and again at the end; the install still goes ahead. The exes are explained too: the co-op exe, an `H5_Game.exe` that is byte for byte a copy of it (the GameRanger guide), or the game's own exe.
- **Finding the game:** Steam (now reading `appmanifest_15370.acf` for the folder's real name and the language), **GOG** (`HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\*\path`; we match on the folder, not on GOG's id, which we do not know for Heroes V), and **MMH5.5's own uninstall entry** (`{CEC93C99-...}_is1`, `InstallLocation`: the folder of any copy it was installed into). Nothing found, or several: a numbered list, **B** for a folder window (Windows `FolderBrowserDialog`), or a pasted folder; each choice is checked (`bin` with a known exe). Linux: Steam (native, Flatpak, and now Snap), numbers and pasted folders, no GOG or window.
- **The game is running:** the installer and uninstaller ask to close it (Windows: the game's processes in that `bin`; Linux: the exe names in `/proc/*/cmdline`), `S` goes on, `Q` stops. New exit code **8**.
- **Kept as it was:** the settings refresh and backups, the move from the open-homm5 names, templates, language files, exit codes 0 and 3 to 7, `install.sh` printing the Steam launch option, ASCII-only scripts, the guard that the shared "find the game" blocks match (one pair for Windows, one for Linux; the block grew from about 120 lines to about 430 for Windows and 320 for Linux).
- **Not done, on purpose or for lack of a test copy:** no GOG copy of Heroes V to try (the registry code was tried against this PC's real GOG list, which holds Heroes III, and against fake keys); the folder window itself was only built, not clicked in the tests (a person has to look at it once); an "admin needed" check for copies under `Program Files` (the install says "run as administrator" when a copy fails); the Linux Steam Deck desktop mode and `localconfig.vdf` launch-option check of step 1.4; a menu on `install.sh`.

### Today

- A zip with `install.bat` → `install.ps1` (Windows) and `install.sh` (Linux), and matching uninstallers ([tools/package/README.md](../../tools/package/README.md)).
- They find the game through Steam, copy `dinput8.dll`, refresh `h5-together.ini` (keeping the player's values: `Merge-Settings` / `merge_ini`, byte-for-byte equal on Windows and Linux, tested by `test_scripts.py`) and copy 24 templates.
- Weak spots for strangers:
  - "Extract All" first. Double-clicking `install.bat` inside the zip fails with exit code 7.
  - A black console window with text.
  - No entry in Windows "Apps", so people don't know how to remove it.
  - MMH5.5 is only checked by "is `MMH55_Utility_64.exe` there?", and its version is not checked at all.
  - English only.

### The options

| Option | New download? | Wizard, languages | "Apps" entry | Antivirus / SmartScreen | Keeps our tested scripts? | Effort |
|---|---|---|---|---|---|---|
| **IExpress** (built into Windows) | no | none: it unpacks and runs `install.bat` | only if our script writes it | often flagged: old format, much used by malware | yes | 1 |
| **Inno Setup** (free) | yes, but on GitHub's machines only | a real wizard, translations included (French, German, Polish, Russian, Spanish, Italian, Czech, Hungarian...) | yes, built in | normal for an installer; the community already ran MMH5.5's Inno setup | yes (it can call `install.ps1`) | 3.5–4 |
| **NSIS** (free) | yes | wizard, translations | yes | more often flagged than Inno (much used by malware) | yes | 4 |
| **Our own small C++ setup** | no (same compiler as the DLL) | we build every screen ourselves | we write it | the same "unknown publisher" warning; a new exe that writes into game folders can look odd to antivirus heuristics | we would port the scripts to C++ (a third copy to keep equal) | 4.5–5 |
| **Keep the zip, improve it** | no | none | only if our script writes it | `.bat` files from the internet get their own warning | yes | 1 |

### Recommendation: Inno Setup, built on GitHub

- GitHub's Windows build machines list Inno Setup among their ready tools (to confirm in step 0). If it is missing, the workflow would install it there with Chocolatey, which is a download on GitHub's machine: **your OK first**. Nothing is installed on your PC.
- One file: `h5-together-setup-0.3.0.exe`, next to the zip on the Releases page. The zip stays, for Linux and for manual installs.
- **The file work stays in `install.ps1` and `uninstall.ps1`**, so the settings refresh keeps exactly one tested Windows version. The setup passes the folder (`-GameDir`, `-NoPause`) and shows what the script printed on its last page.
  - Fallback, only if antivirus tests (VirusTotal) dislike "a setup that starts PowerShell": port the work to Inno's own Pascal script. `test_scripts.py` would then check that the setup writes the same bytes as the scripts.
- Why not our own exe: more work, and no gain against SmartScreen (the warning is about the missing signature, not the tool).

### What the player sees

1. Language page: Inno picks Windows' language; the player can change it.
2. "Found your game": the folder, "MMH5.5 RC19k found", "the co-op exe `MMH55_Utility_64.exe` found". The player can change the folder. Several copies found: a list.
3. Warnings in plain words, if any, for example:
   - "MMH5.5 is not installed in this folder: the add-on needs it";
   - "unknown MMH5.5 version: the add-on may switch itself off; see the log";
   - "a `dinput8.dll` from another mod is here" (see "Other dinput8.dll mods" in part 4).
4. Install. The last page shows the script's summary ("settings file refreshed: 12 of your settings kept, 3 new settings available"), with a box "Open the read-me".
5. Running the setup again over an older version updates it, and the settings are kept (the same refresh as today).

### Finding the game

In order:
1. **Steam:** Steam's folder from the registry (`HKCU\Software\Valve\Steam`, `SteamPath`), then every library in `steamapps\libraryfolders.vdf` (as today).
   - New: in each library, read `steamapps\appmanifest_15370.acf`, the game's Steam file (15370 is ToE's Steam id). Its `installdir` gives the folder exactly, instead of guessing its name. Its `language` line ("french" on your PC) helps part 3.
2. **GOG:** look in GOG's registry keys (`HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\*`, value `path`) for a Heroes V entry. Not checked with a GOG copy yet: ask a GOG player.
3. **Anything else** (old CD copy, a copy made by hand): the folder page with "Browse...". Either the game folder or its `bin` is accepted (as today).

### Detecting MMH5.5 and its version

- **MMH5.5 is there** when `bin\MMH55_Utility_64.exe` and `data\MMH55-Index.pak` exist.
- **Which version:** two independent checks, both read-only.
  1. `data\MMH55-Index.pak` is a zip. Its `scripts/H55-Version.lua` holds `H55_Version` ("19d" today) ([mmh55-rc19k.md](mmh55-rc19k.md)). Inno can't read zips itself, so the PowerShell script reads it (Windows has the zip classes built in).
  2. The SHA-256 of `bin\55u.dll`, MMH5.5's runtime patcher. RC19d: `a6de006f…`, RC19k: `a28663da…` (full hashes in mmh55-rc19k.md). An unknown hash means a version we have not tested.
- **How the DLL itself tells them apart** (for comparison): `src/hook/game_build.cpp` checks the exe's bytes (MMH5.5's patch at `0x0081eeb7`), and `host_rules.cpp` checks the jump that RC19k's `55u.dll` changes at start-up. Both versions use the same co-op exe file. So the installer's check is a friendly warning; the DLL keeps the final say (anything unknown: "unknown build", the add-on stays off, the game runs normally).
- **The exe:** `MMH55_Utility_64.exe`, or `H5_Game.exe` when it is a copy of it (the GameRanger guide tells players to rename it). The setup recognises the file by its hash and says "the co-op exe (renamed for GameRanger)".
- **MMH5.5's own uninstaller** is `Uninstal.exe` for RC19d and older, and `unins000.exe` from RC19e on: a third hint, shown in the log only.

### Uninstall from Windows "Apps"

- Inno registers "H5 Together (Heroes V add-on)" with its version.
- **Our uninstaller must not live in the game folder.** Inno names its uninstaller `unins000.exe`, which is MMH5.5's own uninstaller name since RC19e. MMH5.5's upgrade guide tells players to run `unins000.exe`, so a second one there would be a trap. Ours goes in `%LOCALAPPDATA%\Programs\h5-together\`, with a copy of `uninstall.ps1`, the list of our template names, and the game folder(s) it installed into.
- Uninstalling runs `uninstall.ps1 -GameDir <folder>`. The "also delete your settings?" question becomes a normal Windows message box (a new switch for the script, so it doesn't wait for typing).

### Admin rights

- **Steam copies need none.** Steam gives every user full control of its folder. Checked on your PC (read only): `BUILTIN\Users:(F)` on the game's `bin`, inherited from Steam's folder. So the setup installs "for me only" (`PrivilegesRequired=lowest`), with no admin prompt.
- **Other copies under `Program Files`** (CD, some GOG installs) may need admin. The setup tests whether it can write there first. If it can't, it offers "Install as administrator" (Inno's own dialog).
- **A trap for those copies:** when the game itself can't write to its folder, Windows quietly redirects the game's writes to a hidden copy (`%LOCALAPPDATA%\VirtualStore\...`). Our panels' saves (the builder, the game rules, soon the settings menu) would then land there, and the next install would not see them. Plan: at start-up, the DLL checks that it can write next to the exe. If it can't, the log and the settings menu say "settings can't be saved here", and the FAQ explains it.

### "Windows protected your PC" and antivirus

**What happens today and with any unsigned setup:**
- A file downloaded from the internet carries a "came from the internet" mark.
- When it runs, SmartScreen checks the file's **reputation**: unsigned files start with none. The player sees "Windows protected your PC" and must click "More info", then "Run anyway".
- For unsigned files, reputation belongs to **each file**, so it starts again at zero with every new version.
- SmartScreen does not check DLLs. But antivirus programs may dislike a `dinput8.dll` that changes the game's code in memory: game cheats use the same trick. Not seen yet, but it is the biggest risk for strangers.

**What helps for free:**
- Version information inside the DLL and the setup (product name, description, version). Today the DLL has none. Files without it look more suspicious to heuristics. About 0.25 session.
- No packers or exe compressors (we use none).
- Every release: scan the files on VirusTotal, put the link and the SHA-256 in the release notes, and report false alarms to Microsoft (its file-submission page for software developers) and to any other vendor that flags us.
- GitHub "build provenance" (free): GitHub signs a statement that the file was built from our repo by our workflow. It does not remove the SmartScreen warning, but it proves to technical users where the file came from.
- **SignPath Foundation:** free code signing for open-source projects. They review the project and sign the files that GitHub builds; the certificate says "SignPath Foundation", not your name. Conditions:
  - an OSI licence (MIT is one) and a public repo;
  - no closed-source parts;
  - two-factor login for everyone who can sign;
  - signing only from the CI build.

  Whether they accept a game hook DLL is not known: we ask. With a signature, reputation builds up across versions instead of starting again with each one.

**Paid options (prices from search results on 2026-11-02; check before buying):**

| Option | For whom | Cost | Notes |
|---|---|---|---|
| Certum "Open Source Code Signing" | individuals, EU included | roughly €70 to €360, depending on the product (cloud or card) | the cheapest real certificate; in your name |
| Microsoft "Artifact Signing" (formerly "Trusted Signing") | individuals **only in the USA and Canada**; companies in the EU | about $10 a month (was announced; check) | not for you as a private person in France |
| An ordinary OV certificate from a reseller | anyone | about €150 to €500 a year | the key must sit on a USB token or in a cloud vault (a rule since 2023) |
| EV certificate | companies | more | since 2024 it no longer skips SmartScreen's warning, so it is not worth it for us |

None of them removes the warning on day one: reputation grows with downloads. **Recommendation:** do the free things, apply to SignPath, and decide on a paid certificate only if strangers really get stuck.

### Linux: keep the script, make it friendlier

- `bash install.sh` stays, with a short menu: Install / Uninstall / Make a bug report.
- It finds Steam in more places:
  - native Steam (as today);
  - the Flatpak (as today);
  - the Snap (`~/snap/steam/common/.local/share/Steam`);
  - the Steam Deck in desktop mode.
- It runs the same MMH5.5 check (`sha256sum` of `55u.dll`).
- **The launch option:** it reads (only reads) Steam's `localconfig.vdf` to see whether `WINEDLLOVERRIDES="dinput8=n,b" %command%` is already set. If not, it copies the line to the clipboard (when `wl-copy`, `xclip` or `xsel` is installed) and tells the player where to paste it. Steam's files are still never edited: Steam overwrites them while it runs.
- Colours only when the terminal supports them; plain ASCII text; the same `merge_ini` (tested byte-for-byte against Windows).
- GOG under Wine/Lutris/Heroic: `--game-dir`, plus an FAQ entry on adding the same override in that launcher.

### Steps and effort

| # | Step | Sessions |
|---|---|---|
| 1.0 | Check that Inno Setup is on GitHub's Windows machines; a "hello" setup built by the workflow | 0.25 |
| 1.1 | The setup: pages, finding the game (Steam + appmanifest + GOG + browse), MMH5.5 and exe checks, calling `install.ps1`, version info in the DLL and setup | 1.5 |
| 1.2 | "Apps" entry, uninstaller outside the game folder, `uninstall.ps1` switches, the "can't write here" case | 0.5 |
| 1.3 | CI: build the setup on every push; `test_scripts.py` runs it silently (`/VERYSILENT /DIR=...`) against the fake game folders it already makes | 0.75 |
| 1.4 | Linux script: menu, more Steam places, launch-option check, clipboard | 0.5 |
| 1.5 | VirusTotal step in the release checklist; the SignPath application (if you agree) | 0.5 |
| | **Total** | **4** (+0.5 if SignPath accepts and we wire their signing into the workflow) |

### Risks

- Antivirus false alarms on the DLL (the biggest one, whatever the installer).
- Inno missing from GitHub's machines (then a Chocolatey install on their side, your OK).
- GOG or other copies we can't test ourselves.
- A setup that starts PowerShell could look suspicious; the Pascal port is the fallback.

### Questions

- OK to use Inno Setup, built on GitHub's machines only?
- OK to apply to SignPath (free; the signature would say "SignPath Foundation")? Any budget for a paid certificate, or none for now?
- The name in "Apps": "H5 Together (Heroes V add-on)"? A Start-menu folder (settings help, uninstall) or nothing?

---

## Part 2: an in-game settings menu

### Today

- Players edit `h5-together.ini` in Notepad: 103 settings in 13 sections, all commented out, each with a comment. Then they restart the game.
- Some panels already write there: the template builder (`[rmg] builder_last`) and the game rules panel (`[rules]`, with `WritePrivateProfileStringW`).
- The DLL reads almost every setting **once at start-up** (each mod's `Apply...FromSettings()` in `src/hook/hooks.cpp`, or `Install...Early()` for the few that must act before the game reads its first data). That is why "restart the game" is the rule today.

### What the inventory shows

The full table is in [Appendix A](#appendix-a-every-setting). In short:

| Who it matters to | Settings | In the menu |
|---|---:|---|
| **Player:** each player chooses for themselves | 35 | 24 on the normal pages, 8 on "Advanced", 3 only in the file (the hero button's exact place, the recipe list that the builder replaces) |
| **Host:** only the host's value counts | 5 | 4 on the "Host" page, 1 on "Advanced" (AI pathfinding) |
| **Group:** each PC has one, but everyone in a game should have the same | 7 | "Advanced", with a warning |
| **Written by a panel** | 21 | no: the 20 game rules have their own panel (F2); the builder's last choice |
| **Tester:** logs, experiments, parts of a feature | 35 | no (file only) |
| *Hidden:* read by the code but not in the shipped file | 7 | no |

### Pages (proposed grouping)

1. **Interface:** player list (on, key, position), "your turn" notification (on, sound, pop-up), the panels' keys (game rules F2, hero looks F7, builder F11), the rules button in the waiting room, the hero-screen "Look" button.
2. **Camera and visuals:** zoom out (adventure map, battles), start zoom, black map edges, mouse wheel fix (shown only under Wine/Linux).
3. **Sound:** original Heroes V music, wider volume range.
4. **Language:** the add-on's language (part 3), the game's texts (`[lang] texts`: shown only when the packs are in `h5-together\lang\fr`).
5. **Random maps:** template builder (on, key), our templates in the game's list.
6. **Host:** teams on maps without teams, team arrows in the lobby, recipe-map teams, start terrain by faction. A line: "The game rules are in the waiting room: Rules... (F2)."
7. **Advanced (co-op and speed):** a banner "Everyone in the same game should use the same values here". Then:
   - co-op timing (on/off), co-op steps (50 ms / 100 ms / the game's 200 ms: one choice for `fast_segments` + `segment_ms`), view delay;
   - AI pathfinding speed-up;
   - autosaves: fast list, compression, every N days;
   - LAN over VPN: all adapters, extra addresses (a text box);
   - our templates in the list, the game rules exchange (`[rules] enabled`), live hero screen;
   - freeze detector (on, seconds).
8. **Help and logs** (not settings): our version, the game build and MMH5.5 version the DLL found, "Make a bug report", "Open the add-on's folder", the update check box (part 4), links.

### Restart, next game, or at once

Today all 44 menu settings need a restart. With a small `Reload()` in each mod (the values are plain variables that the code reads when it uses them), v0.3 can do this:

| When it applies | How many | Which |
|---|---:|---|
| **At once** | 20 | notification sound and pop-up; player-list key and position; panel keys; the add-on's language; hero button; live hero screen; rules button; LAN adapters and addresses; save compression; autosave days; freeze seconds; camera zooms and start zoom (from the next map or battle) |
| **From the next game** | 9 | co-op timing and steps, view delay; the host's teams and start terrain; showing others' hero looks |
| **After a restart** | 15 | the ones that install a hook at start-up or that the game reads only once: network timings, AI pathfinding, fast save list, our templates, the builder, the notification itself, the player list itself, original music, volume range, game texts, wheel fix, black edges, hero looks, the rules exchange, the freeze detector |

The menu marks each row ("restart" / "next game"). After OK it says "2 changes apply after a restart of the game". It never restarts the game itself.

**During a multiplayer game**, the "Advanced" and "Host" pages are read-only: "Used from the next game". Changing them halfway would only make this PC disagree with the others: the co-op guard would refuse, or worse.

### Where it opens

Proposed: all three below (one panel, three ways in).
- **A button in the game's Options screen.** The screen's files are in `data.pak` under `UI/GameOptions/` (buttons Accept, Back, Default, Combat replay, a category list), and it is reachable from the main menu and from the map's menu. We add a native button there, the way the builder's "Template..." button is added to the random map screen (`rmg_screen.cpp` hands the game an edited copy of the screen's file). Step 0: find the screen's class name with the robot, and a free spot.
- **A small "H5 Together 0.3.0" button in a corner of the main menu.** It shows everyone's version (useful before a co-op game; already wished for in [install-and-join.md](install-and-join.md)) and opens the menu.
- **A hotkey**, for example F8 (the game's own keys and MMH5.5's must be checked; ours are F2, F7, F9, F10 (testing), F11, F12 (testing)).

Not in the waiting room: it already has the Rules button, and the settings that matter for a game are the host's.

### Proposed layout

The same frame, fonts and buttons as the template builder and the game rules panel (`rmg_builder_widgets.cpp`: `Button`, `Combo`, `Stepper`, `CheckBox`, `Fit`, `NineSlice`; `overlay::OpenModal` so the game behind gets no clicks). The size is the builder's (1024×768 layout, scaled with the screen height). Page tabs on the left, rows on the right, and a help line at the bottom for the row under the mouse.

```
+----------------------------------------------------------------------+
|  H5 Together settings                        0.3.0 . MMH5.5 RC19k    |
+------------------+---------------------------------------------------+
| > Interface      |  Player list on the map            [x]            |
|   Camera         |     Key to hide or show it         [ F9     v]    |
|   Sound          |     From the top (%)          [-]  34  [+]        |
|   Language       |     From the left (%)         [-]   2  [+]        |
|   Random maps    |  "Your turn" notification          [x]  restart   |
|   Host           |     Sound                          [x]            |
|   Advanced       |     Pop-up (Windows)               [x]            |
|   Help and logs  |  Game rules key                    [ F2     v]    |
|                  |  "Rules..." button in the lobby    [x]            |
|                  |  Hero looks key                    [ F7     v]    |
|                  |  "Look" button on the hero screen  [x]            |
|                  +---------------------------------------------------+
|                  |  Shows who is playing and who has ended the turn. |
+------------------+---------------------------------------------------+
|  ! 1 change applies after a restart of the game.                     |
|  [   OK   ]     [ Defaults for this page ]               [ Cancel ]  |
+----------------------------------------------------------------------+
```

### How it saves

- **The ini stays the storage.** Nothing is written before OK; Cancel forgets everything.
- **In place, the installer's way:** a changed setting fills its own commented line `;key=...` in the same section (the same rule as `Merge-Settings`), so the file stays tidy and the next install keeps it. "Default" puts the `;` back. Other lines and comments are never touched.
  - Today's `WritePrivateProfileStringW` would add `key=value` at the end of the section and leave `;key=default` above it. That works, but it is untidy.
  - A small C++ "edit in place" function, tested with the same cases as `test_scripts.py`.
- **One table for every setting** in the C++ code: section, key, default, type, limits, page, who, when it applies, text id. The menu is drawn from it. A test checks that it matches the shipped `h5-together.ini` (same keys, same defaults), so the file and the menu can't drift apart.
- Values are checked with the same limits the mods use (for example zoom 100 to 400, `segment_ms` 50 or 100).
- If the file can't be written (a protected folder, see part 1), the menu says so.

### Steps and effort

| # | Step | Sessions |
|---|---|---|
| 2.1 | The table of settings + the test against the shipped ini | 0.75 |
| 2.2 | The in-place ini writer and its tests | 0.5 |
| 2.3 | The panel: pages, rows, help line, OK/Cancel/Defaults, read-only in a game | 1.5 |
| 2.4 | The ways in: Options screen button, main menu version button, hotkey | 1 |
| 2.5 | `Reload()` for the 20 "at once" settings; "restart" / "next game" marks | 0.75 |
| 2.6 | Robot screenshots of every page (English, French, the long test language from part 3), and tests | 0.5 |
| | **Total** | **5** |

### Risks

- The Options screen's button: its file and class are not researched yet. Fallback: the main-menu button and the hotkey only.
- A setting made "live" that isn't really safe halfway through a game: each `Reload()` is checked with the robot.
- 44 rows and help lines: the texts must stay short (part 3).

### What to ask the owner

- **Sketches over screenshots, please:** the game's Options screen (from the main menu and from the map), and the main menu with where the version button should go.
- Do the pages and their contents look right? Anything that should move to the Game rules panel instead (the host's teams and start terrain are host settings too)?
- Which hotkey? (F8 proposed.)
- Should "Advanced" be hidden behind a "Show advanced settings" box?

---

## Part 3: language-independent texts

### Built (2026-11-02, branch `v03-texts`)

How it works now: [src/mods/README.md, "Our panels in your language"](../../src/mods/README.md#our-panels-in-your-language-srcmodstextstxt-srchooktextscpp); for translators: [src/mods/texts/TRANSLATING.md](../../src/mods/texts/TRANSLATING.md).

- **201 texts per language** in `src/mods/texts/en.txt` and `fr.txt`, shipped as `bin\h5-together\texts\` (installer, uninstaller, `test_scripts.py`). English is built into the DLL (`build.ps1` makes `texts_en.inc` from `en.txt`). Every text is the same as before in English and French, except the few listed below.
- **Steps 3.1, 3.2, 3.3 and 3.5 done**; 3.4 in part: `tools/lang/ui_texts.py long` makes the long test language and the robot can install it (`--texts-dir`), but its screenshots and any overflow fixes are left for the settings menu block, which adds ~150 texts that need the same check.
- **Where it differs from the plan, and why:**
  - The code uses string keys (`texts::Get("rules.title")`), and each panel keeps its table of words as a table of keys (`texts::Table`), so the drawing code did not change. `tools/lang/ui_texts.py check` (in `build.ps1 -Test`) finds every key the code uses and checks it against `en.txt`.
  - Rule 3 (the game's own texts): instead of a table in the code, each texts file names the game's red player in its language (`[language] red_player`), so a translator adds the clue with the file. A name that no file has means "a language we have no file for": English, as before for a non-French game.
  - Rule 2 (Steam's file): used only when its `installdir` is the game folder the exe runs from, so a copy of the game elsewhere (the robot's test copies) does not borrow Steam's language.
  - `en.txt` in the game folder is not read: the built-in English is always the reference (an `en.txt` left from another version cannot bring old words back). It is there for translators.
  - The "why" reasons shown in panels were 11 sentences plus 3 file names, not ~35 (the other "why"s in the code only go to the log). They became `rules.why.*` and `rules.differ.*`; the technical detail stays in the log.
  - `Localize()` is gone: the template maker gives each message an id and its numbers (`rmg::MessageId`), next to its English sentence (which stays the web page's, for the parity test and the log).
- **Text changes** (all others are word for word): the "why" sentences of the rules panel are now short and translated (before: English only, with technical details); the "your turn" pop-up has French words (before: English only); "Not saved: error N" says "erreur" in French; the builder's "Auto" is a text (same word in both languages). New: `[language] name` and `red_player`.
- **Still open:** the two buttons placed on French screens (items 6 and 7 of the audit) need screenshots from players of other languages; the installer's own messages (item 11) belong to block 4; a font range for Greek, Romanian's ș/ț, Vietnamese and CJK when a translator comes.

### Today (before block 2)

- Every panel has two tables in its C++ file: one English, one French (`kEnglish` / `kFrench`). The French words are written as UTF-8 bytes (`E_ACUTE` and friends).
- `overlay::PanelLanguage()` (`src/hook/overlay.cpp`) picks one, once:
  1. `[ui] language=en/fr`;
  2. otherwise French when `[lang] texts=fr`;
  3. otherwise French when the game's text `Text/Game/Player/Colors/red_player.txt` contains "Joueur";
  4. otherwise French when Windows is in French;
  5. otherwise English.
- So a German, Russian or Polish player gets English: readable, but not friendly.

### The plan: one text file per language

- **Where:** `bin\h5-together\texts\en.txt`, `fr.txt`, `de.txt`, ... (not `h5-together\lang\`, which holds the game-text packs of `[lang] texts=fr`).
- **Format:** plain UTF-8 text (a byte-order mark is allowed), easy in Notepad:

  ```
  # H5 Together texts: Deutsch
  # language: de
  # translated by: (name)
  # for H5 Together: 0.3.0

  [player_list]
  day = Tag
  ready = bereit
  computer = Computer {n}

  [rules]
  title = Spielregeln
  help.governance = Ein Held kann eine Stadt regieren und ihr Boni geben.
  ```

  - `key = text`; `\n` makes a new line.
  - **Named placeholders** like `{n}` or `{hero}` instead of C's `%s`/`%d`. A translator who breaks a `%s` could crash the game; a wrong `{n}` only shows as text. The DLL checks that a line has the same placeholders as the English one; otherwise it uses the English line and writes it in the log.
  - The game's colour tags (`<color=yellow>`, `<br>`) stay where they are in the few tooltips that go into the game's own texts. They are checked the same way.
- **Fallback:** each missing or broken line falls back to English. **English is also built into the DLL** (from `en.txt` when it is compiled), so the panels work even without the folder.
- **Changing the language** in the settings menu applies at once: the panels look up their words each time they draw.
- **A translator's steps** (in `docs/translating.md`):
  1. copy `en.txt` to `de.txt`;
  2. translate the right-hand sides, keeping `{...}` and `<...>`;
  3. put it in `bin\h5-together\texts\` and choose the language in the menu (or `[ui] language=de`);
  4. send it to us (a GitHub issue form "Translation", or Discord).
- **A tool:** `tools/lang/ui_texts.py check` lists, for each language, the missing lines, the lines English no longer has, and placeholder mistakes. CI runs it. `ui_texts.py new de` writes a starter file.
- Later, if many translators come: the same files can be loaded into a free translation website (Weblate hosts open-source projects for free). Not needed for v0.3.

### Detecting the player's language

The rule for `[ui] language=auto`, the first answer wins:
1. `[lang] texts=<code>` when the player chose game texts in a language.
2. **Steam's language for the game:** the `language` line of `steamapps\appmanifest_15370.acf`. On your PC: `"language" "french"`. That file sits two folders above the game folder, so the DLL can find it. Steam's names map to codes: english→en, french→fr, german→de, russian→ru, polish→pl, spanish→es, italian→it, czech→cs, hungarian→hu.
3. **The game's own texts:** `red_player.txt` compared with a small table of known translations ("Red player", "Joueur rouge", ...). We know only English and French today; translators can add their line.
4. Windows' language.
5. English.

If the language found has no file, English is used. The log says which rule decided.

### Audit: what depends on the game's language

Good news first:
- **No widget is found by its text.** Our code recognises screens by their internal class names (`.?AVCAdventureScreen@NUI@@`, `CHeroScreen2`, `CRMGScreen`, `CMPWaitScreen`...), the game's files by their paths, heroes by their internal names. These are the same in every language.
- The template builder's "Use" fills the game's screen by values and positions (sizes, "ARMG Templates"), not by labels.
- The robot finds no text either; it is a test tool anyway.

What must change, or be checked:

| # | Where | What | Fix |
|---|---|---|---|
| 1 | `overlay.cpp` `PanelLanguage()` | Knows only "French or not": the "Joueur" test and Windows' French | The detection above, for any language |
| 2 | `rmg_builder_data.cpp` `Localize()` | Translates the generator's messages into French by **matching the English sentence** ("Zones are small (...") with `sscanf` | Give each message an id and its numbers; translate by id |
| 3 | `host_rules*.cpp`, `rmg_maker.cpp`/`rmg_recipe.cpp` | About 35 "why" reasons shown in panels are English only ("the file was not found", "...'s H55-AdvMap.lua is not the host's") | Short translated sentence in the panel; the technical detail stays English, in the log |
| 4 | `host_rules_bonus.cpp` | Our tooltips **replace the game's own** lobby bonus tooltips (English or French), with game colour tags | In the text files, like the others; on a German game today they show in English |
| 5 | `rmg_screen.cpp` | The native button's label and tooltip ("Template..." / "Modèles...") are served as game text files | In the text files |
| 6 | `host_rules_panel.cpp` | The "Rules..." button sits at the right end of the waiting room's title row. That place was measured on the French screen ("Options du mode Standard"); a longer title in another language could run under it | Screenshots in German/Russian from community testers; move it if needed |
| 7 | `hero_skins_panel.cpp` | The "Look" button's place in the "Armée & Stats" header was measured on your French screenshots. It takes MMH5.5's square button slot, so the text length probably doesn't matter | Check with another language's screenshot |
| 8 | Every panel (`Fit()`) | Fixed widths in the 1024×768 layout; a text too long is cut with "...". German and Russian are often 30–40% longer than English | A **test language** `[ui] language=xx` (every text 40% longer, with accents) and robot screenshots of every panel |
| 9 | `notify.cpp` | The pop-up ("Heroes V: your turn", "Your turn (day N).") is English only | In the text files |
| 10 | `languages.cpp` | `[lang] texts=` knows only `fr` | Optional extra below |
| 11 | `h5-together.ini` comments, installer scripts | English only | The ini stays English (the menu replaces it); the setup's own pages come translated by Inno, and our ~40 setup messages get English and French |

The log stays English: it is for us.

### Strings to move

| File | Texts per language |
|---|---:|
| `host_rules_panel.cpp` (game rules panel) | 94 |
| `rmg_builder_data.cpp` (template builder, its button) | 56 + 5 messages translated by `Localize()` |
| `host_rules_bonus.cpp` (lobby bonus tooltips) | 10 |
| `hero_skins_panel.cpp` (hero looks) | 10 |
| `player_list_draw.cpp` | 4 |
| `notify.cpp` (English only today) | 3 |
| "Why" reasons shown in panels, English only today (`host_rules*.cpp`, generator) | ~35 |
| **Today's total** | **~215** |
| New: the settings menu (labels, help lines, pages) | ~150 |
| **v0.3 total** | **~365 per language** |

Moving them is mechanical work, a good fit for Sonnet, with a test that every id used in the code exists in `en.txt`.

### Fonts

- The panels use a Windows font (Tahoma Bold, or Segoe UI Bold, or Arial Bold; Wine has Tahoma too). It is loaded with Latin-1, Latin Extended-A and Cyrillic letters (`overlay.cpp`, `AddFont`).
- That covers English, French, German, Spanish, Italian, Polish, Czech, Hungarian and Russian.
- Not Chinese: that would need a font with Chinese letters and a much bigger letter sheet. Later, if a translator shows up.

### Optional extra: the whole game in another language

`[lang] texts=fr` serves MMH5.5's French pack from `h5-together\lang\fr\` ([languages.md](languages.md)). The code works by folder, so `texts=de` with MMH5.5's German pack would be a small change: the MMH5.5 team hosts German, Spanish, Hungarian, Italian, Polish, Russian, Czech and Chinese translations. It is not needed for our own texts. About 0.5 session plus a test by a player of that language.

### Steps and effort

| # | Step | Sessions |
|---|---|---|
| 3.1 | Text file format, loader, built-in English, placeholder/tag checks, `ui_texts.py`, tests | 1 |
| 3.2 | Move the ~215 texts out of the 8 files; message ids for `Localize()` and the reasons | 1.5 |
| 3.3 | Language detection (Steam's manifest, the game text table, Windows) | 0.5 |
| 3.4 | The long test language; robot screenshots of every panel; fix what overflows | 0.75 |
| 3.5 | The translator guide and the "Translation" issue form | 0.25 |
| | **Total** | **4** |

### Risks

- Button places measured on French screens (items 6 and 7): we need screenshots from players of other languages.
- A translation for one MMH5.5 version may describe rules that changed (the lobby tooltip story: [lobby-audio-classes.md](lobby-audio-classes.md) item 2): `ui_texts.py` flags lines whose English changed since the translation.

### Questions

- Ship English and French ourselves, and others from the community? May we ask on the MMH5.5 Discord for translators?
- The test language: robot screenshots only, or also something you check by eye?

---

## Part 4: community kit

**Built (block 5, branch `v03-safety`, 2026-11-02):**
- **Crash reports** (`src/hook/crash.cpp`): on a crash, `h5-together-crash-<date>-<time>.txt` (what, where as module + offset, the callers, our version, the last log lines) and a small minidump (30 to 100 KB) next to the log; the 3 newest kept; the next start's log names the newest. The game's own filter (`0x00c67a50`, which recovers from some access violations) sits in front of ours and passes the real crashes on; ours then calls the filter before it, so the game closes as before. `[debug] crash_report=1`. The bug report scripts add the newest report (`-NoDump` / `--no-dump`). Robot: a deliberate crash (`crash_test_at`) was reported and the game closed by itself; normal play and the hang watchdog unchanged.
- **Versions in the lobby** (`src/mods/host_rules*.cpp`): a line at the bottom of the waiting room on every PC: all the same, or who has another version, another build, an older one, or none. Two more requests in the rules' hello ('V', 'B'), and the host's list to clients that said theirs; nothing changes for 0.2.7. Robot: same version (607 checksums equal), another version, an older host, a client without the rules.
- **Update check** (`src/mods/update_check.cpp`): `[update] check=0` by default; when on, at most once a day at the main menu, one request to GitHub's latest-release address; a newer version shows two lines in the main menu and the log. The settings menu will show the switch (and a first-start question, block 3). Robot: one real request, and the notice from a stored newer version.
- **Chain-loading** (`src/hook/chain.cpp`): the other mod's `dinput8.dll` renamed `dinput8_next.dll` (or `[chain] dll=`) is loaded and DirectInput goes through it; its calls back to "dinput8.dll" go to Windows' DLL. Not the installer's "move it for you" option: the installer's refusal now says how to rename it, the uninstaller says to rename it back. Robot: a stand-in mod DLL chained in a single-player game.

### Docs (English first)

- **README:**
  - a picture or GIF at the top;
  - "what it does" in 5 lines;
  - "Install: download the setup";
  - "Is it safe?": open source, built by GitHub, VirusTotal link, never changes game files, how to remove it;
  - "Play with friends: everyone needs the same version";
  - then today's details further down.
- **Install guide** ([install.md](../install.md)): with screenshots of the setup, and a section on the "Windows protected your PC" box.
- **FAQ** (new, `docs/faq.md`):
  - "Windows protected your PC";
  - "My antivirus removed `dinput8.dll`";
  - "Does it work without MMH5.5?" (not yet);
  - GOG;
  - GameRanger and the H5 Lobby app;
  - "out of sync";
  - "where is the log?";
  - "how do I change settings?";
  - Linux and Steam Deck;
  - uninstalling;
  - "bugs of MMH5.5 itself go to MMH5.5's team".
- **Compatibility table** (new, in the README):

| Game | Status |
|---|---|
| Steam ToE 3.1 + MMH5.5 RC19d or RC19k, co-op exe `MMH55_Utility_64.exe` / `MMH55_Utility.exe` (also when renamed `H5_Game.exe` for GameRanger) | supported |
| The 2008 3.1 exe (`H5_GameOLD.exe`, RC19k's `H5_Game.exe` 3.1.2.98) | recognised (vanilla 3.1); the MMH5.5-only parts do nothing |
| MMH5.5's single-player exe `MMH55_64.exe` / `MMH55.exe` (3.1j AI) | not recognised: the add-on stays off, the game runs normally |
| Steam's 2025 rebuild `H5_Game.exe` 3.1.2.99 | not recognised (later) |
| Plain ToE without MMH5.5 | later |
| GOG | not tested |
| Windows 10 / 11 | yes |
| Linux, Steam + Proton | yes, with the launch option |
| Steam Deck, Wine/Lutris/Heroic, the H5 Lobby app | not tested |

### Screenshots and GIFs to make

- The player list during a co-op turn.
- The game rules panel, host view and player view.
- The template builder with its zone map, and "Use".
- Hero looks: the panel, then the hero before and after.
- The settings menu (new).
- Zoom out: 100% against 150%.
- The "your turn" pop-up.
- The setup's pages.
- The same panel in English and in French.

GIFs:
- an AI turn before/after (two games side by side);
- applying a hero look;
- the builder's "Use".

Who makes them:
- the robot for the panels, in English (`[ui] language=en`);
- you, in a real game, for the GIFs.

Other players' names stay out unless they agree.

### GitHub issue forms

In `.github/ISSUE_TEMPLATE/`:
- `bug_report.yml`:
  - the add-on and MMH5.5 versions;
  - how the game was started (GameRanger, LAN, alone);
  - Windows or Linux (Proton version);
  - how many PCs, and whether everyone had the same version;
  - what happened, and the steps;
  - **the report zip, or `h5-together.log`, attached** (required);
  - a tick box "after a freeze I waited 30 s before closing".
- `feature_request.yml`, `translation.yml`.
- `config.yml`: a link to MMH5.5's Discord "for problems of MMH5.5 itself".

### "Make a bug report" helper

- A button on the menu's "Help and logs" page, and `report.bat` / `report.sh` in the package (for when the game won't start).
- It makes `h5-together-report-<date>.zip` on the Desktop with:
  - `h5-together.log` and the previous one;
  - `h5-together.ini`;
  - a short `system.txt`: Windows version, the game build, MMH5.5 version and file hashes, the add-on version, the other DLLs in `bin`;
  - the crash file, if there is one.

  It never sends anything itself. A note tells the player that the log holds player names and LAN addresses.
- **The log grows forever today:** it is opened in "append" mode and never trimmed; yours is 1.4 MB. Plan: at start-up, if it is over 5 MB it becomes `h5-together.old.log` (one kept). Each run starts with a clear separator line.

### Crash information

- Today: the freeze detector (`src/hook/watchdog.cpp`) writes where a frozen game is stuck. Nothing catches a **crash**.
- Plan:
  - on a crash, write a small "minidump" (`h5-together-crash-<date>.dmp`, usually under 1 MB) with Windows' own `dbghelp.dll`, taken from `System32`: the game ships a 2002 copy in `bin`;
  - and a log line with the crash address in the watchdog's format (module + offset, the function name where we know it).
- The game's or MMH5.5's own crash handler, if any, is called after ours, as before.
- Privacy: a dump holds part of the game's memory (player names, paths). It stays on the PC; the bug report zip includes it only with a tick box.

### "New version available" (opt-in)

- Off by default (`[update] check=0`).
- At the first start after install, a small welcome panel:
  - "H5 Together 0.3.0 is running. Settings: F8.";
  - "Check for new versions? [Yes] [No]".
- When on: at most once a day, at the main menu, one HTTPS request to GitHub's public "latest release" address for our repo (`api.github.com/repos/jozrcr/h5-together/releases/latest`). It sends nothing about the player; GitHub sees the address it comes from, as with any website.
- A new version shows as one line under the version button, with a link to the Releases page. Nothing is downloaded or installed by the DLL.

### Version check in the lobby (more useful than the update check)

- Co-op only works when everyone has the same version. The DLL already sends small hellos at the start of a game (co-op guard, game rules).
- Add the version to the hello. The waiting room then shows "Bob: H5 Together 0.2.7 (you: 0.3.0)" before the game starts, instead of a silent "co-op timing OFF" in the log.
- About 0.5 session.

### Other `dinput8.dll` mods (chain-loading)

- Today the installer refuses (exit code 3) when a `dinput8.dll` that isn't ours is there. Other mods use the same name, for example "ASI loaders" that load widescreen fixes.
- Plan:
  - the setup offers "keep the other mod working": it moves the other file to `bin\h5-together\chain\dinput8.dll` (with the player's OK; the uninstaller puts it back);
  - our DLL loads that file instead of Windows' own `dinput8.dll` (one change in `dllmain.cpp`'s `Real()`), and that file then loads Windows' one.
  - The log says "chained: <file>".
- Risk: two mods changing the same game code. The log and the FAQ say how to undo it.
- Shipping ourselves as an `.asi` file instead would not work as is: our hooks start on the game's first DirectInput call, which an ASI loader would take for itself.

### Licences and notices

- Our code: MIT (`LICENSE`).
- Bundled third-party code, the only one: **Dear ImGui 1.91.9b** (MIT, `src/hook/third_party/imgui/LICENSE.txt`). Its `imstb_*` parts (stb by Sean Barrett) are MIT or public domain.
  - The MIT licence asks for its notice to go along with copies, so the zip and the setup must carry `THIRD-PARTY-NOTICES.txt` (ImGui's licence text) and our `LICENSE.txt`.
  - **Today the zip carries neither: to fix.**
- **Not bundled, and that must stay so:**
  - fonts (we use the system's);
  - `zlib1.dll` (the game's own);
  - the bonus icons and hero looks (read from each player's own game files);
  - MMH5.5's scripts (each PC edits its own copy in memory);
  - our private French gap packs (they hold translations of Ubisoft's and MMH5.5's texts).
- Our templates are made by our own tool, but some of their numbers were copied from MMH5.5's templates ([mmh55-rc19k.md](mmh55-rc19k.md)): credit MMH5.5 in the README, and mention it when we tell their team.
- The README already says we are not linked to Ubisoft.

### Where to announce (you post)

1. **First, tell the MMH5.5 team** (their Discord): what the add-on does, that it changes their settings only in memory, and that bug reports about it should come to us. It is polite, and they may have advice.
2. MMH5.5's Discord, in its mods/showcase channel.
3. Heroes Community forum (Heroes V mods section), and Celestial Heavens.
4. ModDB: a news post or an "add-on" entry under the MMH5.5 mod page, linking to GitHub. A second download copy on ModDB means one more place to update with every release: a link only is simpler.
5. Maybe Reddit (r/heroes).

I can draft the posts (English, and French if you want) for you to post.

### Steps and effort

| # | Step | Sessions |
|---|---|---|
| 4.1 | Log size limit, notices and licence in the package, issue forms | 0.75 |
| 4.2 | Report helper (in-game button, `report.bat` / `report.sh`) | 0.5 |
| 4.3 | Crash dumps | 0.75 |
| 4.4 | Version in the lobby hello and the waiting room | 0.5 |
| 4.5 | Welcome panel and the opt-in update check | 0.75 |
| 4.6 | Chain-loading (DLL and installer) | 0.5 |
| 4.7 | README, install guide, FAQ, compatibility table; screenshots with the robot; your GIFs | 1 |
| 4.8 | Release notes and the announcement drafts | 0.25 |
| | **Total** | **5** (1.25 of it sits in the "quick wins" and "docs and release" blocks of the order below) |

### Risks

- A crash handler must never cause a crash itself: it is tested with a deliberate crash (a hidden testing setting, like `hang_test`).
- Chain-loading an unknown mod: anything can happen. The log and the FAQ say how to undo it.
- More users means more bug reports: the issue forms and the report zip keep them useful.

### Questions

- OK for crash dump files (they stay on the PC)?
- The update check: OK to contact GitHub when the player says yes? Or no internet at all?
- Do you or your friends know any other `dinput8.dll` mod used with Heroes V?
- Enable GitHub Discussions for questions (a repo setting: your click)?
- Should I draft the announcement posts? English only, or English and French?

---

## Leaving room for later: plain ToE and Steam's 2025 exe

Not in v0.3 (your choice: after online play). The v0.3 work leaves room:
- **Settings table:** each setting gets a "needs MMH5.5" mark. Examples: the game rules, black edges (MMH5.5's dusk sky), original music, game texts (MMH5.5's packs), the hero-screen button (MMH5.5's layout). The menu can then hide them on plain ToE with no redesign.
- **Installer:** it already tells "plain ToE 3.1, no MMH5.5" and "Steam's 2025 exe" apart and says "not supported yet". The same pages later say "supported".
- **Texts:** generic sentences don't name MMH5.5 where they needn't.
- **DLL:** the "unknown build: stay off" rule stays the safety net.

---

## Proposed order of work for v0.3

| Order | Block | Why this order | Sessions |
|---|---|---|---:|
| 1 | **Quick wins:** log size limit, licence and notices in the zip, version info in the DLL, issue forms, report script; send the SignPath application (if you agree), since it takes time | cheap, useful at once, and the application can wait in their queue | 1.5 |
| 2 | **Texts in files** (part 3) | the settings menu then gets its ~150 new texts the right way from the start | 4 |
| 3 | **Settings menu** (part 2) | the biggest change for players; needs your sketches, so ask early (during block 1) | 5 |
| 4 | **Installer** (part 1; the version info is already done in block 1) | ships the new `texts\` folder; independent, could also go before 2 | 3.75 |
| 5 | **Safety nets:** crash dumps, lobby version check, welcome panel + update check, chain-loading | they make strangers' bug reports useful | 2.5 |
| 6 | **Docs and release:** README, FAQ, compatibility, screenshots, notes, announcement drafts; then a co-op test game with your friends before publishing | last, so the screenshots show the final panels | 2 |
| | **Total** | | **about 19** |

**A smaller cut (about 14 sessions)** if you want it sooner:
- block 1;
- from block 2 only 3.1 and 3.2 (the text files, English and French);
- block 3 without the Options-screen button (main-menu button and hotkey only: 4);
- blocks 4 and 6.

The language detection, the long test language, the Options-screen button and all of block 5 then wait for v0.3.1.

---

## All questions for the owner

1. **Installer:** OK for Inno Setup, built on GitHub's machines only (nothing on your PC)?
2. **Signing:** apply to SignPath (free; signed as "SignPath Foundation")? A budget for a paid certificate (about €70 to €500 a year), or none?
3. **Settings menu, your sketches:** please draw on screenshots of the game's Options screen (from the main menu and from the map) and of the main menu (the version button's corner).
4. **Settings menu, content:** do the 7 pages look right? Should the host's teams and start terrain move into the Game rules panel? Which hotkey (F8)? Should "Advanced" be hidden behind a tick box?
5. **Languages:** English and French by us, the others from the community? May we ask for translators on the MMH5.5 Discord?
6. **Update check:** OK as opt-in (a question at first start), contacting only GitHub? Or no internet at all?
7. **Crash dumps:** OK to write them on a crash (kept on the PC, sent only by the player)?
8. **Other mods:** do you know any other `dinput8.dll` mod used with Heroes V (widescreen fix, ASI loader)? Any GOG player in reach for a test?
9. **"Apps" entry and shortcuts:** "H5 Together (Heroes V add-on)"? A Start-menu folder or none?
10. **Announcements:** shall I draft the posts (English, and French)? OK to tell the MMH5.5 team first?
11. **Full plan or the smaller cut?**

---

## Appendix A: every setting

All 103 settings of the shipped `tools/package/files/h5-together.ini` (0.2.7), section by section. Checked against the code on 2026-11-02 (`GetPrivateProfile*` calls in `src/hook` and `src/mods`): every one is read by the code, all of them at start-up.

- **Who:** Player = each player for themselves. Host = only the host's value counts. Group = each PC, but everyone in a game should have the same. Panel = written by a panel. Tester = logs, experiments, parts of a feature.
- **Menu:** Show = a normal page. Adv = the Advanced page. Rules = the Game rules panel (F2). No = file only.
- **v0.3:** when a change would apply. Restart; Next game; Live = at once (camera: from the next map or battle).

### [net]: network timings, LAN over VPN (`net_tuning.cpp`, `lan_discovery.cpp`, `lockstep.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `tuning` | 1 | modern network timings (quicker AI turns in co-op) | Group | Adv | Restart |
| `keepalive_ms` | 50 | how often an idle link sends something | Tester | No | Restart |
| `send_interval_ms` | 10 | how often a link may send | Tester | No | Restart |
| `batched_segments` | 0 | "segment done" reports held back | Tester | No | Restart |
| `lan_all_adapters` | 1 | the LAN "any games?" message also into ZeroTier/VPN adapters | Player | Adv | Live |
| `lan_peers` | (empty) | addresses to ask directly (VPNs without broadcasts) | Player | Adv (text box) | Live |
| `drain_reports` | 1 | take in every waiting step report each frame | Tester | No | Restart |
| `view_delay_ms` | 100 | how long after its step a move is shown (50–400) | Group | Adv | Next game |
| `trace_lockstep` | 1 | a summary in the log every 10 s (helps bug reports) | Tester | No | Restart |
| `catch_up_ms` | 600 | how far the step timer may jump after a freeze | Tester | No | Restart |
| `scale_pacing` | 1 | the game's "slow the PC ahead" rule scaled to short steps | Tester | No | Restart |
| `net_sleep_ms` | 2 | rest of the network thread between rounds (1–10) | Tester | No | Restart |

### [coop]: faster co-op steps (`coop_guard.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `guard` | 1 | take part in the shorter co-op steps (only if every PC agrees) | Group | Adv | Next game |
| `fast_segments` | 1 | ask for shorter steps | Group | Adv (one choice with `segment_ms`) | Next game |
| `segment_ms` | 50 | the shorter length: 50 or 100 ms | Group | Adv (same choice) | Next game |

### [ai]: faster AI pathfinding (`fast_paths*.cpp`, `ai_pacing.cpp`); the AI runs on the host

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `fast_paths` | 1 | the AI finds the same paths, sooner | Host | Adv | Restart |
| `remember_checks`, `fast_queue`, `port_relax`, `port_search`, `port_expand`, `fast_ashore` | 1 each | the parts of `fast_paths`, each can be turned off | Tester | No | Restart |
| `ashore_check` | 0 | also ask the game and log differences (slow) | Tester | No | Restart |
| `one_decision_per_frame` | 1 | the host-freeze fix (one AI decision per frame) | Tester (keep on) | No | Restart |
| `max_expansions` | 0 | experiment: changes the AI's paths | Tester | No | Restart |

### [save]: autosaves (`autosave.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `fast_save_list` | 1 | no long freeze when the Saves folder holds many saves | Player | Adv | Restart |
| `trace_autosave` | 1 | a log line per autosave | Tester | No | Restart |
| `compression` | 1 | save compression, 1 (fast) to 9; -1 = the game's 6 | Player | Adv | Live |
| `autosave_every` | 1 | autosave every N days | Group | Adv | Live (between games) |

### [notify]: "your turn" (`notify.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `turn` | 1 | flash the taskbar button when it is your turn and the game is behind | Player | Show | Restart |
| `sound` | 1 | also a short sound | Player | Show | Live |
| `popup` | 1 | also a Windows pop-up | Player | Show | Live |
| `trace` | 0 | log every turn change | Tester | No | Restart |

### [ui]: player list, panels' language (`player_list*.cpp`, `overlay.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `player_list` | 1 | the player list on the adventure map | Player | Show | Restart (F9 hides it at once) |
| `player_list_key` | F9 | its key (F1–F12, none) | Player | Show | Live |
| `player_list_top` | 34 | its place from the top, % of the height | Player | Show | Live |
| `player_list_left` | 2 | its place from the left, % | Player | Show | Live |
| `language` | auto | the panels' language (part 3: any code with a file) | Player | Show | Live |
| `trace_players` | 0 | log turn changes | Tester | No | Restart |
| `player_list_test_rows` | 0 | fake rows for tests | Tester | No | Restart |
| `overlay_test_panel` | 0 | the input test panel | Tester | No | Restart |
| `overlay_test_key` | F10 | its key | Tester | No | Restart |

### [rmg]: random maps (`rmg_teams.cpp`, `rmg_templates.cpp`, `rmg_builder*.cpp`, `rmg_factions.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `teams` | (empty) | teams on maps without their own: humans_vs_ai, pairs, or a list | Host | Show (Host) | Next game |
| `lobby_teams` | 0 | the lobby's team arrows pick team numbers | Host | Show (Host) | Next game |
| `trace_teams` | 1 | log the teams | Tester | No | Restart |
| `recipe_teams` | 1 | recipe maps get their template's teams | Host | Show (Host) | Next game |
| `templates` | 1 | our templates and recipe templates in the game's list | Player (everyone needs the same) | Adv | Restart |
| `recipes` | (empty) | recipe names to offer in the list | Player | No (the builder replaces it) | Restart |
| `builder` | 1 | the template builder's button and panel | Player | Show | Restart |
| `builder_key` | F11 | its key | Player | Show | Live |
| `builder_last` | (written) | the builder's last choices | Panel | No | (panel) |
| `faction_terrain` | 0 | start terrain that matches each faction (the map is rebuilt) | Host | Show (Host) | Next game |
| `trace_factions` | 0 | research log | Tester | No | Restart |

### [audio] (`original_music*.cpp`, `audio_volume.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `original_music` | 0 | Heroes V's music where MMH5.5 changed it | Player | Show | Restart |
| `log_music` | 0 | log every music file started | Tester | No | Restart |
| `volume_curve` | game | `wide`: the sliders' lowest step 10 times quieter | Player | Show | Restart |
| `log_volume` | 0 | log volume changes | Tester | No | Restart |

### [lang]: the game's texts (`languages*.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `texts` | game | `fr` = the game's texts from the French packs in `h5-together\lang\fr` | Player | Show (only when packs are there) | Restart |
| `log_texts` | 0 | log the first N texts served | Tester | No | Restart |

### [camera] (`camera.cpp`, `wheel.cpp`, `edge_sky.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `zoom_out` | 100 | adventure map zoom-out limit, % (100–400) | Player | Show | Live (next map) |
| `combat_zoom_out` | 100 | battle zoom-out limit, % | Player | Show | Live (next battle) |
| `start_zoom` | (empty) | where the adventure camera starts | Player | Show | Live (next map) |
| `wheel_fix` | auto | the mouse wheel fix under Wine (auto, 1, 0) | Player (Linux) | Show (under Wine only) | Restart |
| `edge_sky` | game | `none` = black past the map's edge | Player | Show | Restart |
| `edge_sky_test_light` | (empty) | test one more light file | Tester | No | Restart |

### [skins]: hero looks (`hero_skins*.cpp`; the choices are in `h5-together-skins.ini`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `skins` | 1 | hero looks on | Player | Show | Restart |
| `key` | F7 | the looks panel's key | Player | Show | Live |
| `hero_screen_button` | 1 | the "Look" button on the hero screen | Player | Show | Live |
| `hero_screen_button_x` | 403 | its place (1024×768 layout) | Player | No | Live |
| `hero_screen_button_y` | 94 | its place | Player | No | Live |
| `live_hero_screen` | 1 | reopen the hero screen after Apply to show the new figure | Player | Adv | Live |
| `show_others` | 1 | take part in multiplayer looks | Player | Show | Next game |
| `trace` | 0 | log each figure built | Tester | No | Restart |

### [rules]: game rules from the host (`host_rules*.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `enabled` | 1 | take part in the host's rules (the hello) | Group | Adv | Restart |
| `key` | F2 | the rules panel's key | Player | Show | Live |
| `button` | 1 | the "Rules..." button in the waiting room | Player | Show | Live |
| `governance`, `town_conversion`, `extra_conversions`, `town_gate`, `mine_control`, `dimension_doors`, `ai_cheat`, `mixed_stacks`, `neutral_stack_size`, `bank_difficulty`, `negative_artifacts`, `rpg_potions`, `map_scripts`, `competitive_towns`, `mana_observatories`, `moon_disc`, `game_mode`, `toe_bonuses`, `class_skills`, `class_chance` (20) | (empty) = MMH5.5's file | the host's choices, written by the panel's OK ([host-rules.md](host-rules.md)) | Host (Panel) | Rules | Next game |
| `trace_skills` | 0 | log level-up skill offers | Tester | No | Restart |
| `class_skills_single` | 0 | use `class_skills` in single player | Tester | No | Restart |
| `trace` | 0 | log script files opened, start resources | Tester | No | Restart |

### [debug] (`watchdog.cpp`, `wheel.cpp`)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `hang_watchdog` | 1 | the freeze detector | Player | Adv | Restart |
| `hang_seconds` | 20 | seconds without an answer before it looks (5–600) | Player | Adv | Live |
| `hang_test` | 0 | F12 freezes the game on purpose | Tester | No | Restart |
| `trace_wheel` | 0 | log the mouse wheel's input | Tester | No | Restart |
| `crash_report` | 1 | a crash report and a small minidump next to the log when the game crashes (0.3.0) | Player | Adv | Restart |

### [update] (`update_check.cpp`, 0.3.0)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `check` | 0 | at most once a day, ask GitHub for the latest version; a notice in the main menu if newer | Player | Show | Restart |
| `last_check`, `latest` | | written by the add-on: when it last asked, and what it was told | Panel | No | |

### [chain] (`chain.cpp`, 0.3.0)

| Key | Default | What it does | Who | Menu | v0.3 |
|---|---|---|---|---|---|
| `dll` | (dinput8_next.dll) | another `dinput8.dll` mod to load and pass DirectInput to; none = never | Player | Adv | Restart |

### Totals

- 35 Player, 5 Host, 7 Group, 21 Panel (the 20 rules and `builder_last`), 35 Tester = 103 in 0.2.7; 0.3.0 adds `crash_report`, `[update] check` (and the two values it writes) and `[chain] dll`.
- In the menu: 28 Show + 16 Advanced = 44. The other 59: 20 in the Game rules panel, 39 file only.

## Appendix B: hidden settings and other files

- **Read by the code, not in the shipped file** (all for testing; keep them out of the menu):
  - `[audio] log_data_files`;
  - `[debug] hang_test_at`, `hang_test_ai_step`, `hang_test_frame_ms`, `crash_test_at` (0.3.0: crashes on purpose);
  - `[rules] test_version` (0.3.0: another version, or none, in the waiting room);
  - `[rmg] templates_selftest`;
  - `[skins] test_ai_heroes`, `test_button_on_map`.
- `h5-together-skins.ini`: the hero look choices, written by the looks panel.
- `h5-together-robot.ini` and the `OPEN_HOMM5_INSTANCE` variable: the robot's own settings, and per-copy file names (`h5-together-<instance>.ini` / `.log`) when two copies of the game run on one PC. The settings menu must use the same file-name function (`hooklog::PathNextToExe`) so it edits the right file in robot runs.

## Sources (signing, checked 2026-11-02)

- SignPath Foundation: <https://signpath.org/>, terms: <https://signpath.org/terms>
- Microsoft's code-signing options: <https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options>; Artifact Signing eligibility (individuals only in the USA and Canada): <https://learn.microsoft.com/en-us/answers/questions/5915697/can-an-irish-sole-trader-(non-incorporated)-qualif>
- Certum open-source code signing: <https://shop.certum.eu/open-source-code-signing.html>

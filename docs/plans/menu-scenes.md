# Plan: a random main menu scene (and its music) on launch

Status: **built** (2026-11-02, branch `menu-scene`): `[menu] scene=default|toe|homm5|random` is in `src/mods/menu_scene*.cpp`; what was decided and what was built are in "Built" just below. The rest of this page is the research it was built from (written the same day, before building); where the build differs, "Built" says so. The research: Done by reading the paks in the Steam copy (read only), the music folder, the reference exes in `ref\` and Ghidra (`H5_Game_31_vanilla.exe`), plus a few web searches. The game and the robot were not run. Nothing in the repo changed except this file. The owner's ask: "Random main menu background on launch (between Heroes V / Hammers of Fate / Tribes of the East) + associated music."

## Built (2026-11-02)

**The owner's decisions** (2026-11-02), which this build follows:

1. **Scenes: Heroes V and Tribes of the East only.** Hammers of Fate's scene is not in the files, so there is no `hof`. (Question 1: option a.)
2. **Heroes V on a PC without MMH5.5: unavailable.** `homm5` falls back to `default` with a log line. (Question 2.)
3. **Logo: MMH5.5's logo stays on every scene.** We do not touch it. (Question 3.)
4. **Music: each scene gets its own theme** (Heroes V: `music\MainMenu\MainMenu-Theme.ogg`; ToE: `music\a2\Main_Menu.ogg`), and `random` picks both together. The multiplayer sub-menu's theme is left alone. (Question 4.)
5. **The setting:** `[menu] scene=default|toe|homm5|random`; **`default` is the default and installs nothing**. (Question 5.)
6. **One pick per game launch** (not a new one when you come back to the menu). (Question 6.)

**What differs from the plan above** (section 3): the music is **not** served as `a2-MainMenu.xdb`. The plan's table did that and noted the side effect on the one ToE campaign dialog that plays that definition (`A2C3/M4/S2`). The owner asked to limit it. Two ways were looked at:
- *Serve `a2-MainMenu.xdb` only while the main menu is up:* does not work. The game keeps a music definition once it has read it, so the dialog would play whatever the menu loaded. It would also need us to know when the menu is shown.
- *Point `UIGameRoot`'s `MAIN_MENU_MUSIC_ID` at a name of our own* (chosen, the safest): the game's root table of music (`UI/UIGameRoot.(UIGameRoot).xdb`, 333 KB in MMH5.5's pak) is served with that one item changed to `Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb`, and that name (in no pak) is served with the chosen theme's 7-line definition read from `data.pak`. **Nothing else that plays music definitions is touched:** `a2-MainMenu.xdb` (the dialog), `a1-MainMenu.xdb` (a Hammers of Fate campaign dialog), `MP-MainMenu.xdb` (the multiplayer sub-menu) and every other definition stay as the game has them. It also makes `[audio] original_music=1` harmless: that setting sets `a2-MainMenu.xdb` back to the ToE theme, and the menu no longer asks for that file when `scene` is not `default`. **So `[menu] scene` wins over `original_music`** whatever the order; with `scene=default`, `original_music` does what it did. (The two hooks wrap the same file system slot; ours goes in after `original_music`'s so that it is the outer one and sees the root table as `original_music` left it.)
- *The one case it cannot cover:* the root table is read about 1.1 s after the game starts. Our hook goes in at the DLL's load, before that, on every exe the DLL hooks at load (all MMH5.5 exes). On a Steam-wrapped exe (hooked later, 4 s) the table is already read; then, and only then, `a2-MainMenu.xdb` itself is served with the theme (logged), and that dialog changes with it. A plain Steam ToE has nothing to change anyway (`homm5` is unavailable there and `toe` is what it shows).

**How a scene is judged available** (section 3's "checks before serving", built a bit more generally): at the first menu file the game opens, the directories of all `data\*.pak` are read (16 paks, 129,415 files, 60 to 80 ms). Each arena description found under `Arenas/Interface/MainMenu/MainMenu.(ArenaDesc).xdb` is told apart by the scene file it was exported from (`/H5A2/UI/MainMenu/MainMenu.mb` = ToE, `/models/MainMenu/M_Menu.mb` = Heroes V), not by which pak holds it; every file the description names (objects, effects, light, camera animation) must be in some pak; the theme's definition must be in `data.pak` or `a2p1-data.pak`; and its `.ogg` must be in `music\`. Anything else is "not available", with the reason in the log. For both scenes the description we serve is the one found this way (not "whatever the layering gives"), so the scene shown does not depend on which pak is newest.

**Robot checks** (2026-11-02, `C:\Games\H5_test_k`, MMH5.5 RC19k, `MMH55_Utility_64.exe`):

| Check | Result |
|---|---|
| `default` (no `[menu]` section) | The Heroes V picture, MMH5.5's logo, `MMH55\MainMenu_Dynasty_002.ogg`: as today. The log says only `scene=default (the menu as installed: nothing changes)`, no hook installed. |
| `toe` | The ToE picture (a cyclops at a castle wall; MMH5.5's logo and buttons on top), `a2\Main_Menu.ogg`. **The arena description is read through the file hook** (risk 3 of this plan: tested, it is). |
| `homm5` | The Heroes V picture (the burning arch devil and the bent tree, same as `default`), `MainMenu\MainMenu-Theme.ogg`. |
| `random`, 6 launches | `toe`, `toe`, `homm5`, `toe`, `toe`, `homm5`: both come up, and the theme always matches the picture. |
| Menu after a game (risk 2): `toe` and `homm5`, start A1M4, the game's menu (Esc), "Main menu", OK | The same scene is drawn again, the camera flies in again, and the theme restarts (`music file 4`). Only one pick per launch, as decided. (The robot opens the game's menu with the message `EscMenu`, presses its "main menu" with `ui NUI::CEscMenuScreen main_menu` and confirms with the Enter key, `dik 1c`.) |
| `[audio] original_music=1` | With `default`: the ToE theme as before. With `homm5` or `toe`: the scene's theme wins; `original_music`'s line for `a2-MainMenu.xdb` (`[26]`) is absent, so that file is not opened by the menu. |
| The campaign dialog | Not run (no robot way to start `A2C3/M4/S2`). By construction `a2-MainMenu.xdb` is not touched while `scene` is set: no `a2-MainMenu.xdb` line from us in the log; the code logs a line if anything else opens it. |
| Hooked late (a test build whose hook goes in at 4 s, as on a Steam-wrapped exe), `toe` | The scene is still served (its file is read at 4.8 s, when the menu is built); the root table (read at 1.1 s) was too early, so the fallback served `a2-MainMenu.xdb` with the theme, and the log says so. |
| LAN game, host `toe`, client `homm5` | Both joined and played 3 AI turns on A1M4; 516 world checksums equal on both copies (no desync). |
| Cost | Reading 16 paks' directories (129,415 files): 62 to 79 ms, once, at about 1.2 s into the start. No visible delay in the menu. |

Screenshots stay on the test PC (`build\robot\` is not committed).

**Not tested:** a PC without MMH5.5 (plain ToE, where `homm5` falls back and `random` is `toe`): covered by the unit tests of the pick, not run in the game; the Steam-wrapped exe (its late install is run with a test build, see the table); the campaign dialog itself (no robot way to start `A2C3/M4/S2`; the log shows `a2-MainMenu.xdb` is not served by us when `scene` is set).

**Still open:** the Hammers of Fate scene (option b of question 1, "scene packs", if friends find a community menu mod); a settings panel row (the v0.3 settings menu is another branch).

## Short answer

- **Music: all three themes are there** and are real, playable `.ogg` files in `music\`: Heroes V `MainMenu\MainMenu-Theme.ogg` (3:14), Hammers of Fate `a1\MainMenu-Theme.ogg` (2:58), Tribes of the East `a2\Main_Menu.ogg` (2:52). Their "music definition" files are in `data.pak` too. They ship with plain ToE, so any player has them.
- **Scenes: only two of the three exist in this install.**
  - **Tribes of the East** (orcs and goblins storming a Haven town wall): complete, in `data.pak`. Under MMH5.5 it is *hidden*, not removed: MMH5.5 replaces exactly one file (`MainMenu.(ArenaDesc).xdb`).
  - **Heroes V (2006)** (a Haven cleric, a champion on a horse and knights against an arch devil): **only in MMH5.5's `MMH55-Data.pak`**, which is what an MMH5.5 player sees today. Plain ToE does not have it.
  - **Hammers of Fate: the scene is missing everywhere.** Only loose pieces are left over (snow, meteors, arrows and spears, a dwarf "warlord" effect), with no terrain, no models and no camera.
- **Logos:** only ToE's logo (`data.pak`) and MMH5.5's two logos exist. No Heroes V or Hammers of Fate logo.
- **How the game picks:** the scene's path is a fixed text inside the exe; the file at that path is whatever the paks serve. The music comes from one list in `UI/UIGameRoot.(UIGameRoot).xdb` (`MAIN_MENU_MUSIC_ID`). So **we change the content of two files at the file-system hook** (the same trick as `original_music`, `edge_sky`, `languages`).
- **What works first:** `default`, `toe`, `homm5` (on MMH5.5 only), `random` among those two, each with its own music. About **2 sessions**. Hammers of Fate scene needs a decision from the owner (below).
- **`[audio] original_music=1` already changes the menu music** (to ToE's `a2\Main_Menu.ogg`), but only that one theme and it cannot pick Heroes V's or Hammers of Fate's.

## 1. What exists

The game layers its `.pak` files and, for a file name in several of them, **the copy with the newest date inside the zip wins** ([audio.md](../subsystems/audio.md), "Which paks, and which copy of a file wins"). I rebuilt that layering from the paks' directories and followed every reference of the menu scene to see what it needs.

| Scene | What it shows (from the object names) | Where its data is | Complete? | Music |
|---|---|---|---|---|
| **Tribes of the East** (A2) | Orcs, goblins, a cyclops, marksmen, footmen, griffins and peasants storm a Haven wall with towers and stairs; arrows, a fire wall, meteors, dust and blood, a storm. Sky "Rampart_Sky". | `data.pak`, `Arenas/Interface/MainMenu/` (317 files, dated Aug 2007), source scene `/H5A2/UI/MainMenu/MainMenu.mb` | **Yes.** 486 files reached, all in `data.pak` (52 MB of textures), none missing | `a2-MainMenu.xdb` -> `music\a2\Main_Menu.ogg` |
| **Heroes V (2006)** | A cleric, an arch devil, a champion with a horse, knights' and rangers' flags, a helmet and a sword, trees, grass and leaf effects; "Day_light_GrassArena01" sky. | **Only `MMH55-Data.pak`**: 109 files of the same folder, dated 2015-05-17, source scene `/models/MainMenu/M_Menu.mb` (the base game's naming; ToE uses `/H5A2/`). The effects, light and a few leftovers it needs are in `data.pak`. | **Almost.** 271 files reached, 0 missing. One model, `Devil_mesh`, has no geometry data in any pak (MMH5.5 ships it that way, so it cannot be fatal) | `MainMenu.xdb` -> `music\MainMenu\MainMenu-Theme.ogg` |
| **Hammers of Fate** (A1) | Not in the files. Leftovers named `A1_MainMenu`: snow, meteors with charge effects, arrows, spears, a top fire, a Fortress "WarLord" model whose hand glows. | Nothing references them any more (`Effects/_(Effect)/Interface/A1_MainMenu/`, `Effects/_(Effect)/MainMenu/A1/`, `_(Model)/Effects/Towns/Fortrest/MainMenu/`); a few ToE-scene effects reuse some of the particles | **No.** No terrain, no models, no camera animation | `a1-MainMenu.xdb` -> `music\a1\MainMenu-Theme.ogg` |
| **MMH5.5's own** | Not a scene: its menu is the Heroes V one above. It brings a logo ("Might & Magic Heroes V I/II") and a track. | `MMH55-Data.pak` | - | `a2-MainMenu.xdb` is re-pointed at `music\MMH55\MainMenu_Dynasty_002.ogg` (3:38, from the Heroes VI soundtrack); `MP-MainMenu.xdb` at a hero theme |

**What the owner sees today** (checked by the layering, and the same in the Steam copy, `C:\Games\H5_test` and `H5_test_k` = RC19k: the menu files are byte-identical in all three):
- plain ToE 3.1: the ToE scene, ToE's music, ToE's logo;
- with MMH5.5: **the Heroes V scene**, MMH5.5's music, MMH5.5's logo.

So with MMH5.5 "the ToE scene" is the one that is missing from the owner's eyes, and "Heroes V" is the one they already have.

**The Hammers of Fate and Heroes V scenes are not in ToE's data on purpose**: ToE is standalone, and its `data.pak` kept only effects and a few leftovers (a Heroes V `Camera.(AnimSet)` and `Sword` in the arena folder). The `music\` folder, on the other hand, does keep all three themes (they ship in ToE's Steam depot, dated 2021).

**Where they would come from if we ever want them on a PC without MMH5.5 or for Hammers of Fate:**
1. The community. Search results point at two `.h5u` mods for ToE (installed by putting the file in `UserMODS`): one for the original Heroes V menu, one for the Hammers of Fate menu, plus a 2007 Heroes Community thread "Change your main menu background anim and menu design" with both. I could not open those pages (blocked or rate-limited), so their contents, authors and licence are **unchecked**. We cannot ship them (Nival's data) and I did not look further.
2. The original games' own `.pak` files on a PC that owns them. Steam sells Heroes V and Hammers of Fate separately (ToE is standalone and does not need them). The DLL could read the menu files from the player's own copy at run time, as `original_music` reads `data.pak`, but that means serving about 100 files by name from another install, and a setting to say where it is. A bigger step, not needed for the owner's group (they play MMH5.5, which already has the Heroes V scene).
3. Never: shipping any game file, in the repo or in the zip.

## 2. How the game chooses

**Scene.** No data file names it. The path `/Arenas/Interface/MainMenu/MainMenu.(ArenaDesc).xdb#xpointer(/ArenaDesc)` is a text constant in the exe (`0x00e1dc78` in vanilla 3.1; the MMH5.5 exes have it too, at other addresses). The code that builds the menu's 3D scene is `0x00694610`: it resolves that path through the game's lazy database pointer (so through the file system), then creates the scene for the screen `N_MAIN_MENU2_SCREEN_ID`. It is called when the menu screen is built (`0x00694bf0`) and every time the screen is shown again (`0x006947d0`, for example back from a game). The screen's own window file `UI/MainMenu2.(WindowScreen).xdb` does not name the scene. `Roots/roots.xdb` lists the arena only as a "preload" root. **So the content of that one file decides the scene**; it holds the list of the scene's objects (relative file names), the effects, the sky light and the camera animation.

**Music.** `UI/UIGameRoot.(UIGameRoot).xdb` has a list `<musics>`: `MAIN_MENU_MUSIC_ID` -> `Sounds/_(Music)/MainMenu/a2-MainMenu.xdb`, and `MP_MAIN_MENU_MUSIC_ID` -> `.../MP-MainMenu.xdb`, the theme of the multiplayer sub-menu. (Same in data.pak, the 3.1 patch and MMH5.5's `MMH55-Frame.pak`, RC19d and RC19k.) The exe looks them up in `0x006931a0` and `0x006938c0`, called from the screen's "shown" code `0x006947d0`. The `MainMenu.xdb` (Heroes V) and `a1-MainMenu.xdb` (Hammers of Fate) definitions exist but **nothing in the paks of `data\` points at them** (I searched every `.xdb` there under 400 KB, maps included). Two other files use these music definitions: the Hammers of Fate campaign dialog `A1C1/M5/S1` (in the campaigns pack in `UserMODS`) plays `a1-MainMenu.xdb`, and the ToE dialog `A2C3/M4/S2` plays `a2-MainMenu.xdb`. Each definition is a 7-line file: the `.ogg`'s name and the fade times.

**Logo.** `UI/MainMenu2/Logo.(Texture).dds` (+ four small window files). ToE's: 512x256, "Heroes V of Might and Magic, Tribes of the East". MMH5.5's (2022): 1024x512 "Might & Magic Heroes V I/II", plus an older 512x256 `Logo_old`. The logo is a UI picture on top of the scene, independent from it.

## 3. How we would swap it with what the DLL already has

The hook is the one used by `original_music.cpp` (slot 7 of `NVFS::CWinVFS`, `0x00e6651c`, stacked with the others). We call the game's open, look at the name, and give the game a stream with other content. A new `src/mods/menu_scene.cpp` (about the size of `edge_sky.cpp`) would serve **two names**:

| Name served (the game asks for it) | With `toe` | With `homm5` | With `hof` |
|---|---|---|---|
| `Arenas/Interface/MainMenu/MainMenu.(ArenaDesc).xdb` | `data.pak`'s copy (5702 bytes). Read at run time. | the one MMH5.5 serves (nothing to do), not available without MMH5.5 | **not available** (no scene) |
| `Sounds/_(Music)/MainMenu/a2-MainMenu.xdb` | `data.pak`'s `a2-MainMenu.xdb` | `data.pak`'s `MainMenu.xdb` | `data.pak`'s `a1-MainMenu.xdb` |

Why this is enough:
- **The layering does not get in the way.** I followed the ToE scene's 486 files: **exactly one** is also in another pak, the arena description itself (MMH5.5's, dated 2015, beats `data.pak`'s, dated 2007). Everything else the ToE scene uses (models, textures, effects, camera, light) comes from `data.pak` unchanged, so serving that one file brings the whole scene back under MMH5.5. The names of the two scenes' objects do not clash (only `Camera` and `Sword`, which the ToE description does not use).
- **We replace content after the game opened the file**, so which pak "won" does not matter. The reading of `data.pak` can reuse `lang::ReadPakDirectory/ReadPakFile` (`languages.h`, already used by `host_rules_icons.cpp`) and the `WithContent` stream code of `edge_sky.cpp`.
- **`[audio] original_music` does not do this.** With it on, `a2-MainMenu.xdb` becomes ToE's original (`a2\Main_Menu.ogg`) and `MP-MainMenu.xdb` the original MP theme (it is in the log: `[26]`, `[27]`). It never picks Heroes V's or Hammers of Fate's theme and does not touch the scene. With a `[menu]` choice set, ours must win: install our hook after `original_music`'s (outermost), or let `original_music` skip `a2-MainMenu.xdb` when a scene is chosen (a small shared flag). With `scene=default` we install nothing and everything is as today.
- **Music for `hof` is real even though its scene is not.** Playing the Hammers of Fate theme over another scene is possible but would look odd; see the questions.
- **The MP theme** (`MP-MainMenu.xdb`, the multiplayer sub-menu) I would leave alone: only Heroes V has its own MP theme; Hammers of Fate has none.
- **The logo** I would leave alone too (no Heroes V or Hammers of Fate logo exists to show). Serving ToE's logo for `toe` is possible (`Logo.(Texture).xdb/.dds`, 512x256 would be stretched to MMH5.5's 1024x512 box, so a bit soft), but it is a branding choice for the owner.

**Checks before serving** (names only, nothing from the game in the repo): at start-up read the directories of `data.pak` and the MMH5.5 paks (the same cost as `original_music`, about 40-60 ms) and look for a short list of sentinel names per scene (the arena description, two or three of its textures and models). If a scene's sentinels are not all there, it is "not available" (logged), and `random` skips it; an explicit choice that is not available falls back to `default` with a log line. Also check that the `.ogg` exists in `music\`.

**When:** the hook goes in at DLL load, like `original_music` (the menu's music definition is read 7 to 15 s after start, after the splash screen; the owner's logs show `a2-MainMenu.xdb` opened once per run). On the Steam-wrapped exe the early install is not possible and the later one is still in time for the menu, as for `original_music`.

## 4. The setting, the random pick, effort and risks

```ini
[menu]
scene=default    ; default = as installed | toe | homm5 | hof | random
```

- **default**: nothing is installed. As today, as the project rule says.
- **toe / homm5 / hof**: that scene and its music. If this PC cannot show it: `default`, with one log line (`menu scene: hof is not available (no scene files), using the game's menu`).
- **random**: one pick when the DLL loads, uniformly among the scenes **available on this PC**, with that scene's music (and, if the owner wants, logo). Today: plain ToE -> only `toe` (so `random` = `default`); MMH5.5 -> `toe` or `homm5`; `hof` joins if a Hammers of Fate scene ever exists. One log line: `menu scene: random picked toe (available: homm5, toe)`. The scene and the music read the same global, so they cannot disagree. "Never the same twice in a row" would need a small state file; I would not do it first.
- Changing the setting needs a restart (the game builds the scene when it starts). It fits the v0.3 settings panel as one more "pick one" row.
- It is **only a picture and a tune**: no game state, no multiplayer data. Each player can pick on their own, like `original_music`.

**Effort (sessions):**

| Step | Sessions |
|---|---|
| `menu_scene.cpp` + header + `[menu] scene`, `toe` and `homm5` with their music, availability check, `random`, unit tests of the pick and the sentinel check, README / ini text, ordering with `original_music` | **about 2** |
| Robot check: `log_music=1` shows which theme starts, `log_data_files=N` shows the scene's file being opened, plus the owner's eyes (or a screenshot) that the picture matches; one run per scene, then `random` three times | 0.5 (the robot is the other agent's, so it waits for a free test copy) |
| Settings panel row | 0.3 |
| A Hammers of Fate scene, if wanted (owner's choice, see below) | 0.5 to 4 |
| Reading the Heroes V / Hammers of Fate scenes from the player's own installs | 3 to 4 (not recommended now) |

**Risks:**
1. **A scene with a missing file** could give a black menu or an error. Mitigated by the sentinel check; the ToE scene is verified complete, the Heroes V scene is exactly what MMH5.5 ships.
2. **Menu shown again after a game:** the game rebuilds the scene from the same database pointer (`0x006947d0` -> `0x00694610`), so it keeps showing the pick of this launch. If it ever reads the file again, our hook serves the same choice (one global). Not tested; it is on the robot checklist (menu -> map -> back to menu).
3. **Does the game really read the arena description through the file hook?** The music definitions are (checked by the robot). The arena is loaded by the same lazy database pointer, but this is **not tested**: first thing to try, with `log_data_files`.
4. **Heavier menu:** the ToE scene is 52 MB of textures against 14 MB for the Heroes V one. A slightly longer first menu load under MMH5.5 with `toe` (to measure).
5. **Other users of the music files:** serving `a2-MainMenu.xdb` also changes the music of the one ToE campaign dialog that uses it (`A2C3/M4/S2`). Harmless, and MMH5.5 already changes it. (Changing `UIGameRoot` instead would avoid it, but it is a 333 KB file that the game reads at start-up, so I would not.) `a1-MainMenu.xdb` itself must never be rewritten: a Hammers of Fate campaign dialog uses it.
6. **`MP-MainMenu`** keeps whatever it plays today (MMH5.5's hero theme, or the original with `original_music=1`).
7. **A future MMH5.5** may change these files; everything is decided at run time from the paks, and unknown situations fall back to `default`.
8. **Wine/Proton:** nothing special, it is file serving. File names are matched in lower case, as everywhere else in the DLL.
9. **Which exe:** the scene's path and the music IDs are in every exe I looked at (vanilla 3.1, Steam's `H5_Game.exe`, `MMH55.exe`, `MMH55_Utility_64.exe`). The hook only goes in on the builds the DLL already knows (vanilla 3.1 and `MMH55_Utility(_64).exe`, `game_build.h`), the same limit as every other mod, and it needs no new address: it uses the file-system slot the other mods use.

## Questions for the owner

1. **Hammers of Fate scene.** It is not in any file we have. Options: (a) leave it out for now: `hof` stays "not available" and `random` is Tribes of the East / Heroes V (the music of Hammers of Fate is then unused); (b) allow "scene packs": any pak the player adds (for example a community Hammers of Fate menu mod) that provides an arena description with a name of ours is picked up as one more scene; (c) build an approximate one from the leftover snow, meteor and warlord effects (needs your sketches or screenshots of the original, and several sessions). I suggest (a), then (b) if friends find a pack.
2. **Heroes V on a PC without MMH5.5:** accept that it is unavailable (all your friends use MMH5.5)?
3. **Logo:** keep MMH5.5's logo with every scene (my suggestion), or show ToE's logo with the Tribes of the East scene?
4. **Music:** always the scene's own theme, with `random` picking both together (my suggestion)? And leave the multiplayer sub-menu's theme alone?
5. **Wording of the setting:** `scene=default|toe|homm5|hof|random` fine, with `homm5` = "Heroes V (2006)" in the panel?
6. **One pick per launch** is enough (not a new one each time you come back to the menu)?

## What I could not verify

- What the scenes look like on screen: the descriptions come from the object and effect names, not from a screenshot. The Heroes V and Hammers of Fate pictures in particular are inferred (Haven and Inferno units; a snowy dwarf battle from the effect names).
- That MMH5.5 took its menu from the original 2006 game (consistent with the `/models/` source name, the Haven and Inferno objects and the 2015 date; no text source says so).
- The community mods and the Heroes Community thread: seen only as search-result summaries.
- Whether the arena description is read through the file hook (risk 3) and what happens after returning from a game (risk 2).

## Sources

- The paks and music in the Steam copy (read only); `C:\Games\H5_test_k` for RC19k's menu files; `UserMODS\All_campaigns.data.h5u` for the dialog that uses `a1-MainMenu.xdb`.
- Vanilla 3.1 exe in Ghidra: `0x00694610`, `0x006947d0`, `0x00694bf0`, `0x006931a0`, `0x006938c0`.
- [Heroes V wiki, "Modifying Music"](https://heroes5.fandom.com/wiki/Modifying_Music) (search summary: `a1-MainMenu.xdb` for Hammers of Fate, `a2-MainMenu.xdb` for ToE); a Bilibili soundtrack list that has three "Main Menu Theme" versions (Heroes V, Hammers of Fate, Tribes of the East); [Nexus: "HOMM V TOTE (Original Main Menu Theme)"](https://www.nexusmods.com/heroesofmightandmagicv/mods/4); [Heroes Community thread 24393](http://heroescommunity.com/viewthread.php3?TID=24393); a Russian Steam guide to ToE mods ([id 3638251859](https://steamcommunity.com/sharedfiles/filedetails/?id=3638251859)) that names `ClassicMenu_c` and `HOFMenu`. All through search summaries only.

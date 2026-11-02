# Robot: unattended game runs

`run_ai_test.py` plays the game on its own in a **test copy** of the game and reports AI timings. No clicking, no window focus needed (the game runs in a window in the background).

```
powershell -File src\hook\build.ps1                  # build the hook DLL first
python tools\robot\run_ai_test.py                     # defaults: map A1M4, 8 AI turns
python tools\robot\run_ai_test.py --turns 12 --setvar "ai_time_limit = 20000"
python tools\robot\run_ai_test.py --restore-intro     # put the intro videos back in the test copy
```

Results go to `build/robot/<date-time>/` (gitignored): `summary.json`, the DLL log `h5-together.log`, the game's own console `console.txt`, screenshots (`frame-*.png`, the game's own frames saved by the DLL), and the crash reports the copy wrote, if it crashed (`h5-together-crash-*.txt` / `.dmp`, moved out of the test copy).

`run_lan_test.py` runs **two copies of the game at once** on this PC, a LAN host and a LAN client (see "LAN games" below):

```
python tools\robot\run_lan_test.py                      # host creates a game on A1M4, client joins
python tools\robot\run_lan_test.py --client-step "27|peek|NUI::CLobbyScreen 1dc 8"
python tools\robot\run_lan_test.py --seconds 400 --turns 6 --trace-mp      # time 6 AI turns, log the lockstep
python tools\robot\run_lan_test.py --seconds 400 --turns 6 --net-tuning off  # the game's own network timings
python tools\robot\lan_report.py --last 2                                   # compare the last two LAN runs
```

Results go to `build/robot/<date-time>-lan/host/` and `.../client/`. The host creates a game on the chosen map, the client joins, both press "ready", the host sets the third slot to a computer player and starts; then the robot ends both humans' turns and the DLL times every AI step on both copies.

Both copies run the LAN link tuning mod (`src/mods/README.md`) unless `--net-tuning off`; the runner writes its settings to `bin/h5-together-<host|client>.ini` and removes them afterwards.

## Checking the AI and its speed-ups (saves, battles, virtual clock)

```
python tools\robot\run_ai_test.py --save-file <save> --load-save save_11 --quick-combat --virtual-clock 50 --turns 4 --trace-paths
python tools\robot\path_check.py save_11 save_10 --turns 4 --ai-setting port_search=1 --robot-arg=--quick-combat --robot-arg=--virtual-clock=50
python tools\robot\path_report.py build\robot\<plain run> build\robot\<mod run>
```

- `--load-save` / `--save-file`: start from one of the owner's saves (copied into the robot's own profile).
- **Battles:** when the AI attacks the robot's (passive) human player, the robot starts the battle (`input_start_combat`), turns on auto-combat (`input_Combat_AutoCombat`) and answers the retreat box (flee). With `--quick-combat` (`adventure_quick_combat = 1`) battles resolve at once. A run that still cannot go on closes the game and reports `stuck` instead of idling until its time limit.
- `--virtual-clock [ms]`: the game's main thread sees a clock that advances by a fixed step per frame (and by the time slept), not with real time. The AI's decisions depend on how fast it runs (`docs/subsystems/ai-pathfinder.md`); with this clock a save replays the same however fast the code is, so a plain run and a mod run can be compared over whole turns. 50 ms per frame also makes animations and hero walks pass faster.
- `--trace-paths` / `--trace-paths-all`: fingerprints of the AI's path searches (big ones / all), compared by `path_report.py`; `path_check.py` does plain, plain again (reproducible?) and mod for several saves.
- `--fast-paths on`, `--ai-setting key=value`: the pathfinding mod and its parts (`src/mods/README.md`).
- `--profile-ai`: where the AI's thinking time goes (`profile_report.py`; samples in our DLL can be mapped with `build/hook/dinput8.map`).

Some saves do not replay the same even with the virtual clock (sauvegarde_14): `path_check.py` reports and skips them.

## Checking the music (the "original music" mod)

```
python tools\robot\run_ai_test.py --turns 1 --quick-combat --audio-setting log_music=1                            # MMH5.5's music
python tools\robot\run_ai_test.py --turns 1 --quick-combat --audio-setting original_music=1 --audio-setting log_music=1
python tools\robot\run_ai_test.py --no-map --seconds 20 --step "8|console|credits" --audio-setting log_music=1       # the credits
python tools\robot\run_ai_test.py --no-map --seconds 150 --turns 1 --quick-combat --step "8|console|rmg RMG/Templates/2P-Duel-T.xdb -players 2 start" --audio-setting log_music=1
python tools\robot\run_lan_test.py --turns 3 --host-audio original_music=1 --client-audio log_music=1
```

- `--audio-setting key=value` writes `[audio]` settings (`src/mods/README.md`); `run_lan_test.py` has `--host-audio` / `--client-audio`.
- `log_music=1`: the DLL logs every music file the game starts (`music file 2: ..\Music\AdvMap-Terrain\Terrain-Grass.ogg`). Music still streams with the robot's volume 0, so nothing is heard.
- The `rmg` console command makes and starts a random map (template path without a leading `/`). Each run gives other factions, so the hero starts on other terrains (sand, dirt, snow, ...); the map is written to the test copy's `data\RMGTemp`. The test maps (A1M4, A2M1, ...) all start on grass.

## Checking autosaves (the autosave mod)

```
python tools\robot\run_lan_test.py --seconds 400 --turns 6 --autosave                          # the mod's defaults
python tools\robot\run_lan_test.py --seconds 400 --turns 6 --autosave --save-setting compression=-1   # the game's zlib level
python tools\robot\run_ai_test.py --load-save "Sauvegarde auto" --turns 2 --quick-combat         # load an autosave
```

- `--autosave`: the host ticks the setup screen's autosave option (the checkbox named `realtime`), so every PC autosaves at the start of each day. Without it, multiplayer games never autosave (the console variable `autosave` only counts in single player).
- `--save-setting KEY=VALUE` (both copies), `--host-save` / `--client-save`: `[save]` settings (`src/mods/README.md`). `run_ai_test.py` has `--save-setting` too.
- The trace lines start with `autosave:` in both logs. Robot autosaves go to `<test copy>/robot_home-<host|client>/.../Profiles/Robot/Saves/` and stay there between runs (3 files, "Sauvegarde auto 1..3"). With `fast_save_list=0`, the time of each autosave grows with the multiplayer saves in that folder (docs/subsystems/save.md). Saves copied there whose name is an autosave's ("Sauvegarde auto 2") get overwritten: the game reuses the file of the save with the same name.
- Multiplayer saves do not show in the single-player load list, so the load check uses a single-player autosave (`run_ai_test.py` makes one each day: the robot profile has `autosave = 1`).

## Checking the "your turn" notification (the notify mod)

```
python tools\robot\run_lan_test.py --seconds 300 --turns 4 --notify-setting trace=1
python tools\robot\run_lan_test.py --seconds 300 --turns 4 --notify-setting sound=1 --notify-setting popup=1 --host-step "16|option|simultaneus 0"
```

- The robot's windows are in the background, so each new turn of a copy's player gives a `notify: your turn (day N): ...` line (none on the first turn).
- Robot runs are quiet by default (`[notify] sound=0 popup=0`: no sound, no pop-up on the desktop; the taskbar flash and the log lines stay). `--notify-setting KEY=VALUE` (both copies; `run_ai_test.py` has it too) changes that; `trace=1` logs every change of the players' turn states.
- `--host-step "16|option|simultaneus 0"` ticks the setup screen's simultaneous turns option.
- The client's robot ends its player's turn every few seconds without knowing whose turn it is. When that end-of-turn command runs in the same frame as the turn starts, the turn is over before anyone could see it, and there is (rightly) no notification for it.

## Checking the player list (the overlay)

```
python tools\robot\run_lan_test.py --seconds 240 --turns 4 --simultaneous --shot-every 6 --ui-setting trace_players=1
python tools\robot\run_lan_test.py --seconds 240 --turns 4 --host-rmg teams=humans_vs_ai --shot-every 6 --ui-setting trace_players=1
```

- The screenshots (`frame-*.png`) are taken after the overlay has drawn, so they show the panel as a player sees it.
- `--simultaneous`: the host ticks the setup screen's simultaneous turns option (`option|simultaneus 0`).
- `--ui-setting KEY=VALUE` (both copies; `--host-ui` / `--client-ui` for one; `run_ai_test.py` has `--ui-setting` too): `[ui]` settings (`src/mods/README.md`). `trace_players=1` logs each change of a player's status (`players: day 3 (simultaneous turns): player 2 "Robot-client" (human, colour 2, team 0) hero "Anwen": turn ended (turn=1 ended=1)`); `trace_players=2` also dumps the game's raw player and hero data once (research).
- `--host-rmg teams=humans_vs_ai` puts the two humans on one team (A1M4 has no teams of its own), to see the team buttons.
- **A random map in a LAN game:** pass part of its path, e.g. `--map "EDD31DF8-A828-4414-A615-E8AC32ACA9C0/"` (a saved random map in the test copy's `Maps`; the trailing `/` keeps rebuilt copies out). The client rebuilds the map when it joins, which takes a few seconds, so give it `--client-ready-at 56 --start-at 70`, and set the map's other start slots to computer with `--host-step "38|arrow|3 right"`. To pick a faction, use `"50|arrow|1 race right"` (before "ready"). Example (`src/mods/rmg_factions.cpp`): `--host-rmg faction_terrain=1 --host-rmg trace_factions=1 --client-rmg trace_factions=1`.
- `--resolution WxH` (both runners; default 1024x768) sets the game windows' size, e.g. `1920x1080` or `2560x1080`, to see how the list scales next to the game's own interface.
- `--client-ui player_list_test_rows=8` makes the client show 8 rows (copies of the real players, in 4 teams of 2: the tallest list), to check that it still fits above the chat box and the minimap.
- Not possible with the robot: F9 and the fade under the mouse (the windows are in the background).

## Checking the camera zoom

```
python tools\robot\run_ai_test.py --turns 1 --quick-combat --shot-every 5 --step "3|console|mission_camera_def_pos 50 -53 16 200"
python tools\robot\run_ai_test.py --turns 1 --quick-combat --shot-every 5 --step "3|console|mission_camera_def_pos 50 -53 16 200" --camera-setting zoom_out=150
python tools\robot\run_lan_test.py --seconds 300 --turns 4 --host-camera zoom_out=200 --host-camera combat_zoom_out=200
```

- The robot cannot turn the mouse wheel (its windows are in the background), so the first two runs move the camera's start position (`mission_camera_def_pos <field of view> <tilt> <turn> <distance>`, before the map starts) far out. The game then puts the camera at the farthest distance it may go: the game's 70, or 105 with `zoom_out=150` (compare the screenshots of day 2: same place, the minimap's view frame grows). Results: [docs/subsystems/camera.md](../../docs/subsystems/camera.md), "Robot checks".
- Battles: without `--quick-combat`, a long run (`--turns 24 --seconds 480`) usually gets one battle where the AI attacks the robot's hero; with `--camera-setting combat_zoom_out=150` the log then has `camera: battle: farthest zoom 56 -> 84`.
- `--camera-setting KEY=VALUE` (both runners; `--host-camera` / `--client-camera` for one copy of a LAN game): `[camera]` settings (`src/mods/README.md`). The log lists each limit the mod changed (`camera: adventure map, normal camera: farthest zoom 70 -> 105`).
- `--setvar "adv_camera_mode = 1"` uses the "vertically fixed" camera mode of the options screen.
- The LAN run gives only the host the farther camera: the world checksums must stay equal (2026-10-25: 698, all equal). Add `--host-step "3|console|mission_camera_def_pos 50 -53 16 200"` to have the host's camera really far out during the game.

## Checking random maps from recipe templates (in-game builder, steps 2 and 3)

```
python tools\robot\run_ai_test.py --no-map --seconds 200 --turns 1 --quick-combat --rmg-setting templates_selftest=1 --step "8|console|rmg RMG/Templates/OH-X1-22001gnnxxxxxxxM.xdb -players 4 -size 2 -underground 0 start"
python tools\robot\save_rmg_map.py OH-X1-robot-2v2              # prints the map's guid
python tools\robot\run_lan_test.py --map <guid> --seconds 220 --turns 2 --host-step "38|arrow|3 right"
python tools\robot\run_lan_test.py --map <guid> --seconds 150 --turns 1 --host-rmg lobby_teams=1 --host-step "38|arrow|3 right" --host-step "40|arrow|2 team left"
python tools\robot\run_lan_test.py --map <guid> --seconds 100 --turns 1 --client-rmg templates=0 --host-step "38|arrow|3 right"
python tools\robot\save_rmg_map.py --remove OH-X1-robot-2v2
```

- The console's `rmg` asks for the template by its name: no file is needed for a recipe (`OH-X1-...`). The DLL log says `... built from its name`.
- `save_rmg_map.py` packs the generated map the way the generator screen's Save does, so the LAN host can pick it. The map has 4 players: `38|arrow|3 right` makes the fourth one a computer player.
- Look for `rmg join:` and `rmg teams:` lines in both logs; `lan_report.py` checks the checksums.
- Not possible yet: opening the generator screen. Its button sends `rmg_screen` as a `UISSendGameMessage`, and the robot's `msg|rmg_screen` is not taken (by the single-player or the multiplayer screen).
## Checking the template builder (a modal panel of the overlay)

Two runs: the builder, Use, then the game's Create and Save; then the saved map is started.

```
python tools\robot\run_ai_test.py --no-map --seconds 95 --shot-every 0 --step "8|menu|single custom_game" --step "13|rmg|" --step "20|shot|rmg-screen" --step "22|ui|NUI::CRMGScreen oh_builder" --step "26|shot|builder-filled" --step "28|panel|preset 1" --step "29|panel|distance on" --step "31|shot|builder-preset" --step "32|panel|scroll end" --step "34|shot|builder-scrolled" --step "36|panel|use" --step "39|shot|after-use" --step "41|ui|NUI::CRMGScreen create" --step "70|shot|created" --step "72|ui|NUI::CRMGScreen save" --step "77|shot|saved"
python tools\robot\run_ai_test.py --no-map --seconds 170 --turns 1 --quick-combat --shot-every 0 --step "8|menu|single custom_game" --step "13|rmg|" --step "18|key|F11" --step "22|shot|builder-filled" --step "24|panel|cancel" --step "26|ui|NUI::CRMGScreen back" --step "30|console|advmap /Maps/RMG/<RMGguid>/map.xdb#xpointer(/AdvMapDesc) 0 1 1"
```

- `rmg|` opens the random map screen from the custom game screen (`msg|rmg_screen` does not: see the step table).
- `ui|NUI::CRMGScreen oh_builder` sends what our "Template..." button sends: the panel opens as from a click. `key|F11` opens it too.
- `panel|...` gives the open panel what a click would (`rmg_builder.cpp`, `RobotCommand`): `preset <number in the list, recipe name or part of a ready name>`, `distance on|off`, `scroll <units>|end`, `use`, `save`, `cancel`, `open`.
- `ui|NUI::CRMGScreen create` and `... save` press the game's own Create and Save. The map is saved as `Maps\<the screen's name>.h5m`; its `RMGguid` (inside: `Maps/RMG/<guid>/map.xdb`) starts it in the second run. Remove the saved map afterwards: `python tools\robot\save_rmg_map.py --remove <name without .h5m>`.
- `--resolution 1920x1080` checks the panel at another size.
- What to look for in the log: `rmg screen: the random map screen gets our "Template..." button`, `rmg screen: "oh_builder"`, `rmg builder: Use OH-X1-...: size ..., "ARMG Templates" ticked, OH-X1-... chosen (n of m that fit)`, then at Create and when the saved map starts `rmg teams: setup ... is for a random map made from /RMG/Templates/OH-X1-....xdb`.
- Checked 2026-10-25 at 1024×768 and 1920×1080 on the French test copy: every line above, our template chosen 14th of the 19 that fit, the map built from it, saved and started, and the computer players took their turns.
- Not possible with the robot: clicks on the panel (the drop-down lists, the arrows, the boxes, the scroll bar), Esc, and checking that the game behind gets no input (the robot's window is in the background). That is checked by hand, with the test panel's help (`[ui] overlay_test_panel=1`, `src/mods/README.md`).

## Checking the hero looks (`src/mods/hero_skins.cpp`)

```
python tools\robot\make_skins_file.py build\skins-a.ini
python tools\robot\make_skins_file.py build\skins-b.ini --look TOWN_HEAVEN=Duncan --look TOWN_NECROMANCY=Arantir
python tools\robot\run_ai_test.py --turns 24 --seconds 420 --shot-every 20 --step "3|console|mission_camera_def_pos 50 -40 16 12" --skins-file build\skins-a.ini --skins-setting trace=1
python tools\robot\run_lan_test.py --seconds 270 --turns 5 --host-skins-file build\skins-a.ini --client-skins-file build\skins-b.ini --skins-setting trace=1
python tools\robot\run_lan_test.py --seconds 200 --turns 3 --host-skins-file build\skins-a.ini --client-skins skins=0
```

- `make_skins_file.py` gives every hero of a faction one look, so whatever heroes A1M4 deals out, the robot's players show it. The camera position step brings the camera close to the hero.
- What to look for: `hero skins: ...: the map figure is rebuilt with ...'s look` (single player), `looks on for this game` and the same rebuilds at the same moment on both copies (LAN), `no looks in this game: PC 1 sent none in time` (LAN, client off), and `lan_report.py`: checksums all equal. `trace=1` also lists who calls the hero's figure slots.
- **The live hero screen** (2026-11-01): `python tools\robot\run_ai_test.py --turns 100 --seconds 104 --end-turn-every 600 --shot-every 0 --step "27|panel|skins hero_screen" --step "30|shot|hs-0" --step "31|panel|skins open" --step "32|panel|skins pick 1" --step "33|panel|skins apply" --step "36|shot|hs-1"` (and so on). Look for `the hero screen closes and opens again for ...` and `the hero screen is open again after 500 ms (...): <hero> with <look>'s look, battle model ...`.
- **The panel** (F7): `--step "18|panel|skins open"`, then `"21|panel|skins pick <n or InternalName>"` (n = the tile, 0 = the hero's own look), `"23|panel|skins apply"` or `"skins cancel"`; `"20|shot|panel"` for screenshots. In a LAN run use `--host-step` (after segment 20, about 70 s into the run). Commands that start with `skins ` only go to this panel (`robot::TakePanelCommand` with a prefix); the builder never takes them.
- The hero screen's Look button: `--skins-setting test_button_on_map=1` draws it on the adventure map too, at the same place, so a screenshot shows its look and size (`--resolution 1348x750` is the owner's window). Its place on the real hero screen is checked by hand.
- **The hero screen:** `panel|skins hero_screen` opens it for the selected hero, on page 1 ("Army & Stats"). It calls the adventure screen's own handler of the game message `hero_attributes` (`0x00599b30` → `0x00599ab0`), at the next adventure frame. The hero screen opens only during this PC's own turn: in `run_ai_test.py` the robot ends the first turn at once, so use a step after "your turn (day 2)" (about 25 s) with `--end-turn-every 600`; in a LAN run, `--client-step` (the host ends its turn first). `panel|skins town 0` selects the first town and shows its screen (`input_select_town_01`, `adv_show_town`); `panel|skins town_hero_screen` opens the hero screen from the town screen (its `hero_attributes`, `0x006194b0`; it needs a hero in the town). `dik|01` (Escape) closes the hero screen. `msg|hero_attributes` and `ui|NUI::CAdventureScreen hero_attributes` do nothing (those screens' observers drop a message posted without a sender, and it is not a button reaction); `ui|NUI::CAdventureScreen hero_dbl_click` crashes, caught (it needs the clicked list item).

## Checking the host's game rules (a modal panel; `src/mods/host_rules*.cpp`)

```
python tools\robot\run_lan_test.py --game C:\Games\H5_test_k --seconds 220 --turns 4 --start-at 48 --host-rules governance=0 --host-rules ai_cheat=0 --host-rules mixed_stacks=0 --host-step "39|key|F2" --host-step "40|shot|host-rules-panel" --host-step "42|panel|rules cancel" --client-step "40|key|F2" --client-step "41|shot|client-rules-panel" --client-step "43|panel|rules close"
python tools\robot\run_lan_test.py --client-rules enabled=0 --host-rules mixed_stacks=0          # a client without the rules: the files' rules for everyone
python tools\robot\run_lan_test.py --game C:\Games\H5_test_k --seconds 150 --turns 2 --start-at 48 --host-rules game_mode=1 --host-rules toe_bonuses=1 --rules-setting trace=1 --host-step "30|arrow|0 bonus right" --host-step "31|arrow|0 bonus right" --client-step "33|arrow|1 bonus right" --client-step "34|arrow|1 bonus right" --client-step "35|arrow|1 bonus right"   # ToE start bonuses: host gold, client resources, mode Default
python tools\robot\lan_report.py --last 1
```

- **Game mode and ToE start bonuses** (step 2): with `trace=1` the console gets one line per player and copy, `H5 Together: start of player 1 (computer 0): gold 20900 wood 20 ... ; minor artifacts: ...; game mode 1`, after MMH5.5's `H55 Players chose Default mode`. The DLL logs each lobby row's bonus (`rules: the lobby's row 0: start bonus 2 (gold)`) and the served start script (`rules: the scripts read scripts/H55-AdvMap.lua: served with game mode Default, ... hash ...`, the same on both copies).

- `--host-rules KEY=VALUE` / `--client-rules` / `--rules-setting` (both): `[rules]` settings. The host's choices are keys such as `governance=0`; `enabled=0` makes a copy behave as a PC without the rules; `trace=1` logs every script file the game opens. `run_ai_test.py` has `--rules-setting`.
- `key|F2` opens the panel (setup screen, waiting room, adventure map); `panel|rules <command>` gives it what a click would: `open`, `set <key> <value>`, `file` (MMH5.5's values), `ok`, `cancel` / `close`, `scroll <units>|end`. Commands that start with `rules ` go to this panel only (`robot::TakePanelCommand` with a prefix).
- In the logs look for `rules: hello`, `rules: the host's rules (lobby)`, `rules: START`, `rules: the host's start message` and `rules: the scripts read scripts/H55-Settings.lua: served ... hash`: the hash must be the same on both copies.
- What MMH5.5's scripts did is in the shared `console.txt` (both copies write to it): e.g. `H55 Mixing 36 neutral stacks!` once per copy with mixed stacks on, none with them off; `H55 Towns prepared for Competitive ARMG Map!` on random maps that run the scripts.
- A random map straight from the generator has no map script: in a LAN game MMH5.5's scripts run on it only with `map_scripts=1` (the default with RC19k, not RC19d). Or use a saved map with one, such as the owner's `MAP_JoacJoz` (`--map 81CD0EF2-7D11-41D7-AE71-0A74BF0872A0/`, 4 players: `--host-step "38|arrow|3 right"`).
- **`map_scripts` on a generated random map** (the copy's `MMH55_RMG_Map_002`, 4 players; for RC19k use a map made by RC19k, see "Checking random maps from recipe templates" with `--game C:\Games\H5_test_k`):
  ```
  python tools\robot\run_lan_test.py --map "EDD31DF8-A828-4414-A615-E8AC32ACA9C0/" --client-ready-at 56 --start-at 70 --host-step "38|arrow|3 right" --seconds 220 --turns 3 --host-rules map_scripts=1 --host-rules mixed_stacks=0 --rules-setting trace=1
  ```
  Look for `rules: the world starts its scripts: ... map_scripts=1: it gets an empty one` in both logs, `the game opens scripts/advmap-startup.lua` (trace), every `H55 ...` start-up line twice in `console.txt`, the same served hash, and equal checksums. `map_scripts=0`: `... no scripts, as in the original game`, no `H55` line. `--client-rules enabled=0`: `no host's rules in this game: MMH5.5's own way`.
- **The class skill lock** (`class_skills`): `--host-rules class_skills=1` (or in the waiting room `--host-step "24|panel|rules open" --host-step "25|panel|rules set class_skills 1" --host-step "27|panel|rules ok"`; on the setup screen the default `17|msg|create` comes too soon), `--rules-setting trace_skills=1` (one line per new skill a level-up offers: class, skill, on the class's MMH5.5 list or `NOT`), `--host-rules class_chance=30` to see the other skills often (testing). Look for `rules: class skills: UNLOCKED for this game ... table hash` (the same on both copies). The level-ups are the AI's own (a few per run: 12 AI turns gave 3 to 20); the host's game computes some offers twice as often as the client's, with the same result, so compare the skills, not the counts. **Saves:** `run_ai_test.py --rules-setting class_skills_single=1 --rules-setting class_skills=1` makes an unlocked single-player game and its daily autosave (`a save of the world is written with "OHW1;..."`); `--load-save "Sauvegarde auto N"` then shows `a saved game is read: its rules` and `UNLOCKED` (the autosave's number: the robot's saves rotate).

## Checking our texts in another language (`src/hook/texts.cpp`, `src/mods/texts/*.txt`)

Both runners copy `src/mods/texts/*.txt` into the test copy's `bin\h5-together\texts\` with the DLL, as the installer does. `--ui-setting language=en` (or `fr`, `xx`...) picks the panels' language; without it the DLL finds it by itself (a test copy outside Steam's folders has no Steam file for the game, so the red player's name in the game's texts decides: French on the French test copies). `--texts-dir <folder>` installs other files instead: a partial or broken translation, or the long test language (`python tools\lang\ui_texts.py long`, then `--texts-dir build\texts --ui-setting language=xx`).

```
python tools\robot\run_ai_test.py --game C:\Games\H5_test_k --ui-setting language=en --no-map --seconds 38 --shot-every 0 --step "8|menu|single custom_game" --step "13|rmg|" --step "18|shot|rmg-screen" --step "20|ui|NUI::CRMGScreen oh_builder" --step "24|shot|builder" --step "26|panel|preset 1" --step "29|shot|builder-preset" --step "30|panel|scroll end" --step "32|shot|builder-scrolled"
python tools\robot\run_ai_test.py --game C:\Games\H5_test_k --texts-dir <folder with a partial fr.txt> --ui-setting language=fr --turns 100 --seconds 52 --end-turn-every 600 --shot-every 0 --step "30|panel|skins open" --step "33|shot|looks" --step "40|key|F2" --step "43|shot|rules-map"
```

Look for the `texts:` lines in the log (written when a panel first asks for a word): the language, which clue chose it, and each line of the file that was not used.

## Checking the safety nets (crash reports, versions in the lobby, update check, chain-loading)

```
python tools\robot\run_ai_test.py --game C:\Games\H5_test_k --debug-setting crash_test_at=30 --seconds 80 --turns 100 --quick-combat --shot-every 0
python tools\robot\run_lan_test.py --game C:\Games\H5_test_k --seconds 100 --turns 2 --start-at 58 --client-rules test_version=0.2.9 --host-step "52|shot|host-versions" --client-step "53|shot|client-versions"
python tools\robot\run_ai_test.py --game C:\Games\H5_test_k --no-map --seconds 20 --shot-every 0 --setting update.check=1 --setting update.last_check=<seconds since 1970, now> --setting update.latest=0.9.0 --step "12|shot|menu-notice"
```

- **Crash:** `crash_test_at=<s>` crashes the main thread on purpose that many seconds after its main loop starts. The run ends "other (crash or closed)" with exit code 3221225477 (`c0000005`), and the results hold the report and its minidump. The runner deletes the log before each run, so the next run's "the last run crashed" line cannot show here (`test/crash_test.cpp` checks it).
- **Versions in the lobby:** both copies run the same DLL, so `[rules] test_version=0.2.9` makes one of them say another version, and `test_version=old` none (as a PC before 0.3.0); `--client-rules enabled=0` is a client without the rules (shown as "no H5 Together"). A client's "the host has an older H5 Together" comes 15 s after it joined: start later (`--start-at 60`). Look for `rules: versions:` and `rules panel: versions line` in both logs.
- **Update check:** `--setting update.check=1` alone makes one real request to GitHub (keep it to one now and then). With `last_check=` now and a newer `latest=`, the notice shows without a request. The runner restores the player's own `h5-together.ini` afterwards, so what the DLL wrote there goes.
- **Chain-loading:** copy `build\hook\chain_dummy.dll` (made by `build.ps1 -Test`) to the test copy's `bin\dinput8_next.dll`; the log says `chained: dinput8_next.dll ...` and `bin\chain_dummy.txt` says `loaded`, `called`. Remove both files afterwards.
- `--setting SECTION.KEY=VALUE` (`run_lan_test.py`: `--setting`, `--host-setting`, `--client-setting`) writes a setting of any section the runner has no option for (`[update]`, `[chain]`); not for a section it writes itself (the DLL reads only the first section of a name).

## Robot script

Besides starting a map, the DLL runs a script of timed steps (`--step`, or `--host-step` / `--client-step`), `"<seconds>|<kind>|<text>"`, seconds counted from the first main-loop call:

| Kind | Text | What it does |
|---|---|---|
| `console` | a console command | runs it like the game's console |
| `menu` | `<page> <command>` | a main-menu button: pages `main`, `single`, `multi`, `hotseat`, `mission`, `campaign`, `replay` (e.g. `multi lan`); `robot_menu.cpp` |
| `ui` | `<screen class> <command>` | a button of any screen that takes commands (`IProgrammedReactionsAndChecks`), e.g. `NUI::CLobbyScreen create`; the screen is found through the game's RTTI and a memory scan (`robot_ui.cpp`) |
| `msg` | `<name> [number]` | posts a "game message", like buttons with an `ARSendGameMessage` reaction: e.g. `create`, `load`, `back` on the multiplayer setup screen, `start` in the waiting room (names from `UI/<screen>/*.(ARSendGameMessage).xdb` in the game data) |
| `map` | part of a map file path | highlights that map on the multiplayer setup screen, e.g. `A1M4/` (`robot_lobby.cpp`) |
| `save` | part of a save's name | highlights that save on the single-player load screen (`menu|single load_game`) and selects it; then `msg|load` loads it (`run_ai_test.py --save-file <path> --load-save <name>` does it all) |
| `option` | `<checkbox> <state>` | a checkbox of the multiplayer setup screen: `quick_combat`, `ghostmode`, `realtime` (it is the autosave option), `simultaneus` (simultaneous turns); state 0 = checked (`run_lan_test.py` turns quick combat on, so battles resolve by themselves) |
| `game` | a number | highlights that game (0 = first) in the LAN lobby's list, before `ui|NUI::CLobbyScreen join` |
| `arrow` | `<row> left\|right` | presses a player row's state arrow in the multiplayer waiting room: on your own row it toggles "ready", on an empty slot it cycles open → computer → closed; `<row> team left\|right`, `<row> race left\|right`, `<row> hero left\|right` and `<row> bonus left\|right` press the team, faction, hero and start bonus arrows (bonuses: 0 random, 1 minor artifact, 2 gold, 3 resources; a new row starts on random); `<row> ready` presses your own row's arrow only while it is "not ready" (safe to repeat, e.g. while a client rebuilds a random map) |
| `slots` | (none) | research: logs the waiting room's rows (state, player) and whether the host can start |
| `mem` | hex addresses | research: logs the 32-bit values there (e.g. `f27398` = `net_batched_segments`) |
| `shot` | a label | saves the next frame as `frame-NNN-<label>.png` |
| `rmg` | (none) | opens the random map screen from the custom game screen (`menu|single custom_game` first). Its button posts the game message `rmg_screen`, but the screen's observer (`0x0077b090`) drops a message without the button as sender, so `msg|rmg_screen` does nothing; the step calls the observer's handler `0x0077a6c0` on the live `NUI::CHSCreate` instead (`robot_ui.cpp`) |
| `key` | `F1`..`F12` | presses one of **our overlay's** keys (the DLL's own key handling sees it; the game does not): e.g. `F11` opens the template builder on the random map screen, `F9` is not covered (the player list reads its key itself) |
| `dik` | `<hex DIK code> [hold ms]` | presses a **game** key through the robot's DirectInput keyboard (`robot_input.cpp`), held 150 ms or the given time: e.g. `d1 1500` = Page Down (zoom out) for 1.5 s, `c9 1500` = Page Up |
| `wheel` | `<notches> [steps]` | turns the mouse wheel: that many notches (+ up = zoom in, - down = zoom out), one per read of the game's mouse events, as positions (the running sum, as Windows gives them) or with `steps` as +-120 steps (as Wine gives them, to check `[camera] wheel_fix`) |
| `panel` | a command | for our open panel, what a click would do (the robot cannot click): the template builder takes `preset <n>`, `distance on\|off`, `scroll <units>\|end`, `use`, `save`, `cancel` and `open` (`rmg_builder.cpp`) |
| `peek` | `<screen class> <hex offset> ...` | research: logs the objects along a pointer chain from a screen, with their class names |

When the game hangs, the DLL logs where: if the main loop stops calling `PeekMessageA`, or stops showing frames, for 3 s, it logs the main thread's position and the game's return addresses on its stack (`robot: stall ...`), and how many game messages are queued.

## How it works

The runner prepares the test copy (default `C:\Games\H5_test`, never the Steam folder):

- copies `build/hook/dinput8.dll` into `bin/` and writes `bin/h5-together-robot.ini`, which turns on robot mode in the DLL (`src/hook/robot*.cpp`). The ini is deleted after the run, so the copy stays playable by hand;
- moves the intro videos (`video/logo-ubi6.ogg`, `logo-nival.ogg`, `intro.ogg`) to `video/robot_disabled/`;
- writes a robot profile in `<test copy>/robot_home/My Games/...`: windowed 1024×768, muted, no "press any key" on the loading screen (`pwl_press_any_key_enabled = 0`), console log on (`game_writelog = 1`).

In robot mode the DLL:

| What | How (data patches only, no game code is modified) |
|---|---|
| Uses the robot profile, never yours | redirects `SHGetFolderPathA(CSIDL_PERSONAL)` (import table) to `robot_home` |
| Keeps the game running at full speed in the background | wraps the game's window procedure (class `"H5"`, via `RegisterClassA` in the import table) so it never hears that it lost activation or focus |
| Starts the map | from the game's main loop (`PeekMessageA` import), runs the console command `advmap <map> 0 <difficulty> 1` the way WinMain runs `mainmenu` (0x00456220 + 0x00c125f0) |
| Plays player 0 as a passive human | when no AI has taken a step for 3 s, calls `NUI::CAdventureController::EndTurn` (0x00604e80); the controller's address comes from its vftable slot 0 |
| Measures the AI | the AI turn trace (`src/hook/ai_trace.cpp`): one log line per AI step |
| Stops | after `--turns` AI turns, or `--seconds` |
| Shows what the game says | copies `printf` / `fprintf` / `OutputDebugStringA` output to the log; the console goes to `console.txt`; message boxes are logged (and plain "OK" boxes answered) |
| Takes screenshots | saves the back buffer just before `IDirect3DDevice9::Present` shows it, after the player list overlay drew on it (`robot_screen.cpp`, through the shared Direct3D hooks of `src/hook/d3d9_hook.cpp`) |

## LAN games (two copies on one PC)

Each copy gets `OPEN_HOMM5_INSTANCE=host` or `client` in its environment; the DLL then uses `h5-together-robot-<instance>.ini` and `h5-together-<instance>.log`, the runner gives it its own profile (`robot_home-<instance>`), and the DLL renames the game's "already running" mutex (`NIVAL_H5`).

LAN games are found through UDP broadcasts on the discovery port (`net_lan_match_maker_port`, default 8889): every copy sends a 5-byte "02" packet each second, and a host also sends its game (an 826-byte "03" packet) to the copies it heard from. Two copies cannot both bind port 8889 ("address in use", the second copy's LAN screen never opened), and with a shared port (`SO_REUSEADDR`) they could not tell each other apart. So each copy gets its own discovery port (host 8889, client 8891) and the DLL also sends every discovery broadcast to the other copy's port on 127.0.0.1 (`net_lan_relay_ports`, `robot_net.cpp`). The game ports are 8888 (host) and 8890 (client). `net_log=1` logs the socket calls and the first packets.

**Packets byte by byte, and the virtual LAN** (online play, [docs/plans/online-steam.md](../../docs/plans/online-steam.md)):

```
python tools\robot\run_lan_test.py --turns 8 --seconds 300 --net-log-bytes 2048 --net-scan          # the game's own LAN
python tools\robot\run_lan_test.py --turns 8 --seconds 300 --net-log-bytes 2048 --net-scan --vlan   # through the virtual LAN
python tools\robot\packet_map.py --last 1      # where addresses and ports sit in each kind of packet
python tools\robot\lan_report.py --last 2      # checksums of both runs
```

- `--net-log-bytes N` (`net_log_bytes`): each logged packet's first N bytes, 64 per line (`robot: net:   +0040 ...`).
- `--net-scan` (`net_scan_addresses=1`): looks for this PC's own IPv4 addresses inside **every** packet the game sends or gets; logs the first 3 packets of each kind that hold one, whole, and a count every 10 s (`robot: net: address scan: ... N hold an address of this PC`). Matches in reversed byte order are counted apart: they turn up by chance in acknowledgement bits.
- `--vlan`: both copies run the virtual LAN with its loopback transport (`[online] mode=loopback`, ports 47101 and 47102, `src/mods/virtual_lan.cpp`), and the DLL's broadcast copies to the other copy's port are off, so the client can find the host only through the virtual LAN. The `vlan:` lines in both logs say what it carried and dropped. With it, the scan's count must be 0.

**Desync check:** the runner sets `mp_check_crc = 1` on both copies (`--no-crc` leaves it off); without it the game computes no world checksums at all. Both copies log every checksum (`trace_crc`), and `lan_report.py` says "all equal on both copies (no desync)" or where they first differ. `--fast-paths on` and `--ai-setting` set the fast pathfinding mod on both copies, as in `run_ai_test.py`. The client never runs the AI (the host does), so its "stuck" exit is off (`stuck_exit=0`). Not handled yet: a battle between the AI and the client's human player can leave the host waiting (the host then exits as stuck).

## Things learned getting here (2026-10-21)

- `-advmap <map>` on the command line is useless: WinMain always runs `mainmenu` right after it.
- `advmap <map> -1 ...` ("no human players", as the command's help says) is rejected: "Invalid player index -1!". The AI-test mode (`aai_testmap`) is not registered, and the "AI plays for humans" toggle (0x00590830) is dead code.
- The game ignores keyboard input when its window is in the background (DirectInput is polled once, window messages are ignored), so the robot does not "press keys"; `robot_input.cpp` has a DirectInput keyboard that is kept for later experiments.

## First result

In single player on A1M4, the AI **thinks ~90 ms per turn** but **waits ~400 ms after every step** (25–30 steps per turn), whatever the AI animation speed (`adventure_speed_ai` 1 or 10) or `AI_SEGMENT_DURATION`. See `docs/subsystems/ai-turn.md`.

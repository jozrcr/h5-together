# Plan and status: game rules chosen by the host

Status: **step 1 built and robot-checked** (2026-11-01): MMH5.5's own switches, chosen by the host in a panel, used by every PC; then **MMH5.5's scripts on random maps** (`map_scripts`, protocol 2, [below](#mmh55s-scripts-on-random-maps-map_scripts-2026-11-01)). Code: `src/mods/host_rules.cpp` (the game's side), `host_rules_logic.cpp` (the pure work, unit-tested by `src/hook/test/host_rules_test.cpp`), `host_rules_panel.cpp` (the panel). Settings: `open-homm5.ini` `[rules]`.

The owner (2026-11-01): "Rules chosen by the host each game, with no file swapping: YES, this would be awesome as a new settings menu when setting up a game." He likes most of MMH5.5, so step 1 is MMH5.5's own switches. The same menu gets later: the class skill lock and the ToE start bonuses ([lobby-audio-classes.md](lobby-audio-classes.md), items 3 and 4).

Background: [mmh55-vs-vanilla.md](mmh55-vs-vanilla.md), B.5 (every setting of `H55-Settings.lua`) and C.1 (ways a rule can reach every PC).

## In short

- **The host chooses.** On the game setup screen or in the waiting room, the host presses **F2** (or clicks our **"Rules..."** button at the right of the waiting room's title) and gets a panel with 15 of MMH5.5's switches and "MMH5.5 scripts on random maps", each with a one-line explanation, in French or English. Gold = changed from the host's own MMH5.5 file. **OK** saves the choice in `open-homm5.ini` `[rules]` (it is there next time); **MMH5.5's file** puts every value back to the file's.
- **Nothing changes unless the host changes something:** the panel starts from the host's `scripts/H55-Settings.lua` as installed (read at run time, so a group's own settings pak still counts).
- **The other players see the host's rules** in the waiting room (F2 or "Rules...", read only), and everyone sees the rules of the running game with F2 on the adventure map.
- **Every PC uses the same values:** they travel inside the game's own start message, so it works on **any map**, random or hand-made.
- **A PC without the add-on (or with it off) cannot start on other rules:** the host then sends no rules, and every PC uses its own MMH5.5 file, exactly as without the add-on. The host's log and panel say so ("Without open-homm5: Robot-client").
- **The rules check:** every PC's add-on tells the host a fingerprint of its MMH5.5 files (`H55-Settings.lua` and the stats paks). The host's log and panel name a player whose files differ, and that player's panel says so too.
- **Saves keep the rules the game started with:** the scripts read the file once, at the map's start, and a save keeps their values (robot-checked in single player).
- **MMH5.5's scripts on a random map straight from the game's generator, in a multiplayer game:** they need a map script, which such a map does not have. With RC19d they do not run there (then the other switches change nothing); RC19k's `55u.dll` runs them anyway. The rule **"MMH5.5 scripts on random maps"** (`map_scripts`) now decides it for every PC of the game; its default is what the installed MMH5.5 does (RC19d: off, RC19k: on). Hand-made maps and random maps saved with a map script (MMH5.5's Mapmixer or the editor: all of the owner's saved maps but the `MMH55_RMG_Map_...` ones) always run them, and so does single player.

## The rules (step 1)

From `H55-Settings.lua` ([mmh55-vs-vanilla.md](mmh55-vs-vanilla.md), B.5). `[rules]` key = the name in the ini and in the carried text.

| Group | `[rules]` key | MMH5.5 variable | Kind |
|---|---|---|---|
| Towns and heroes | `governance` | `H55_GovernanceEnabled` | on/off |
| | `town_conversion` | `H55_TownConvEnabled` | on/off |
| | `extra_conversions` | `H55_AmountExtraTownsDwellings` | 0 to 10 |
| | `town_gate` | `H55_TownGateEnabled` | on/off |
| | `mine_control` | `H55_MineControl` | on/off |
| | `dimension_doors` | `H55_DimensionDoors` | on/off |
| Computer players and neutral armies | `ai_cheat` | `H55_AICheatMode` | Off, 40% ... 140% (0 to 6; 4 = 100%) |
| | `mixed_stacks` | `H55_GenerateMixedStacks` | on/off |
| | `neutral_stack_size` | `H55_NeutralStackSize` | multiplier: 0.5, 0.75, 1, 1.25, 1.5, 2, 3 (or the file's own) |
| | `bank_difficulty` | `H55_BanksDifficulty` | multiplier, the same |
| Artifacts and items | `negative_artifacts` | `H55_NegativeArtifacts` | on/off |
| | `rpg_potions` | `H55_RPGPotions` | on/off |
| Random maps | `map_scripts` | none: the add-on applies it ([below](#mmh55s-scripts-on-random-maps-map_scripts-2026-11-01)); default = what the installed MMH5.5 does | on/off |
| | `competitive_towns` | `H55_CompetitiveARMGTowns` | on/off |
| | `mana_observatories` | `H55_ManaObservatories` | on/off |
| | `moon_disc` | `H55_MoonDiscOnARMGMaps` | on/off |

Left out on purpose: `H55_DimensionDoorsOnThisMap`, `H55_MoonDiscQuest`, `H55_ForceMixedStacks` (MMH5.5 says they are for map makers), the town pre-build levels, the Duel template's numbers, the governors' experience. They stay as in each PC's file. Adding one later is one line in `kRules` (`host_rules_logic.cpp`), its words in the panel, and a new protocol version (below).

## How the choice reaches every PC (research, 2026-11-01)

| Channel | Direction, when | Verdict |
|---|---|---|
| **Waiting-room requests** (`NUI::CMPWaitChangeRequest`: a row, a type, a byte; the faction mod's hello) | client → host, in the lobby | **Used for the hello.** The host's handler (`0x0071f3d0`) ignores types it does not know. A hello is type `0x4f52` ('OR') with the protocol version in the byte; three more carry 32-bit hashes, the mark in the type's top byte ('N', 'S', 'X') and 24 bits, the top 8 bits in the byte. Sent every 4 s, one per frame on the first row's free request slot, as the faction mod does. |
| **A UI command to one PC** (the host's transport, logic `+0x10`, slot 8: "SendDirect", as the game's own `0x0071e7b0` sends each PC its arrow locks) | host → client, in the lobby | **Used for the lobby display.** A client's handler (`0x0071ef10`) gives an `IMPWaitUICommand` to a row only when the row exists, so a command for row `0x4f52xxxx` is ignored by a client without the mod. The **hero-name command** (`CMPWaitChangeHeroNameUICommand`, it holds a string) left the host ("SendDirect: ... to 1" in the console) but **never reached the client**: the game does not send that class over the network. The **lock-buttons command** (`CMPWaitLockButtonsUICommand`, `0x007111d0`: a row and a 32-bit mask) does arrive. So the text goes 4 bytes at a time: rows `0x4f52 << 16 | seq << 8 | index`, piece 0 = the length (`LobbyPieces`, `LobbyAssembler`). The text is short: `L1|<flags>|0,1,5,1,1,1,2,0,1,1,0,1,0,1,1` (11 pieces). |
| **The start message** `NMP::CMPStart` (vftable `0x00e3a4dc`; serializer slot 3 `0x007208a0`) | host → every client, at the start | **Used: the binding channel.** The game's binary saver (`CStructureSaver`) writes numbered chunks; its reader looks a chunk up by its number (`0x00cb6370`) and skips the others. After the message's own chunk 3, the host writes **chunk `0x4f`**, 512 bytes: `OHR1;governance=0;...;moon_disc=1` (every rule's value), zero-filled. A client without the mod skips it; ours reads it while the message is read (before the world is made). The host's own game runs from the same message object, not a copy (`0x0071feb0` sends it, then hands it to its own handler). |
| The game info the host advertises (`NUI::CGameData`, the LAN list's 826-byte answer) | host → PCs looking for games | Not used: it is for finding games (and GameRanger has its own), not for the PCs already in the lobby, and it would not reach a PC that joined by address. |
| The co-op guard's ignored checksum messages ([coop-timing-guard.md](coop-timing-guard.md)) | every PC → every PC, after the first segment | **Too late.** The scripts read `H55-Settings.lua` about 0.3 s after the game starts (robot: the start at 47.8 s, the read at 48.2 s), before the first segment. |

**Every PC applies the identical rules before the scripts read the file:**
1. The host decides when it starts (`0x0071feb0`, the call at `0x007200da`): every human player's PC (slot state 2, not the host's) must have said hello with the same protocol version.
2. A client takes the chunk's values when it reads the start message; the host takes its own.
3. When the world starts (`NUI::CRunMPGame`, slot 7 `0x0071d660`), those values become **this game's rules**. A single-player start (`NUI::CStartSingleAdventureMap`, slot 7 `0x007e1030`) drops them: single player always uses the file.
4. When the map's scripts read `scripts/H55-Settings.lua` (`advmap-startup.lua` → `advmap-common.lua` → `H55-Core.lua`, every PC), the file hook serves it with those values.

**A PC without the mod**, or with `[rules] enabled=0`, or with another protocol version, says no hello (or another version), so the host sends no chunk, and every PC uses its own file, as without the mod. The host's log has `rules: START: Robot-client without open-homm5's rules: every PC uses its own H55-Settings.lua`, its panel the same warning, and our lobby button a red "!".

**Versions.** The carried text starts with `OHR1` (the lobby's with `L1`); a PC refuses a text with an unknown key or a bad value (`Decode`), and the hello carries `kProtocol` (1). A new rule = a new protocol version, so an older PC is treated as "without the mod" and nobody plays on rules it cannot apply.

## How each PC applies them

The game's "open file" (`NVFS::CWinVFS` slot 7, `0x009e0020`, stacked with the other mods' wraps) is wrapped. When the scripts open `scripts/H55-Settings.lua` and this game has the host's rules:
- each plain assignment `NAME = number` of a rule's variable, at the start of a line (or after a `;` on it), outside `--` comments and `--[[ ]]` blocks, gets the new number: **only the number's characters change** (unit test: every other byte is the same);
- a variable the file does not have, or that it also sets with an expression, gets a line at the end (`H55_X = v; -- open-homm5: the host's rules`): Lua's last assignment wins;
- with the file's own values, the served file is the same bytes as the file;
- the log has the values, what changed and the served file's hash, e.g. `rules: the scripts read scripts/H55-Settings.lua: served with the host's rules (governance 1->0, ai_cheat 4->0, mixed_stacks 1->0; ...); 8206 -> 8206 bytes, hash a07c5c34`. Every PC of a game should log the same hash.

The host sends the value of **every** rule (its file's, with its choices), not only its changes, so a PC whose file has other values for these switches still plays on the host's. The rest of the file must be the same everywhere: that is the check below.

Never committed: the file itself (MMH5.5's text). The unit test uses a text of ours in the file's format.

## The rules check (C.1 way 2)

Each PC hashes (FNV-1a, 32 bits), once per session, when the lobby needs it:
- its `H55-Settings.lua` as installed, **without the rules' values** ('N': only that part must match, the host's values replace the rest), and whole ('S');
- MMH5.5's stats files the way `55u.dll` looks for them (its own list: `..\UserMODs\MMH55-CustomStats.h5u`, `..\data\MMH55-Stats.pak`, `MMH55-Stats1..9.pak`): the names and bytes of those that exist ('X').

The host compares them with its own: `rules: CHECK: Robot-client (PC 1) has another MMH5.5's stats files than the host (...): the game may not stay in step`. The panel lists the players whose files differ, the button gets a red "!", and the client's own panel says "Your MMH5.5 files differ from the host's". This only warns; the game still starts.

Not done: the same check in the co-op hello once the game runs (the co-op guard's message carries one number; a second message would do, see "Later").

## Saves

**A loaded game keeps the rules it was started with.** MMH5.5's scripts read `H55-Settings.lua` once, when a map starts; a save keeps the scripts' variables and threads. Robot check (RC19k, single player, `[rules] trace=1`, which logs every script file the game opens): a new game on A1M4 opened `advmap-startup.lua`, `H55-Core.lua`, `H55-Settings.lua` and the rest (run `20261101-203522`); loading its day-2 autosave (`Sauvegarde auto 3`, run `20261101-203603`) opened **no script file at all**, printed no `H55 Lua Library and Tables initialized`, and MMH5.5's scripts went on (`H55 Daily Event 2 Finished`, `H55 Player Configuration: ...` on each new day). So whatever the host's panel says when a save is loaded, the game goes on with the values it started with, on every PC (each PC loads its own copy of the save, made from the same world). A multiplayer load was not robot-tested (the robot cannot open the multiplayer load screen yet); it uses the same script engine.

## The panel

A modal overlay panel in the template builder's and the hero looks' style (`rmg_builder_widgets.cpp`: the game's check boxes, drop-down lists, arrows and buttons), 649×508 in the game's 1024×768 layout, centred and scaled with the height.
- **Host** (setup screen, waiting room): check boxes, the arrows (`extra_conversions`), drop-down lists (AI bonus, multipliers). Gold = changed from the file, with "(your file: On)" after the explanation. Under the list: "Every player has open-homm5: these rules will be used", or who lacks it, or whose files differ. Buttons OK, MMH5.5's file, Cancel.
- **The lobby's map without a map script** (a random map straight from the generator): the host reads the map's `map.xdb` once (`HasMapScript`) and logs it. With `map_scripts` off, both panels say the rules change nothing on it and the button gets its red mark; with it on, they say the scripts will run on it all the same. When the host changed `map_scripts`, its row says "(MMH5.5 alone: On/Off)" instead of "(your file: ...)".
- **Client** (waiting room): the same rows, read only, as the host sent them; "They will be used: every player has open-homm5" or why not.
- **In game** (adventure map): this game's rules, read only.
- **Words:** French or English (`[ui] language`, `overlay::PanelLanguage`).
- **The key:** F2 (`[rules] key`). The game binds F4 to F8 and F10 (options, saves, menu); we use F9 (player list), F10 (test panel), F11 (builder), F12 (hang test). F2 is free. **Temporary**, until the owner's sketch places the button.
- **The button:** "Rules..." at the right end of the waiting room's title row (`[rules] button=1`). It reads the mouse itself, like the builder's fallback button; the game gets the same click, on its title, which does nothing.

## MMH5.5's scripts on random maps (`map_scripts`, 2026-11-01)

The owner: "Shall we add an option to select if we process through MMH 5.5?" Yes: one more rule in the panel, **"MMH5.5 scripts on random maps"** / **"Scripts MMH5.5 sur les cartes aléatoires"** (`[rules] map_scripts`, group "Random maps"). It is not a line of `H55-Settings.lua`: the add-on applies it itself, on every PC, from the start message like the other rules.

### What makes a map run MMH5.5's scripts

Two places of the game run `scripts/advmap-startup.lua` (which loads `advmap-common.lua` → MMH5.5's `H55-Core.lua`, `H55-Settings.lua`...):

| Where | When | Needs a map script? |
|---|---|---|
| The adventure screen's own script context: `NUI::CAdventureController`'s constructor (`0x006089a0`) → `0x00603b20` | a new game, only where "console script commands" are allowed (`0x006b9de0`; otherwise the log has "Warning: console script commands are not allowed"): **single player** | **No**: it runs `advmap-startup.lua`, `createAdvmapAliases();`, then the map's script file if it has one |
| The world's script host (`NWorld::CWorld` +0x1d0): world slot 128 `0x009941a0` → `0x00992d00`, given the map's `NDb::SScript` by the world's creation `0x00c9fa80` (`FUN_00441d00(&desc->MapScript)`, NULL for `<MapScript/>`) | a new game whose world settings (world slot `0x14`, two bytes) say multiplayer: **LAN and GameRanger games** | **Yes**: `if (map_script == 0) return;` (the `jz` at `0x00992d2a`), then it makes the `advmap` script machine and runs `advmap-startup.lua`, `createAdvmapAliases();` and the map's file (`SScript` +0x44..+0x48, its file name) if the name is not empty |

So on a random map straight from the generator (`map.xdb` has `<MapScript/>`), MMH5.5's scripts run in single player but not in a multiplayer game.

**MMH5.5's Mapmixer** (`MMH55_Mapmixer.exe`, RC19d's way) adds the script to a saved map: `MapScript.xdb` (`<Script><FileName href="map.lua"/><ScriptText/></Script>`), a `map.lua` that holds only a comment (`-- This lua file was created by the Mapmixer`), and `<MapScript href="MapScript.xdb#xpointer(/Script)"/>` in `map.xdb`. The owner's `MAP_JoacJoz` has exactly that (and `RMGmap` false: it is a fixed map, sent as a file, not rebuilt by each PC). `MMH55_RMG_Map_002` (and the rebuilt copies `.1` ... `.11`) has `<MapScript/>`. RC19k's own generated map (`data\RMGTemp\CurrentMap` in `H5_test_k`) has `<MapScript/>` too: **RC19k does not add a map script to the map**.

**RC19k's way** (its notes: "map script activation is now the default", "also applies to LAN maps"): its `55u.dll` changes 6 bytes at `0x00992d2a`, the `jz` above (a spot RC19d's `55u.dll` does not have: compare `python tools\inventory\mmh55_patch_sites.py <55u.dll> --list` for both). Its patch table entry: address `0x00992d2a`, 6 bytes, kind 6. In the running game (our log) the `jz` becomes **six `nop`s**, and RC19k also puts two jumps of its own into `0x00992d00` (not in its table): at `0x00992db1` (`e9 ... 90 90`, before the kept `8b 50 44`) its code skips the name when the script is 0 and otherwise reads it as the game does (`xor eax,eax; cmp ss:[esp+0x3c],eax; jz ...; mov eax,ss:[esp+0x3c]; mov ecx,ds:[eax+0x48]`, then back), and a second one skips the map file when the script is 0. So RC19k runs `advmap-startup.lua` and `createAdvmapAliases();` on a map without a script: the same as an empty script.

No adventure-map switch for scripts in multiplayer was found besides that test: `MultiplayerCombatScripts` (MMH5.5's stats) is about combat scripts only.

### How the add-on does it

Our wrap of world slot 128 (`0x00e6280c`, the vtable cell of `NWorld::CWorld`), only while a multiplayer game's world is made (inside `NUI::CRunMPGame` slot 7), and only with the host's rules:
- the map has its own script: nothing changes;
- no map script and `map_scripts=1`: the world gets **an empty script** (a block whose file name, at +0x44..+0x48, is empty). `0x00992d00` then does what it does for a Mapmixer map: it makes the script machine, runs `advmap-startup.lua` and `createAdvmapAliases();`, and skips the map file (empty name). Every PC does the same, from the same start message;
- no map script and `map_scripts=0`: the wrap does not call the slot, which is what the original game does without a map script (`0x00992d00` returns at its `jz` and does nothing else). So "off" also turns RC19k's scripts off on those maps;
- no host's rules in this game (a player without the add-on, `enabled=0`): nothing changes: each PC's MMH5.5 decides, as without the add-on.

**Byte checks** at start-up: the slot holds `0x009941a0`; its code is `8b 89 d0 01 00 00 85 c9 74 05 e9 51 eb ff ff` (the script host at +0x1d0, then a jump to `0x00992d00`); `0x00992d26` is `39 5c 24 34` (the null test); the name read at `0x00992db1` is the game's `8b 44 24 3c 8b 48 48 8b 50 44` or RC19k's jump `e9 <rel> 90 90 8b 50 44` whose target starts with the 18 bytes quoted above (the only reads of the script: its name, +0x44 and +0x48). If any differs, the whole rules mod stays off (no hello: no PC plays on rules it cannot apply); the log says which (`rules: map_scripts: ... (code 110)`). The name read is checked again before an empty script is given (an MMH5.5 that patched later: then no scripts, logged `FAILED`). Log at start-up: `rules: map_scripts: the world's script start is as expected (its name read: the game's own)` (RC19d) or `(... through MMH5.5 RC19k's 55u.dll, the same for a script)` (RC19k).

**The default** ("MMH5.5's file" in the panel, and what the host sends when it chose nothing) is what this PC's MMH5.5 does by itself: the 6 bytes at `0x00992d2a`, read in the lobby (55u.dll changed them at start-up). The original `0f 84 4e 01 00 00` (the original game, RC19d) = off; anything else (RC19k: `90 90 90 90 90 90`) = on. Logged once: `rules: map scripts: the game's "no map script" jump (0x00992d2a) is 90 90 90 90 90 90: changed (MMH5.5 RC19k: ...)`. The panel shows it as "MMH5.5 alone: On/Off" when the host changed it.

**The protocol** is now version 2 (16 rules): a PC with version 1 says hello 1, the host treats it as a PC without the rules, and nobody gets the host's rules (as before for a PC without the add-on).

**Saves.** A multiplayer load does not make the world from the map: it goes through `NMP::CMPLoad` (vftable `0x00e3ce4c`, built at `0x00754240`, `0x007542f0`, `0x00823cb0`, `0x00825240`), not through `NMP::CMPStart` and `NUI::CRunMPGame` (built only at `0x00720570`, `0x007205e0`, `0x007207d0`), and the world's creation from the map (`0x00c9fa80`) is called only from the new-game paths (`0x00820830` multiplayer, `0x007e0a50` single player, `0x00c1d4f0`). Our wrap acts only inside `CRunMPGame`, so a load is never changed: a game keeps the scripts it started with, exactly as a game on a Mapmixer map does (the script machine and its Lua state are part of the world). **Not robot-checked:** the robot cannot open the multiplayer load screen yet ("Later", item 6). In single player a load opens no script file (above, "Saves").

### Robot checks (2026-11-01, LAN, two copies; `--rules-setting trace=1` logs every script file opened)

| Run | MMH5.5, map, rules | Result |
|---|---|---|
| `20261101-222702` | RC19k, single player, a map made by the console's `rmg` (`OH-X1-22001gnnxxxxxxxM`, saved as `OH-robot-mapscripts`, guid `D2083C27-...`) | Our wrap passed it through (`not a multiplayer game's start`); MMH5.5's scripts ran from the adventure screen as before |
| `20261101-222759-lan` | RC19k, that RC19k map, the default (`map_scripts=1`) and `mixed_stacks=0` | Both PCs: `it gets an empty one, so MMH5.5's scripts run`, the settings file served with the same hash (`b468d8f7`), every `H55` start-up line twice (`Lua Library and Tables initialized`, `Towns prepared for Competitive ARMG Map!`, `All AI Players have allied against the human players`...), **no `H55 Mixing`**. **327 world checksums, all equal** |
| `20261101-223114-lan` | RC19k, the same map, `map_scripts=0` | Both PCs: `no scripts, as in the original game (this PC's MMH5.5 would run them)`; no script file opened, no `H55` line. 325 equal |
| `20261101-223859-lan` | RC19k, the same map, a client with `enabled=0` (no host's rules: RC19k's own way) | Both PCs ran the scripts (lines twice). 327 equal |
| `20261101-223636-lan` / `20261101-224204-lan` | RC19k, `MMH55_RMG_Map_002` (made by RC19d), `map_scripts=0` / `1` | No scripts, 230 equal / scripts on both, 324 equal |
| `20261101-221540-lan`, `20261101-224512-lan` | RC19k, `MMH55_RMG_Map_002`, client `enabled=0` (RC19k's own way) | Scripts ran; the first run desynced from the first checksum, the second was in step (228 equal): see "Findings" |
| `20261101-225921-lan` | **RC19d**, `MMH55_RMG_Map_002`, `map_scripts=1`, `mixed_stacks=0` | Both PCs: the empty script, the same served hash (`a4483adb`), `H55` lines twice, no mixing. **365 equal** |
| `20261102-000511-lan` | **RC19d**, the same map, the default (`map_scripts=0`), `mixed_stacks=0`; panels open (host in French) | No scripts on either PC (as before the rule), no `H55` line. 328 equal. Screenshots: the new row "Scripts MMH5.5 sur les cartes aléatoires" in "Cartes aléatoires", off, and under the list "Cette carte n'a pas de script ... ces règles n'y changent donc rien" on both panels |

One run (`20261101-230337-lan`) stopped when the client joined, before any rules step (the host's log just ends); the same run again was normal.

## Robot checks (2026-11-01)

| Run | What | Result |
|---|---|---|
| `20261101-191940-lan` (RC19k, A1M4) | the host's `[rules]`: `governance=0 ai_cheat=0 mixed_stacks=0` (its file: 1, 4, 1) | Hello and lobby text; the start message carried the rules; both PCs logged `served with the host's rules (governance 1->0, ai_cheat 4->0, mixed_stacks 1->0 ...) hash a07c5c34`. The console: `H55 Stacks Qty: 36` on both PCs and **no `H55 Mixing ... neutral stacks!`** (an earlier A1M4 run with MMH5.5's file had `H55 Mixing 36 neutral stacks!` on both). **417 world checksums, all equal.** The client's lobby panel was still empty: the hero-name command never arrived (above). |
| `20261101-192743-lan` (RC19k, A1M4) | the same, with the lock-buttons pieces | The client logged `the host's rules (lobby): governance=0 ... ai_cheat=0 mixed_stacks=0 ...` 0.4 s after the host sent them, and its panel showed them; in game its F2 panel said "This game uses the host's rules". Served on both PCs, the same hash; no mixing. **707 world checksums, all equal.** |
| `20261101-193017-lan` (RC19d, the random map `MMH55_RMG_Map_002`, no map script) | `governance=0 competitive_towns=0 mixed_stacks=0 ai_cheat=2` | Lobby and start message as above, both PCs had the rules for the game, but **neither PC read `H55-Settings.lua`, and the console has no `H55` line at all: MMH5.5's scripts did not run.** 365 world checksums, all equal. |
| `20261101-193713` (RC19d, single player, the same random map) | the file's rules | The scripts ran (`H55 Towns prepared for Competitive ARMG Map!`), `rules: the scripts read scripts/H55-Settings.lua: this game uses the file's rules`. So the same map runs MMH5.5's scripts in single player, not in multiplayer. |
| `20261101-202901-lan` (RC19k, A1M4) | **a client without the rules** (`[rules] enabled=0`); the host chose `mixed_stacks=0` **in the panel on the setup screen** (robot `panel|rules set mixed_stacks 0`, `panel|rules ok`) | The host's choice was saved (`rules: the host's choices: mixed_stacks=0 (saved in open-homm5.ini [rules])`); in the waiting room its panel said "Without open-homm5: Robot-client" and the button had its red mark; at the start `rules: START: Robot-client without open-homm5's rules: every PC uses its own H55-Settings.lua`, no chunk; both PCs used their file: **`H55 Mixing 36 neutral stacks!` on both** (the file's value). 518 world checksums, all equal. |
| `20261101-203209-lan` (RC19k, the owner's `MAP_JoacJoz`: a random map saved with a map script, 4 players) | `governance=0 mixed_stacks=0 competitive_towns=0 ai_cheat=0` | The host logged `the lobby's map /Maps/RMG/81CD0EF2-.../map.xdb has a map script`; both PCs served the same file (hash `15eef79d`, `governance 1->0, ai_cheat 4->0, mixed_stacks 1->0, competitive_towns 1->0`); the scripts ran on both (`H55 Stacks Qty: 163` twice) and **did not mix** the stacks. 262 world checksums, all equal. |
| `20261101-204054-lan` (RC19k, A1M4, after merging main 0.2.5 with the hero looks) | `governance=0 mixed_stacks=0 ai_cheat=2` | Both mods on together: the same served hash on both PCs (`1827cd4e`), `hero skins: looks on for this game`, no mixing. **517 world checksums, all equal.** |

Screenshots: the session's `scratchpad\rules-shots\`.

## Findings worth knowing

- **MMH5.5's scripts in multiplayer need a map script** (with RC19d). A random map straight from the game's generator has `<MapScript/>` (empty) in its `map.xdb`; in a multiplayer game no `advmap-startup.lua` runs for it, so none of MMH5.5's script features run there (governors, conversion, town gate, mine guards, mixed stacks, the AI bonus, the random map extras, the Savage/Nightmare modes' script part). In single player they do run on the same map. MMH5.5's Mapmixer (and the map editor) save random maps with a script; the owner's saved maps (`MAP_JoacJoz`, `6P-epic-...`, `TEAMJOACJOZ`...) have one, while `MMH55_RMG_Map_002` / `_007` and the robot's rebuilt copies do not. RC19k runs them anyway; the rule `map_scripts` now decides ([above](#mmh55s-scripts-on-random-maps-map_scripts-2026-11-01)).
- **One RC19k LAN run desynced at once and the same run again did not** (robot `20261101-221540-lan`: RC19k's own way, no host's rules, `MMH55_RMG_Map_002`: 299 of 300 world checksums different from the first one; `20261101-224512-lan`, the same set-up: 228 equal). The same map was in step under RC19k with no scripts and with our empty script, and RC19k's own way was in step on a map made by RC19k. Cause not found. Something to watch in the group's first RC19k games on generated maps (the co-op guard reports a desync).
- The scripts open `H55-Settings.lua` once per map start, on every PC, about 0.3 s after the world starts (after `advmap-startup.lua`, `events.lua`, `advmap-common.lua`, `common.lua`, `H55-Tables.lua`, `H55-Core.lua`, `H55-Library.lua`).
- With one client, the host serializes `CMPStart` three times (the console has "SendDirect: command: class NMP::CMPStart" to PC 0, the host itself, and to PC 1; the third is probably a size or checksum pass): each carries the chunk. The host's game does not read its own copy back (no "the host's start message" line in its log).

## Later

1. **The class skill lock** ([lobby-audio-classes.md](lobby-audio-classes.md) item 4) and **ToE start bonuses** (item 3) in the same panel: a new group, new keys in the carried text (protocol 2), served data files (`HeroClass.xdb`, the hero files, `H55-AdvMap.lua`) the same way.
2. **The button's place** from the owner's sketch (and one on the setup screen).
3. **The check in the co-op hello** too, for games joined without our lobby (a second ignored checksum message with the served file's hash).
4. ~~MMH5.5's scripts on generated random maps in multiplayer~~: done, the rule `map_scripts` ([above](#mmh55s-scripts-on-random-maps-map_scripts-2026-11-01)).
5. Single player and hotseat could use the host's choices too (today they use the file; `map_scripts` changes nothing there).
6. A multiplayer **load** robot check (the robot cannot open the multiplayer load screen yet): a game started with `map_scripts=1` on a generated map, saved, loaded with the rule off, should go on running MMH5.5's scripts (reasoning in "Saves" of the `map_scripts` section).

## Addresses (vanilla 3.1, also `MMH55_Utility`)

| What | Address |
|---|---|
| Host lobby logic `NUI::CMPUbiComWaitHostLogic` | vftable `0x00e3a5dc`; slot 0 update `0x00720080`, slot 5 a PC's message `0x0071f3d0`; `+0x10` transport (slot 8 = send to one PC), `+0x34` the host's PC, `+0x38/+0x3c` rows, `+0x74` the setup, `+0x104` broadcast |
| Client lobby logic `NUI::CMPUbiComWaitClientLogic` | vftable `0x00e3a544`; slot 0 update `0x0071df50`, slot 5 the host's message `0x0071ef10` (the pump `0x00710800` calls slot 5 for each message) |
| Waiting-room request | vftable `0x00e39838`, ctor `0x00711580`: `+0xc` row, `+0x10` type, `+0x14` byte |
| Lock-buttons command | vftable `0x00e397d0`, ctor `0x007111d0` (`__fastcall(row, const DWORD *mask)`), serializer `0x007d3d20` (chunk 2 row, chunk 3 mask) |
| Host start | `0x0071feb0` (called at `0x007200da`): builds `NMP::CMPStart` (`0x98` bytes), broadcasts it, hands it to its own handler |
| `NMP::CMPStart` | vftable `0x00e3a4dc`, slot 3 serializer `0x007208a0` → `0x005a1400`(chunk 3) → `0x005a1430` (chunks 2..7) |
| `IBinSaver` (`CStructureSaver` `0x00e93dfc`) | slot 7 StartChunk `0x00cb6890`, 8 FinishChunk, 10 DataChunk `0x00cb6570` (reading: zero-fills a missing chunk), 15 IsReading |
| World start | `NUI::CRunMPGame` slot 7 (`0x00e3a498` → `0x0071d660` → `0x00820830` → `0x00c9fa80`); `NUI::CStartSingleAdventureMap` slot 7 (`0x00e454dc` → `0x007e1030`) |
| The world's scripts | `NWorld::CWorld` (vftable `0x00e6260c`) slot 128, cell `0x00e6280c` = `0x009941a0` (`__thiscall(world, const NDb::SScript *)`, ret 4; the call at `0x00ca01c8` in `0x00c9fa80`, script from `0x00441d00(&desc +0x1a0)`) → `0x00992d00` (the "no script" `jz` at `0x00992d2a`; name read `0x00992db1`; script `+0x44..+0x48` = its file name). The adventure screen's own: `0x006089a0` (`NUI::CAdventureController` ctor) → `0x00603b20` |
| Multiplayer load | `NMP::CMPLoad` vftable `0x00e3ce4c` (built at `0x00754240`, `0x007542f0`, `0x00823cb0`, `0x00825240`); `NUI::CRunMPGame` is built only at `0x00720570`, `0x007205e0`, `0x007207d0` |

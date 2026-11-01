# Plan and status: game rules chosen by the host

Status: **step 1 built and robot-checked** (2026-11-01): MMH5.5's own switches, chosen by the host in a panel, used by every PC. **Step 2 built and robot-checked** (2026-11-01, night): the game mode and ToE's start bonuses (section "Step 2" below). Code: `src/mods/host_rules.cpp` (the game's side), `host_rules_logic.cpp` (the pure work, unit-tested by `src/hook/test/host_rules_test.cpp`), `host_rules_panel.cpp` (the panel), `host_rules_bonus.cpp` (the lobby's bonus tooltips). Settings: `open-homm5.ini` `[rules]`.

The owner (2026-11-01): "Rules chosen by the host each game, with no file swapping: YES, this would be awesome as a new settings menu when setting up a game." He likes most of MMH5.5, so step 1 is MMH5.5's own switches. Step 2 is the ToE start bonuses ([lobby-audio-classes.md](lobby-audio-classes.md), item 3; the owner chose option A: each player picks a bonus with the lobby's arrows, and the mode gets its own chooser). Later in the same menu: the class skill lock (item 4).

Background: [mmh55-vs-vanilla.md](mmh55-vs-vanilla.md), B.5 (every setting of `H55-Settings.lua`) and C.1 (ways a rule can reach every PC).

## In short

- **The host chooses.** On the game setup screen or in the waiting room, the host presses **F2** (or clicks our **"Rules..."** button at the right of the waiting room's title) and gets a panel with 15 of MMH5.5's switches, each with a one-line explanation, in French or English. Gold = changed from the host's own MMH5.5 file. **OK** saves the choice in `open-homm5.ini` `[rules]` (it is there next time); **MMH5.5's file** puts every value back to the file's.
- **Nothing changes unless the host changes something:** the panel starts from the host's `scripts/H55-Settings.lua` as installed (read at run time, so a group's own settings pak still counts).
- **The other players see the host's rules** in the waiting room (F2 or "Rules...", read only), and everyone sees the rules of the running game with F2 on the adventure map.
- **Every PC uses the same values:** they travel inside the game's own start message, so it works on **any map**, random or hand-made.
- **A PC without the add-on (or with it off) cannot start on other rules:** the host then sends no rules, and every PC uses its own MMH5.5 file, exactly as without the add-on. The host's log and panel say so ("Without open-homm5: Robot-client").
- **The rules check:** every PC's add-on tells the host a fingerprint of its MMH5.5 files (`H55-Settings.lua` and the stats paks). The host's log and panel name a player whose files differ, and that player's panel says so too.
- **Saves keep the rules the game started with:** the scripts read the file once, at the map's start, and a save keeps their values (robot-checked in single player).
- **MMH5.5's scripts do not run on a random map straight from the game's generator in a multiplayer game** (found by the robot, below). There, these switches change nothing, with or without the add-on. They work on hand-made maps, on random maps saved with a map script (MMH5.5's Mapmixer or the editor: all of the owner's saved maps but the `MMH55_RMG_Map_...` ones), and in single player.

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
| Random maps | `competitive_towns` | `H55_CompetitiveARMGTowns` | on/off |
| | `mana_observatories` | `H55_ManaObservatories` | on/off |
| | `moon_disc` | `H55_MoonDiscOnARMGMaps` | on/off |

Left out on purpose: `H55_DimensionDoorsOnThisMap`, `H55_MoonDiscQuest`, `H55_ForceMixedStacks` (MMH5.5 says they are for map makers), the town pre-build levels, the Duel template's numbers, the governors' experience. They stay as in each PC's file. Adding one later is one line in `kRules` (`host_rules_logic.cpp`), its words in the panel, and a new protocol version (below).

## Step 2: game mode and start bonuses (built 2026-11-01)

**What MMH5.5 does** ([lobby-audio-classes.md](lobby-audio-classes.md), item 2). Each lobby row has bonus arrows (ToE's start bonus: random, minor artifact, gold, resources). The game hands the bonus out when the map starts. Then MMH5.5's start script, `scripts/H55-AdvMap.lua` (`H55_PrepareAdvMap`, the part from `H55_DEBUG = {102,"StartingBonus"` to `{103,"StartingArmies"`, dofile'd by `H55-Core.lua` on every PC), looks at each human's gold, resources and first hero's artifacts, recognises the bonus, **takes it back**, gives each human's first hero a random minor artifact of its own (`H55_ArtifactStartBonus`; its creature bonus is commented out), and makes one **game mode** of the humans' picks: any artifact = Default, else any gold = Savage, else any resources = Nightmare. Savage and Nightmare stop the AI heroes from fleeing (Nightmare: also unlootable) and team the computer players against the humans (`H55_TeamHumansvsAI`, or `H55_TeamAI` with `H55_NoAutoTeamHumans = 1`). Scenarios, campaigns and games without computer players get Default; "casual" is decided elsewhere (`H55_UpdateGameMode`) and is left alone.

**The two rules** (group "Game mode and start bonuses", first in the panel; `[rules]` keys; not in `H55-Settings.lua`, so "0" is each PC's default):

| `[rules]` key | Values | Default |
|---|---|---|
| `game_mode` | 0 = the bonus arrows (MMH5.5's way), 1 Default, 2 Savage, 3 Nightmare, 4 Random (one of the three, drawn in the script with the game's shared random numbers, `random(3)+1`) | 0 |
| `toe_bonuses` | 1 = each human keeps the ToE bonus picked with the arrows, and MMH5.5's start artifact is not given | 0 |

With `toe_bonuses=1` and `game_mode=0`, the mode is Default (the arrows now choose bonuses). **Why MMH5.5's start artifact is not given with the ToE bonuses:** it is exactly ToE's "minor artifact" bonus given to everyone; on top of a ToE bonus, an artifact picker would get two. With `toe_bonuses=0` and a mode, MMH5.5 still takes the bonus back and gives its artifact (only the mode is the host's).

**The served script** (`PatchStartScript`, every PC, when a start rule is not MMH5.5's way). Three edits inside the "StartingBonus" part, each place found **exactly once** (and in this order), else the file is served as it is and the log says why (`REFUSED`):
1. after `local amountai = table.containsamount(H55_PlayerStatus,1);`: a line `local OH55_KeepToEBonus, OH55_Mode = <toe>, <mode>; -- open-homm5: ...`;
2. the humans' loop `if H55_MapType ~= "Campaign" then` gets `and OH55_KeepToEBonus ~= 1` (the whole take-back and MMH5.5's artifact are skipped);
3. before `if H55_MapType == "Scenario" then` (the mode's decision): one line that sets MMH5.5's three "somebody chose" flags from `OH55_Mode`. MMH5.5's own decision code then runs unchanged: its prints, the AI heroes that never flee, the teams.

The checks also require the three flags to be locals of the part before the AI count, the loop to contain the take-back (`SetPlayerResource(i,6,`, `RemoveArtefact(heroes[0]`, `H55_ArtifactStartBonus(heroes[0]);`), and the decision to read the three flags. Each new line takes its neighbour's indent and line end (the file mixes CRLF and LF). Unit-tested on a text of ours in the file's layout (`TestStartScript`: the edits, nothing else changes, refusals); checked on the real RC19d and RC19k files in the scratchpad (never committed): both patch, and their "StartingBonus" parts are **identical** (hash `da18d7b6`), so an RC19d and an RC19k PC can play together on these rules. With `[rules] trace=1`, one more line before the next part prints each player's start resources and minor artifacts to the console (testing only).

**Every PC the same:**
- the rules travel in the start message with the others (protocol 2: `OHR2`, `L2`; a PC with protocol 1 counts as "without the add-on");
- each client's hello also sends the hash of its "StartingBonus" part (mark `'A'`). The host uses the start rules only when its own part takes our edits and every human client sent the same hash; otherwise it sends them as "0" (MMH5.5's way, the arrows choose the mode), logs `rules: START: the game mode and ToE start bonuses stay MMH5.5's way ...`, and both panels say why (the lobby text's flag `kNoStart`);
- a PC without the add-on: no rules at all, MMH5.5's way everywhere (step 1's rule).

**The lobby's tooltips** (`host_rules_bonus.cpp`). The game reads each text file once per session, and each lobby row copies the four bonus tooltips when it is made (`0x00714f00`), before a joining client knows the host's rules, so serving other text files cannot follow the rules. Instead, every 0.5 s, the lobby logic's update rewrites each row's own copies: the row (`NUI::CMPWaitItem`, vftable `0x00e39ce8`) keeps at `+0x108` a vector of five `hash_map<int, wstring>` (0x18 bytes each); the fourth (`+0x48`) holds the bonus tooltips by bonus number; `0x0040e960` (`wstring::assign(first, last)`) puts our text in, and the icon (`+0x9c`) gets the one of the row's bonus (`+0x84`) through its vtable slot `0x68`, exactly as the row's refresh `0x00713fe0` does. Our texts (English or French, the panel's language): ToE's bonus (e.g. "Start bonus: gold. 500 to 1000 more gold at the start.") and "Game mode: Savage (the host's game rules)"; with only a mode, "No start bonus to choose: MMH5.5 gives every human player's first hero a random minor artifact." MMH5.5's texts come back when the rules go back to MMH5.5's way (or a player without the add-on joins). **Gaps:** the icons stay MMH5.5's mode pictures (a fist and a burning town for gold...): the game loads them once too, and swapping them would need the icon's texture list (not studied; MMH5.5's pak still has three of ToE's icons as `Gold_ret`, `RandomArtifact_ret`, `Random_ret`); the tooltips were set without an error in every run, but the robot cannot hover, so nobody has seen them on screen yet.

**Found on the way:** the lobby's arrow kinds are a row's pending request type `+0xc4`: 1 state, 2 team, 3 faction, **4 hero** (`hero_change_*`, `0x007138e0`/`0x00713900`), **5 start bonus** (`bonus_change_*`, `0x00713920`/`0x00713940`). The host's handler asks the game setup (`NWorld::CPlayersStartInfo`, vtable `0x00e92744`) first: `+0x3c`/`+0x40` for a hero, `+0x44`/`+0x48` for a bonus. The robot's `arrow` step has `<row> bonus left|right` and `<row> hero left|right`. ToE's resources bonus is 3 to 6 of one random resource (seen: +4 sulfur, +6 mercury, +5 gems, +4 mercury); gold is +500 to +1000 (seen: +900, +800).

**Robot checks** (2026-11-01, LAN on A1M4, `[rules] trace=1` on both copies; the console lines are each copy's):

| Run | Rules | Lobby | Result |
|---|---|---|---|
| `20261101-225141-lan` (RC19k) | `toe_bonuses=1 game_mode=1` | host: gold (2× right), client: resources (3× right) | Both PCs served the same script (hash `cfaff75f`). `H55 Players chose Default mode`. Host: gold 20900 (difficulty base 20000), client: gems 15 (base 10): **both kept their bonus**, no MMH5.5 artifact. The panels: the host's editable, the client's read only with the host's values (screenshots). **332 world checksums, all equal.** |
| `20261101-225339-lan` (RC19k) | MMH5.5's way (no start rules) | the same picks | Script served with only the trace line. MMH5.5 as before: `H55 Players chose more difficult mode` (gold → Savage), `All AI Players have allied against the human players`, gold 20000 and gems 10 (taken back), MMH5.5's artifacts 113 and 61. The tooltips stayed the game's own. **332 checksums, all equal.** |
| `20261101-225529-lan` (RC19k, client `texts=fr`) | `toe_bonuses=1 game_mode=2` | host: minor artifact, client: random | Same hash on both (`08191c58`). Savage: the AI heroes fight to death and the AIs ally against the humans. Host kept its ToE artifact (175), the client's random gave an artifact (14); **one artifact each** (no MMH5.5 extra). **324 checksums, all equal.** |
| `20261101-225727-lan` (**RC19d**, `C:\Games\H5_test`) | `toe_bonuses=1 game_mode=3` | host: resources (1× left: random wraps to resources), client: gold | Same hash on both (`ce53e759`). `H55 Player chose most difficult mode, AI heroes are unlootable and fight to death`, AIs allied. Host +4 mercury, client gold 20800. **327 checksums, all equal.** |
| `20261101-224738-lan` (RC19k, first try) | `toe_bonuses=1 game_mode=1` | the robot pressed the **hero** arrows (type 4) by mistake: both bonuses stayed random | Both humans got a random resources bonus and kept it; Default. 326 checksums, all equal. |

Not robot-checked: a client without the add-on with these rules chosen (the step 1 path, already checked), a client whose start script differs (the hash check: RC19d and RC19k have the same part, so no test copy differs), Random mode, a scenario or a campaign map.

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

**Versions.** The carried text starts with `OHR2` (the lobby's with `L2`; step 1 used `OHR1`, `L1`); a PC refuses a text with an unknown key or a bad value (`Decode`), and the hello carries `kProtocol` (2 since step 2). A new rule = a new protocol version, so an older PC is treated as "without the mod" and nobody plays on rules it cannot apply.

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
- **The lobby's map without a map script** (a random map straight from the generator): the host reads the map's `map.xdb` once (`HasMapScript`), logs it, and both panels say the rules change nothing on it; the button gets its red mark.
- **Client** (waiting room): the same rows, read only, as the host sent them; "They will be used: every player has open-homm5" or why not.
- **In game** (adventure map): this game's rules, read only.
- **Words:** French or English (`[ui] language`, `overlay::PanelLanguage`).
- **The key:** F2 (`[rules] key`). The game binds F4 to F8 and F10 (options, saves, menu); we use F9 (player list), F10 (test panel), F11 (builder), F12 (hang test). F2 is free. **Temporary**, until the owner's sketch places the button.
- **The button:** "Rules..." at the right end of the waiting room's title row (`[rules] button=1`). It reads the mouse itself, like the builder's fallback button; the game gets the same click, on its title, which does nothing.

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

- **MMH5.5's scripts in multiplayer need a map script.** A random map straight from the game's generator has `<MapScript/>` (empty) in its `map.xdb`; in a multiplayer game no `advmap-startup.lua` runs for it, so none of MMH5.5's script features run there (governors, conversion, town gate, mine guards, mixed stacks, the AI bonus, the random map extras, the Savage/Nightmare modes' script part). In single player they do run on the same map. MMH5.5's Mapmixer (and the map editor) save random maps with a script; the owner's saved maps (`MAP_JoacJoz`, `6P-epic-...`, `TEAMJOACJOZ`...) have one, while `MMH55_RMG_Map_002` / `_007` and the robot's rebuilt copies do not. Making the scripts run on generated random maps in multiplayer would change the game for everyone (a separate decision, see "Later").
- The scripts open `H55-Settings.lua` once per map start, on every PC, about 0.3 s after the world starts (after `advmap-startup.lua`, `events.lua`, `advmap-common.lua`, `common.lua`, `H55-Tables.lua`, `H55-Core.lua`, `H55-Library.lua`).
- With one client, the host serializes `CMPStart` three times (the console has "SendDirect: command: class NMP::CMPStart" to PC 0, the host itself, and to PC 1; the third is probably a size or checksum pass): each carries the chunk. The host's game does not read its own copy back (no "the host's start message" line in its log).

## Later

1. **The class skill lock** ([lobby-audio-classes.md](lobby-audio-classes.md) item 4) in the same panel: a new group, new keys in the carried text (protocol 3), served data files (`HeroClass.xdb`, the hero files) the same way. (The ToE start bonuses, item 3, are step 2.)
2. **The button's place** from the owner's sketch (and one on the setup screen).
3. **The check in the co-op hello** too, for games joined without our lobby (a second ignored checksum message with the served file's hash).
4. **MMH5.5's scripts on generated random maps in multiplayer** (an owner's choice: it changes the game; MMH5.5's way is the Mapmixer). Where to start: the two places that run `scripts/advmap-startup.lua`, `0x00603b20` (called from `0x006089a0`) and `0x00992d00` (called from `0x009941a0`; it returns early when two bytes of the world's settings, its slot `0x14`, are both 0). Not studied further.
5. Single player and hotseat could use the host's choices too (today they use the file).
6. **The bonus icons** with the ToE bonuses on: ToE's own pictures instead of MMH5.5's mode pictures (step 2, "Gaps"). And the owner's look at the new tooltips by hand (hover the bonus icon in the waiting room).

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
| World start | `NUI::CRunMPGame` slot 7 (`0x00e3a498` → `0x0071d660`); `NUI::CStartSingleAdventureMap` slot 7 (`0x00e454dc` → `0x007e1030`) |

# Turn features: undo End Turn, resume after a crash, parallel AI turns

Status (2026-11-04): part 1, undo End Turn, **built** (`[coop] undo_end_turn`, off by default; robot-tested, waiting for a hand test); part 2, quick resume, **built** (the save at the drop and its transfer over the virtual LAN; robot-tested over the loopback, not yet over real Steam); part 3, parallel AI turns, **built** as the host's setting (`[ai] parallel_turns`, off by default, multiplayer only, agreed between the PCs by the co-op features' hello; robot-tested). Three ideas from the owner's games of 2026-11-03 and 04, in the order we plan to build them. Each one is a setting that is **off by default** (the original game), chosen by the host for the whole game. Addresses are in the vanilla 3.1 exe (the same in `MMH55_Utility(_64).exe`). What players, turn states and "pressed End Turn" are: [players.md](../subsystems/players.md). How the PCs stay in step: [multiplayer-sync.md](../subsystems/multiplayer-sync.md).

## 1. Undo End Turn (while the others still play)

**The wish** (owner, 2026-11-04): with simultaneous turns, a player who pressed End Turn too early can take it back while the other humans have not finished.

### How the game ends a human's turn (static reading, 2026-11-04)

- **End Turn** (`NWorld::CEndOfTurn` → the world's slot `0xd0`, `0x00998760`): for a living player whose turn state is 0, a human only gets "pressed End Turn" (`+0x178` = 1, the player's slot `0x50`, `0x00500c50`, which also tells the player's watchers that it changed: `0x0050c020`). A computer player's End Turn goes straight to the next turn (`0x00998060`).
- **Every world tick** (`0x009987b0`, which ends with `0x00998520`), the turn manager `0x00998520` decides:
  - if a computer player is at state 0 (it is moving): nothing;
  - **if some player at state 0 has not pressed End Turn** (`0x0098d570`): someone is still playing. With simultaneous turns (`world +0x90`), each player at state 0 who **has** pressed End Turn gets state **1** ("done for today"; state 2 instead when `world +0x8a` is set). That is why a human who presses End Turn gets state 1 "at once" (robot, 2026-10-25);
  - else (everyone at state 0 has pressed End Turn) and nobody is at state 2 (`0x0098d5c0`): **the next turn** (`0x00998060`, "GiveTurnToNextPlayers"), in the same tick: every `+0x178` is cleared and the computer players' turns begin.
- Without simultaneous turns the humans play one after the other, so there is nothing to undo.

### What "undo" would do

Set the player back to **state 0** and **`+0x178` = 0** (the player's own setters, slots `0x48` and `0x50`, so the game's watchers hear of it). The next tick then sees one more player still playing, as before the click.

It must happen **at the same moment of the game on every PC** (it changes the world). The way our other shared changes do it (hero looks, `hero_skins.cpp` part 5): the request travels as one of our small messages next to the world checksums, and every PC applies it **at the start of the same segment** (the segment runner's call `0x007c9a67`, before that segment's commands). At that moment every PC checks, on the same world, that the undo is still allowed:

- simultaneous turns are still on (they stop for the rest of the game when human heroes meet: `CBreakSimultaneousTurnsCmd`);
- the player is alive, human, at state 1 (or 0) with `+0x178` = 1;
- **another human is still playing** (at state 0, End Turn not pressed). If the last human ended the turn in the meantime, the computer players have started and the undo is refused, the same way on every PC.

**On screen:** a button in the game's own style, next to the player list's "Turn ended" line or near the End Turn button, only while the undo is possible, and a key. Words in en/fr.

### Robot experiment (2026-11-04): it works

`src/mods/turn_undo.cpp`, robot runs only (no setting, no button yet): the robot step `panel|turn undo <player> <segment>` (given to both copies) undoes the player's End Turn at the first segment from `<segment>` on where it is allowed, on every PC; `panel|turn states` logs every player's state. To get a picture of the "ended" screen, the undo waits 30 segments after it is first allowed, and a second picture comes 30 segments after it. New robot options: `run_lan_test.py --client-end-turn-every N` (the client plays on while the host has ended its turn) and `--no-stuck-exit` (the host's robot would else close a copy that waits for the other's End Turn).

| Run (test copy `C:\Games\H5_test_k`, A1M4, 2 humans + 1 computer, simultaneous turns) | Result |
|---|---|
| `20261104-082135-lan` | Both PCs: `segment 60: player 1 plays again (before: 1h:1/1 2h:0/0 3ai:1/0; after: 1h:0/0 ...)` (number, human, state / pressed). The host's robot pressed End Turn again 5 s later, and it counted (`1h:1/1` at segment 355): the game takes the player's End Turn again. The day went on (the client ended, the computer played, day 3). **2254 world checks, all equal.** |
| `20261104-082502-lan` | The undo also works between the click and the next world tick (`1h:0/1` → `1h:0/0`). 1868 checks equal. |
| `20261104-082900-lan` | Pictures of the host: **before** (segment 150), the game's own banner "Veuillez attendre les autres joueurs..." and the player list "1/2 Prêts", the host ticked; **after** the undo (segment 180, the picture at 210), the banner is gone, "0/2 Prêts", the host back to "playing". Both PCs at the same segments. 1668 checks equal. |

So **the game's screen follows the state change by itself**: nothing else needs calling. Both open questions are answered (state 1 does nothing else here: `world +0x8a` is 0 in these games, so the turn manager uses state 1, not 2).

### Built (2026-11-04)

How it works for players, the setting and the log lines: [src/mods/README.md](../../src/mods/README.md), "Undo End Turn". In short: a hello from each PC at each game's start (the host's says whether it allows it), one decision at segment 20; a click sends "undo player P", which every PC takes in at the same segment (the sender's report + 10) and checks there on the same world (`undo::Allowed`, unit-tested in `src/hook/test/turn_undo_test.cpp`); a button "Reprendre mon tour" under the game's banner, whose clicks the overlay keeps from the game (`overlay::BlockClicksIn`, new). The robot experiment's direct undo (`panel|turn undo`) is gone: the robot now asks as a player would (`panel|turn request`).

| Run (test copy `C:\Games\H5_test_k`, A1M4, 2 humans + 1 computer, simultaneous turns) | Result |
|---|---|
| `20261104-084713-lan`, the host's `undo_end_turn=1` | Hellos both ways, `on for this game` on both at segment 20. The host ended its turn: the button "Reprendre mon tour" shows under "Veuillez attendre les autres joueurs...". The robot's click: `request sent for player 1 (after this PC's report 156; it counts at segment 166)`; the client `PC 0 asks to undo player 1's End Turn (counts at segment 166)`; **both** `segment 166: player 1 plays again`. The picture after: banner and button gone, "0/2 Prêts". 1840 world checks equal. |
| `20261104-085100-lan`, a forced request while the host still plays | Both PCs at segment 49: `PC 0's request for player 1 refused: the player has not ended its turn`. 1066 checks equal. |
| `20261104-085254-lan`, the default (`undo_end_turn=0`) | Both: `off for this game (the host does not allow it)`; no button (`robot: no button now`). 1065 checks equal. |

**Not covered by the robot:** a real mouse click (the robot cannot click): that the click on the button does not also send the selected hero to the map tile under it. The owner's hand test checks it. Also not yet: three PCs, and a request that crosses the last other human's End Turn (the rule is the same as the forced request above: refused on every PC, the computer players play).

## 2. Resume after a crash

**The wish** (owner, 2026-11-04): a friend's game crashed; could they come back?

### What happens today (robot run `20261104-080739-lan`, two copies, the client's process killed in the middle of day 8)

- The host's lockstep stops: no segment for ~20 s (the client's reports no longer come).
- Then the host's transceiver **drops the player**: `0x007c8d10` (`CMultiPlayerTransceiver`, just before the pacing slot `0x007c8dd0`) clears the player's bit in the active players mask (`+0x58`), sets its reported segment to -1 and prints `Drop player 1` to the game's console.
- The host gets the game's box "Infos multijoueur: Robot-client a été déconnecté" (OK), and **the game goes on without that player**: from the next turn its letter is `N` (not in the game) in `GiveTurnToNextPlayers`, and our player list no longer shows it.

So a save made **after** the drop no longer has that player in the game: reloading it would not bring the friend back. Today the group can reload the **last autosave** (the start of the day, if autosaves are on), losing part of a day.

### Plan, in two steps

1. **Quick resume** (smaller): when the host drops a human, the host's add-on saves the game **at that moment**, before the player is taken out of the world, as "Reprise - <player>" ("Resume"). A banner says: "<player> left. Saved as Resume: Multiplayer, LAN, Create, Load to play on with them." Everyone (the friend included, after restarting the game) joins the loaded game; the loaded game's waiting room now has "Invite friends..." (0.3.1). To find: when exactly the world takes the player out (a command after the drop, or the next turn), and how to make one save on the host alone (the save code our autosave speed-up already knows: `autosave.cpp`, [save.md](../subsystems/save.md)). The world is the same on every PC, so the host's save is enough.
   **Built (2026-11-04):** `src/mods/quick_resume.cpp`, `[coop] resume_save` (the host's, on by default). Found with the robot (`--trace-mp`, run `20261104-091110-lan`): right after the drop the host sends `NWorld::CDefeatPlayerCmd` (vtable `0x00e0e75c`, the player at `+0x10`) through the transceiver (slot 0): that is what takes the player out. The mod sends, just before that defeat, an `NMP::CMPSaveWorldCmd` named "Reprise" ("Resume"; `0x008236d0`, as the autosave request does); the commands of a segment run in order, so every PC still connected saves the world with the player in it. Robot `20261104-091909-lan`: `the resume save sent to every PC 94 ms after PC 1 left, just before the defeat of player 2`, the host saved "Reprise" in 77 ms, and a banner shows on the adventure map.

   **The catch: the player who crashed does not have the save.** To join a loaded game, each PC loads its own copy of the save: the client's game says "Vous n'avez pas de sauvegarde avec cette partie" without it (robot `20261104-092313-lan`). With the host's file copied into the client's Saves folder, the client joins the loaded game in its own seat (player 2, `20261104-092739-lan`). So the friend needs the file. The owner chose (2026-11-04): **the add-on sends it**, over the virtual LAN (Steam; the robot's loopback).

   **The transfer** (`src/mods/resume_transfer*.cpp`): every PC with the virtual LAN on asks the hosts it knows, every 4 s, "do you offer a save?" (a 4-byte message on channel `0xff52`). A host offers the last resume save it made (its file, kept in `h5-together-resume.txt` so a restarted game still offers it: `quick_resume.cpp` learns the file from the game's `CreateFileA` while our save command runs). If this PC has no save of that size and CRC-32 in its saves folder (learnt from the game reading `...\Profiles\<name>\user_a2.cfg` at start-up), it asks for the file, 32 pieces of 1800 bytes at a time, and asks again for a window that did not all come (the loopback is plain UDP; Steam's messages are reliable). Then it checks the CRC and writes `h5together-reprise.sav` into its saves folder. The LAN screen says how far it got. Unit-tested (`resume_transfer_test.cpp`: a 1.1 MB file, also with every 7th message lost).

   | Run (test copy, two copies over the loopback virtual LAN, `--vlan`) | Result |
   |---|---|
   | `20261104-100349-lan`: the client copy killed on day 9 | The host: `the resume save sent to every PC 94 ms after PC 1 left, just before the defeat of player 2`, `the resume save's file: sauvegarde_3.sav`, saved in 77 ms. |
   | `20261104-100659-lan`: the host loads "Reprise" (`--host-load reprise`); a fresh client | The host: `this PC offers its resume save (sauvegarde_3.sav, 1143948 bytes, CRC 6d00dc41)`. The client: `this PC lacks it, fetching it`, then 25/50/75%, `the host's resume save is here (1143948 bytes, checked), written as h5together-reprise.sav` (47 ms on the loopback). The client's Join then put it **in its own seat** of the loaded game (player 2). |

   **Not tested yet:** over real Steam between two PCs (the channel is the same code; Steam's speed decides the time: about 1 MB/s through a relay); starting the loaded game with the robot (its "ready" arrows do not know the loaded game's waiting room yet).
2. **True reconnect** (big, later): the friend joins the running game. The host would send its world to the friend (the save, a few MB), the friend loads it and joins the lockstep at the right segment. A project of its own.

## 3. Computer players' turns in parallel

**The wish** (owner, 2026-11-03): AI turns are long with 3+ computer players. Either the computer players move **at the same time as each other** (a separate phase, after the humans), or even **at the same time as the humans**.

### What we know

- The turn manager above starts the computer players one after another, in player order (`0x00998060`; one AI at state 0 at a time).
- Only the host runs the AI; its decisions are commands sent to everyone ([ai-turn.md](../subsystems/ai-turn.md)). The AI's thinking happens between frames on the host.
- The game has its own "simultaneous turns" for **humans** (the setup screen's `simultaneus` option), which stops when human heroes meet.

### To study before any code

- Can two computer players be at state 0 at the same time without the AI code breaking? (Each AI object thinks for its own player; they share the world, the pathfinder and the "AI busy" machinery.)
- **Determinism is not the risk** (only the host thinks; everyone runs its commands in order). The risks are the AI's own assumptions (one AI moving at a time) and battles between two AIs that both move.
- First step: a robot experiment where the turn manager gives the turn to all computer players at once, on a test map, AI only. Off by default, host's choice, like the rest.

### The experiment (2026-11-04): it works, and the computer players' part of the day is ~40% shorter with two of them

`src/mods/parallel_ai.cpp`, the host's setting `[ai] parallel_turns` (off by default). Every PC must do it, or none: each PC's DLL says in its hello at the game's start which features it knows (`src/mods/coop_features.cpp`, as undo End Turn's hello), and a phase starts only inside the multiplayer segment runner, in a game where every PC knows it and the host wants it. Single player: not used (yet).

**What the game assumes, and the four hooks:**

- The AI manager (`0x006082f0`) drives the AI of "the computer player playing now", found by `0x0098d480` (players, dl = 0, 1), which answers **nothing when two are at state 0**. Hook at `0x00608658`: during a phase, the next computer player of the phase not done yet. The pick moves on only once that AI has really been driven (`0x00608180` reached, hooked at `0x006086d2`): the manager skips a pass right after another AI's move (the controller's state `+0x44`), and a strict alternation starved one AI (robot `20261104-104844`: player 2 picked 23 times, driven 9, all after player 3 ended).
- The wait between two steps of one AI is per player: the world's "is this player busy?" (slot `0xa4`, `0x0098e1c0`: that player's actions in progress, list `+0x5c`, against the game time minus 400 ms, `0x007c8b00`). It never said "busy" in these runs.
- Start of the computer players' part of the day: the world's tick (`0x00998520`, hooked at its call `0x0099892e`), before it runs: when one computer player has just got the turn and no human plays, every other computer player in the game gets state 0 (its own setter, slot `0x48`). They make up the phase.
- A computer player's End Turn (the world's slot `0xd0`, `0x00998760`), which would call the next turn at once: during a phase it is only marked done, unless it is the last. Then the game's own End Turn runs; inside its next turn (`0x00998060`), the call at `0x009980a9` gets "the computer player playing now" = the last of the phase in player order, so the day moves on as after the last computer player, and every player of the phase (still at state 0) gets the game's end-of-turn work (`0x0099827a`: its heroes' slot `0x1e4`, its slot `0x60`).
- Our AI pacing (`ai_pacing.cpp`, one decision per frame, and never before the AI's own last moves ran) now keeps its wait **per AI**: with a single AI at a time it is exactly as before.

**Results** (test copy `C:\Games\H5_test_k`, robot):

| Run | Computer players' part of a day | Notes |
|---|---|---|
| Single player, A1M4, 1 human + 2 computer players, normal (`20261104-103747`) | days every 11.9 s | |
| the same, parallel (`20261104-105314`) | days every 9.4 s | the game's console: `GiveTurnToNextPlayers before: W C C`, two current players; no crash. The AI manager makes one decision per game step (200 ms in single player): that limits it. |
| LAN, two copies, random map EDD31DF8, 2 humans + 2 computer players, normal (`20261104-105824-lan`) | **4.9 s** on average (14 days) | 2953 world checks, all equal |
| the same, parallel (`20261104-110218-lan`) | **3.0 s** on average (15 days): **39% shorter** (ideal with two: 50%) | **2958 world checks, all equal**: both PCs ran the parallel turns the same |
| Single player, the owner's MAP_JoacJoz (with its map script: MMH5.5's scripts run), 1 human + **3 computer players**, normal (`20261104-112338`) | **18.8 s** on average (10 days in 4 minutes) | |
| the same, parallel (`20261104-112741`) | **10.2 s** on average (14 days in 4 minutes): **46% shorter** | `W C C C`: three playing at once; the same script messages as the normal run (MMH5.5's own "not belong to AI player", on purpose); no freeze |

With the agreement (2026-11-04, `20261104-133921-lan`, the same random map, 2 humans + 2 computer players, the script clock on too): 20 phases, 3.3 s median, **4071 world checks, all equal**.

**Not looked at yet:** big maps with long AI turns, MMH5.5's scripts (events per player turn), battles between two computer players moving at the same time, how the game's own screens show two players "thinking".

### The bug of 0.3.2 to 0.3.5: only the first computer player moved its heroes (found 2026-11-09)

**What the owner saw** (a long 3-human game against three computer players, 0.3.4 then 0.3.5, the 6-player builder map): one computer player strong, the two others "barely an inconvenience": small armies, no mines taken.

**What the logs said.** The phase's research counters (the host's `parallel ai: ... ended its turn, the last of phase N (... picks/busy/free/reached ...)`) gave, per day: the first computer player of the phase 50 to 190 AI steps, its heroes often "busy" (walking); the others 10 to 20 steps, **never busy after day 2**. When the first one was beaten (day 73), the next one in player order became the first and moved at once; the last stayed still. The robot benchmark of 2026-11-07 showed the same with four computer players: hero moves per day (a step whose commands include `CStartMoveHeroCmd`) 7 on day 1, then **0** for all but the first. The earlier robot measures (39% and 46% shorter, above) were taken with this bug: the computer players after the first did almost nothing.

**Why.** The AI's MOVE_HERO stage picks its next hero with `0x00869770`: only a hero whose world actor (world slot `0x100`, `0x0098dae0`, the list at world `+0x5c`) says yes at its second base's slot `0x44` (`0x008db210`: movement points over 999). A player's heroes get their movement back in **the start of its turn**, `0x00994920` (`__thiscall(world, player)`, only for a player at state 0): the new week's message (`0x009946e0`, once a week), every hero's and both tavern heroes' slot `0x220`, then world slot `0x1fc` (`0x0098e8a0`) for its heroes. The next turn (`0x00998060`) runs it at its end (`0x00995ef0`, for every player, so only for the ones it just gave the turn to). The phase's other computer players got state 0 from our tick hook, one tick later, through their state setter only: they kept the day before's empty movement. On day 1 every hero starts with full movement, so the bug showed from day 2.

**The fix (0.3.6).** At the phase's start, after the state setter, our tick hook runs `0x00994920` for each computer player it brings in, as the next turn does for the first. It changes the world, so every PC must do it: the co-op features' hello has a new "knows" bit, `parallel_ai_starts` (bit 4), and this version no longer says it knows the old `parallel_ai` bit. The host still asks with the old bit; the phase runs only if every PC knows the new one, and a 0.3.5 PC (which would run the phase without the fix) sees the feature off too (`coop_features_logic.h`, tested in `coop_features_test.cpp`).

**Robot, after the fix** (the same benchmark: two copies of the original game, the 320x320 builder map `OH-X1-23321gnnpnrxssxa`, two humans and four computer players, 5 days; `parallel ai research: heroes able to move ...`, robot only, logs each computer player's heroes that can move when it changes):

| Run | Hero moves per computer player over 5 days | Computers' part of a day |
|---|---|---|
| one after another (`20261107-192850-lan`) | 48, 22, 48, 14 | 20.1 s (days 2-5: 18.1) |
| 0.3.5 together (`20261107-191636-lan`) | 33, 7, 10, 9 (the last three: all on day 1) | 16.2 s (14.0) |
| **0.3.6 together** (`20261109-005537-lan`) | **36, 32, 36, 33** | **17.6 s (15.7)** |

At each phase's start every computer player's heroes can move (day 2: 2/2, 2/2, 3/3, 2/2; before: 2/2, 0/2, 0/3, 0/2). The last one to end its turn changes from day to day. Together is about 12% shorter than one after another on this map: the first computer player, an ally of the humans whose walks are shown (and so take time), sets the pace.

The same game with world checksums (`20261109-005945-lan`, 3 days): **1302 world checks, all equal** on both PCs; hero moves 18, 14, 25 and 12.

**MMH5.5** (`MMH55_Utility_64.exe`, its scripts on, random map `EDD31DF8`, two humans and two computer players, 12 days, world checksums on): together (`20261109-010315-lan`) 4.7 s a day, hero moves 80 and 84, every hero ready at each phase's start, 2195 checks all equal; one after another (`20261109-010652-lan`) 5.4 s a day, hero moves 80 and 114, 2318 checks all equal. So together is about 13% shorter here, not the 39% measured with the bug. The second computer moved less in this run (84 against 114): one run each, perhaps only other choices (it sees the first one's moves as they happen); to watch in real games.

**Long games and a crash: off by default in 0.3.6.** Four 15-day robot games on the 320x320 map (original game, four computers, no checksums):

| Run | Mode | Days | Hero moves per computer | Computers' part of a day |
|---|---|---|---|---|
| `20261109-011551-lan` | together | **crashed on day 9** | 56, 47, 71, 53 (8 days) | 18.5 s |
| `20261109-013257-lan` | together | 15 | 120, 99, 100, 103 | 20.6 s |
| `20261109-014116-lan` | together, `fast_paths` off | 15 | 89, 98, 102, 108 | 19.0 s |
| `20261109-012102-lan` | one after another | 15 | 115, 116, 115, 58 | 23.4 s |

The crash (host, `h5-together-host-crash-20261109-012042.txt`): an access violation reading `0x3000` in a buffer copy (`0x008c7650`, from `0x008c7e70`) inside `0x0050c7c0`, slot 8 of the world's change log (world slot `0x98`, vftable `0x00e1a540`): the log's "back to position N", which copies saved entries back. The AI uses that log to try things in its head and undo them (the turn driver keeps the position at `+0xdc`; `0x00bc5480`, under a hero estimate `0x00bc5740` called from the pathfinder's tile check `0x009d15e0`/`0x009d1700`, rolls back to a position it holds at `+0x14`). The saved entry it copied from was garbage. A guess, not proven: a position kept by one computer's AI no longer matches the log after another computer's moves ran in between. Until 0.3.6 this never showed, because the computers after the first did not move. Not seen in the run without our ported pathfinder, nor one after another, but that is two runs. So **0.3.6 ships with `[ai] parallel_turns=0` by default**: the gain is only 12-19%, and the game's own order is safe. Next: reproduce it (a save from the crashed run's day 8, or many long runs), then find who keeps the stale position.

## Order

1. Undo End Turn: built (2026-11-04). Next: the owner's hand test (a real click on the button), then a release.
2. Quick resume: find when the world removes a dropped player; then the host's save at the drop, the banner, the robot test (kill a copy, reload, rejoin).
3. Parallel AI turns: built (0.3.2, the host's setting; on by default in 0.3.5, off again in 0.3.6). Its first measures (~40% shorter) were taken with the bug above; fixed in 0.3.6, 12-19% shorter. Next: the crash above (reproduce, then find the stale rollback position), then the owner's hand test.

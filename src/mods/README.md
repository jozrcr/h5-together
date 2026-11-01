# Mods: our improvements to the game

Everything here changes how the original game behaves, on purpose. It is kept apart from the decompiled code (`src/decomp/`), which must stay byte-for-byte faithful. The mods are built into the hook DLL (`src/hook/`, `dinput8.dll`) and switched on and off with a settings file, `open-homm5.ini`, next to the game exe.

**Contents**

- Faster co-op: [LAN link tuning](#lan-link-tuning-net_tuningcpp), [LAN games over VPNs](#lan-games-over-vpns-lan_discoverycpp-on-by-default), [faster co-op segments](#faster-co-op-segments-coop_guardcpp-on-when-every-player-agrees), [quicker lockstep](#quicker-lockstep-lockstepcpp-the-drain-and-the-trace-on-by-default), [one AI decision per frame](#one-ai-decision-per-frame-ai_pacingcpp-on-by-default), [quicker autosaves](#quicker-autosaves-autosavecpp-the-trace-and-level-1-on-by-default), [fast AI pathfinding](#fast-ai-pathfinding-fast_pathscpp-on-by-default)
- Interface: ["your turn" notification](#your-turn-notification-notifycpp-on-by-default), [player list](#player-list-player_listcpp-on-by-default), [hero looks](#hero-looks-hero_skinscpp-on-but-nothing-changes-until-a-look-is-chosen), [French texts](#one-language-french-texts-languagescpp-off-by-default)
- Random maps: [teams](#teams-on-random-maps-rmg_teamscpp-on-by-default-only-for-recipe-maps), [templates and recipe names](#random-map-templates-rmg_templatescpp-rmg_makercpp-rmg_recipecpp-on-by-default), [template builder](#template-builder-rmg_buildercpp-rmg_screencpp-on-by-default), [start terrain that matches the factions](#start-terrain-that-matches-the-factions-rmg_factionscpp-off-by-default)
- Rules: [game rules from the host](#game-rules-from-the-host-host_rulescpp-on-and-nothing-changes-unless-the-host-changes-something)
- Camera, sound and visuals: [original music](#original-music-original_musiccpp-off-by-default), [camera zoom and start distance](#farther-camera-zoom-start-distance-cameracpp-off-by-default), [the mouse wheel under Wine](#the-mouse-wheel-under-wine--proton-wheelcpp-wheel_logiccpp-on-under-wine-only), [wider volume range](#wider-volume-range-audio_volumecpp-off-by-default), [black past the map's edge](#black-past-the-maps-edge-edge_skycpp-off-by-default)
- The freeze detector (hang watchdog) is part of the DLL itself: [src/hook/README.md](../hook/README.md#the-hang-watchdog-watchdogcpp).

## LAN link tuning (`net_tuning.cpp`)

**What it fixes:** slow AI turns in co-op (LAN and GameRanger games).

In a multiplayer game only the host computes the AI, and each AI move has to reach the other PCs before the next one can start. The game's network link was tuned for 2006 modems. It sends at most one small packet every 50 ms (about 5 KB/s), and a PC with nothing to send acknowledges only once per second. During AI turns the host's link is full, every PC thinks the others are lagging and slows itself down, and each AI move waits 1 to 2.5 seconds.

The mod changes three network timings on the PC that runs it:

| Setting | Game | Mod |
|---|---|---|
| `keepalive_ms`: how often an idle link sends (and so acknowledges) | 1000 | 50 |
| `send_interval_ms`: how often a link may send a packet | 50 | 10 |
| `batched_segments`: how many "segment done" reports an idle PC holds back | 3 | 0 |

Game commands still run in the same order on every PC; only the network gets faster. **Every player should run it**: each PC only speeds up what it sends.

**Measured** with the robot (two copies of the game on one PC, 2 human players plus 1 computer player, map Trèfle), seconds per AI move:

| | per AI move | typical AI turn |
|---|---|---|
| Game's timings | 2.5 s | 15–36 s |
| With the mod | 0.6 s | 2–7 s |

(Each change alone helps less: send interval only 2.2 s, keepalive only 1.6 s, both 0.9 s, all three 0.6 s.) Over the Internet the ping between the PCs comes on top.

**Settings** (`open-homm5.ini` next to the game exe; without the file the mod is on with the values above):

```ini
[net]
tuning=1             ; 0 = the game's original timings
keepalive_ms=50
send_interval_ms=10
batched_segments=0
```

The DLL's log (`open-homm5.log`) says what it changed, for example `net tuning: links may send every 10 ms (game: 50)`.

**MMH5.5 RC19k** sets the send interval to 20 ms itself (`<TransmissionInterval>` in `MMH55Stats.xml`, applied by `55u.dll` at start-up). The mod also accepts that value and still makes it 10 ms; the log then says `(game: 50, MMH5.5: 20)`. Measured on RC19k (A1M4, robot LAN run): 0.46 s per AI step with MMH5.5's 20 ms, 0.26 s with 10 ms (RC19d with 10 ms: 0.26 s). `send_interval_ms=0` keeps MMH5.5's 20 ms.

## LAN games over VPNs (`lan_discovery.cpp`, on by default)

**What it fixes:** over a virtual network such as ZeroTier (for a friend on Linux, or without GameRanger), the host's game never shows up in the LAN list.

The game finds hosted games by broadcasting a small "any games here?" message about once a second. Windows and Linux send that broadcast out of the main network card only, so it never reaches a VPN. The mod also sends it into **every network adapter**, the VPN's included. For VPNs that carry no broadcasts at all (Tailscale, for example), list the host's address in `lan_peers`; the mod then also asks the host directly. The game's own packets are not changed.

The log says where the messages go, and where games were found and joined:

```
lan: game queries also go to 192.168.1.255 (Ethernet 3), 10.21.174.255 (ZeroTier One [...])
lan: game found at 10.21.174.28:8889 (826 bytes)
lan: join request to 10.21.174.28:8888
```

**Settings** (`open-homm5.ini`):

```ini
[net]
lan_all_adapters=1   ; 0 = only where the game itself sends (the original)
lan_peers=           ; e.g. 10.21.174.28 (comma-separated): also ask these hosts directly
```

Every player who looks for games needs it (the host only answers). On a Windows host the VPN's network must be marked **Private**, or the game's firewall rule blocks it.

## Faster co-op segments (`coop_guard.cpp`, on when every player agrees)

**What it fixes:** in co-op every AI move waits a few "segments" of 200 ms, the steps in which all PCs move the game forward together. Shorter segments make AI moves faster.

**Why it needs agreement:** the segment length decides when things happen in the game world, and every PC runs that world itself. So all PCs must use the same length, or the game desyncs.

**How:**
1. At the start of each multiplayer game, every PC's DLL sends a small hello to the others with the segment length it wants. A PC without our DLL ignores it.
2. Ten seconds in (segment 50), each DLL checks that **every** other PC sent the same wish.
3. If so, they all switch together at that segment. If not (someone has no DLL, an older one, or other settings), nobody switches and the game keeps its own timing.

`open-homm5.log` says which:

```
co-op timing: hello from player 1 (asks for 100 ms segments, protocol 1)
co-op timing: ON from segment 50: 100 ms segments (all 2 PCs agree)
```

**Measured** (robot LAN game, 2 humans + 1 AI, 8 AI turns, network tuning on):

| | game's 200 ms | 100 ms segments |
|---|---:|---:|
| per AI move | ~0.65 s | 0.53 s |
| AI hero move | ~0.84 s | 0.58 s |
| town swap | ~0.42 s | 0.34 s |

The world checksums stayed equal on both PCs (no desync). When the copies disagree, both stay OFF.

**Settings** (`open-homm5.ini`):

```ini
[coop]
guard=1           ; 0 = take no part (no hello; never switch), as without the DLL
fast_segments=1   ; 0 = ask for the game's 200 ms, so nobody switches
segment_ms=50     ; 50 or 100 (50 by default since 0.2.0)
```

## Quicker lockstep (`lockstep.cpp`; the drain and the trace on by default)

Why multiplayer clicks feel slow, and the measurements: [docs/plans/responsiveness.md](../../docs/plans/responsiveness.md). Each setting works on this PC only, so it needs no agreement between the PCs, and none of them changes what happens in the game world.

- **Drain:** the game's message pump (transceiver slot 3, 0x007ca7c0) takes in one "segment done" report per frame. With 4 PCs or a slow frame rate, that holds everyone back. The mod calls the pump again while it still takes in a report, up to two rounds of reports per frame.
- **View delay:** the game's "now" for the screen and the AI (slot 8, 0x007c8b00) is the timer minus 400 ms, so moves start at least 400 ms after their segment.
- **Catch-up (experiment):** the segment timer moves at most 150 ms per frame (0x00475d80), so a PC whose frame froze stays behind.
- **Pacing slope:** the game slows a PC that is ahead of the slowest one by 5% per segment of lead (slot 5, 0x007c8dd0: `speed = max(1 - 0.05 * lead, 0)`, the 0.05 a float at 0x00e12f18). That was made for 200 ms segments: with 50 ms segments, the same lead in milliseconds brakes 4 times harder, and a PC 1 s ahead stops. While co-op timing is ON, the mod points the instruction that reads it (`fmul` at 0x007c8ea2) at its own value, 0.05 × segment / 200 (0.0125 at 50 ms), and raises the lead's cap of 20 segments (0x007c8e7f, 0x007c8ef3; at the cap the speed is 0, which is what stops a PC from running further ahead) to 20 × 200 / segment (80). So the pacing is the game's per millisecond, and a PC stops 4 s ahead, as in the game. (The other `fmul`, at 0x007c8e64, is for a PC that only watches; it counts its lead in 200 ms units, so it stays as it is.) In the robot's games the host then made 17 to 20 segments a second, 20 being real time (without it: 15 to 18).
- **Finer clock:** Windows wakes sleeping threads only every 15.6 ms unless a program asks for more (`timeBeginPeriod`), and the game never does. While the drain or the trace is on, the DLL asks for 1 ms once. The game's network thread (0x0047cbd0) rests `Sleep(10)` between its rounds (`push 0x0a` at 0x0047cc05); `net_sleep_ms` makes that 2 ms, so the 10 ms send interval of the link tuning really is 10 ms.
- **Trace:** every 10 s, one log line with the wait after this PC's own report (average, p80 and longest), who reported last (count per player), frames per second, the drain's extra reports, and the spread of the players' segment counts. A second line times the commands this PC sent (clicks, and the AI's moves on the host), per kind: from sending to the run of their segment (average and longest), and to being shown on this PC (the run, or the margin after this PC's own report if that is later). It ends with a suggested `view_delay_ms`: the p80 of the waits, so that 4 moves in 5 start without a jump.

Example (robot, host, with the AI moving):

```
lockstep: 197 segments in 10 s, waited 117 ms on average (p80 208, longest 557) after this PC's own report; ...
lockstep: 31 commands from this PC, ms from sending to the run of their segment (and to being shown, margin 100 ms): CSetHeroAITargetCmd 8: 137 avg/239 max, shown 174; CSetWalkPathCmd 7: 122 avg/239 max, shown 165; ...; suggested view_delay_ms=208 (p80 of the waits 208 ms)
```

**Robot results** (2 copies on one PC; one run each, the AI decides differently each run):

| Setting | AI moves | AI hero moves | Checksums |
|---|---|---|---|
| Plain | 0.50 s | 0.61 s | all equal |
| Drain plus `view_delay_ms=200` | 0.44 s | 0.46 s | all equal |
| Drain plus `view_delay_ms=250` | 0.42 s | 0.57 s | all equal |
| Drain plus `catch_up_ms=600` | 0.45 s | 0.56 s | all equal |
| 0.2.0 defaults: 50 ms segments, view 100, catch-up 600 | 0.32 s | 0.30 s | 825, all equal |
| **Now:** plus pacing slope, finer clock, `net_sleep_ms=2` (two runs) | **0.27 s, 0.25 s** | **0.20 s, 0.26 s** | 895 and 788, all equal |
| The same with `scale_pacing=0` | 0.35 s | 0.39 s | 825, all equal |
| The same with `view_delay_ms=60` | 0.27 s | 0.23 s | 891, all equal |

```ini
[net]
drain_reports=1   ; 0 = the game's one report per frame
view_delay_ms=100 ; 50..400, only while co-op timing is ON (else the game's 400)
trace_lockstep=1  ; 0 = no summary lines
catch_up_ms=600   ; 150..1000, only while co-op timing is ON (the game: 150)
scale_pacing=1    ; 0 = the game's pacing slope also with co-op timing ON
net_sleep_ms=2    ; 1..10, the network thread's rest between rounds (the game: 10)
```

The robot passes them with `run_lan_test.py --net-setting KEY=VALUE`.

**A side effect, now fixed by `ai_pacing.cpp` (below):** with 50 ms segments and `catch_up_ms=600`, the game's AI manager runs several segments in one frame and calls the AI's turn driver in each of them, so on a slow frame the AI took a second (up to a sixth) decision in the same frame, before its first decision's commands had run. In the owner's game that froze, 17% of the AI's steps came that way; with the Oct 24 builds none did.

**Hardened:** the trace follows the players' report counts from one frame to the next; a count that went back or jumped by more than its 256-segment memory now starts the trace again, instead of a long loop (it never happened in the logs, but those counts are the game's memory).

## One AI decision per frame (`ai_pacing.cpp`; on by default)

**What it is for:** the game itself never lets its AI decide twice in one frame, or before its last moves have happened in the world: with the game's own timing (200 ms segments, the segment timer moving at most 150 ms per frame) the AI manager calls the AI at most once per frame, and the AI's commands run in a later frame's network pump. Our faster co-op timing broke that on slow frames (above), and it is the leading suspect for the owner's freeze of 2026-10-25 ([docs/subsystems/hangs.md](../../docs/subsystems/hangs.md)). This puts the guarantee back.

**What it does,** in multiplayer games (single player is untouched): the AI's turn driver is called at most once between two network pumps (once per frame), and after a call that produced commands, only once the segment holding them has run (no segment record numbered up to it is left in the transceiver's list after a pump). A call held back is simply not made; the AI gets it in a later frame. Only the host runs the AI and every PC applies the commands it sends, so this cannot desync. A safety valve lets the AI go on after 5 s of waiting (logged once; never seen). How: the two calls of the segment runner `0x007c98e0` in the pump (`0x007ca957`, `0x007ca97d`) go through us, after checking the runner's first bytes; the turn driver hook (`src/hook/ai_trace.cpp`) asks before each call. One line per AI turn: `ai pacing: this AI turn: N calls held for the next frame, M until the AI's own commands had run`.

```ini
[ai]
one_decision_per_frame=1  ; 0 = as 0.2.0 to 0.2.3 (several decisions per frame on slow frames)
```

**Robot results** (LAN, 2 copies, A1M4, 6 AI turns each; world checksums equal in every run):

| Run | Same-frame AI steps | AI time per step | Checksums |
|---|---:|---:|---|
| Slow host (every frame +60 ms, 10-13 frames/s), `view_delay_ms=50` (the owner's), **off** | 23 of 58 (40%) | 0.29 s | 976, all equal |
| The same, **on** | **0 of 67** (46 calls held a frame) | 0.42 s | 1181, all equal |
| Defaults (fast frames), **on** | 0 of 66 (none held) | 0.25 s (hero moves 199 ms) | 1085, all equal |

On a fast PC nothing changes (0.25 s per step, as the 0.2.2 robot runs); on a slow frame the AI waits one frame per decision, as the game does with its own timing.

## Quicker autosaves (`autosave.cpp`; the trace and level 1 on by default)

**What it is for:** in co-op games the autosave at the start of each round freezes every PC for a long time. Findings and addresses: [docs/subsystems/save.md](../../docs/subsystems/save.md).

**What the robot found:**
- **One PC asks, every PC saves.** Each round only the PC of the first living human asks for the autosave (a network command); every PC then writes its own file, and everyone waits for the slowest one.
- **The freeze is mostly the game looking through the saves folder.** To pick the name and the file, it reads the header of every multiplayer save in the folder: 4 times on the PC that asks, twice more on every PC. Each header read of a big-map save takes about 85 ms (it rebuilds that game's hero pool).
- With the owner's 17 old multiplayer saves in the folder, a round froze for **about 10 s** (7 s asking, 3.3 s saving). With only the 3 autosaves of the small robot map: 0.1 s and 0.15 s.
- **Without the mod:** moving old multiplayer saves out of your profile's `Saves` folder helps the same way.

**What the mod does:**
- **Trace** (`trace_autosave=1`): one log line per request and per save, with each phase, and how long each PC was frozen:
  ```
  autosave: day 3 (day start): player 0 asks every PC to save (request a6820c3c; finding the oldest autosave and sending took 6763 ms)
  autosave: day 3: player 1 saved "Sauvegarde auto 1" (save 1 this round; request a6820c3c from another PC): 3299 ms in all: 2 ms preparing, 3230 ms until the first stream (disk check, file name, header), 66 ms serializing and compressing (deflate 33 ms, level 1, 5000 KB -> 815 KB in 4 streams), ...
  autosave: player 1 closed no segment for 6661 ms around the save
  ```
- **Faster compression** (`compression=1`): saves use zlib level 1 instead of the game's 6. On the owner's biggest save, the game's own zlib takes 0.60 s instead of 1.75 s, for a file 24% bigger (13.8 MB instead of 11.2). Same format: the game loads them as before (robot: a level-1 autosave loaded and played on). Only saves change, nothing else the game compresses.
- **Fewer autosaves** (`autosave_every=N`): autosave only on days 1, 1+N, 1+2N... (7 = the first day of each week). The day is part of the game, so every PC with the mod skips the same days.
- **Quick look through the saves folder** (`fast_save_list=1`): while the autosave is requested or a multiplayer save runs, the game's reads of the other saves' headers skip the step that rebuilds each one's hero pool. That step only fills a list for the lobby's hero choice, which nothing in a save uses. The names, the choice of the oldest autosave, the file names and the new save's header stay exactly the same. Details and checks in save.md.

A save does not change the game, so none of this can desync: the robot's world checksums stayed equal in every run (2 copies, 6 AI turns each, with and without the owner's saves in the folder).

**Robot results** (A1M4, small map; more in save.md):

| | Request (host) | Each PC's save | Frozen per round | zlib part |
|---|---|---|---|---|
| game's level 6, 3 autosaves in the folder | 0.1 s | 0.16–0.23 s | (not measured) | 0.10 s |
| level 1, 3 autosaves | 0.1 s | 0.12–0.15 s | ~0.3 s | 0.03 s |
| level 1, the owner's 29 saves copied in, `fast_save_list=0` | 6.4–7.2 s | 3.2–3.9 s | ~10 s | 0.03 s |
| **the defaults**, the owner's 29 saves copied in | **0.07–0.08 s** (first: 0.8 s) | **0.11–0.13 s** (first: 0.9 s) | **~0.3 s** (first: ~1 s) | 0.03 s |

```ini
[save]
trace_autosave=1   ; 0 = no autosave lines in the log
compression=1      ; zlib level of saves, 0..9 (-1 = the game's 6)
autosave_every=1   ; autosave every N days (1 = every day, as the game)
fast_save_list=1   ; 0 = the game's full read of every save in the folder
```

The robot passes them with `--save-setting KEY=VALUE`; `run_lan_test.py --autosave` ticks the lobby's autosave option.

**Not built:** making only the host ask. The game already sends one request per round, from the PC of the first living human (not always the host): skipping that PC's request would lose the autosave.

## "Your turn" notification (`notify.cpp`, on by default)

**What it is for:** while the computer players move, the friends switch to another window and miss the moment they can play again.

**What it does:** when this PC's player can play again (the computer players are done and a new day starts, or in a game without simultaneous turns the turn passes to this player) **and the game is not the window in front**:
- the game's **taskbar button flashes** until you come back to the game;
- a **short sound** plays: Windows' "System Notification" sound where Windows has one (Windows 8 and later), otherwise a short chime the mod makes in memory (Wine has no Windows sounds, so on Linux you hear the chime);
- a **notification pop-up** appears from a small icon in the notification area ("Your turn (day 5)."), and the icon goes away when you are back in the game. Where there is no notification area (often under Wine/Proton), nothing shows and nothing breaks.

Not on the very first turn of a game (when it starts, or after loading a save): you are at the screen then. The log has one line per turn:

```
notify: your turn (day 1): the first turn of this game, no notification
notify: your turn (day 2): taskbar flash, the system notification sound, pop-up
notify: your turn (day 3): the game is in front, no notification
```

**How it knows:** every player of the game world has a turn state (0 = playing now, 1 = waiting). After each frame of the adventure map, the mod looks at this PC's player: a living human whose state just became 0 can play. The game finds "the player whose turn it is" the same way. Details and addresses: [docs/subsystems/ai-turn.md](../../docs/subsystems/ai-turn.md), "Whose turn it is". It only reads the game: nothing in the game changes, so it cannot desync.

**Checked with the robot** (A1M4, game windows in the background, 4 AI turns per run):

| Game | Notifications | World checksums |
|---|---|---|
| LAN, 2 humans + 1 AI, turns one after another | host: days 2, 3, 4, 5; client: days 2, 4, 5 (on day 3 the client's robot ended the turn in the same frame it began) | 615, all equal |
| LAN, simultaneous turns, sound and pop-up on | both: days 2, 3, 4, 5 (sound and pop-up shown, no error) | 333, all equal |
| single player | day 2 | (one PC) |

None on day 1. Not checked by the robot: the game window in front (it would take the focus from whoever uses the PC), and Linux.

**Settings** (`open-homm5.ini`):

```ini
[notify]
turn=1    ; 0 = no notification at all
sound=1   ; 0 = no sound
popup=1   ; 0 = no pop-up
trace=0   ; testing: 1 = log every change of the players' turn states
```

The robot keeps robot runs quiet (`sound=0`, `popup=0`; the flash and the log lines stay); `--notify-setting KEY=VALUE` changes that.

## Player list (`player_list*.cpp`, on by default)

**What it is for:** in co-op games with simultaneous turns, everyone wants to know who still plays and who has pressed End Turn.

**What it shows:** a small panel on the adventure map's left edge, between the resource bar and the minimap (the owner's design):
- the day and how many humans are ready ("Jour 34 - 3/4 Prêts", or "Day 34 - 3/4 ready");
- the teams, each with a numbered round button and a thin line (none in a free-for-all);
- one pill per player: the hero's portrait (a little bigger than the pill, on its left end, like the game's round buttons on its bars) in a ring of the player's colour, the player's name, and the turn: **green dot** = playing, **tick** = turn ended, **three blinking dots** = the computer player is moving, **ring** = waiting.

It looks like the game's own interface: the pills are cut from the game's resource bar picture (its metal frame, read from the game's data), round things get the game's thin bronze rims, and the text is white with a dark shadow. If that picture cannot be read, the log says so and the panel uses plain dark capsules.

It only shows the adventure map (not in towns, battles or menus). **F9** hides it or shows it again. When the mouse is over it, it turns almost invisible, so it never hides the map. It never takes a click or a key from the game. Its words follow the game's language (French or English).

**Where the data comes from:** the game world's players (turn order, name, colour, team, selected hero, "my turn now", "pressed End Turn"), the same on every PC; the hero's name and portrait from its game data. Details and addresses: [docs/subsystems/players.md](../../docs/subsystems/players.md). It only reads the game, so it cannot desync a game, and each player can turn it on or off.

**How it draws:** it is one client of the DLL's shared overlay (`src/hook/overlay.cpp`, see `src/hook/README.md`), with the template builder. The overlay sees the game's Direct3D 9 device (`src/hook/d3d9_hook.cpp`: the `Direct3DCreate9` import, then entries of the Direct3D objects' vtables; data patches only) and draws just before each frame is shown, with [Dear ImGui](https://github.com/ocornut/imgui) (`src/hook/third_party/imgui`, v1.91.9b, MIT); it also holds the fonts and the pictures read from the game's data. A device reset (fullscreen switch, alt-tab) is handled. It stays plain Direct3D 9 and Win32, so it should work under Wine/Proton (with their `d3d9.dll`), not checked there yet. The player list is not a modal panel: it never takes input.

**Checked with the robot** (A1M4, LAN: 2 humans + 1 computer player; screenshots show the panel, see `tools/robot/README.md`):

| Game | What the panel showed | World checksums |
|---|---|---|
| simultaneous turns, teams humans vs AI | both humans "playing", then a tick for each one who ended, then "thinking" for the computer; the same on the client | 501, all equal |
| turns one after another, teams humans vs AI | host playing / client waiting, then the other way round, then the computer | 711, all equal |
| simultaneous turns, free-for-all | no team buttons; "Ordinateur 1" for the nameless computer player | 357, all equal |
| single player, 2560×1080 | "Player 1", "AI 2", "AI 3" (the game's names), one at a time | (one PC) |
| simultaneous turns, teams, 1920×1080 | the same, 1.4 times bigger, as the game's own interface | 267, all equal |
| 8 rows (the client's `player_list_test_rows=8`: 4 teams of 2, the tallest list), 1024×768 and 1920×1080 | the list starts higher and gets a little smaller, and stays above the chat box | 259 and 254, all equal |

**Screen sizes:** the game's interface grows with the screen height (it is laid out for 768 lines), so the list does too: its sizes and its distance to the left edge are fractions of the height, and its fonts are made again at the new size. At 1920×1080 and 2560×1080 it keeps the same size next to the resource bar and the minimap as at 1024×768, and stays clear of the minimap and the chat box.

Not checked by the robot: F9 and the fade (the robot's windows are in the background), fullscreen, and Linux.

**Settings** (`open-homm5.ini`):

```ini
[ui]
player_list=1          ; 0 = no player list (nothing is installed)
player_list_key=F9     ; F1..F12, or none
player_list_top=34     ; where it starts, % of the screen height
player_list_left=2     ; its left inset, % of the screen height
language=auto          ; auto (the game's language), en, fr
trace_players=0        ; testing: 1 = log each change of a player's turn
player_list_test_rows=0  ; testing: show N rows (copies of the real players, teams of two)
```

## Fast AI pathfinding (`fast_paths*.cpp`, on by default)

**What it fixes:** long AI thinking late in a game on big maps (several seconds per AI move).

Late in a game, nearly all of the AI's thinking time goes into searching paths for heroes (see [docs/subsystems/ai-pathfinder.md](../../docs/subsystems/ai-pathfinder.md)). The mod keeps every step of every search and makes the steps cheaper:

- **Remembered checks.** One search can look at the same map tile over a hundred times, and each time it repeats checks that only depend on the tile ("can the hero step there?", "is there something to visit?", "what does a step onto it cost?"). The mod remembers their answers until the search ends, asking the game's own functions the first time.
- **Faster open list.** The list of tiles still to look at is sorted with the same algorithm and the same order as the game's, written out again so that it compares in place instead of copying every tile it looks at. At start-up the mod tests it against the game's own code and keeps the game's if anything differs.
- **Ported search core.** The search loop, the node expansion and the "relax a neighbour" step are rewritten from the game's code, step by step, calling the game's own functions in the same order. Inside it, the list of tiles still to look at is kept in a compact form (same order as the game's) and given back to the game when the search stops.
- **Fast "go ashore?" check.** Half of the remaining time was one check that scans every object of the map floor three times per tile. The mod builds, once per AI step, tables of which object covers which tile (same order rules as the game) and answers from them.

Because every search takes exactly the same steps, **every path stays the same**. In co-op only the host runs the AI, so the mod only matters on the host's PC, and it cannot cause a desync.

One subtlety: the original AI thinks in real-time slices, so how fast it runs can change what it decides, just as the original game decides a bit differently on a faster PC. The robot checks the mod with a virtual clock that removes that effect: then the original and the mod make the same decisions over whole turns.

**Checked** with the robot on four of the owner's saves (`tools/robot/path_check.py`, 4 whole AI turns each, virtual clock): every big path search gave exactly the same result as the game's own, node by node (leaving out only padding and flag bits the game never reads), plus the order of reached tiles and what is left of the open list.

**Measured** (owner's day-50 save, huge map, 4 AI turns, 85 AI moves):

| | AI thinking |
|---|---:|
| without the mod | 93.3 s |
| remembered checks + faster open list + ported search core | 44.0 s |
| + fast "go ashore?" check | 22.7 s |
| + cheaper steps (per-tile records, compact open list, fewer copies) | **16.7 s** (x5.6) |

**Settings** (`open-homm5.ini`):

```ini
[ai]
fast_paths=1         ; 0 = the game's own pathfinding (default 1 since 2026-10-24)
remember_checks=1    ; the parts, one by one (all default 1)
fast_queue=1
port_relax=1         ; the ported search core
port_search=1
port_expand=1
fast_ashore=1        ; the fast "go ashore?" check
ashore_check=0       ; 1 = also ask the game every time and log any difference (slow, for testing)
max_expansions=0     ; EXPERIMENT, CHANGES THE AI'S PATHS: expand a tile at most N times per search (0 = off)
```

`max_expansions` is not part of the exact speed-up: it is a first test of a faster search that may change the AI's paths (see the experiment in [ai-pathfinder.md](../../docs/subsystems/ai-pathfinder.md)). With 16 on the owner's day-50 save, AI thinking went from 16.7 s to 8.3 s; only the AI hero's giant long-range searches changed. On another save the AI then chose differently after a few moves. Off by default; not for real games until it has been tested much more.

## Teams on random maps (`rmg_teams.cpp`; on by default only for recipe maps)

Step 2 of the map generator plan ([docs/plans/map-generator.md](../../docs/plans/map-generator.md)).

- **Where teams live:** in the game setup (`NWorld::CPlayersStartInfo`), one slot per player with its team at +0x1c. A random map puts every player on their own team.
- **How it works:** when the host starts the game (0x007200da → 0x0071feb0), it sends its setup to every PC, and each PC creates its players from it (0x00c9fa80). The mod changes the teams in the host's setup just before that.
- **Results:**
  - Every PC gets the same teams, including friends without our DLL.
  - Only the host's setting counts.
  - Maps with teams of their own keep them.
  - A choice that would leave a single team is not applied.

```ini
[rmg]
teams=            ; empty: the map's teams. humans_vs_ai, pairs, or a list like 1,1,2,2,3,3
lobby_teams=0     ; 1 = the lobby's team arrows pick team numbers on maps without teams
trace_teams=1     ; log the teams each PC starts with
recipe_teams=1    ; maps made from a recipe template get the template's teams, and the team arrows on them
```

**Teams of recipe maps** (`recipe_teams=1`, on by default, host only; the owner's choice, 2026-10-25). A map made from a recipe template (`OH-X1-...`, see [Random map templates](#random-map-templates-rmg_templatescpp-rmg_makercpp-rmg_recipecpp-on-by-default)) knows its teams: with 3 teams of 2, players 1+2, 3+4 and 5+6 start in the zones that belong together. **They are set as soon as the game builds the setup** (single player and multiplayer), so the setup screen and the lobby show them from the start (the owner's request, 2026-10-25), and on these maps **the team arrows work** as with `lobby_teams=1`, so the players can still change them. `teams=` still wins (then the recipe's teams are not set). When the host starts, it keeps the teams it has. How: the setup constructor gets the map's description, whose `sRMGProps` (`+0x254`) names the template (`+0x28`); we remember the recipe's teams per setup, as for `lobby_teams`. **Robot test** (LAN, a 2v2 recipe map, 2 humans and 2 computer players): both PCs started with the humans on team 0 and the computer players on team 1, checksums equal; with a lobby arrow pressed (`lobby_teams=1`), the arrows' teams; with `recipe_teams=0`, one team per player. Again with the teams set when the setup is built (a map made with the builder): the host's and the client's lobby showed teams 1, 1, 2, 2 from the start, an arrow changed player 3's team without `lobby_teams`, and both PCs started with 1+2 against 3+4.

**Choosing teams in the lobby** (`lobby_teams=1`, host only). On maps without teams of their own, each row's team arrows move that player's team number up or down, instead of moving the player to another colour (the game's arrows). The pieces:
- **Which maps:** the setup constructor (0x00ca0240) tells whether the map has teams. We remember that per setup.
- **The rules:** for those maps, `CanChangeTeam` (vtable cell 0x00e92770) allows every playing slot, and `SetTeam` (cell 0x00e92774) changes the slot's team.
- **The other lobbies:** at 0x0071e67d, a 6-byte jump to a small patch makes the host send a row's team to them also when only the team changed. The game itself never changes a team without the colour.
- **At the start,** the teams are renumbered 0, 1, 2…
- **Clients** need nothing: the host decides and tells them. Lobby choices win over `teams=`.
- **Maps with teams of their own** keep the game's arrows.

**Robot tests** (LAN; the host has the settings, the client has none):
- `teams=humans_vs_ai`: both PCs started with the two humans on team 0 and the computer player on team 1. 352 checksums, all equal.
- `teams=1,2,2`: both PCs started with player 1 alone against players 2 and 3. 466 checksums, all equal.
- `lobby_teams=1`, on A1M4:
  - The host pressed its own team arrow right, and the computer player's left; the client pressed its own right.
  - Both PCs started with the host and the computer player together against the client. 544 checksums, all equal.
  - AI moves were slower (1.7 s), as the AI's ally can now see its heroes walk.
- `lobby_teams=1`, on A1M2 (a map with teams): the arrow moved the host to the other team's open seat, as in the game.

The robot sets these with `run_lan_test.py --host-rmg KEY=VALUE`, and presses a team arrow with the step `"38|arrow|<row> team left|right"`.

LAN robot games on random maps came later: the recipe maps' teams (above) and the faction terrain ([below](#start-terrain-that-matches-the-factions-rmg_factionscpp-off-by-default)).

## Random map templates (`rmg_templates.cpp`, `rmg_maker.cpp`, `rmg_recipe.cpp`, on by default)

Step 1 of the map generator plan ([docs/plans/map-generator.md](../../docs/plans/map-generator.md)), and steps 1 and 2 of the in-game builder plan ([docs/plans/in-game-generator.md](../../docs/plans/in-game-generator.md)).

- **How the game finds templates:** the random map screen asks the game's file system (`NVFS::CWinVFS`) for the files in `RMG/Templates` (List, vtable slot 10), then opens each one (Open, slot 7). The mod sits on those two slots. Nothing on disk changes.
- **Template files:** the `.xdb` files of `open-homm5\templates\` next to the exe (the install package puts the 24 ready ones there) are added to the list and served when the game opens them.
- **Recipe names** (new): a template named `RMG/Templates/OH-X1-<16 characters>.xdb` needs no file at all. Its name holds every choice of the template maker, for example `OH-X1-321110nnxxxxxxxa` = 2v2v2, Normal, best size, underground. When the game opens such a name, the DLL builds the template's bytes from the name with `rmg_maker.cpp`, a C++ copy of the web page's template maker. The format: [tools/rmg/README.md](../../tools/rmg/README.md#recipe-names).
  - **Why:** in multiplayer every PC builds the random map itself from the template's path. With a recipe, every PC with the DLL has the template, and they all get the same bytes.
  - **The list** shows every recipe this PC was asked to open (for example a joined game's), plus those of `recipes=`. In the list, a recipe template's name is the maker's name plus `*`, for example `OH-2v2v2-Lu-Normal*`, so it can't be mixed up with the ready file of the same layout.
  - **Same bytes as the page:** `build.ps1 -Test` checks that `rmg_maker.cpp` writes exactly what `tools/rmg/template_maker.js` writes, on the 24 ready templates and 400 random choices.
  - **Same bytes forever:** what an `X1` name builds never changes (`tools/rmg/x1_fingerprints.txt` records 302 of them; the test fails on any change). And at start, the DLL builds three recorded names itself and serves no recipe if one differs (the log says `self-check FAILED`).
  - A file in `open-homm5\templates\` whose name is a recipe is skipped (the log says so): the name decides.
- **Joining a random map game** (`rmg_join.cpp`, step 3). Each PC rebuilds the host's random map from the template's name, the seed and the options, which the host's game info carries.
  - When this PC reads a host's game info, the game loads the template by its name at once: for a recipe, the Open hook builds it and puts it in the list. Just before the game builds the map, a hook on the connector (`0x00701530`) registers the recipe again from the game info, in case.
  - **The guard:** a PC without the map's template used to crash there, because the game used the missing template without a check (`0x007012b1`). Now the game shows its own "can't make the map" message (`RMG_MAP_FAIL`). This works for any missing template, also with `templates=0`.
  - Players without our DLL still crash on templates they lack: everyone should update together.
- **Teams on recipe maps:** see [Teams on random maps](#teams-on-random-maps-rmg_teamscpp-on-by-default-only-for-recipe-maps) (`recipe_teams=1`).
- **The builder panel** (steps 4 to 7): [Template builder](#template-builder-rmg_buildercpp-rmg_screencpp-on-by-default).

```ini
[rmg]
templates=1     ; 0 = off: neither the folder's files nor recipe names
recipes=        ; recipe names to offer in the list, separated by commas (for tests; the builder adds its own)
```

The log says what was served, for example:

```
rmg templates: 24 template(s) from open-homm5\templates: OH-2v2-Lu-Normal.xdb, ...
rmg templates: recipe names (RMG/Templates/OH-X1-....xdb) are built when the game asks for them (maker self-check ok); in the list from recipes=: OH-X1-321110nnxxxxxxxa
rmg templates: OH-X1-321110nnxxxxxxxa.xdb built from its name: "OH-2v2v2-Lu-Normal*", 6 players, 13 zones, Large (176x176, with underground), 23121 bytes
```

**Robot tests (2026-10-25):**
- A recipe map made with the console and no file: built, generated and played.
- In a LAN game, the client rebuilt the host's recipe map, and all world checksums were equal.
- A client without recipe support got the game's message instead of a crash.

Details: [the plan's robot results](../../docs/plans/in-game-generator.md#robot-results-2026-10-25). The generator screen's template list: checked with the builder (below).

## Template builder (`rmg_builder*.cpp`, `rmg_screen.cpp`, on by default)

The plan: [docs/plans/in-game-generator.md](../../docs/plans/in-game-generator.md) (build steps 4 to 7). Every choice of the web template maker, on the game's own random map screen (`NUI::CRMGScreen`, the same screen in single player and multiplayer).

**How a player uses it:**
1. The random map screen (single player: Custom game, then "Générer une carte"; multiplayer: the same screen from the create-game screen).
2. **Modèles... / Template...**, the new button between Save and Back (or **F11**). The builder covers the screen's main panel; the game behind is dimmed and gets no input.
3. Pick a ready template in **Partir d'un modèle / Start from**, or change the choices. The zone map, the numbers and the warnings follow at once.
4. **Utiliser / Use**: the panel closes and the game's own screen is set: map size, underground level, players, "ARMG Templates" ticked and our template chosen in its list (`OH-2v2v2-Lu-Normal*`). Also, ticked, so that nothing is left to chance: **no water** (our templates are land maps; unticked, the game makes an island map half the time), **resources and experience normal** (×1 on the template's treasure and chests; unticked, the game uses the list's first item, ×0.2), and the **monster level that leaves the template's guards as written** ("Faibles"/weak in MMH5.5, whose levels are ×1.0, 1.9, 2.7, 3.5, 7.0; unticked, the game draws one of the first three). The facts: [docs/subsystems/rmg.md](../../docs/subsystems/rmg.md#the-screens-own-options-and-the-template-static-reading-2026-10-25-robot-checked). Then the game's own options if wanted, **Créer / Create**, **Sauvegarder / Save**, and play the map. In multiplayer, the other PCs rebuild the template from its name ([Random map templates](#random-map-templates-rmg_templatescpp-rmg_makercpp-rmg_recipecpp-on-by-default)).
5. **Enregistrer / Save as file** writes the template as the web page's download gives it, to `open-homm5\templates\<name>.xdb` (listed by the game from its next start; a different file of the same name gets `-2`, `-3`...). **Annuler / Cancel** or **Esc** changes nothing.

**The panel** (`rmg_builder.cpp`, drawn with the game's pictures by `rmg_builder_widgets.cpp`). Its parts sit where the screen's own parts are:
- **Left, in the minimap's box:** the zone map, drawn from the maker's own zones and paths (`rmg_builder_data.cpp`: the page's `positions()`, checked against Node). Start zones in their team's colour with the player's number, each team's treasure zones paler, neutral zones grey, the centre gold ("C"); thicker paths for stronger guards, dashed when open; a small gold mark for a town.
- **Under it:** players and zones, the name the game's list will show (with its `*`), the map size.
- **Right, scrolling in the options list's box** (the wheel, or drag in the game's scroll bar track): the ready templates; teams (steppers, the teammates' path, the start mines); neutral zones (steppers, two town boxes); loot and guards, and "by distance from the bases", which shows a small table (loot and guards per distance ring, with each ring's zone count); size ("Auto" shows the size it picks) and underground; then the maker's warnings.
- **Invalid choices are not offered:** each arrow asks the maker's own check (`rmg::CheckChoices`) first. An arrow that would make more than 8 players is greyed; clicking it shows the maker's message ("Le jeu permet 8 joueurs au plus (9 demandés).").
- **Words:** French in a French game, English otherwise (`[ui] language`). Every text is measured; one too long for its place is cut with "…" (none is today, at 1024×768 and 1920×1080).
- `builder_last=` in `open-homm5.ini` remembers the last choices used or saved (a recipe name); the panel opens on them.

**The native button** (`rmg_screen.cpp`). The screen's window comes from UI files of the game's data. The DLL serves, through the game's file system (nothing on disk changes):
- `UI/RMGScreen/MainPanel.(WindowSimpleShared).xdb`: the game's file with one more child, our button. **Only when the file is the one we know** (its fingerprint); otherwise the builder draws a button of its own at the same place, and the log says why.
- `UI/RMGScreen/OpenHomm5Builder.(WindowMSButton).xdb`: the game's Save button, moved to x 340 (the free slot), enabled, with our label. `OpenHomm5Builder.(UISRunReaction).xdb` sends `oh_builder` instead of `save`. `OpenHomm5Builder.txt` and its tooltip are in the panel's language.
- The screen's command handler (`0x0079a650`, vtable cell `0x00e4110c`) is wrapped: `oh_builder` opens the panel; every other command goes to the game.

**"Use"** sets the screen's options as clicks would: the size, levels and players options are ticked and their item chosen (`NUI::CRMGScreen::CComboOption`, vtable `0x00e41070`); "ARMG Templates" (EP 6) is ticked; the screen's own "change_option" (`0x00799500`) refills its template list with the templates that fit; our recipe is chosen in it. If anything is not as expected, nothing more is changed, the panel stays open with a message, and the log says what it found.

```ini
[rmg]
builder=1           ; 0 = no panel and no button (nothing is installed); needs templates=1
builder_key=F11     ; the panel's key on the random map screen (none = no key)
builder_last=       ; written by the panel: the last choices used or saved

[ui]
language=auto          ; the panels' words: auto (the game's), en or fr
overlay_test_panel=0   ; testing only: 1 = F10 opens the overlay's test panel on any screen
```

The log says what happened, for example:

```
rmg screen: the random map screen gets our "Template..." button (its main panel, 1641 bytes, with one more child)
rmg screen: "oh_builder" (the Template... button)
rmg builder: panel opened (the Template... button): OH-X1-321110nnxxxxxxxa, OH-2v2v2-Lu-Normal
rmg builder: Use OH-X1-221110nnxxxxxxxa ("OH-2v2-Lu-Normal*"): size L with underground, 4 players, no water, monsters item 1 (x1.0), resources and experience normal (x1), "ARMG Templates" ticked, OH-X1-221110nnxxxxxxxa chosen (14 of 19 that fit)
rmg teams: setup 139D1380 is for a random map made from /RMG/Templates/OH-X1-221110nnxxxxxxxa.xdb#xpointer(/RMGTemplate)
```

**Robot tests (2026-10-25, the French test copy):** the native button on the screen; the panel opened by the button's command and by F11, filled, at 1024×768 and 1920×1080; a preset, then Use set the screen (Large, underground, 4 players, our template 14th of the 19 that fit); Create built the map from `OH-X1-221110nnxxxxxxxa`; Save wrote it to `Maps\`; the saved map started and its AI played. Commands: [tools/robot/README.md](../../tools/robot/README.md#checking-the-template-builder-a-modal-panel-of-the-overlay).

**Checking by hand** (the robot cannot click: its game runs in the background): the drop-down lists, the arrows, the check boxes, the wheel and the scroll bar; Save as file; Esc; and that the game behind gets no click. On Linux (Proton) too. With `overlay_test_panel=1`, the overlay's test panel shows what input arrives.

## Start terrain that matches the factions (`rmg_factions.cpp`, off by default)

Plan and research: [docs/plans/faction-terrain.md](../../docs/plans/faction-terrain.md). On a random map, the generator gives each start zone the faction its player had when the map was made. Factions picked later in the lobby change only the player (or a "Random towns" town), not the ground. With this mod, the map is rebuilt at the start with the lobby's factions.

- **The lobby (host only):** on random maps, every start slot may take any of the 8 factions. The host's setup decides every PC's arrows.
- **The hello:** in a random map's lobby, each client's DLL sends the host a waiting-room request of a type the game does not use (0x4f48). A host without the mod ignores it. A client says hello whatever its own setting: it can follow a host that rebuilds.
- **The host's start:**
  1. Factions left on "Random" get the faction the map was made with (through the game's own SetRace, slot 0x38).
  2. If a faction then differs and every human client said hello, the start message names another map, `Maps/RMG/<guid>-OH1-<one hex digit per player>/`.
  3. Otherwise the map stays as it is. On fixed-town maps, the changed factions go back to the map's.
- **Every PC** that sees such a name rebuilds the map, the way a joining client builds a random map: the original map's settings, the factions from the name, the game's own generator, written as another `Maps\<name>.N.h5m`. A client does it when it reads the start message (before the game looks the map up), the host when its world starts. If a package with that name already opens (the same game played before), it is used as it is.
- **If a rebuild fails** on a PC, that PC starts on the original map and logs `REBUILD FAILED` (its world will not match the others').

```ini
[rmg]
faction_terrain=0  ; 1 = as the host, rebuild random maps with the lobby's factions
trace_factions=0   ; research: lobby setups, the start, map builds, map files, faults
```

**Robot tests** (LAN, 2026-10-25; MMH5.5's `4P-Collar-L` map `MMH55_RMG_Map_002`, large with underground, "Random towns"; the host had the setting, the client had none):
- **A faction changed** (the host picked Haven where its zone was made for Inferno):
  - both PCs rebuilt the map: 9.2 s on the host, then 8.7 s on the client (the robot's two copies share a folder, so they take turns; real PCs build at the same time);
  - the two packages were byte for byte the same;
  - the host's start zone went from Inferno ground (`Dead_Land`) with lava and dirt decorations to Haven ground (`Dark_Grass`) with grass decorations;
  - **504 world checksums, all equal.**
- **Another change** (the client picked Inferno where its zone was made for Dungeon): 507 checksums, all equal. That zone is underground, and underground zones get cave ground whatever the faction, so its ground did not change; its town did.
- **No faction changed** (setting on): no rebuild; the game started on the map as it was. 508 checksums, all equal.

**Not done yet:**
- **The lobby unlock on fixed-town maps** (every slot locked to one faction) is not robot-tested: the only such saved maps use recipe templates, which the DLL did not serve yet when this mod was tested.
- **Linux (Proton).**
- **A line on the loading screen** while the map is rebuilt: the screen just waits a few seconds.

**Each rebuild leaves a map package in the game's `Maps` folder** (`Maps\<name>.N.h5m`), like each join of a random map in the game itself. **It is hidden from the game's map lists** (on every PC with the DLL, whatever its settings): only the map it was rebuilt from shows. The lists (multiplayer, single player, random map screen) get their maps from `0x007c2ce0`, every `map-tag` resource under a folder from the game's resource index; the mod takes out those under `Maps/RMG/<guid>-OH1-<8 hex digits>/`. Log: `rmg factions: map list of ...: 1 rebuilt map(s) (-OH1-) hidden, 23 shown`.
- **The packages are not deleted:** a save of such a game names its map's path, and reading a save's setup needs the map's description (`0x00ca0c70` → the setup constructor). The start and the saves find the package by that exact path, which the lists do not touch.
- **Robot check** (LAN, 2026-11-01, `MMH55_Utility_64.exe`, map `MMH55_RMG_Map_002`, the client picked Haven where its zone was made for Dungeon): the host's map list log said `map list of Maps/RMG: 2 rebuilt map(s) (-OH1-) hidden, 55 shown`, and none of the 95 maps of the multiplayer list was an `-OH1-` one; both PCs rebuilt the map (`-OH1-838A0000`) and started on it; **496 world checksums, all equal.**
- So do not delete them by hand while you keep saves of those games. (A save's setup is read by the same code as the start message, `0x005a1430` → `0x00ca0c70`, where the mod rebuilds a missing rebuilt map; not tested with a save.)

## Original music (`original_music*.cpp`, off by default)

**What it is for:** MMH5.5 brings its own music (new tracks for terrains, the main menu, the credits, battles and hero themes). With this switch on, the game plays the **original Heroes V music** wherever MMH5.5 changed it. Off (the default), you hear MMH5.5's music, exactly as without the add-on.

Nothing on disk changes. The game finds its music through small data files in the `.pak` files (see [docs/subsystems/audio.md](../../docs/subsystems/audio.md)): one "music definition" per track, and map tiles, heroes and a faction table that point at them. When the game reads one of those files from MMH5.5's paks, the mod reads the same file from the original game's `data.pak` and gives the game the original music in it:

- **music definitions and the faction music table**: the original file (main menu, credits, the 5 terrain tracks MMH5.5 replaced, the battle music lists);
- **map tiles and heroes**: only their music line goes back to the original's. Everything else in those files (MMH5.5's hero stats, ...) stays MMH5.5's.

So the original choice is made tile by tile: on flowers you hear the original grass music, on a bog the original water music, on roads the terrain's music (MMH5.5 removed it there), even where MMH5.5 plays the same new track on all of them.

**Where the original game has no music** (heroes and tiles only MMH5.5 has, and hero themes MMH5.5 gave to heroes that had none): no hero theme, no tile music. The game then picks, as it always does, among the other music of that place: the terrain under the hero and the map's own music. MMH5.5's new tracks are never played.

The log (`open-homm5.log`) says what was set back, for example:

```
original music: on; mods: MMH55-Data.pak, MMH55-Index.pak, ... (16951 data files, 3028 of them also in the original game); indexed in 32 ms; ...
original music: [5] MapObjects/Haven/Nicolai.(AdvMapHeroShared).xdb: AdventureMusic AdvMap-Heroes/Nicolai.xdb -> none
original music: [26] Sounds/_(Music)/MainMenu/a2-MainMenu.xdb: MMH5.5 track ..\Music\MMH55\MainMenu_Dynasty_002.ogg -> original ..\Music\a2\Main_Menu.ogg
original music: [40] MapObjects/_(AdvMapTile)/Grass/Flowers.xdb: AdventureMusic AdvMap-Terrain/Flowers.xdb -> AdvMap-Terrain/Grass.xdb
```

**Checked** with the robot (switch off, then on, `log_music=1` lists the tracks that start):

| Where | Off (MMH5.5) | On |
|---|---|---|
| main menu | `MMH55\MainMenu_Dynasty_002.ogg` | `a2\Main_Menu.ogg` |
| credits | `MMH55\MainMenu_Credits_002.ogg` | `Credits\Final-Credits.ogg` |
| map A1M4, hero on a stone road | `MMH55\Adventure_Terrain_SummerPlains_001.ogg` | `AdvMap-Terrain\Terrain-Grass.ogg` |
| map A2M1, hero on grass | `MMH55\Adventure_Terrain_SummerPlains_002.ogg` | `AdvMap-Terrain\Terrain-Grass.ogg` |
| random maps (other starts each time) | `MMH55\Adventure_Terrain_ElvenForest_001.ogg`, ... | `Terrain-Dirt.ogg`, `Terrain-Sand.ogg`, `a2\2ndCampaign_Stronghold.ogg`, ... |

In a LAN game with the switch on for the host and off for the client, the world checksums stayed equal (no desync): music is only sound, so each player can choose.

Only mods in the game's `data\*.pak` files are considered (not `UserMODS\`). It works with any MMH5.5 version: the original files are read when the game starts (~40 ms).

**Settings** (`open-homm5.ini`):

```ini
[audio]
original_music=0   ; 1 = the original Heroes V music where MMH5.5 changed it
log_music=0        ; testing: 1 = log every music file the game starts
log_data_files=0   ; testing: log the first N files the game reads from its data
```

## One language: French texts (`languages*.cpp`, off by default)

**What it is for:** with MMH5.5, a French game mixes languages. The base game's texts are French, but the 3.1 patch's and MMH5.5's texts are English, and they replace many French ones. With `texts=fr`, the game takes every text it can from the French packs in `open-homm5\lang\fr\` next to the exe:

- **MMH5.5's own French pack.** The MMH5.5 team publishes it on MMH5.5's ModDB page. Each player downloads the one for their MMH5.5 version, unzips it and puts its `.pak` there. Any version works, and the log names it. A `.zip` of texts is read too; a `.zip` that only wraps the `.pak` is reported in the log.
- **Our pack** of what that pack leaves out, shared within the group.

Nothing on disk changes. `game` (the default) installs nothing. The plan, with the counts and the version question, is [docs/plans/languages.md](../../docs/plans/languages.md).

**How:** the game reads each text the first time it is shown, through its file system, and keeps it until it closes ([docs/subsystems/texts.md](../../docs/subsystems/texts.md)). The mod wraps the same "open file" function as the music mod (`CWinVFS` slot 7). When the game opens a `.txt` path that one of our packs has, it gets that file instead.

- **Only `.txt` files** are ever taken from a pack. A language pack cannot change game data, only words on the screen.
- **The `FF FE` mark** that the game needs before a text (without it the game shows an empty text) is always there. A UTF-8 text in a pack is converted.
- **Between our packs,** the newest file of a path wins, as in the game. On equal dates, the pack whose name sorts last wins, the same on Windows and Linux.
- **The player list** (`[ui] language=auto`) uses French words with `texts=fr`.

Texts are only shown, never part of the game world: the game objects keep a text's path, not its words. So each player can choose for themselves.

**Checked** with the robot (2026-10-25, MMH5.5 RC19d, its French pack and ours in the test copy):
- **Single player, a random map:**
  - 6,024 French texts indexed;
  - the version line matched ("5.69d" both);
  - 383 texts given to the game (379 from MMH5.5's pack; 4 from ours: random map settings and the Rage message);
  - the player list in French.
- **LAN on A1M4, only the host on `texts=fr`:** 8 AI turns, 1,465 world checksums, all equal on both copies (no desync).
- **The same on MMH5.5 RC19k** with its closest French pack (RC19i) and our pack for RC19k: 6,083 French texts; LAN with only the host on French, 1,557 world checksums, all equal.
- **A `.zip` that only wraps the `.pak`, and no folder at all:** the log says so, and the game keeps its own texts.

**Not seen yet:** the combat log (the robot's battles resolve at once) and the random map screen's labels.

The log (`open-homm5.log`) says what was found, for example (MMH5.5 RC19k, its RC19i French pack and our pack):

```
languages: texts=fr: the game's texts come from open-homm5\lang\fr\*.pak where they have them
languages: texts=fr: 6083 French texts from open-homm5\lang\fr: MMH55-Texts-FR.pak (5603), open-homm5-fr.pak (480); 272 of them newer than another pack's; indexed in 16 ms
languages: version text: French pack "Version: 5.69i" (MMH55-Texts-FR.pak), the game "Version: 5.69k" (another MMH5.5 version: some French descriptions may not match the game)
```

The version line compares the MMH5.5 version written in the French pack with the one the game shows in its main menu. When they differ, some French descriptions may describe another version's numbers. The menu itself keeps showing the game's own version.

## Farther camera zoom, start distance (`camera.cpp`, off by default)

**What it is for:** the camera cannot zoom out far. `zoom_out=150` lets the adventure map's camera go 1.5 times as far from the ground as the game allows; `combat_zoom_out` does the same in battles. Off (the default, 100), the game's own limits.

**Where the limit comes from** (details in [docs/subsystems/camera.md](../../docs/subsystems/camera.md)):
- **Adventure map:** the game's `profiles\autoexec_a2.cfg` sets it at start-up: the farthest distance is 70 in every camera mode of the options screen, with or without MMH5.5 (MMH5.5 does not change this file or this code). The "vertically fixed" mode also keeps the camera looking down steeply (65°), so it shows less of the map than the "normal" mode (30° to 55°).
- **Battles:** the game picks the farthest distance by the battlefield's size. MMH5.5's DLL changes the closest zoom and the tilt of its bigger battlefields, and keeps the game's farthest distance.

**What it does:** when the game sets up the camera (map start, camera mode change, each battle), the mod hands it the game's own limits with the farthest distance (and the "soft" limit the camera springs back to) multiplied. Nothing else changes: the closest zoom, the tilt, the game's files and its own copy of the limits stay as they are. So it follows whatever the game and MMH5.5 set, in every camera mode.

The log says what it changed:

```
camera: adventure map: the camera may zoom out to 150% of the game's limit (zoom_out)
camera: adventure map, vertically fixed camera: farthest zoom 70 -> 105 (nearest 37)
camera: adventure map, vertically fixed camera, soft limit: farthest zoom 69 -> 103.5 (nearest 38)
camera: battle: farthest zoom 56 -> 84
```

**Only your screen changes:** the camera is not part of the game world, so it is not in the world checksums or in the commands the PCs send each other. Each player can choose, also in co-op games.

**Checked with the robot** (A1M4; the robot starts the camera far out, and the game stops it at its farthest allowed distance; details in camera.md):

| Run | Camera stopped at | World checksums |
|---|---|---|
| the game's limits | 70 | (one PC) |
| `zoom_out=150`, normal and "vertically fixed" camera modes | 105 | (one PC) |
| `zoom_out=300` | 200 (the start position), the sky shows past the small map's edge | (one PC) |
| `combat_zoom_out=150`, a battle the AI started | battle limit 56 -> 84, the battle ran to its end | (one PC) |
| LAN game, only the host with `zoom_out=200` and `combat_zoom_out=200` | host 140, client 25 | 698, all equal |

Not checked by the robot: the mouse wheel during play (the robot's windows are in the background), the frame rate on a big explored map, Linux.

**What to watch** far out: near the map's corners you see past its edge; on big maps more of the map is drawn, so a slow PC may lose frames; scrolling does not speed up with the distance. It works in the vanilla exe and MMH5.5's co-op exe (`MMH55_Utility`), not in MMH5.5's single-player exe (`MMH55.exe`, another build, where the DLL installs nothing); camera.md says how to get the same there by hand.

**Settings** (`open-homm5.ini`):

```ini
[lang]
texts=game     ; game (the texts as installed), fr; en (English) is planned, not built yet
log_texts=0    ; testing: log the first N texts taken from the packs

[camera]
zoom_out=100          ; adventure map: % of the game's farthest zoom (100 = the game's; e.g. 150; up to 400)
combat_zoom_out=100   ; battles: the same
start_zoom=           ; adventure map: the camera's start distance (the game: 25; empty = the game's)
wheel_fix=auto        ; the mouse wheel under Wine (wheel.cpp, below); 0 = off, 1 = from the first turn
```

The robot passes them with `--camera-setting KEY=VALUE` (`run_lan_test.py`: also `--host-camera` / `--client-camera`).

**Start distance** (`start_zoom`, off by default): the adventure map's camera starts at distance 25 (the game's `mission_camera_def_pos`). With `start_zoom=70` it starts at 70 (the game keeps it within its limits, so with `zoom_out=150` up to 105). The camera set-up asks for its soft limits right before it reads the start position, so the mod's soft-limit hook writes the distance there (`0x00f339f0`). Log: `camera: adventure map: the camera starts at distance 70 (the game: 25; start_zoom)`.

## The mouse wheel under Wine / Proton (`wheel.cpp`, `wheel_logic.cpp`; on under Wine only)

**The problem** (a Linux player, 2026-11-01): the wheel zooms in once and never out; Page Up / Page Down work. It happened before our DLL too.

**Why** (static reading of the exe; [docs/subsystems/camera.md](../../docs/subsystems/camera.md), "The mouse wheel"):
- The game sets its DirectInput mouse to **absolute** axis mode and works the wheel's motion out itself: "this value minus the last one". Windows reports positions (the running sum of the notches), so that difference is the motion.
- **Wine reports the wheel as steps** (+120 or -120 a notch), even in absolute mode. The difference then turns every repeated notch into 0.
- The game also drops a "press" (bit 7 of the value's low byte) while it thinks its window is inactive or the cursor is outside it. With positions that is harmless (the next value carries the motion). A step down, -120, ends in `0x88`: it looks like a press, so wheel-down can be dropped every time.

**The fix:** the game gets positions, the running sum of Wine's steps, in its wheel events and in the mouse state's `lZ`, as on Windows. Its own "this minus the last" then gives each notch, both ways.
- `wheel_fix=auto` (the default) does it **only under Wine** (ntdll's `wine_get_version`), and only after a check: after a batch of wheel events the mouse's own state is read. With positions its `lZ` is the last event's value; with steps it is their sum. Two clear "steps" checks switch the fix on, within the first 3 or 4 notches. Positions, or nothing clear in 16 checks: left as they are.
- `wheel_fix=1` converts from the first notch (for a Wine the check misreads; never on Windows, where it breaks the wheel). `wheel_fix=0` leaves the wheel alone.
- **On Windows nothing is installed** (unless `trace_wheel=1`).
- DirectInput filter order -10: before the robot's keys (0) and the modal panels (100).
- Only this PC's camera changes: the camera is not part of the game world.

**The log:**

```
wheel: Wine 9.0: the first mouse wheel events are checked; if Wine reports them as steps, the game gets positions, as on Windows (wheel_fix=auto)
wheel: first mouse wheel event from DirectInput: -120 (20-byte events; checking)
wheel: Wine reports the mouse wheel as steps (2 of 3 checks): from now on the game gets positions, as on Windows, so the wheel zooms both ways (wheel_fix=auto)
```

**The trace** (`[debug] trace_wheel=1`): every wheel event (raw and what the game gets, and whether the game drops it as a press), the game's own `GetDeviceState` reads, each Acquire, each check, the game's checks for presses at the first event (window active, cursor inside), and the window's `WM_MOUSEWHEEL` messages (a `WH_GETMESSAGE` hook on the game's thread, watch only).

**Tests:** `test/wheel_test.cpp` models the game's reading, Windows' and Wine's mice: without the fix, 5 notches up zoom in once and 10 down do nothing; with it, every notch counts, also when the game drops presses; positions are never converted.

**Robot checks** (2026-11-01, A1M4, `MMH55_Utility_64.exe`, `builder=1`; the robot's `wheel` step gives the game's mouse notches as Windows or as Wine would, its `dik` step presses Page Down / Page Up). The robot's game window is in the background, so the game's press filter is on (`trace_wheel`: "window active 1 1, cursor in its window 0"), as it seems to be for the Linux player:

| Run | Wheel down 6 notches | Wheel up 6 notches |
|---|---|---|
| `wheel_fix=0`, Wine-like steps | **no zoom** (every -120 dropped as a press) | zooms in **one** notch, then nothing |
| `wheel_fix=0`, Windows-like positions | zooms out to the farthest | |
| `wheel_fix=1`, Wine-like steps | zooms out to the farthest | zooms in to the nearest |

Page Down (1.5 s) zoomed out to the farthest and Page Up back in, with `builder=1` (the modal panels' input hooks installed and idle). **Not checked yet:** a real Linux PC (that `auto` switches on there by itself).

**Keyboard:** Page Down zooms out and Page Up zooms in (`bind +camera_zoom 'PG_DOWN'`, `bind -camera_zoom 'PG_UP'` in the profile's `input_a2.cfg`), on every system.

## Hero looks (`hero_skins*.cpp`; on, but nothing changes until a look is chosen)

**What it is for:** the owner's request, "a way in game to select our hero's skin". A hero can wear the look of another hero **of the same faction**: its 3D figures on the adventure map, in battle and on the hero screen. The portrait, the class, the skills, the music and everything the rules use stay the hero's own. Plan and findings: [docs/plans/hero-skins.md](../../docs/plans/hero-skins.md).

**How a player chooses:** the small **Look / Apparence** button on the hero screen (in the "Armée & Stats" header, left of MMH5.5's two buttons, where the owner's sketch put it; it shows the portrait of the hero's look and a tooltip), or **F7** (`[skins] key`) on the adventure map, the hero screen or the town screen, opens a panel for this PC's player's selected hero (`hero_skins_panel.cpp`, the template builder's style, a modal overlay panel: the game gets no input while it is open). It shows the looks of the hero's faction as portraits, its own first ("son apparence"), each with the name of the hero it is named after and how many others wear it; the selected one lists them ("Aussi portée par : ..."). **Appliquer / Apply** saves the choice to `open-homm5-skins.ini` (next to `open-homm5.ini`, by the hero's internal name: `Godric=Freyda`; "Own look" removes the line) and makes it count: at once in single player and hot seat (the map figure is rebuilt); in multiplayer for every player a few seconds later (below). The choice is remembered for every game. The button is drawn by the DLL (ImGui) at MMH5.5's hero screen layout, scaled with the screen height: the screen's panel `UI/HeroScreen3/SingleHero` is at x 60, centred vertically, and the skill wheel's round button (`MMH55-Skillwheel.pak`, `Single/Army`, `Button_HS`) at 372,48 inside it; ours is 24×24 at 403,94 of the 1024×768 reference: the slot of MMH5.5's small square button (which MMH5.5 shows only sometimes, and which is in none of the screen's UI files), centred on the round button's line (y 106, measured in the owner's two screenshots). `[skins] hero_screen_button_x` / `_y` move it. It reads the mouse itself (the click also reaches the game, on an empty spot of the header). Not native yet: serving the screen's own UI file would need its command handler, not researched.

**The hero screen follows at once** (`[skins] live_hero_screen=1`): after Apply on the hero screen (single player), or when a changed look counts at its segment (multiplayer), the hero screen showing that hero closes and opens again, on the same page, at the next adventure-map frame. It is what the screen's own "next hero" arrow does when the player has one hero (`0x006c4880(screen, hero)`: the game's close, then a new screen with the old one's values); the new screen builds the 3D figure with the look. Only in the screen's own-hero mode, when it is shown and its close would close it (no dialog, nothing waiting for "are you sure"); otherwise nothing is done and the screen shows the look when opened again. The game never rebuilds the figure of a screen that is already shown, and making it do so crashed (DLL `9823B52E`). Log: `the hero screen closes and opens again for <hero> ...`, then `the hero screen is open again after 500 ms (...): <hero> with <look>'s look, battle model <file>`. Details: [the plan](../../docs/plans/hero-skins.md), "The hero screen's 3D figure changes at once".

**Who decides a hero's look:** its owner, if human. In single player and hot seat, the file's choices count for every human player's heroes. In multiplayer, a PC's choices count for its own player's heroes; the other players' choices arrive by message. Computer players' heroes keep their looks. A look of another faction is left out (the log says so).

**How it works** (details at the top of `hero_skins.cpp`):
- The game asks the hero object for its battle figure and its map figure (two virtual functions, `IHero` slots `0x248` and `0x24c`). For a hero with a look they answer the look hero's figures; the hero object itself, which goes into saves and world checksums, never changes.
- The look's strike figure, map animations and hero screen camera are put into the hero's data object, in memory only, with counted references; the hero's own are put back when the look goes.
- The DLL notes each map figure the game builds. In single player and hot seat it rebuilds a figure with the game's own code when its look should change (just after the map shows, when a hero changes owner). Battles and the hero screen pick up the look when they open.
- **Multiplayer: all or nothing.** Rebuilding a map figure makes new game objects, which take numbers from the world's counter, so every PC must rebuild the same figures at the same moment of the game, or the PCs' worlds part (the robot saw the world checksums split). After its second segment each PC sends its choices as world-checksum messages with check number −1, which the game ignores (the co-op guard's trick): a header with a fingerprint of the hero list (`k`), then one message per choice (`K`). At the start of segment 20 every PC applies the same rule: if every PC sent its looks in time (before its own report of segment 11) with the same game data, every PC rebuilds the same figures, in the order of their object numbers, and the game builds later figures with the looks; otherwise **nobody's looks show in that game**. So one friend without the DLL (or with another MMH5.5 version, or `show_others=0`) means normal looks for everyone in that game; the log says which PC.
- **A choice made during a multiplayer game** (the panel's Apply) is sent at this PC's next segment report R, and every PC (this one too) takes it in at the start of segment R + 10 and rebuilds that hero's figure there: the same moment everywhere, a fraction of a second to two seconds later. In a game whose looks are off, it is only saved, for the next games (the panel says so).

The log says what happened, for example:

```
hero skins: on; 3 line(s) in open-homm5-skins.ini (read when the game's hero list is); multiplayer looks when every PC takes part
hero skins: 179 heroes in the game's list (fingerprint 0518, other entries skipped); 2 look(s) chosen in open-homm5-skins.ini
hero skins:   Godric wears the look of Freyda
hero skins:   Axel=Giovanni: another faction: left out (same faction only)
hero skins: Jeddite: the map figure is rebuilt with Freyda's look
hero skins: sent to the other PCs: 2 look(s) for player 1 (after this PC's report 2)
hero skins: from PC 1: player 2 sends its looks (same game data)
hero skins: looks on for this game (all 2 PCs sent theirs, with the same game data)
hero skins: no looks in this game: PC 1 sent none in time (no open-homm5 DLL, an old one, other game data, or show_others=0)
```

**Checked with the robot** (A1M4; details in the plan's "Robot results"): single player (the look on the map and in a battle the AI started; the panel opened, a look picked and applied: the map figure changed at once), LAN with different looks on each PC (793 checksums, all equal), LAN with one PC taking no part (no looks, 517 checksums, all equal), LAN with a look changed through the panel during the game (both PCs rebuilt the figure at the same segment, 702 checksums, all equal).

**Settings** (`open-homm5.ini`):

```ini
[skins]
skins=1            ; 0 = install nothing
key=F7             ; the panel's key (F1 to F12, or none)
hero_screen_button=1   ; 0 = no Look button on the hero screen
live_hero_screen=1     ; 0 = an open hero screen shows a new look only when opened again
test_button_on_map=0   ; testing only: the button also on the adventure map
show_others=1      ; 0 = this PC takes no part in multiplayer looks (then nobody's show there)
trace=0            ; testing: log each figure built and who asks for the figures
test_ai_heroes=0   ; testing only: computer players' heroes take this PC's looks too
```

The robot passes them with `--skins-setting KEY=VALUE` and the choices with `--skins-file PATH` (`run_lan_test.py`: `--host-skins` / `--client-skins`, `--host-skins-file` / `--client-skins-file`). `tools/robot/make_skins_file.py` writes a choices file where every hero of a faction wears one look, read from the test copy's data.

## Wider volume range (`audio_volume.cpp`, off by default)

**What it is for:** even at the lowest step of the options screen's sliders (music, sounds), the game was too loud. The sliders have 11 steps from 0 to 100%, and they are straight. The 10% step is a tenth of the sound wave's size, only 20 dB below full volume. With `volume_curve=wide` each step is about the same number of decibels: 10% becomes about −40 dB (10 times quieter), 50% −12 dB, and 100% stays as it is. Off (the default, `game`), the game's own sliders.

**How** (details in [docs/plans/lobby-audio-classes.md](../../docs/plans/lobby-audio-classes.md), item 1):
- The sliders store their value times 255 in the sound effects' manager and the music system.
- Every channel's volume then goes to FMOD through one function, `0x00b26e80`, called from two places: `0x00d0439b` (sound effects) and `0x00d047ac` (music).
- The mod redirects those two calls and multiplies the volume once more by its slider's value, so a slider value *s* counts as *s*².
- Nothing stored changes: the slider, your profile and every place that lowers the music for a while (dialogs, cutscenes) keep working as before.
- A sound the game makes audible stays audible at FMOD's quietest step.

**Only your speakers change:** sound is not part of the game world. Each player can choose, also in co-op games. The log says `audio: volume_curve=wide: ...`.

**Checked with the robot** (2026-11-01; `--audio-setting volume_curve=wide --audio-setting log_volume=200 --setvar "sound_sfxvolume = 0.1" --setvar "sound_musicvolume = 0.1"`):
- the log has `audio: volume_curve=wide: ...`;
- the main menu's music fades in from `game volume 1/255 -> 1/255` to `game volume 25/255 -> 2/255` and stays there;
- on the map, a sound effect has `audio: sound: slider 25/255, game volume 25/255 -> 2/255`. So at the lowest step FMOD gets 2 instead of 25 (about −42 dB instead of −20 dB);
- the game ran normally, with a battle.

Still to check: the owner's ears (the lowest step quiet but audible, music and sounds balanced as before). In a LAN game where only the host used it, the world checksums stayed equal (700, below).

**Settings** (`open-homm5.ini`):

```ini
[audio]
volume_curve=game   ; game (the game's sliders), wide (each step about the same number of decibels)
log_volume=0        ; testing: log the first N changes of an audible channel volume (slider, game's volume, ours)
```

## Black past the map's edge (`edge_sky.cpp`, off by default)

**What it is for:** zoomed out (above), you see past the adventure map's edge. On many random maps that is a flat dark violet with a lavender corner. It is a **sky dome**, the Sylvan siege battlefield's sky: MMH5.5 gave it to `Lights/_(AmbientLight)/Arena/Dusk_light_BoatArena01(VicTest).xdb`, one of the 4 lights the random map generator picks from (`RMG/Params/Default.xdb`). The original game's copy of that light has no sky dome. With `edge_sky=none`, the game gets that light without its sky dome, so past the edge the screen stays black, like unexplored land. Off (the default, `game`), the game's sky.

**How:** when the game opens that light file, the mod hands it the same file with an empty `<SkyDome/>`, as the lights without one have. This uses the game's file system hook (the one the original music and the French texts use). Nothing on disk changes.
- In the game data only the generator's list names this light, so **battles keep their sky**.
- The other 3 random-map lights and hand-made maps keep theirs. They show the Haven sky; if that should go too, it is the same work.
- Restart the game after a change: the game keeps a light once it has read it.

**Only your screen changes:** lights are drawn, not simulated. Each player can choose. The log says `edge sky: ... served without its sky dome (edge_sky=none)` when a map uses that light.

**Checked with the robot** (2026-11-01, `zoom_out=300` and the far start position of camera.md):

| Map | `edge_sky=game` | `edge_sky=none` |
|---|---|---|
| a random map of the owner's that drew the dusk light (`MAP_JoacJoz`, started with `--map /Maps/RMG/81CD0EF2-7D11-41D7-AE71-0A74BF0872A0/map.xdb`) | past the top-left edge: the flat violet and lavender of the owner's screenshot | black; the log has `edge sky: lights/_(ambientlight)/arena/dusk_light_boatarena01(victest).xdb served without its sky dome` |
| A1M4 (another light: tested with `edge_sky_test_light=Lights/_(AmbientLight)/AdvMap/Addon/AC1M4.xdb`) | a pale sky glow above the far edge | black |

- **Battles:** a battle in the same game drew its battlefield as usual. The log showed no other light served, so only the named light files change.
- **LAN game**, only the host with `edge_sky=none`, the test light, `volume_curve=wide` and French texts: 700 world checksums, all equal on both copies.
- Give a Git Bash command line `MSYS_NO_PATHCONV=1`, or use PowerShell: Git Bash rewrites `--map /Maps/...` into a Windows path, and the game then says `Couldn't find map`.

**Settings** (`open-homm5.ini`):

```ini
[camera]
edge_sky=game            ; game (the game's sky), none (no sky dome on the random maps' dusk light: black)
edge_sky_test_light=     ; testing: one more light file to show without its sky dome
```

## Game rules from the host (`host_rules*.cpp`; on, and nothing changes unless the host changes something)

Plan, research and robot checks: [docs/plans/host-rules.md](../../docs/plans/host-rules.md).

**What it is for:** MMH5.5 has switches for many of its rules (governors, town conversion, the town gate, mine guards, the computer's weekly bonus, mixed neutral armies, prank artifacts, the random map extras...), in `scripts/H55-Settings.lua`. Until now a group changed them by swapping a settings pak on every PC. With this mod **the host chooses them for each game**, in a panel, and every PC plays with them.

- **The host:** on the game setup screen or in the waiting room, **F2** (or **"Rules..."** at the right of the waiting room's title) opens the panel: 15 switches, each with one line of explanation, in French or English. Gold = changed from your MMH5.5 file. **OK** keeps the choice (also for your next games, in `[rules]`); **MMH5.5's file** puts everything back.
- **The other players** see the host's rules there, read only. On the adventure map, **F2** shows the running game's rules to everyone.
- **They are used only when every player has the add-on.** Otherwise every PC uses its own MMH5.5 file, as without the add-on; the host's panel names who lacks it, and the button gets a red "!".
- **The check:** each PC tells the host a fingerprint of its MMH5.5 files (`H55-Settings.lua`, the stats paks). If a player's files differ, the host's panel and log name that player (the game could go out of step); that player's panel says so too.
- **How:** the values travel in one more part of the game's own start message, so it works on any map; when the map's scripts read `H55-Settings.lua`, every PC's add-on hands them the file with the host's values (only those numbers change). The log says which values were used, on every PC, with the same hash:
  `rules: the scripts read scripts/H55-Settings.lua: served with the host's rules (governance 1->0, ai_cheat 4->0, mixed_stacks 1->0; ...); 8206 -> 8206 bytes, hash a07c5c34`
- **Good to know:** MMH5.5's scripts only run on maps with a map script. Hand-made maps and random maps saved with a script (MMH5.5's Mapmixer, the editor: the group's usual maps) have one. **A random map straight from the game's generator, in a multiplayer game, has none: there MMH5.5's scripts do not run at all**, so these switches change nothing (the settings pak does not either).
- Single player uses your MMH5.5 file, as before.

**Game mode and ToE start bonuses** (the panel's first group; `host_rules_bonus.cpp` for the lobby's tooltips). In MMH5.5 the lobby's bonus arrows choose the game mode (artifact = Default, gold = Savage, resources = Nightmare, random = one of them), and MMH5.5's start script then takes every human's bonus back and gives each first hero a random minor artifact instead. Two host rules change that:
- **Game mode:** "Bonus arrows (MMH5.5)" (the default: as today), or Default, Savage, Nightmare, Random for everyone. Savage and Nightmare still team the computer players against the humans, as MMH5.5 does (MMH5.5's own code runs; only its input changes). Scenarios, campaigns and games without computer players keep MMH5.5's choice (Default).
- **ToE start bonuses:** on = each player picks a start bonus with the arrows, as in Tribes of the East (gold, resources, a minor artifact, or random), and keeps it; MMH5.5's start artifact is not given (it is the same kind of bonus, so a player would get two). With this on and the mode on "Bonus arrows", the mode is Default.
- **How:** when the map's scripts read `scripts/H55-AdvMap.lua`, every PC serves it with three small edits in its "StartingBonus" part. Each edit is checked against MMH5.5's text; if MMH5.5 changes that text in an update, the file is served as it is (MMH5.5's way) and the log says why. RC19d and RC19k have the same part. The start rules are used only when every PC has the same part (its hash travels in the hello); otherwise the panel says why and MMH5.5's way stays.
- **The tooltips:** with these rules on, the bonus icons' tooltips in the waiting room describe ToE's bonuses and the game mode (English or French, as the panel). Not changed: the icons themselves (MMH5.5's mode pictures).
- The log: `rules: the scripts read scripts/H55-AdvMap.lua: served with game mode Default, each human keeps the ToE start bonus; ... hash ...` (the same hash on every PC).

**Robot checks** (2026-11-01, LAN, two copies; all world checksums equal):
- A1M4 (RC19k) with `governance=0 ai_cheat=0 mixed_stacks=0`: both PCs served the same file (hash `a07c5c34`), MMH5.5 no longer mixed the neutral stacks (`H55 Stacks Qty: 36`, no `H55 Mixing`). The client saw the host's rules in the waiting room and in game.
- The owner's `MAP_JoacJoz` (a random map with a map script): the same, plus `competitive_towns=0`.
- A client without the rules (`enabled=0`): no rules sent, both PCs used their file (and mixed the stacks, as the file says).
- A random map straight from the generator: MMH5.5's scripts did not run on either PC (no map script); the host's panel says so.
- **Saves:** loading a save opens no script file; the scripts go on with the values the game started with (single player, robot).

More in the plan.

**Settings** (`open-homm5.ini`):

```ini
[rules]
enabled=1     ; 0 = never take part (no hello, always this PC's own file)
key=F2        ; the panel's key (setup screen, waiting room, adventure map); none = no key
button=1      ; the "Rules..." button in the waiting room
governance=   ; the host's choices, written by the panel (empty = MMH5.5's file); also
              ; town_conversion, extra_conversions, town_gate, mine_control, dimension_doors,
              ; ai_cheat, mixed_stacks, neutral_stack_size, bank_difficulty, negative_artifacts,
              ; rpg_potions, competitive_towns, mana_observatories, moon_disc
game_mode=    ; 0 or empty = the bonus arrows (MMH5.5), 1 Default, 2 Savage, 3 Nightmare, 4 Random
toe_bonuses=  ; 1 = each player keeps the ToE start bonus picked with the arrows (0 or empty = MMH5.5's way)
trace=0       ; testing: log every script file the game opens, and print each player's start
              ; resources and minor artifacts to the game's console
```

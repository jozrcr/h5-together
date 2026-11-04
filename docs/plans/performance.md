# Performance: click delay, script lag, AI turns (analysis, 2026-11-04)

Status: **analysis**, with the first measurements. The owner's question (2026-11-04): what can remove or drastically cut the delay of actions on the adventure map, the very bad frame rate with MMH5.5's scripts on, and the rest. The owner's copy of the analysis is the doc "Smoother co-op: delays and lag" (https://claude.ai/artifact/9PCViB3DVK8peePJrJ9Z8s); this file keeps the evidence and the numbers for later sessions.

## Clicks

Where a click's time goes, and what is already built: [responsiveness.md](responsiveness.md). In 0.3.1 a click takes about 0.12 s from sending to the move (robot over Steam, Windows host and Linux laptop, [online-steam-test2.md](online-steam-test2.md) run 3). What is left: half a game step (25 ms at 50 ms steps), the wait for the slowest PC's report, the display margin `view_delay_ms` (100 ms), a frame or two. Options, cheapest first: an adaptive margin (the trace already prints a "suggested view_delay_ms"), closing the step early when the local player sends a command, an instant on-screen cue (display only, cannot desync). Prediction with rollback is not worth it in this engine.

## Frames on the adventure map

### Robot profiles (tooling added the same day)

- `run_ai_test.py --profile-all` / `run_lan_test.py --host-profile-all`: the sampling profiler (`src/hook/robot_profile.cpp`) samples the game's main thread every ~2 ms all the time (not only while the AI thinks), with a time mark every 0.5 s (`# t <ms>`, the log's clock): `tools/robot/profile_report.py --from-ms N --to-ms M` keeps a time window (the map, not the menus).
- With `--profile-all`, single player also logs frame times every 10 s: `robot: frames: N/s, median, p95, longest` (multiplayer already has them in the lockstep line).

| Run (test copy `C:\Games\H5_test_k`, 1024x768, two copies on one PC) | Frames | Main thread |
|---|---|---|
| `20261104-114239-lan`: the owner's 6P-epic-3v1v1v1 (map script), 2 humans + 4 computer players, idle day 1 | ~95/s, longest frame ~30 ms | the game view `NGScene::CGameView` ~50% of samples; skeletal animation (`NAnimation::CAddBoneFilter`) ~31% inclusive; particle effects ~9% self; Lua and `NScript` ~0.3% |
| `20261104-114937-lan` / `-115401-lan`: random map EDD31DF8, 2 humans + 2 computer players, 16 days, `map_scripts=0` / `1` | 95 to 250/s in both | drawing (`NGScene`) 37% / 41% of the map's samples; `NScript` 0.3% in both |

So in robot games (humans with one or two heroes, early days) the scripts cost almost nothing; drawing dominates. The robot cannot yet reproduce a late game with many heroes: the owner's recent saves are multiplayer saves, and the robot cannot start a loaded multiplayer game (its waiting room arrows do not work there yet, [turn-features.md](turn-features.md) part 2).

### A real game: the friend's log of 2026-11-03 (3 players over Steam, a map with MMH5.5's scripts)

The lockstep lines (every 10 s): **20 to 100 frames per second on the map, and a frame of 150 to 1000 ms every few seconds** (Linux, Proton). That is the stutter the owner felt.

### MMH5.5's scripts (read by a research agent, not timed)

- One loop does nearly all the continuous work: `H55_ContinuesActivator` (`H55-Core.lua` 11038, in `MMH55-Index.pak`): `while (H55_Switch == 1)`, `sleep(H55_CycleSpeed)` (2, set at line 21), then `H55_ContinuesEvent(i)` (lines 7518 to 10690) for each player whose turn it is, which goes through every hero of that player: about 300 checks, at least ~400 engine calls (`HasHeroSkill`, `HasArtefact`, `GetHeroSkillMastery`...). In simultaneous turns every human is current. All of it in one go: one long frame per run.
- The costliest parts: Mana Control (line ~7771: for a hero below its mana, `H55_GetTownRace(town)` for every town, each a map-wide `GetObjectNamesByType` per town type); the six town lists rebuilt per player per run (7525 to 7532); artifact-set counts recomputed 2 to 4 times (from 8137); possibly duplicate loops after the new-day crash check (10692 to 10701: the console line `H55 Amount of Recovered Crashes: N` tells).
- **Our co-op timing makes it run more often**: `sleep` most likely counts game steps (to confirm), so at 50 ms steps the loop runs every 0.1 s instead of MMH5.5's 0.4 s. `H55_CycleSpeed` is set before `H55-Settings.lua` is read, so the settings file our host rules already serve can override it (8 at 50 ms steps restores 0.4 s).
- Leaner versions with the same results (served in place of MMH5.5's file, identical on every PC): town lookups from the lists already built, set counts once per hero, town lists once per run, a loop that cannot run twice.
- Polled although the game has a trigger: daily and weekly events (`NEW_DAY_TRIGGER`), after-battle bonuses (`COMBAT_RESULTS_TRIGGER`), level-up spells (`HERO_LEVELUP_TRIGGER`), newly hired heroes (`PLAYER_ADD_HERO_TRIGGER`).

## Computer players' turns

Parallel turns ([turn-features.md](turn-features.md) part 3): the computer players' part of a day 39% shorter with two (LAN, PCs in step), 46% with three (single player, the owner's MAP_JoacJoz with its scripts: 18.8 s to 10.2 s). The AI's own decisions can take 0.3 s inside one frame on huge maps (earlier logs, the 80th percentile): to profile on a late-game save.

## Next

1. Measure a late game with the scripts: one long frame every 0.1 s on the host, gone with `H55_CycleSpeed = 8`, would confirm the cause. Needs a way to run a loaded multiplayer game with the robot (the loaded waiting room's arrows), or the owner's logs and bug reports.
2. The rhythm fix (a host rule line in the served settings), if confirmed.
3. Parallel turns as a host setting; the adaptive display margin; the leaner core script; a drawing study.

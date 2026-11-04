# Steam test 3: a real LAN game between two PCs over Steam

Status: **Wi-Fi run passed (2026-11-03 morning); 4G runs and a hand-played game the same evening: in step everywhere, one failed join (fixed: reliable messages)** (see [Results](#results)). Both PCs use **the same `dinput8.dll`: the one in the package zip GitHub builds for the pull request** (the Actions run's artifact `h5-together-<version>`; downloading it needs a GitHub login: the owner, in a browser, or `gh run download`). The laptop's helper is `tools/robot/laptop_test.py` in the repo (`git pull`). The first kit, `h5-together-steam-test3.zip` in the session's scratchpad, could not reach the laptop (no DLLs in git, no file sharing over the LAN), so the test moved to the next day.

This is the first real test of step 3 of [online-steam.md](online-steam.md): the virtual LAN with its **Steam transport** (`[online] mode=steam`). The earlier two-PC test (the spike) only sent test messages between the two games. This time the **game's own LAN play** goes through Steam: the host creates a LAN game, and the other PC sees it in the game's own LAN list, joins it, and they play.

## In short

- **Desktop (Windows) = host:** the test copy `C:\Games\H5_test_k` (MMH5.5 RC19k), driven by the robot. It creates a LAN game on A1M4 and waits about 7 minutes for the laptop, then starts.
- **Laptop (Ubuntu 25.04, Snap Steam, Proton 11) = joiner:** the Steam game folder with MMH5.5 RC19k. A Claude session on the laptop installs the test DLL and starts the game. The in-DLL robot can do the joining by itself (no human needed), or a person clicks.
- Both PCs use their own Steam account; the two accounts are Steam friends (as in the spike).
- **It worked if:** the laptop's LAN list shows the host's game, the laptop joins, the game starts, a few AI turns play on both PCs, the world checksums stay equal, and nothing crashes.

## What each PC needs

| | Desktop (host) | Laptop (joiner) |
|---|---|---|
| Game | `C:\Games\H5_test_k`, MMH5.5 RC19k, `MMH55_Utility_64.exe` | `/home/joz/snap/steam/common/.local/share/Steam/steamapps/common/Heroes of Might and Magic 5 Tribes of the East`, MMH5.5 RC19k (`bin/55u.dll` sha256 `a28663da…`), Steam's launch option starts `MMH55_Utility_64.exe` |
| Our DLL | the PR package's `dinput8.dll` (`run_ai_test.py --dll <path>` copies it in) | the same file → `bin/dinput8.dll` (`laptop_test.py install --dll <path>`) |
| Valve's `steam_api.dll` (SDK 1.65, 32-bit) | `bin\h5-together\steam_api.dll`, from `ref\steamworks_sdk_165\sdk\redistributable_bin\` (copied in for the run, removed after) | already in `bin/h5-together/` since the spike (kept) |
| `bin\h5-together.ini` | the robot writes `[online]` `mode=steam` | `[online]` `mode=steam` (`laptop_test.py install` adds it and removes an old `spike=` line) |
| Steam | running, logged in | running, logged in; launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%` |

Nothing else changes in either game: `mode=steam` only changes how the game's LAN packets travel. Without `steam_api.dll` or without Steam, the add-on says so in its log and the game plays its own LAN, as without the setting.

## What to get

- **`dinput8.dll`:** from the package zip of the pull request's GitHub Actions run (artifact `h5-together-<version>`, inside `h5-together-<version>/dinput8.dll`). Both PCs use this one file; its SHA-256 is checked on both sides before the run. (Two different builds of one version would show as "another build" in the waiting room.)
- **`tools/robot/laptop_test.py`:** the laptop's helper (Python 3, no extra packages): `install [--dll PATH]`, `robot`, `collect`, `clean`, `uninstall`. It keeps everything it replaces in `bin/h5-together-test3-backup/` first. It comes with the repo (`git pull`).
- **`steam_api.dll`:** Valve's, Steamworks SDK 1.65; the laptop has it since the spike, the desktop copies it from `ref\`.

## Steps

The order matters a little: **start the host first**, then start the laptop's game **between 1 and 4 minutes later**. The host waits until about 7 minutes after its start, then starts the game.

### 1. Desktop (host): start the robot

From the repo's worktree of the branch (PowerShell). Before the run: no game running (`H5_Game`, `MMH55_Utility_64`, `MMH55_Utility`, `MMH55_64`, `MMH55`), the shared robot lock taken, `bin\dinput8.dll`'s sha256 noted, `steam_api.dll` copied in. The session's helper does all four:

```powershell
& "$scratch\steam-transport-robot.ps1" -Step before -SteamDll
```

(`$scratch` = the session's scratchpad folder.) Then the host run (one line):

```powershell
python tools\robot\run_ai_test.py --game C:\Games\H5_test_k --dll <the PR package's dinput8.dll> --no-map --turns 8 --seconds 1200 --shot-every 0 --online-setting mode=steam --trace-crc --net-log --net-scan --setvar "lan_last_login = Robot-host" --setvar "mp_check_crc = 1" --step "8|menu|multi lan" --step "12|ui|NUI::CLobbyScreen create" --step "16|map|A1M4/" --step "18|msg|create" --step "24|shot|host-waiting" --step "240|slots|" --step "360|slots|" --step "400|arrow|0 ready" --step "402|arrow|2 right" --step "405|slots|" --step "406|shot|host-ready" --step "410|msg|start" --step "430|shot|host-started"
```

What the robot does: Multiplayer > LAN > Create, map A1M4, create the game (about 25 s after the window opens). Then it waits. At 400 s it gets ready and makes the map's third player a computer player, at 410 s it starts the game, and it closes the game after 8 AI turns. The player's human turns are ended by the robot.

After the run (also if it was stopped):

```powershell
& "$scratch\steam-transport-robot.ps1" -Step after
```

It puts `dinput8.dll` back, removes `steam_api.dll` and the files the run added, and drops the lock. The results are in `build\robot\<time>\` (`h5-together.log`, screenshots).

### 2. Laptop (joiner): for the Claude session on the laptop

Do these in order. `GAME` below is the game folder: `/home/joz/snap/steam/common/.local/share/Steam/steamapps/common/Heroes of Might and Magic 5 Tribes of the East`.

1. **Check that no game is running** (`pgrep -fa MMH55` shows nothing). Quit the game if it is.
2. **Get the files:** `git pull` in the repo clone (for `tools/robot/laptop_test.py`); unpack the package zip the owner downloaded into `~/h5-steam-test3/` (outside the game folder) and check `dinput8.dll`'s SHA-256 against the one the desktop session sends. Then go to the repo clone's `tools/robot/` folder.
3. **Install:** `python3 laptop_test.py install --dll ~/h5-steam-test3/h5-together-<version>/dinput8.dll`
   - It copies our `dinput8.dll` into `GAME/bin/`, `steam_api.dll` into `GAME/bin/h5-together/` (if it is not there yet), and puts `[online]` `mode=steam` in `GAME/bin/h5-together.ini`. The old files go to `GAME/bin/h5-together-test3-backup/`.
   - Check that Steam's launch option for the game is still `WINEDLLOVERRIDES="dinput8=n,b" %command%` (plus whatever starts `MMH55_Utility_64.exe`).
4. **Choose who joins:**
   - **No person at the laptop (the robot joins):** `python3 laptop_test.py robot`. It writes `GAME/bin/h5-together-robot.ini` (the in-DLL robot's script), makes a robot profile in `GAME/robot_home/` (windowed 1024×768, muted, player name "Laptop", the world checksum on), and moves the three intro videos to `GAME/video/robot_disabled/`. Once the game's window is up, the robot opens Multiplayer > LAN at 10 s, picks the first game in the list at 40 s and joins it, presses "ready" every 20 s until it is ready, ends its own turns during the game, and closes the game after 20 minutes. It saves screenshots in `GAME/robot_shots/`.
   - **A person at the laptop:** skip this step. They open Multiplayer > LAN, pick the host's game, click Join, then the ready arrow.
5. **Wait for the go:** the host must have been started at least a minute before (step 1), so its game is waiting when the laptop's LAN list opens.
6. **Start the game:** `/snap/bin/steam steam://rungameid/15370`
7. **While it runs** (about 10 minutes), the log is `GAME/bin/h5-together.log`. Useful lines (`grep -a "vlan\|robot: games\|robot: waiting\|AI turn" GAME/bin/h5-together.log`):
   - `vlan steam: SteamAPI_InitFlat -> 0 (OK)` and `Steam started in … as app 15370; we are #…; our game is "mmh55/276f239a"`: Steam started. The game's name must be the same as on the host (same game build and MMH5.5).
   - `vlan steam: relay network: OK`
   - `vlan steam: 1 of our N Steam friends play app 15370 now, 1 of them in a lobby`: the host is seen through the friends list.
   - `vlan steam: friend #… hosts a game: lobby #…, H5 Together 0.2.7, discovery port 8889, …`: its lobby data was read. **Or** `… its data does not come; our queries go to its game at the usual port 8889 anyway`: Steam did not show the lobby's data from outside (the fallback; please report which of the two lines appeared).
   - `robot: games list: 1 games, game 0 highlighted`: the host's game is in the laptop's LAN list (the screenshot `client-lobby` shows it).
   - `vlan: our address 198.… is final now (our game's first packet to a peer)` and `vlan steam: our game plays with #…'s game: joining its lobby #…`, then `we are in lobby #… now, 2 members`: the join.
   - `vlan steam: the session with #…: connected, ping … ms, direct (not relayed)` (or `through Steam's relay in …`).
   - `robot: waiting room: …` and, once the host starts, the game's own lines (the map loads, AI turns).
   - Lines with `CLASH`, `could not be sent` going up, `crash`: report them.
8. **After the game has closed** (or after 20 minutes; `pkill -f MMH55_Utility_64` if it hangs on a "host disconnected" box): `python3 laptop_test.py collect`. It copies the log, the old log, `console.txt`, the settings and the screenshots to `~/h5-steam-test3-results/<time>/`.
9. **Report back:** the lines of step 7 that appeared (with their times), the screenshots' names, the last `vlan: so far: …` and `vlan steam: so far: …` lines, and `robot: net: address scan: …` (the last one). Keep the results folder; the whole log is wanted later.
10. **Clean up:** `python3 laptop_test.py clean` removes the robot's files and puts the intro videos back; our DLL and `mode=steam` stay for the next test. `python3 laptop_test.py uninstall` also puts back the DLL and `h5-together.ini` from before the test.

### 3. Reading the results (desktop)

- Put the two logs side by side as a robot LAN run: `build\robot\<time>-steam2pc\host\h5-together.log` (the host's) and `…\client\h5-together.log` (the laptop's), then `python tools\robot\lan_report.py build\robot\<time>-steam2pc`. It compares the world checksums of the two PCs and shows the AI timings.
- In both logs: the address scan (`robot: net: address scan: … 0 hold an address of this PC`) must say 0: no real address of either PC went into a game packet.

## What success looks like

| Check | Where |
|---|---|
| Steam starts on both, same game name `mmh55/276f239a` | `vlan steam: Steam started …` on both |
| The host makes its lobby when its game hosts | host: `vlan steam: our lobby #… is made` |
| The laptop sees the host through the friends list and its game in the LAN list | laptop: `friend #… hosts a game` (or the fallback line), `robot: games list: 1 games`, screenshot `client-lobby` |
| The laptop joins | host: `#… (a friend) starts talking to us: accepted`, `#… entered the lobby`; laptop: `we are in lobby #… now, 2 members`; host's `slots` lines show two human rows |
| The game starts and AI turns play | host: `robot: AI turn …`; screenshots `host-started`, `client-started` |
| No desync, no crash | `lan_report.py`: world checksums all equal; no crash report |
| No real address in any packet | `robot: net: address scan: … 0 hold an address of this PC` on both |

## If something goes wrong

| What the log says | What it means | Next |
|---|---|---|
| `cannot load …steam_api.dll` | the file is not in `bin/h5-together/` | `install` again |
| `SteamAPI_InitFlat -> 1 or 2` | Steam is not running, or not reachable from Proton | start Steam first; start the game from Steam |
| `0 of our N Steam friends play app 15370` on the laptop | Steam does not show the host as playing ToE | both accounts friends? the host's game running (`vlan steam: our lobby … is made` on the host)? |
| `friend #… is in lobby #…: no H5 Together data (yet)` and no fallback line after 6 s | the lobby's data never came, and the fallback did not start | report it (a bug of ours) |
| `hosts a game we cannot join … its game is "…", ours "…"` | another MMH5.5 or game build | install the same RC19k on both |
| `wants to talk to us: ignored` | the other account is not a Steam friend | make them friends |
| `robot: games list: 0 games` | the host's game was not in the list at 40 s | start the laptop's game a bit later (the host must be waiting), or join by hand |
| The game starts, then a desync box | the virtual LAN carried something wrong | send both logs |

## Results

### Run 1: home Wi-Fi (2026-11-03, morning): passed

Both PCs ran the package DLL of `main` at 4f51455 (artifact `h5-together-0.0.0-dev.196`, `dinput8.dll` sha256 `4bd557223abe…`). The desktop was on Ethernet, the laptop on the same home network over Wi-Fi. Robots on both sides, nobody clicking.

| Check | Result |
|---|---|
| Steam on both, same game | `mmh55/276f239a` on both; the relay network was ready on the host in 10 s |
| Host's lobby | made 25 s after the host's game started, friends only |
| Laptop finds the host | through the friends list, 2 s after its Steam started (`friend #… hosts a game: lobby #…`); the host's game is in the game's own LAN list ("Trefoil", 1/3) |
| Join | the laptop's robot joined at 49 s of its run; Steam session **direct, 3 to 5 ms**; the waiting room said "Every player has H5 Together 0.2.7." on both |
| Game | started by the host at 410 s; co-op timing ON (50 ms segments, both PCs agree); 9 days, 8 AI turns; the host's robot closed the game on purpose (`8 AI turns done`) |
| Desync | none: about 1,500 world checksums compared on each PC, all equal |
| Real addresses | none in any packet, on either PC. Each side's scan also found 3 packets holding `01 00 00 7f`, which reads as 127.0.0.1 backwards: the PC's own loopback address, not a real one, and probably just game data that happens to look like it |
| Messages | about 17,000 each way through Steam, 0 refused, 0 unreadable |

Things seen, not failures:

- **Pacing:** in the game, the host waited on average 360 to 630 ms per segment for the laptop's report (p80 up to 885 ms); the laptop showed 129 to 149 frames per second under Proton. AI moves took about 0.5 s each (median gap 514 ms), close to the 0.44 to 0.49 s measured in LAN games with short segments ([phase1-report.md](../phase1-report.md)). On a home network the Steam path costs little.
- **When the host closed its game**, the laptop's game did not stop: it took over the AI player and played days 11 and 12 alone, until the laptop session ended it. That is the game's own behaviour when a host leaves.
- **A wasted retry, fixed after the test:** after the host left, the laptop kept asking Steam to join the host's old lobby every 5 s (Steam answered 2, "no such lobby"), because its game still sent packets to the host. Now a lobby whose host left is never joined again; a new lobby of the same host still is (`LobbyPlan::Forget`, test in `virtual_lan_test.cpp`).
- The helper's line about Steam's launch option was misleading (the laptop's option also swaps the exe for MMH5.5's); reworded.

### Run 2: 4G (2026-11-03, evening): in step, one failed join

The laptop on a phone's 4G hotspot, the desktop on the home network. Both PCs ran the package DLL of [PR #85](https://github.com/jozrcr/h5-together/pull/85) (online step 4, with `main` and its #84 fix merged in: artifact `h5-together-0.0.0-dev.201`, `dinput8.dll` sha256 `088da6b61cda…`). Three games:

| | 2a: robots | 2b: robots again | 2c: by hand |
|---|---|---|---|
| Route | Steam's relay (Paris), ping 50 ms | relay (Paris), 43 to 50 ms | **direct**, 144 to 156 ms |
| Join | first try | **failed** ("Timeout. May be no more free slots available in the game. Error code 0.18.0"); worked when clicked again | first try, from a Steam invite |
| Game | 4 days, then the host's robot closed it (no AI: see below) | started by a click on the host; 8 AI turns, 8 days | 5 days, 2 humans and 2 AI, 3 autosaves |
| World checksums | 185 compared, all equal | 2,384 compared, all equal | not traced (no robot) |
| Real addresses, refused messages | none | none | none |

**2a.** The owner clicked in the host's waiting room while its robot ran (ready, a start bonus, the AI row). The robot's blind "AI row: arrow right" then closed the AI row instead of opening it, so the game had no computer player, and after 4 turn ends without an AI step the robot's stuck check closed the host's game. Not a fault of the add-on; robots and hands do not mix.

**2b, the failed join.** The laptop's game sent its join request (31 bytes, starting `01 01`) five times, 0.2 to 1.6 s apart; the host's game got all five. In the joins that work, the host answers at once with 33 bytes starting `04` ("accepted"), then both sides talk. This time no `04` ever reached the laptop. Instead the host's game sent its in-game stream (18-byte packets every 50 ms, acknowledging nothing) to the laptop for about 85 s: it had taken the laptop in, but its "accepted" was lost on the way, and the game never sends it again (a duplicate request is ignored). On a LAN nothing is lost, so the game was never made for this. The host's waiting room kept the laptop's row open, and a second click on Join (a new request) worked at once. **Fix:** the Steam transport now sends every message **reliably** (Steam sends a lost one again, as fast as the ping allows; `[online] reliable=1`, the default; `0` = each message once, as before), so the virtual LAN behaves like the LAN the game expects. The log's minute line now also says, for each session, the route, the share of packets that arrive each way, and what waits to be sent.

**Pacing (2b, AI turns).** The host's AI moves took a median of 700 ms each (p80 1.4 s), against 514 ms (p80 0.86 s) on Wi-Fi in the morning. While the AI played, the PCs waited about 0.5 to 1.2 s on average per 10 s window for the other's report (the laptop's worst single wait: 5.2 s); in the morning on Wi-Fi it was 0.4 s on average (worst 2.8 s). So 4G adds to the AI's own slowness; reliable messages may help here too (a lost report waited for the game's own resend before). To measure next time with `reliable=1` and `0`.

**2c, by hand: the owner's verdict "it works extremely well".** The laptop's game was not running: accepting the invite in Steam started it (`+connect_lobby`), the main menu showed the invite, the host's game was in the LAN list at once, and the join worked on the first click (the joining is still by hand: see step 4). Waits per 10 s window: 116 ms on average, the worst single wait 1.3 s. Autosaves took about 0.25 s.

Things seen, not failures:

- **The overlay's first notification froze the host for 10 s.** In 2a, right after Steam showed the host "a friend is now playing", the host's game stopped drawing for 10.5 s inside Steam's overlay code (the game's `BeginScene` call, then `crashhandler.dll` waiting). Not in 2b or 2c. Steam's, not ours; worth watching in real games.
- **Steam's overlay** is "not enabled" when Steam's API starts and enabled 5 s later, on both PCs, whether Steam started the game or not (under Proton too). So the online UI asks Steam about the overlay when the button is clicked, not at the start.
- **Fake addresses:** the laptop's address scan found 1 packet holding `01 00 00 7f` (127.0.0.1 backwards, as in the morning), nothing else.
- **Not ours:** at each autosave of 2c the host's log showed "rmg factions: REBUILD FAILED … the game will not be in step" about 20 times. Faction terrain was off; the start-message hook also runs when the autosave reads old saves' headers. Nothing was rebuilt and the game stayed in step; to be fixed on its own.
- **The online UI (PR #85)**, by hand: our Steam lines on the LAN screen are drawn over the game's error box (they should hide under a game's message box); the game's cursor goes under every panel of ours, and our buttons show no hover. To fix in the online UI and the overlay.

### Run 3: the co-op lag (2026-11-04, night): found and fixed

**Why.** In the first real 3-player games over Steam (the evening before), every click on the adventure map took about 0.2 s and AI turns dragged. The host's log showed it waiting 0.1-0.2 s per segment for one PC, a friend's Linux PC under Proton, although that PC drew at high frame rates and the network was fine.

**Setup.** Desktop host (Windows) and the Linux laptop as joiner, on the home Wi-Fi (direct, 1-12 ms), robots on both, A1M4, 8 AI turns. The laptop ran **on battery** (about 50 frames/s, frames up to 0.5 s), a slow PC, which is the case to fix. New timing lines (PR #91): every 200 segments each PC logs when it saw each player's report of that segment; `tools/robot/segment_timing.py` pairs two logs, so "the reports are slow on the way" and "a PC is behind" can be told apart.

| | before (0.3.0) | catch-up off | catch-up on | catch-up + faster link | **the new defaults, no settings** |
|---|---|---|---|---|---|
| Host: average wait per segment | 577 ms | 461 ms | 389 ms | 124 ms | **122 ms** |
| Laptop: average wait per segment (p80, worst) | 559 ms (1382, 3625) | 520 ms (1398, 2944) | 682 ms (1838, 4872) | 178 ms (340, 1440) | **212 ms (386, 1795)** |
| Host: time between AI moves (median / p80) | 570 / 891 ms | 558 / 891 | 154 / 597 | 175 / 326 ms | **150 / 272 ms** |
| Host: commands, from sending to their run | 523 ms | 484 ms | 193 ms | 164 ms | **118 ms** |
| 8 AI turns | 95 s | 90 s | 90 s | 80 s | **75 s** |
| World checksums | all equal | all equal | all equal | all equal (1,477) | all equal (1,381) |

**What the timing lines showed:**

1. **The PC behind stays behind.** With the catch-up off, at most sample segments the reports were quick on the way (both ways added: 10-180 ms) but the laptop reported 400-700 ms after the host. The game's pacing only slows a PC that is ahead; a PC behind never runs faster, so after a slow moment it stays behind, and everyone waits for it. **Fix:** a PC that the others have out-reported by 2 or more segments runs up to 50% faster until it has caught up (`[net] catch_up_behind=1`, `lockstep.cpp` point 6).
2. **During AI turns the host's link was full.** With the catch-up on, the laptop was no longer behind, and what was left showed: some reports took 0.5-2.4 s on the way, host to laptop. The game's link sends at most one packet of about 256 bytes of game data per send interval (10 ms then), and the AI's big commands (threat maps, paths) queued the host's segment reports behind them. **Fix:** a send interval of 2 ms and a keepalive of 10 ms (`net_tuning.cpp`, now the defaults); the reports then arrived within 0.2 s of each other.
3. **Under Proton, Steam calls cost more**, as expected: a send 80 us (8 on Windows), a read 35 us (2-3), our scan of the Steam friends list 15-20 ms (5 on Windows), every 2 s. The scan held the lock the game's own sends and reads use, so they waited up to 20-30 ms once per scan. **Fix:** the scan lets go of the lock every 8 friends (a 1 ms sleep; with `SwitchToThread` the thread took it back at once). With it, the laptop's longest waits for the lock went from 18-32 ms to 5-8 ms.

A LAN game between two copies on the desktop (no virtual LAN, the new defaults) stayed as good: both copies waited about 30 ms per segment, AI moves 136 ms apart (median), every check equal.

A busy link now sends up to about 140 KB/s (500 packets a second), an idle one a small packet every 10 ms: fine for home connections, to be watched with 4 PCs over the internet.

## Not tested here

- Three PCs, a saved game loaded over Steam.
- Reliable messages (`reliable=1`) on 4G: the next run.

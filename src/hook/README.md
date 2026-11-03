# Hook DLL (`dinput8.dll` proxy)

A DLL the game loads at start-up. It lets our code run inside the game, so improvements can ship before the decompilation is complete (the "hook track" in CLAUDE.md).

**What it does today:** forwards DirectInput to the real Windows DLL (so the game works normally). If the exe is a build we know, it also:

- logs every step of every AI turn with its timing to `h5-together.log` next to the exe;
- applies our mods from `src/mods/`: the co-op speed-ups, the fixes, the panels (player list, template builder, hero looks, game rules) and the camera, sound and text options. Each one is switched in `h5-together.ini` next to the exe; the list, with every setting, is in [src/mods/README.md](../mods/README.md);
- watches for freezes ([the hang watchdog](#the-hang-watchdog-watchdogcpp)) and writes a short report and a small minidump when the game crashes ([crash reports](#crash-reports-crashcpp)).

It can also pass DirectInput on to another mod that is a `dinput8.dll` too ([chain-loading](#another-dinput8dll-mod-chain-loading-chaincpp)).

Robot mode (`robot*.cpp`, used by `tools/robot/`) only turns on when an `h5-together-robot.ini` file sits next to the exe, i.e. in the test copy during robot runs.

**Contents:** [how it gets into the game](#how-it-gets-into-the-game), [drawing over the game](#drawing-over-the-game-d3d9_hookcpp), [the overlay](#the-overlay-overlaycpp-our-panels-over-the-game), [which exes it hooks](#which-exes-it-hooks), [the AI trace](#the-ai-trace), [the hang watchdog](#the-hang-watchdog-watchdogcpp), [crash reports](#crash-reports-crashcpp), [another dinput8.dll mod](#another-dinput8dll-mod-chain-loading-chaincpp), [build](#build), [owner test](#owner-test-phase-0-gate-dll-loads-and-logs-ai-turns-game-runs-normally), [files](#files).

## How it gets into the game

The game imports one function from `DINPUT8.dll` (`DirectInput8Create`). Windows looks for DLLs in the exe's own folder first, so a `dinput8.dll` placed next to the exe is loaded instead of the system one. Ours loads the real `dinput8.dll` from the Windows system folder and passes every call through.

It **adds** one file and replaces none, and it does not collide with MMH5.5 (which uses `55u.dll` in the `um.dll` import slot). It does not replace `d3d9.dll` either (on Linux, Proton/DXVK supply their own): to draw our panels over the game, `d3d9_hook.cpp` only replaces the exe's `Direct3DCreate9` import entry and entries of the Direct3D objects' vtables, whichever `d3d9.dll` made them.

## Another `dinput8.dll` mod (chain-loading, `chain.cpp`)

Other mods also come as a `dinput8.dll` next to the exe ("ASI loaders" for widescreen fixes, for example), and only one file can have that name. The player renames the other mod's file `dinput8_next.dll` ([docs/install.md](../../docs/install.md#another-mod-that-is-also-a-dinput8dll)), or names it in `h5-together.ini`:

```ini
[chain]
dll=           ; empty = dinput8_next.dll next to the exe; a file name or a full path; none = never
```

- **When:** in `DllMain`, after our own start-up, as the game would have loaded that mod itself (an ASI loader does its work when it is loaded). `LoadLibraryEx` with `LOAD_WITH_ALTERED_SEARCH_PATH`, so a mod in a folder of its own finds its own DLLs there.
- **Forwarding:** every export (`Real()` in `dllmain.cpp`) goes to that mod's function instead of Windows' `dinput8.dll`, and the mod passes it on to Windows'. An export the mod lacks goes to Windows' DLL.
- **When it calls us back:** a mod that asks for `"dinput8.dll"` by name gets ours (a DLL of that name is already loaded). A thread-local depth counter tells a call from inside one of ours (the mod calling back) from the game's: those go to Windows' DLL, never back to the mod, and our hooks and the robot only look at the game's own call.
- **Not used** (logged, then Windows' DLL as without it): no such file (silent for the default name), a copy of H5 Together itself (our marker text in it: two copies would hook the game twice), a file that cannot be loaded (not a DLL, 64-bit), one without `DirectInput8Create` (left loaded: its start-up code ran).
- The log: `chained: dinput8_next.dll (another dinput8.dll mod) is loaded, and DirectInput goes through it; ...`.
- The installer still refuses to replace a foreign `dinput8.dll`; its message now says to rename it `dinput8_next.dll`. The uninstaller leaves `dinput8_next.dll` alone and says to rename it back.
- Tests: `test/proxy_test.cpp` with a stand-in mod (`test/chain_dummy.cpp`, below). Robot check (2026-11-02, RC19k test copy): the stand-in as `bin\dinput8_next.dll` was loaded, the game's `DirectInput8Create` went through it to Windows' DLL, our DirectInput hooks went in as usual and a single-player game ran 5 AI turns.

## Drawing over the game (`d3d9_hook.cpp`)

The overlay (below) and the robot's screenshots share one set of Direct3D 9 hooks: `Direct3DCreate9` (the exe's import), `IDirect3D9::CreateDevice`, `IDirect3DDevice9::Present` and `Reset` (vtable entries). Each user registers a callback that runs before each frame is shown, in order: the overlay draws first, then the robot takes its screenshot, so the screenshots show the overlay. Windows' `d3d9.dll` gives **each device a vtable of its own** (on the heap): every new device's entries are checked and hooked, not only the first one's.

## The overlay (`overlay*.cpp`): our panels over the game

Every panel we draw over the game is a **client** of one shared overlay: the player list (`src/mods/player_list.cpp`), the template builder (`src/mods/rmg_builder.cpp`), the hero looks panel (`src/mods/hero_skins_panel.cpp`), the game rules panel (`src/mods/host_rules_panel.cpp`), the online UI (`src/mods/online_ui.cpp`: Steam's lines on the LAN screen, "Invite friends..." and its list) and a test panel (`overlay_test_panel.cpp`). The overlay owns what they share:

- one **Dear ImGui** context. Dear ImGui v1.91.9b (MIT, <https://github.com/ocornut/imgui>, tag `v1.91.9b`) is in `third_party/imgui/`: its core files and its DirectX 9 renderer (`backends/imgui_impl_dx9.*`), unchanged, with its `LICENSE.txt`. Its Win32 backend is not used: it would change the mouse cursor and wants every window message. We feed it ourselves (below);
- the **fonts** (Tahoma Bold, else Segoe UI Bold, else Arial Bold; Latin and Cyrillic). A client asks for the sizes it needs each frame; the font atlas is made again only when a new size is asked for;
- the **pictures** from the game's data (`overlay_textures.cpp`, moved from the player list): `.dds` files read through the game's own file system (so MMH5.5's paks count), turned into Direct3D textures that survive a device reset;
- the **screen on top** (`overlay::TopScreen`: the game's screen list at `[0x00fe719c]`, its last entry, its RTTI class name) and our **keys** (F1..F12 read with `GetAsyncKeyState`, so the game still gets them);
- the Direct3D device's life: on a reset (fullscreen switch, alt-tab) ImGui's objects are released before and made again after; after 10 s with nothing drawn, everything of ours on the device goes.

The panels' **words** are not the overlay's: they come from our texts files, one per language, with English built in (`texts.cpp`, `texts_logic.cpp`; [src/mods/README.md](../mods/README.md#our-panels-in-your-language-srcmodstextstxt-srchooktextscpp)).

Each frame, the overlay asks every client "do you draw now?", lets those that do load pictures and ask for fonts, then runs one ImGui frame in which they draw in the order they registered (the player list first). A client that crashes is caught, turned off and logged.

### Modal panels: the game gets no input (`overlay_input.cpp`, `input_mute.cpp`, `dinput_hook.cpp`)

A modal panel (the template builder, the hero looks, the game rules, the test panel) takes the whole mouse and keyboard while it is open. **When no modal panel is open, everything passes through untouched** (the player list is display-only, as before).

How the game reads its input (static reading: the interface's event names in `0x00489550`, the cursor code `0x00835ce0`, the game's own `profiles/default_profile/input_a2.cfg` and `user_a2.cfg`; not yet checked in a running game, which is why both paths below are covered):

- **Mouse clicks and keys in menus** come from window messages (`win_left_button_down`, `win_key`, ... in `0x00489550`), **hotkeys** (Esc, F10, ...) from DirectInput 8.
- **The cursor is the game's own** by default (`ui_use_hwcursor = 0` in the default profile): the game keeps it in client pixels at `0x00fe6da0`/`0x00fe6da4`. It moves it either from Windows' cursor (`GetCursorPos`, `ScreenToClient`) or from DirectInput's mouse axes (`bind +cursor_x 'MOUSE_AXIS_X'`, 1.2 times the motion), and then puts Windows' cursor there (`SetCursorPos`).

What a modal panel does:

1. **Window messages.** The game window's procedure (class `"H5"`) is subclassed when a modal client is installed. While muted, its mouse and keyboard messages go into a small queue for ImGui (buttons, wheel, keys, characters) and are not given to the game. Alt+F4 still closes the game.
2. **DirectInput.** Our DLL is the `dinput8.dll` proxy, so `dinput_hook.cpp` wraps the keyboard's and mouse's `GetDeviceState` / `GetDeviceData` (vtable entries of the real DirectInput objects, one set of hooks shared with the robot's keyboard, `robot_input.cpp`). While muted, the game sees no key, no button and no wheel. **The mouse's motion still reaches the game**, so its cursor keeps moving over our panel; ImGui follows that same cursor (`0x00fe6da0`), so what the player sees under the cursor is what gets the click. If the game takes the mouse for itself (DirectInput exclusive mode: Windows then sends no mouse messages), the buttons for ImGui come from DirectInput instead.
3. **Esc** closes the panel (seen in the messages or in DirectInput).
4. **Keys held across the change** are handled (`input_mute.cpp`): a key the game saw go down before the panel opened still gets its release; a key pressed while the panel was open (Esc, for one) stays hidden from the game until it is released, also after the panel closed.

The safety nets, so that input always comes back:

- "muted" means: **a panel is open, its screen is on top, and a frame was drawn with it in the last 2 s**. The input hooks check the 2 s themselves, so input comes back even when no frame is drawn at all (alt-tab in fullscreen, a lost device, a panel that cannot draw);
- every frame, the panel **closes if its screen is no longer on top** (another screen, a message box, a lost connection), and after the 2 s without a frame;
- a client that crashes is turned off and its panel closes.

The rules of point 4 and of what is held back are pure code, tested without the game by `test/input_mute_test.cpp`.

**The test panel** (`[ui] overlay_test_panel=1`, testing only): F10 (or `[ui] overlay_test_key`) opens a small modal panel over any screen, with three buttons, a check box and a text line, showing the mouse position, the wheel, the keys down and where the input comes from. Each click is written to the log. On the adventure map, F10 is also the game's own menu key: the game sees the press before the panel opens, opens its menu, and the panel closes at once (its screen left the top). Test on another screen (main menu, random map screen) or choose another key.

## Which exes it hooks

On the first `DirectInput8Create` call (after start-up, when even the Steam-wrapped exe has decrypted its code) it checks the exe: load address, link timestamp, image size, a vtable entry and the first bytes of the AI turn driver. It installs hooks only for:

- vanilla ToE 3.1 (2008): the unwrapped reference exe or the Steam `H5_GameOLD.exe`;
- `MMH55_Utility.exe` / `MMH55_Utility_64.exe` (the co-op exe), which has the same code.

In any other exe (3.1j `MMH55(_64).exe`, the 2025 `H5_Game.exe`, …) it only forwards and logs "unknown build".

One hook goes in earlier, when the DLL is loaded (`hooks::InstallEarly`), if the exe can already be recognized then (not the Steam-wrapped one): the "original music" mod's file system hook, only when that mod is on. The game reads its heroes during its splash screen, before the first `DirectInput8Create` call.

If the log says "unknown build" or "could not hook" in `MMH55_Utility`, MMH5.5's own DLL (`55u.dll`) may have changed the same vtable entry or code at run time. Our DLL then stays out of the way instead of guessing. Tell me, and we'll look at what `55u.dll` does.

## The AI trace

The AI turn driver (`IPlayerAI` slot 0, `0x0086dd40`, see `docs/subsystems/ai-turn.md`) is a virtual method, so we hook it by replacing one pointer in `CPlayerAI`'s vtable (`0x00e4c8bc`). No code is patched. Each call produces one log line:

```
  123456 [ 4321] ai-step ai=0A1B2C30 flag=0 stage 3->4 calls=17 cmds=1 turn_age=850ms think=12.34ms gap=180.5ms
```

| Field | Meaning |
|---|---|
| first number | ms since the DLL loaded |
| `[4321]` | thread id |
| `ai=` | address of the `CPlayerAI` object (one per AI player) |
| `stage a->b` | the turn's state-machine stage before and after this call |
| `calls=` | the AI's own call counter |
| `cmds=` | commands in the output list after the call |
| `turn_age=` | ms since this AI's turn started (its own `GetTickCount()` stamp) |
| `think=` | how long this call took (ms): the AI's thinking time |
| `gap=` | time between this AI's previous call returning and this call starting (ms), i.e. the time the AI waited. **This is the number to look at in co-op**: if hypothesis H1 in `docs/subsystems/ai-turn.md` is right, it is about one network round trip after each command in multiplayer, and near zero in single player. |

## The hang watchdog (`watchdog.cpp`)

On 2026-10-25 the owner's game froze during an AI turn (the window "not responding", one core at 100% for minutes) and nothing told us where. The watchdog writes it down the next time. It is on by default (`[debug] hang_watchdog=1`); details, the suspects of that freeze and how to read a report: [`docs/subsystems/hangs.md`](../../docs/subsystems/hangs.md).

- **Progress:** the exe's `PeekMessageA` / `GetMessageA` import entries go through us (data only). The game's main loop (`0x00c3b490`) calls them every frame in every screen; Windows calls a window "not responding" exactly when its thread stops taking messages. Each call from the main thread is one beat.
- **The look:** after `hang_seconds` (20) without a beat, a background thread suspends the main thread 20 times in 2 s, each time for well under a millisecond: its registers (`GetThreadContext`) and 64 KB of its stack (`ReadProcessMemory`) are copied, and it runs again. While it is suspended, the watchdog does nothing else (no log line, no allocation, nothing that could wait for a lock the main thread holds).
- **The report** (`watchdog:` lines): the main thread's CPU time in those 2 s (busy loop or blocked), where EIP was, the return addresses that stayed on the stack (innermost first), the mods' own state (the AI: inside its turn driver or not, its last 20 steps; the lockstep pump; Direct3D: last frame, lost device, reset), and the busiest threads. Exe addresses are vanilla 3.1 addresses (the same in `MMH55_Utility`), with the names of the functions our docs know; system DLLs get their nearest export; our DLL's offsets are names in that build's `build\hook\dinput8.map`. `python tools\hang\symbolize.py <log>` adds the names from the map and from Ghidra's `analysis/export/functions.tsv`.
- **Once per freeze:** when the beats come back, a line says so (a long load is then visibly only slow), and the next freeze gets a new look.
- **Hand test:** `[debug] hang_test=1`, then F12 while the game is in front freezes it on purpose for 25 s (a busy loop in `HangTestSpin`). Robot runs use `[debug] hang_test_at=<s>` instead (once, that many seconds after the game first took its messages): `run_ai_test.py --debug-setting hang_test_at=60`, `run_lan_test.py --host-debug ...`. Also for robot runs: `hang_test_frame_ms=N` (every frame N ms longer: a slow PC) and `hang_test_ai_step=N` (3 s once, instead of the first AI call that starts a turn from the N-th call on).
- **AI pacing:** the turn driver hook first asks `src/mods/ai_pacing.cpp` whether the AI may decide now (at most once per frame in multiplayer, and only once its last commands have run); a held call is not made, and gets no `ai-step` line.

## Crash reports (`crash.cpp`)

The watchdog writes where a frozen game is stuck; nothing caught a **crash** before 0.3.0. Now, when the game crashes:

- **`h5-together-crash-<date>-<time>.txt`** next to the log: what crashed (the exception, and for an access violation the address read or written), where (module + offset in the watchdog's words: the exe's vanilla 3.1 address and the function name where our docs know it, a system DLL's nearest export, our DLL's nearest named function), the thread (the game's main one or not), the registers, the probable callers (return addresses found on the stack), our version and build time, the game build, and the last 20 log lines;
- **`h5-together-crash-<date>-<time>.dmp`**: a small minidump (normal, with thread information and unloaded modules: 30 to 100 KB), written with Windows' own `dbghelp.dll` from the system folder, loaded by its full path at start-up (the game's `bin\dbghelp.dll` is from 2002 and stays the game's). It holds part of the game's memory at the crash (player and folder names), so it stays on the PC; the bug report scripts add the newest report, `.txt` and `.dmp` (`-NoDump` / `--no-dump` leave the `.dmp` out);
- two log lines, `crash: the game crashed: ...` and `crash: wrote ... next to this log`.

At the next start the 3 newest reports are kept (older ones are deleted), and the log names the newest one: `crash: THE LAST RUN OF THE GAME CRASHED: its report h5-together-crash-....txt ...` when it was written at about the time the log last was, else `crash: an older crash report: ...`. A robot copy (`OPEN_HOMM5_INSTANCE`) has its own names, `h5-together-<instance>-crash-...`.

**How:** one top-level exception filter (`SetUnhandledExceptionFilter`), installed when the DLL loads (`hooks::InstallEarly`, before the game's own code runs; for the Steam-wrapped exe, at its first DirectInput call). The game's own filters, read in the exe ([docs/subsystems/hangs.md](../../docs/subsystems/hangs.md#crashes)):
- `0x00c67b10` sets `0x00c67a50` once, after the renderer made its Direct3D object: an access violation inside one of its memory blocks goes on after the faulting instruction (it recovers from it), anything else goes to the filter that was there before (`[0x010d9e24]`). Ours is there before it, so it hands us the real crashes and keeps the ones it recovers from. Each report says so: `crash filters: the game's own filter (0x00c67a50) is in front of ours and passed this crash on`;
- MMH5.5's runtime (`msvcr71.dll`) has a filter for C++ exceptions nobody caught, which passes the others on; `55u.dll` and the other DLLs only have the C runtime's `/GS` failure report, which removes every filter and ends the game (no report of ours then). The exe's "exception report" dialog filter (`0x00c2dff0`) is never installed.

After the report, ours calls the filter that was there before it, so what happened after a crash before (Windows' "stopped working", Wine's crash dialog, the game closing) still happens. The report is written by a thread of ours that waits for it from the start: a crashed thread may have no stack left (a stack overflow) or hold a lock; the crashed thread waits for it at most 30 s, and a second crashing thread waits for the first report. The report never allocates memory, and a crash inside it is caught (`crash: the crash report itself failed`). The hang watchdog stays quiet after a crash (the main thread waiting for the report is not a freeze). The log's last lines come from a ring of 20 lines kept by `log.cpp`; while the crashed thread holds the log, the report's log lines are dropped after a second instead of waiting.

**Settings** (`[debug]`): `crash_report=1` (0 = no filter at all); testing: `crash_test_at=<s>`, the game's main thread writes to address 0 once, that many seconds after it first took its messages (needs the hang watchdog on: `watchdog::RunOnMainLoopAt`).

**Tests:** `test/crash_test.cpp` (below). Robot (2026-11-02, RC19k test copy, `run_ai_test.py --debug-setting crash_test_at=30`): the crash came 30 s in, during the AI turns, on the main thread; the game's filter passed it on; the report and an 84 KB minidump were written in 235 ms; the game closed by itself 3 s later with the crash's code (`c0000005`), as without the add-on. A normal single-player run and a hang test (`hang_test_at=28`, one look, "takes its messages again after 25 s") were unchanged.

## The old name (`old_names.cpp`)

Until 0.2.7 the add-on was called open-homm5, and so were its files next to the exe. The installer gives them the new names ([tools/package/README.md](../../tools/package/README.md#the-old-name)). For a player who only copies the new `dinput8.dll` over an old install, the DLL does the part it needs itself:

- **At start-up**, in `DllMain` before the log is opened: `open-homm5.ini` becomes `h5-together.ini` and `open-homm5-skins.ini` becomes `h5-together-skins.ini`, only when the new name is not there yet (otherwise the old file stays and the new one is used). The old log becomes `h5-together.old.log` while there is no log of the new name yet. Each robot copy (`OPEN_HOMM5_INSTANCE`) has its own names. The log says what moved, just after its first line:

  ```
  H5 Together hook DLL 0.2.7 loaded (built Nov  2 2026 13:22:54)
  old names: open-homm5.ini is now h5-together.ini (the add-on was called open-homm5 until 0.2.7)
  old names: the log open-homm5.log is now h5-together.old.log (the add-on was called open-homm5 until 0.2.7)
  old names: our folder is still open-homm5 (there is no h5-together yet): its files are used from there
  ```

- **Our folder:** a file or folder of ours (`lang\fr`, `steam_api.dll`, where "Save as file" writes) is looked for in `bin\h5-together\`, else in `bin\open-homm5\` (`PathInOurFolder`). The templates are read from both folders (a name in both counts once). The DLL never moves the folder itself; the installer does.
- `open-homm5.nohooks` still switches the hooks off, like `h5-together.nohooks`.
- The pure rules (when a file moves) and the moves themselves are tested by `test/old_names_test.cpp`.

Kept on purpose under the old name (they are identifiers, not names a player sees): the `OPEN_HOMM5_VERSION` macro, the `OPEN_HOMM5_INSTANCE` variable of the robot, the `OpenHomm5-*` names of the files the DLL serves to the game, the network and save tags, and the `Editor` line of the templates (`open-homm5 template maker`: every PC must build the same bytes from a template's name).

## Build

Needs Visual Studio 2019 or 2022 (C++ build tools, x86). Already on the owner's PC.

```powershell
powershell -File src\hook\build.ps1 -Test
```

This builds `build\hook\dinput8.dll` (with our English texts built in, from `src\mods\texts\en.txt`, and its version information: `dinput8.rc`, compiled by `rc.exe` from the Windows SDK that Visual Studio's C++ tools bring; check it with `(Get-Item build\hook\dinput8.dll).VersionInfo`) and runs these tests outside the game:

- `test/proxy_test.cpp`: loads the DLL, calls `DirectInput8Create`, and checks that a real DirectInput object comes back. Then chain-loading (`chain.cpp`), each case in a child process: with `test/chain_dummy.cpp` (a stand-in for another mod's `dinput8.dll`, built as `chain_dummy.dll`) renamed `dinput8_next.dll`, DirectInput goes through it and still gives a real object; when it asks for `"dinput8.dll"` by name it gets ours and its call goes to Windows' DLL (no endless loop); a copy of our own DLL, a broken file, a missing `[chain] dll=` file and `dll=none` are not used; a name from the ini in a folder of its own is.
- `test/ai_trace_test.cpp`: calls the AI hook exactly the way the game calls the AI (`__thiscall`, command list then flag) on a fake AI object, built with the compiler's runtime stack check (`/RTCs`). It checks that the arguments reach the original function unchanged and that the log line is right. A calling-convention mistake here would crash the game, so this test matters.
- `test/original_music_test.cpp`: the "original music" mod's text work (`src/mods/original_music_xdb.cpp`) on small made-up data files: which music lines change and which stay.
- `test/languages_test.cpp`: the French texts mod's work on bytes (`src/mods/languages_text.cpp`): paths, text encodings and a small zip archive made in the test.
- `test/log_test.cpp`: the log's size limit (`log.cpp`): a log over the limit becomes the old log and the next one starts empty; a small one is kept and the next run is appended after a separator; a log that another copy of the game holds open is not moved.
- `test/old_names_test.cpp`: the move from the old name (`old_names.cpp`): the rules, then a 0.2.7-style set of files next to the test exe (settings, hero looks, logs) moved byte for byte, nothing moved at the next start or when both names are there, and the folder looked for under its old name only while the new one is missing.
- `test/rmg_maker_test.cpp`: the C++ template maker (`src/mods/rmg_maker.cpp`) against the JavaScript one. First `node tools/rmg/test_template_maker.js --expected build/rmg_parity` writes what the page's maker builds for the 24 ready templates, 400 random choices and a few special ones; the test must build the same bytes, names, warnings and numbers, and refuse bad choices with the same message. Needs Node.js.
- `test/rmg_recipe_test.cpp`: recipe names (`src/mods/rmg_recipe.cpp`): choices → `OH-X1-...` → the same choices, every bad name and path refused, reading a template link the way the game keeps it (`src/mods/rmg_join.cpp`), the teams of a recipe map, and each ready template built from its recipe name, compared with the file in `src/mods/rmg/templates/`. It also checks that 302 recorded `X1` names still build what they built when recorded (`tools/rmg/x1_fingerprints.txt`): what an `X1` name builds must never change.
- `test/rmg_builder_test.cpp`: the template builder's data (`src/mods/rmg_builder_data.cpp`): each preset builds its ready file byte for byte and every ready file has a preset; every choice the panel offers has a recipe name, and the maker's check (`CheckChoices`) answers as `MakeTemplate`; the zone map's positions equal the web page's (numbers from Node); each of the maker's messages has an id, said in English exactly as the maker says it and in French with its numbers (`src/mods/texts/*.txt`), and every word of the panel is in both files; the game's text format; a UI file we do not know is left alone.
- `test/texts_test.cpp`: our texts (`texts_logic.cpp`): a texts file read with sections, comments, the byte-order mark, CRLF or LF, UTF-16, and each kind of broken line; the named placeholders; a translation checked against English (unknown keys, other placeholders or tags) and the fallback to English; the clues to the language (Steam's `appmanifest_15370.acf`, Windows' ids); `src/mods/texts/en.txt` reads cleanly and is byte for byte the English built into the DLL (`texts_en.inc`), `fr.txt` fits it and has every text.
- `tools/lang/ui_texts.py check` (Python): every text key the code uses is in `en.txt`, every key of `en.txt` is used, and the translations have no key English lacks and the same `{names}` and `<tags>`.
- `test/settings_test.cpp`: the settings menu's pure work (`src/mods/settings_table.cpp`, `settings_ini.cpp`, `settings_menu_data.cpp`): the table of every setting against the shipped `tools/package/files/h5-together.ini` (the same settings, the same defaults), each menu row's words in `en.txt` and `fr.txt` and its choices, the in-place ini writer (the cases of `tools/package/test_scripts.py`: a byte-order mark, LF, upper case, spaces, a second copy, a missing section, UTF-16; and what Windows' `GetPrivateProfileString` reads back), and our category added to a stand-in for the game's Options list.
- `test/online_ui_test.cpp`: the online UI's pure work (`src/mods/online_ui_logic.cpp`): the auto-join's lines, the lines of the game's LAN screen for each state of Steam play (not available and why, friends, invites, games that cannot be joined), the invite banner's words in English and French, Steam names cut for the screen, the invite list's order, Steam's `+connect_lobby` on the command line, the made-up friends of `[online] ui_test=1`, and every word in `en.txt` and `fr.txt`.
- `test/hero_skins_test.cpp`: the hero looks' rules (`src/mods/hero_skins_logic.cpp`): the choices file, the messages between PCs and who decides a hero's look.
- `test/audio_edge_test.cpp`: the wider volume curve (`src/mods/audio_volume.h`) and the light file edit of the black map edge (`src/mods/edge_sky.h`).
- `test/menu_scene_test.cpp`: the main menu scene's pure work (`src/mods/menu_scene_logic.cpp`): the setting's words (`hof` is not one), the pick (an explicit scene this PC cannot show, `random` over the scenes available, uniform), which scene an arena description is, the files it needs, and the two small edits: the music definition served under our name and the root table's one item.
- `test/input_mute_test.cpp`: the modal panels' input rules (`input_mute.cpp`): what the game gets of its window messages and of DirectInput while a panel is open and just after (held keys, Alt+F4, the mouse's motion, buffered events).
- `test/wheel_test.cpp`: the mouse wheel fix for Wine (`src/mods/wheel_logic.cpp`) against a model of the game's own wheel reading, of Windows' mouse (positions) and Wine's (steps): the bug without the fix, every notch both ways with it (also when the game drops "presses"), no jump when it switches on or after an Acquire, positions never converted.
- `test/host_rules_test.cpp`: the game rules' pure work (`src/mods/host_rules_logic.h`): reading MMH5.5's settings file, serving it with other values (only the numbers change), its hash, the text carried in the start message and the lobby, and H5 Together's version in the waiting room (the 'V' and 'B' requests, the host's list, who differs).
- `test/crash_test.cpp`: the crash report (`crash.cpp`). The names and which old reports go, then real crashes in a child process that installs the filter as the DLL does (after a filter of its own, as the C runtime's): a write to address 0 (the report's lines, the callers in the exe, the log's last lines, a minidump that starts with `MDMP`, the filter before ours called after it), a stack overflow (the report written by our thread), a crash on another thread, a filter set after ours that recovers from an exception (no report), `crash_report=0` (no report), the next start ("the last run crashed"), and only the 3 newest reports kept.
- `test/new_release_test.cpp`: the update check's pure work (`src/mods/update_check_logic.cpp`): version numbers and their order, the tag in GitHub's answer, the once-a-day rule. (Not called `update_check_test`: Windows takes an exe with "update" in its name for an installer and asks for administrator rights.)
- `test/watchdog_test.cpp`: the hang watchdog on a thread that freezes on purpose, once in a busy loop and once in a long `Sleep` (1.5 s threshold instead of 20). It checks one look per freeze, 20 samples, EIP in the spinning function, the caller's return address on the stack, ~100% CPU for the busy freeze and ~0 for the blocked one, and that the thread goes on normally afterwards.

The build runs in `src\hook` with relative source paths: with full paths, a deep folder (a git worktree) made the command line longer than `cmd.exe` accepts (8191 characters).

## Owner test (phase 0 gate: "DLL loads and logs AI turns; game runs normally")

Use a **separate copy** of the game, never the Steam folder (CLAUDE.md).

1. Copy `build\hook\dinput8.dll` into the copy's `bin\` folder, next to `MMH55_Utility_64.exe`.
2. Start a single-player skirmish **with `MMH55_Utility_64.exe`** (the 3.1j `MMH55_64.exe` is not hooked) against 1–2 AI players, and play 2–3 turns.
3. Check `bin\h5-together.log`. It should say `game build: ToE 3.1 + MMH5.5 patches (MMH55_Utility)`, then `hooked AI turn driver`, then many `ai-step` lines during AI turns.
4. If anything goes wrong: delete `dinput8.dll` (or create an empty file `h5-together.nohooks` in `bin\` to keep only forwarding), and send me the log.

On **Linux** (Wine/Proton) Wine prefers its own built-in `dinput8.dll`. Tell it to use ours first: Steam launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%`, or in `winecfg` → Libraries → add `dinput8` as "native, builtin" (for GameRanger's Wine prefix).

## Files

| File | What |
|---|---|
| `dllmain.cpp` | DLL entry, the forwarded exports (to Windows' DLL, or a chained mod's), lazy hook installation |
| `chain.cpp/.h` | another `dinput8.dll` mod, renamed `dinput8_next.dll`: loaded, and DirectInput passed on to it |
| `crash.cpp/.h` | crash reports: what crashed and a small minidump next to the log |
| `dinput8.def` | export names (same as the real `dinput8.dll`) |
| `game_build.cpp/.h` | which exe are we in? |
| `hooks.cpp/.h` | installs all hooks (honours `h5-together.nohooks`, or the old `open-homm5.nohooks`); `InstallEarly` for the few that must be in before the game reads its data |
| `old_names.cpp/.h` | the add-on's old name (open-homm5, until 0.2.7): moves its files to the new names at start-up, finds our folder under either name |
| `ai_trace.cpp` | the AI turn trace |
| `log.cpp/.h` | `h5-together.log` writer. At start-up a log over 5 MB becomes `h5-together.old.log` (one older log is kept) and a new one starts; a log that goes on from earlier runs gets a `---- new run ----` line before the new run's first line |
| `d3d9_hook.cpp/.h` | the shared Direct3D 9 hooks (overlay, robot screenshots) |
| `version.h` | the version number, the one place that holds it (log's first line, install package, the DLL's version information) |
| `dinput8.rc` | the DLL's version information (file properties > Details: product name, description, version, copyright). `build.ps1` compiles it with `rc.exe` (found through `vcvarsall`, like `cl`) and links it in; the version text comes from `version.h`, the numbers are cut from it by the build script |
| `patch.cpp/.h` | redirecting the game's `call` instructions to our code (several hooks may stack on one call) |
| `watchdog.cpp/.h` | the hang watchdog (freeze detector) |
| `dinput_hook.cpp/.h` | the shared DirectInput 8 hooks (the mouse wheel fix under Wine, `mods/wheel.cpp`; the robot's keyboard; the modal panels' mute). With none of them on (Windows, no modal panel installed, no robot, no `trace_wheel`), DirectInput is not hooked at all. |
| `overlay.cpp/.h`, `overlay_internal.h` | the overlay: clients, the ImGui frame, fonts, the screen on top, our keys |
| `texts.cpp/.h`, `texts_logic.cpp/.h` | our panels' and messages' words: the built-in English (`build\hook\texts_en.inc`, made by `build.ps1` from `src/mods/texts/en.txt`), the player's file `h5-together\texts\<code>.txt`, the choice of the language; the pure part (reading a file, placeholders, the check against English, Steam's file) is `texts_logic.cpp`, tested |
| `overlay_input.cpp` | modal panels: the window procedure, the ImGui feed, the DirectInput mute, the safety nets |
| `input_mute.cpp/.h` | the rules of what is held back from the game while a modal panel is open (pure code, tested) |
| `overlay_textures.cpp` | pictures from the game's data, as textures |
| `overlay_test_panel.cpp` | the test panel (`[ui] overlay_test_panel=1`) |
| `third_party/imgui/` | Dear ImGui v1.91.9b (MIT): core + DirectX 9 renderer, for the overlay |
| `../mods/online_ui*.cpp`, `online_join.cpp` | online play's UI, an overlay client (step 4 of docs/plans/online-steam.md): Steam's lines on the LAN screen, "Invite friends..." and its list, an invite's banner, the auto-join after an invite, the watch of the game's message boxes; robot steps `panel\|online ...` ([src/mods/README.md](../mods/README.md#online-play-over-steam-the-ui-online_uicpp)) |
| `robot.cpp/.h`, `robot_*.cpp` | robot mode for unattended test runs; only active when `h5-together-robot.ini` is next to the exe (see [tools/robot/README.md](../../tools/robot/README.md)) |
| `test/` | the tests that `build.ps1 -Test` runs ([Build](#build)) |
| `build.ps1` | build script |

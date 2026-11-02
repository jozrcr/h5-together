# open-homm5

An unofficial add-on for **Heroes of Might and Magic V: Tribes of the East** with the **MMH5.5** mod. It is made for a small group of friends who play co-op games against the computer, and anyone else is welcome to use it.

The add-on is one file, `dinput8.dll`, that the game loads when it starts, plus a settings file and 24 random map templates. It **never changes the game's own files**. Its main job is faster, more stable co-op games. It also adds a few panels and options on top.

## What it does

### Faster co-op multiplayer

- **Quicker AI turns and clicks.** The game's network timings were made for 2006 modems; the add-on uses modern ones. When every player has the add-on, the PCs also agree on shorter game steps (50 ms instead of 200 ms). In robot tests (two copies of the game on one PC), an AI move went from 2.5 s to about 0.3 s. Over the internet, the ping between the PCs comes on top.
- **Less AI thinking on big maps.** The AI finds exactly the same paths, faster: on a day-50 huge map, 93 s of AI thinking over 4 turns became 17 s.
- **No long autosave freeze** at the start of each round (7 to 10 s down to a fraction of a second when the `Saves` folder holds many multiplayer saves).
- **LAN games over ZeroTier** and other virtual networks show up in the LAN list, so a friend (on Linux too) can join without GameRanger.

### Stability

- **A rare freeze of the host during computer turns is fixed.** The computer takes at most one decision per frame, as in the original game.
- **A freeze detector.** If the game stops answering for 20 seconds, `open-homm5.log` says where it is stuck. If that happens, wait 30 seconds before closing the game, then send the log.

### Interface

- **Player list** on the adventure map: who is still playing, who has ended the turn, and when the computer is moving. **F9** hides it.
- **"Your turn" notification** when the game is in the background: the taskbar button flashes, a short sound plays, and a pop-up appears.
- **Hero looks.** Give a hero the 3D figure of another hero of the same faction (the small button on the hero screen, or **F7**). In multiplayer, friends see your looks when everyone has the add-on.
- **French texts for the whole game** (off by default). With MMH5.5, a French game mixes French and English; `[lang] texts=fr` shows French everywhere it can. It needs MMH5.5's French pack and our pack of the remaining texts (shared in the group).

### Random maps

- **Template builder in the game.** A **Modèles... / Template...** button on the random map screen (or **F11**) opens every choice of our template maker, with a map of the zones. **Use** fills in the game's screen for you.
- **Recipe maps.** A template's name carries its whole setup, so every friend with the add-on rebuilds the same template when joining; there is nothing to share. A friend who lacks a template gets the game's error message instead of a crash.
- **24 ready templates** in the game's list (names start with `OH-`).
- **Teams.** Maps made from a team template get their teams in the lobby, and the team arrows still change them. The host can also set teams on other random maps (`[rmg] teams=`, `lobby_teams=1`).
- **Start terrain that matches your faction** (off by default, the host's choice). With `[rmg] faction_terrain=1`, each start zone gets the ground of the faction picked in the lobby; the map is rebuilt for a few seconds at the start. Terrain changes movement costs, which is why it is off.

### Game rules menu

- The **host** chooses MMH5.5's rule switches for each game: governors, town conversion, the town gate, mine guards, the computer's weekly bonus, mixed neutral stacks, prank artifacts, random map extras and more. **Règles... / Rules...** in the multiplayer waiting room, or **F2**.
- Every PC plays with the host's rules, so nobody swaps settings files any more. The others see the rules in the lobby and during the game.
- **Tribes of the East start bonuses:** each player keeps the bonus picked with the lobby arrows (gold, resources, artifact), and the game mode (Default, Savage, Nightmare, Random) gets its own choice.
- **MMH5.5 scripts on random maps:** in multiplayer, MMH5.5's scripts (and so these rules) also run on random maps straight from the game's generator. Off on RC19d and on on RC19k by default, which is what each version does by itself.
- **Skills of each class:** MMH5.5's lock (the default), or **Unlocked**: each class keeps its own skills and can also be offered the others at a small chance, as in the original game.
- It works only when **every player** has the add-on (0.2.7 or later for the last three). Otherwise everyone uses their own MMH5.5 file, as before.

### Camera, sound and visuals (off by default)

- **Zoom out farther:** `[camera] zoom_out=150` (adventure map) and `combat_zoom_out=150` (battles); `start_zoom` sets where the camera starts.
- **Black map edges:** `[camera] edge_sky=none` replaces the violet sky MMH5.5 put past some random maps' edges.
- **Quieter volume range:** `[audio] volume_curve=wide` makes the sliders' lowest step about 10 times quieter.
- **Original Heroes V music** instead of MMH5.5's: `[audio] original_music=1`.
- **Mouse wheel on Linux:** under Wine / Proton the wheel could zoom in but never out. This one is automatic, on Linux only.

## Install

Download the latest `open-homm5-<version>.zip` from the [releases page](https://github.com/jozrcr/open-homm5/releases/latest) and extract it.

- **Windows:** double-click `install.bat`.
- **Linux (Steam / Proton):** run `bash install.sh`, then paste the line it prints into the game's Steam launch options. Full walkthrough with ZeroTier: [docs/linux.md](docs/linux.md).

Start the game: a file `open-homm5.log` appears in the game's `bin` folder, which means the add-on is running. **Everyone in the same co-op game should install the same version.** To remove it, run `uninstall.bat` (Windows) or `bash uninstall.sh` (Linux); only our own files are removed. More detail (what gets installed, updating): [docs/install.md](docs/install.md).

## Settings

The settings file is `open-homm5.ini`, in the game's `bin` folder next to the exe. Every line in it is commented out and shows its default, so you can leave it alone.

The defaults keep the original game: the speed-ups, fixes and panels are on, and anything that changes how the game plays, looks or sounds is off until you turn it on. Restart the game after a change. Your hero look choices are kept in `open-homm5-skins.ini` next to it. Every setting is explained in [src/mods/README.md](src/mods/README.md).

## Supported versions

- The Steam copy of Tribes of the East 3.1 with **MMH5.5 RC19d or RC19k** already installed. The add-on does not install MMH5.5.
- The add-on works in MMH5.5's co-op exe, `MMH55_Utility_64.exe` (the one GameRanger starts), and in the 2008 3.1 exe. In any other exe (for example MMH5.5's single-player `MMH55_64.exe`) it loads, writes "unknown build" in its log and changes nothing.

## For curious readers

- [src/mods/README.md](src/mods/README.md): every change the add-on makes, how it works, and how it was tested.
- [src/hook/README.md](src/hook/README.md): the DLL itself, and how to build it.
- [docs/plans/](docs/plans/): plans for each feature, and the [backlog](docs/plans/backlog.md) of ideas.
- [docs/subsystems/](docs/subsystems/): what we found out about how the game works (AI turns, multiplayer sync, saves, random maps, camera, sound...).
- [docs/releases/](docs/releases/): the notes of every release.

## Project status

This is a hobby project, built for a group of friends; others are welcome to use it. Bug reports with your `open-homm5.log` help.

The project started with a bigger long-term idea: rebuild the original exe function by function (each function checked to compile to the same bytes), and one day grow it into an open Heroes V engine, the way VCMI did for Heroes III. That decompilation is **set aside for now**: the work goes into the add-on. What was done stays in `src/decomp/`.

## Legal

This repository contains only our own code, notes and tools. It contains **no game files**, so you need your own legal copy of Heroes of Might and Magic V: Tribes of the East (for example from Steam) to use anything here. Heroes of Might and Magic is a trademark of Ubisoft; this is an unofficial fan project, not affiliated with or endorsed by Ubisoft.

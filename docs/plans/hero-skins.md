# Plan: choose your hero's look ("skins")

Status: **research only** (2026-11-01). Static reading of the vanilla 3.1 exe (Ghidra dumps) and of the game data in the test copies (`C:\Games\H5_test`, MMH5.5 RC19d, and `C:\Games\H5_test_k`, RC19k). No game was run. Addresses are in the vanilla 3.1 exe. The functions and vtable cells this plan would touch have the same bytes in `MMH55_Utility_64.exe` of RC19d and of RC19k, and none of the spots MMH5.5's `55u.dll` patches at run time (202 in RC19d, 222 in RC19k, listed by `tools/inventory/mmh55_patch_sites.py`) falls inside them. Related notes: [../subsystems/players.md](../subsystems/players.md) (players, heroes, portraits), [../subsystems/audio.md](../subsystems/audio.md) (the game's file system, and how our DLL serves changed data files), [../subsystems/multiplayer-sync.md](../subsystems/multiplayer-sync.md) (lockstep and world checksums).

The owner asked: "A way in game to select our hero's skin would be awesome."

**The owner's answers (2026-11-01):**
1. **No portrait change.** Only the 3D figures change: map, battle, hero screen.
2. **Shared with friends.** Choices go to the other PCs; a PC without the add-on sees the normal looks.
3. **Same faction only**, from the ~40 real looks. No experimental models (on foot, cutscene, joke models).
4. **One choice per hero, for every game**, in `open-homm5-skins.ini`.

Defaults for the questions not asked: computer players' heroes keep their looks (only a human owner's choice counts); the group plays with MMH5.5, but vanilla stays working when it costs nothing; the button's place waits for the owner's sketch, so the panel comes first with a key, the button later.

**Built (2026-11-01): steps 1 and 3,** `src/mods/hero_skins.cpp` (game side) and `hero_skins_logic.cpp` (the rules, unit-tested by `src/hook/test/hero_skins_test.cpp`). On by default; it changes nothing until a look is chosen. Choices are read from `open-homm5-skins.ini` (`[skins]`, `Godric=Freyda`); the switches are in `open-homm5.ini` `[skins]` (`skins`, `show_others`, `trace`, `test_ai_heroes`). Details and log lines: [src/mods/README.md](../../src/mods/README.md), "Hero looks". What step 0 and the robot found: [Robot results](#robot-results-2026-11-01).

**Built (2026-11-01, later): step 2, the panel,** `src/mods/hero_skins_panel.cpp`. **F7** (temporary, `[skins] key`; a button on the hero screen when the owner's sketch says where) opens it on the adventure map, the hero screen or the town screen, for this PC's player's selected hero. It is the template builder's panel (same size, background, buttons, fonts, modal input): a grid of the faction's looks as portraits, the hero's own first, each with the name of the hero it is named after and "+N" for the others who wear it; the selected one lists them; a line says what Apply does here. **Apply** writes `open-homm5-skins.ini` and makes the choice count: at once in single player and hot seat; in multiplayer, at the start of segment R + 10 on every PC, R being this PC's segment report when it sends the change (a fraction of a second to two seconds later); in a multiplayer game whose looks are off, it is only saved for the next games. The looks are grouped by the models of the map and battle figures (so MMH5.5's re-textured Anwen is its own look). Names are the game's own texts (French in a French game). No 3D preview in the panel; the hero screen shows the battle figure in 3D when it is opened again. Screenshots: the plan's robot results.

**The hero screen's button (2026-11-01, from the owner's sketch):** a small **Look / Apparence** button in the "Armée & Stats" header, left of MMH5.5's square button and the skill wheel's round one. It shows the portrait of the look chosen for the hero (its own if none), lights up under the mouse with a tooltip ("Apparence" / "Look"), and opens the panel for the selected hero (the hero the screen shows). F7 stays as a shortcut. It is drawn by the DLL with ImGui, at MMH5.5's hero screen layout scaled with the screen height (`UI/HeroScreen3/SingleHero` at x 60, centred vertically; the round button `Button_HS` of `MMH55-Skillwheel.pak` at 372,48 in it): first placed 24×24 at 375,93 of the 1024×768 reference. **The owner's check (2026-11-01): "slightly off but it works well".** Measured in his two screenshots (units of the 768 reference): at 1348×750 MMH5.5's square button 404..427 and the round one 437..460, both centred on y 105; at 977×725 (English, Markal) no square button, the round one 432..458 centred on y 106.5. So the button now takes the square button's slot, 24×24 at **403,94** (centred on y 106, about 5 px left of the round one at 977×725). The square button is in none of the hero screen's UI files (MMH5.5 must add it in code, for some heroes only), so its presence cannot be read from the data: when it shows, ours covers it; `[skins] hero_screen_button_x` / `_y` move ours (for example x=376 puts it left of the square).

**An open hero screen shows the new look the next time it opens (no live refresh).** The owner asked for the 3D figure to change at once. Two tries failed, so it was taken out (2026-11-01):
- DLL `66545B95` set the screen's "needs refresh" byte (`NUI::CHeroScreen2` +0x7d8). Nothing happened: the screen's update (`0x006caee0`) calls its refresh (`0x006ca4e0`) only while its "shown" byte (+0x15a) is 0, and then sets it to 1.
- DLL `9823B52E` set both bytes, as `0x006cb700` (set the screen's hero) does. **The game crashed** right after: access violation at `0x00a50074`, in a window's show/hide (`0x00a50040`, called on most UI windows), whose state object (slot 8) was null. So some window the refresh touches is not in the state the refresh expects.
- Why: **the game never refreshes a hero screen that is already shown.** Every caller of `0x006cb700` sets the hero on a **new** screen: `0x006d1070` makes one (`0x858` bytes, mode 3) and pushes it; `0x006d1150` and `0x006d1260` (the duel presets) likewise. Switching hero means a new screen. The hero screen has no "next hero" command (its commands, `0x006c4b40`; page up/down only change the page, `0x006c1f70`). A second refresh of a shown screen, which rebuilds its 3D scene (`0x006c72e0` → `0x006c7030`, a new scene at +0x71c), is a path the game never runs.
- What could still work, later, with a robot step that opens the hero screen: (c) close the screen and open a new one for the same hero with the game's own opener (`0x006d1070`, or the adventure screen's own call for mode 0), or (b) replace the model inside the existing scene (`0x006c7030` adds it as "herotoremove" with the scene's slot `0x20`; removing it needs a slot not yet found). Neither can be tested without opening the hero screen, so neither ships blind.

The panel says so: "its figures on the map, in battle and on the hero screen (the next time it opens)". The map figure still changes at once (single player) or at the segment (multiplayer).

**The Look button is drawn by the overlay.** A native button (the screen's own UI file with one more child, as the random map screen's "Template...") would need the hero screen's command handler, not researched yet; the drawn one needs nothing from the game. `[skins] hero_screen_button=0` hides it.

**For the owner to check** (the robot cannot open the hero screen):
1. The button's place next to the two others, at your resolution and at another one (it should follow the header when the window size changes).
2. A click opens the panel for the hero shown (also when the hero screen is opened from a town).
3. The trading screen between two heroes: whether the button shows there too, and where (that screen's layout was not checked).
4. The tooltip, and that the click does nothing else in the game behind.
5. After Apply on the hero screen: no crash; close the hero screen and open it again: the 3D figure wears the look.

**One change from the plan, found by the robot: in a multiplayer game, the looks are all or nothing.** Rebuilding a hero's map figure makes new game objects, and those get numbers from the game world's counter. A PC that rebuilds a figure while another PC does not makes the two worlds' object numbers part, and later commands could then point at different objects: a real desync risk, not only a checksum difference. So in multiplayer every PC rebuilds the same figures at the same moment of the game (the start of segment 20, a few seconds in), and only when **every** PC sent its looks with the same game data. If one PC has no open-homm5, an old one, other game data or `show_others=0`, nobody's looks show in that game (the log says which PC). Single player and hot seat are not affected. This replaces the plan's "a PC without the add-on simply sees the normal look" (owner's answer 2): with a friend who lacks the add-on, everyone sees the normal looks.

## In short

- **Yes, it is possible, and it can change while you play.** A hero's look is three things: its **figure on the adventure map**, its **figure in battle** (the same figure is shown in 3D on the hero screen), and its **portrait**. All three are named in the hero's data file. The game keeps every hero's data in memory from start-up, so our DLL can give hero A the figures of hero B at any moment, then ask the game to rebuild hero A's map figure. Battles and the hero screen pick up the new figure the next time they open.
- **What you can pick, for free:** the looks of the other heroes. MMH5.5 has 203 hero files but about **40 different looks** (most heroes of a class share one figure, the campaign heroes have their own). Any look of any faction can go on any hero. Almost all of them ride something (horse, unicorn, elephant, dragon, bear, wolf...). "On foot" figures exist too (cutscene models, MMH5.5's Alaric), and two joke models (a tank and a UFO), but they lack some battle animations: an "experimental" extra at best.
- **Multiplayer:** each PC tells the others the looks its player chose, with a small message that a PC without our DLL ignores (the same trick as the co-op timing hello). *As built:* rebuilding a figure turned out to touch the game world (new game objects), so every PC rebuilds the same figures at the same moment, and only if every PC has the DLL; otherwise everyone sees the normal looks (see "Built" above).
- **Where you pick:** a "Look..." button on the **hero screen** opens a panel in the builder's style: the looks as portraits, grouped by faction, with the names of the heroes who wear them. A 3D preview inside the panel is too hard, but the hero screen itself shows the battle figure in 3D, so after "Apply" the player sees it there.
- **Remembered** per hero in `open-homm5-skins.ini` (`Godric=Freyda`). Nothing changes until someone picks a look.
- **Effort:** about **5 sessions** (4 to 6), in testable steps.

## 1. How a hero's look is defined

### The hero's data file

Every hero has one data file, `MapObjects/<faction>/<name>.(AdvMapHeroShared).xdb` (class `NDb::SAdvMapHeroShared`, vtable `0x00e6d450`; its fields are read by `0x00a0c3b0`, the fields it shares with all map objects by `0x00a07e70`). The parts about its look:

| Field | Offset | What it is | Who uses it |
|---|---|---|---|
| `HeroCharacterAdventure` | `+0x14c` | A **character** (`NDb::SCharacter`): the figure on the adventure map | the map figure (below), dialog scenes |
| `HeroCharacterArena` | `+0x13c` | A character: the figure in **battle** | battles, the **hero screen's 3D figure** |
| `HeroCharacterArenaMelee` | `+0x144` | A character: the figure while the hero strikes in battle (often empty) | battles |
| `AnimSet` | `+0x4c` | Map animations (idle, move), used when the map character has none of its own | the map figure |
| `Model` | `+0x44` | A model of its own; empty for many heroes, and the map figure does not use it | (not found in use for heroes) |
| `HeroIndividualCamera` | `+0x154` | Where the hero screen's camera stands (`camera_hero_screen` joint) | the hero screen |
| `FaceTexture`, `FaceTextureSmall`, `Icon128` | `+0x12c`, `+0x134`, `+0xe0` | The **portrait** (128×128) and smaller versions | the interface everywhere, our player list |
| `CombatVisual` | `+0x15c` | Battle effects of the hero's **class** (for example the Warlock's attack shot) | battles; keep it (it belongs to the class) |
| `Selection`, `Trace` | `+0x1d8`, `+0x1e0` | The selection marker and the footprints on the map | the map; keep them |

Each of these is a reference to another data file (a `CDBPtr`: 8 bytes, a reference-counted pointer at `+0` whose count is at its `+8`, and a byte at `+4`), loaded the first time it is needed (for example `0x004f0dc0` for a character, `0x00453ff0` for a model).

A **character** (`NDb::SCharacter`, vtable `0x00e635d0`, fields read by `0x009b2870`) is small:
- `Model` `+0x44`: the 3D model (geometry, skeleton, textures);
- `ArenaAnimSet` `+0x4c`: battle animations; `AdvMapAnimSet` `+0x54`: map animations;
- `ColourModels` `+0x5c`: the same model with the banner in each player's colour (9 entries: white, then red, blue, green, yellow, orange, teal, purple, tan). About half of the heroes' map figures have them; the unique figures (Freyda, Godric, Raelag...) do not, and show one fixed banner today.

Files in the data: `Characters/Heroes/<name>.(Character).xdb` for battle, `<name>_LOD.(Character).xdb` for the map (LOD = a lighter model for the zoomed-out map).

### What MMH5.5 changes: one folder per hero

In vanilla, a hero's data file points straight at a shared figure: 15 Haven heroes point at `Characters/Heroes/Knight_LOD.(Character).xdb`. **MMH5.5 brings its own hero files** (202 of its 203, in `MMH55-Index.pak`; the one left is a dialog-scene copy of Godric) and gives every hero its own folder of small files:

```
Characters/Heroes/Haven/Godric/
    HeroCharacterAdventure.xdb   -> Model /Characters/Heroes/Godric_LOD.xdb, ...
    HeroCharacterArena.xdb       -> Model /Characters/Heroes/Godric.xdb, ...
    HeroCharacterArenaMelee.xdb, HeroIndividualCamera.xdb, AnimSet.xdb, Model.xdb, CombatVisual.xdb
```

So in MMH5.5 every hero has **its own character objects**, even when the model inside is shared. RC19k has the same 203 heroes and the same looks as RC19d (checked file by file). Exception: the campaign copies with a `C` suffix (`IsherC`, `SufiC`...) use the folder of their original hero.

### Which code shows which figure

| Where | Code | What it reads |
|---|---|---|
| **The map figure** | `NWorld::CAdvMapHero` (vtable `0x00e544a4`), slot 5 = `0x008df730` (vtable cell `0x00e544b8`); its constructors `0x008e23a0` and `0x008e2640` call it directly | The hero's **adventure character** (hero slot `0x24c`, below), the colour model for the owner's colour (`0x008bae00`, owner = the player's slot `0x30`), the character's `AdvMapAnimSet` (`+0x54`, getter `0x00680240`). Then `0x00ce1160`, the map object's "set model": it builds a new scene object and releases the old one. Without `AdvMapAnimSet` it takes the hero file's `AnimSet` (`+0x4c`). Caravans (`0x00ce4530`) and monsters (`0x004ef190`) are built the same way. |
| **The hero screen's 3D figure** | `0x006c7030` (string `N_HERO_SCREEN3_ID`) | The hero's **arena character** (hero slot `0x248`), the colour model, `ArenaAnimSet` (`+0x4c`) |
| **Battle** | `0x00bc2010` (called from `0x00c56b90`), `0x00872bf0` | The arena character (hero slot `0x248`), `HeroCharacterArenaMelee` straight from the hero file (`+0x144`), the colour model |
| Campaign dialog scenes | `CDialogScenePlayer` (vtable `0x00e3de94`): `0x007695b0`, `0x00769b80`, `0x00768ef0`; `0x00764900` | The hero file's `+0x13c` and `+0x14c` directly |
| Portraits | the interface; our player list (`src/mods/player_list_game.cpp`) | `FaceTexture` (`+0x12c`); a map can give a hero another portrait (`SAdvMapHero` `Textures`, `+0x134`, reader `0x00a0c660`) |

**The hero object keeps its own references.** `NWorld::CHero` (its `IHero` part, vtable `0x00e7e484`) holds two characters of its own: slot `0x248` (`0x00b531c0`) returns the one at `IHero +0xb0` (battle), slot `0x24c` (`0x00b531d0`) the one at `IHero +0xb8` (map). They are copies of the hero file's references. A robot save proves they belong to the game world: inflated, `save_0.sav` (12.6 MB of world data) names every hero's `.../HeroCharacterArena.xdb` and `.../HeroCharacterAdventure.xdb` (346 references, 2 for each of 173 heroes) and about 194 portrait textures. It has no `ArenaMelee`, camera or combat visual references. **Our DLL must therefore never change those references in the hero object** (section 3).

**When the data is read.** At start-up (`0x00d10bf0`, the game's main function, calls `0x00837770`), `0x00837590` walks the game's list of all heroes (`HEROES_ANY`) and checks each one (`0x00837300`: "Hero ... has no arena character", "...no adventure character", "...no selection", "...no visual part"). That loads every hero's data file and its characters, during the splash screen. This matches [audio.md](../subsystems/audio.md): "all heroes" are read about 1.5 s after start, before DirectInput, where our hooks normally go in. The 3D models inside the characters are probably loaded later, when first drawn.

### Do heroes share looks?

| | Vanilla ToE 3.1 | MMH5.5 (RC19d = RC19k) |
|---|---|---|
| Hero files | 118 | 203 |
| Different **map** figures | 37 | 41 (42 counting a re-textured one) |
| Different **battle** figures | 38 | 41 |
| Different portraits | 106 | 153 |

Most heroes of a class share the class figure. In MMH5.5: 14 Sylvan heroes ride the Ranger, 13 Necropolis heroes the Necromancer, 13 Academy heroes the Wizard, 11 Haven heroes the Knight, 11 Dungeon heroes the Warlock, 10 Stronghold heroes the Orc warrior. The campaign heroes have their own figures, and MMH5.5 lends them to other heroes too (Axel, Maeve, Nathaniel and Ving ride as Freyda; 11 Stronghold heroes as Kujin; 7 Academy heroes as Zehir; 9 Sylvan heroes as Anwen, the Ranger with new textures).

## 2. What looks are on offer for free

**A. Every hero look of the game (recommended for the first version).** About 40, each a matching pair of a map figure and a battle figure, made to work: complete animations for the map (idle, move) and for battle (cast, buff, happy, strike...).

| Faction | Looks (named after the hero who has it; how many MMH5.5 heroes wear it) |
|---|---|
| Haven | Knight (11), Freyda (6), Markal (7), Ornella (3), Renegade knight (3), Godric (2), Isabel (2), Laszlo (2), Alaric (on foot, 1), Duncan (1), the Ghost knight (1) |
| Academy | Wizard (13), Zehir (8) |
| Dungeon | Warlock (11), Shadwyn (6), Shadya (4), Raelag on his dragon (3), Thralsai (2) |
| Fortress | Rune mage (8, and a second set of 5), Rolf (6), King Tolghar (1), Wulfstan (1) |
| Inferno | Demon lord (7), Veyer (8), Agrael (6), Biara (6), Nelech (2), Kha-Beleth (1) |
| Necropolis | Necromancer (13), Sareth (5), Arantir (2), Ornella the necromancer (1), Giovanni (1), Nicolai the vampire (1) |
| Sylvan | Ranger (5), Anwen (9), Findan (7) |
| Stronghold | Kujin (11), Orc warrior (10), Quroq (1) |

Any look on any hero: a Haven knight can ride Raelag's dragon. The rules don't care (section 4).

**B. Mounted or on foot.** Every look above rides, except Alaric (MMH5.5 gives him the on-foot cutscene model). The data also has on-foot (and some extra mounted) **cutscene models**: `Characters/Heroes/DS/*_DS_U` (on foot: Alaric, Biara, Freyda, Godric, Hangvul, Isabel, Raelag, Randall, Rolf, Shadwyn, Shadya, Tolghar, Wulfstan...) and `*_DS_M` (mounted), `Characters/Heroes/H5A2/*_Unmounted` (Arantir, Gotay, Kujin, the Orc warrior, Ornella, Zephir), MMH5.5's `DM/Sareth_Unmounted`. They have idle and walk animations, so they work on the map, but only some battle animations (Godric's on-foot model: move, idle, cast, death...; no buff, happy or strike). **Experimental** (section 6): each one needs a robot check in a battle first.

**C. Joke models:** `Characters/Heroes/H5A2/Fun_Objects/Tank` and `UFO` (idle, move, attack, cast, ranged attack). Same status as B.

**D. Portraits:** 153 in MMH5.5. A look can bring its portrait along, or the hero keeps its own (question 1).

**E. Other textures:** none for free. Apart from the banner colours (`ColourModels`) and MMH5.5's re-textured Ranger (Anwen), every figure has one set of textures. `MMH55-HDTex.pak` has no hero textures. New textures would mean new picture files, which we cannot ship (they would be game files, or our own art).

**Not offered:** creature models as heroes (a dragon without a rider has no hero animations), and a look's own **class effects** in battle (the attack shot stays the hero's class).

## 3. How to swap it

### (a) At load time: serve changed hero files (possible, not recommended)

Our DLL already serves changed data files to the game: `original_music.cpp` wraps the file system's "open file" (`NVFS::CWinVFS` slot 7, `0x009e0020`, cell `0x00e6651c`), installed when the DLL loads, before the splash screen; `rmg_templates.cpp` and `rmg_screen.cpp` serve whole files. A skin could be served the same way: when the game opens Godric's hero file, give it Godric's file with Freyda's figure references.

Why not:
- **The choice must be known when the game starts.** The hero files and their characters are read during the splash screen (section 1). Changing a look means restarting the game.
- **Friends' choices come too late.** They can only arrive in the lobby or the game, long after the splash screen.
- **It changes the game world.** The hero object copies its figure references from the hero file and keeps them in the world, and in saves (section 1). A served hero file with other references would put Freyda's file names into Godric's save data and into the world checksum. With MMH5.5 this could be avoided by serving the per-hero folder files (`Characters/Heroes/Haven/Godric/HeroCharacterAdventure.xdb` with Freyda's content: same names, other content), but the first two problems stay.

Good as a quick prototype only, or as a fallback if (b) hits a wall.

### (b) At run time: hand the game other figures, then rebuild (recommended)

All hero files and their characters are in memory from start-up, so every look is at hand. When the game builds a figure, our DLL gives it the chosen look's character instead of the hero's own:

1. **Map and hero screen and battle figures:** wrap the hero's two "character" slots in the `IHero` vtable (cells `0x00e7e6cc` = slot `0x248`, battle; `0x00e7e6d0` = slot `0x24c`, map; one `PatchPointer` each, as other mods patch vtable cells). For a hero with a chosen look, they return the look's characters (from the look hero's file, `+0x13c` and `+0x14c`); for all other heroes, the game's answer. The hero object's own references never change, so the world and the saves stay as they are. The code that calls these slots is the figure code of section 1; still to check (step 0): no rules code calls them. (Of the world code that calls a slot `+0x24c`, the two functions read, `0x00989790` and `0x00989c40`, pass it a list to fill, so they use another interface; `0x0097c9f0`, `0x0097f610` and `0x00984a00` are not read yet.)
2. **The fields read straight from the hero file:** `HeroCharacterArenaMelee` (`+0x144`), the map `AnimSet` (`+0x4c`, needed when the look's map character has no animations of its own, as Godric's), `HeroIndividualCamera` (`+0x154`, so a big dragon fits the hero screen). They are in the hero's data object (game data, not the world, one object per hero), so our DLL changes them in memory, keeping the originals to put back. The reference counts must be kept right (each copied reference counts once more, at `+8` of the pointed object, as `0x00837300` does).
3. **Rebuild the map figure at once:** call the hero's `CAdvMapHero` slot 5 (`0x008df730`) again, from the overlay's frame (the game's own thread). It does what the game does when the hero appears: new model, new animations, the old scene object released (`0x00ce1160`). To find a hero's map object, our DLL notes each one when the game builds its figure (the two constructor call sites and the vtable cell above).
4. **Battles and the hero screen** build their figures when they open, so they show the new look next time. The hero screen could later be refreshed while open (calling its builder `0x006c7030` again), as a nicety.
5. **Portrait (if wanted):** the interface reads the portrait in many places. Changing the hero data object's `FaceTexture` (`+0x12c`, `+0x134`, `+0xe0`) in memory covers our player list and probably most screens; the hero object also keeps portrait references of its own (in the save), so the interface's getter must be found first (step 4). A screen that was already open keeps the old picture until it is reopened.

**What it does not touch:** the hero object, its file references, its class, skills, music, footprints, selection marker, and the battle class effects. Campaign dialog scenes read the hero file's own fields and keep the original figure.

**Vanilla (without MMH5.5)** works the same way with (b): the wrapped slots answer per hero, so the shared class figures are never changed.

### Battles

Battle figures are built when a battle starts (`0x00bc2010`), so a look chosen before a battle shows in it. During a battle nothing changes (and the panel is not offered there). The battle "class effects" stay the hero's, so a Warlock riding a Knight's horse still throws his warlock attack.

## 4. Multiplayer

### A look cannot desync the game

- **The world checksum** (there is no `CMPWorldChecksum` class: the check is `CWorldCRCSaver`, vtable `0x00e93664`) is an `adler32` of everything the world writes when it is saved: data (slot 10, `0x00cab140`), strings and arrays (slot 12, `0x00cab190`), objects (slot 13, `0x00cab230`). The game computes it only with `mp_check_crc` on (the robot's setting). With (b) the world's data is never changed: the hero object still names its own figure files, and the figures themselves are game data, not world data.
- **The rules never look at a figure.** Hero movement and battles run in lockstep on every PC from commands; the figures, animations and their speeds are each PC's own (the game even has a per-PC animation speed setting). Earlier mods show the same: `original_music` served different hero files on the host than on the client, `[lang]` served different texts, and the world checksums stayed equal.
- To confirm with the robot anyway (step 1): a LAN game with skins on one PC only, `mp_check_crc` on, checksums all equal; a battle included.

So skins can stay local: each player picks looks, and only their own PC shows them. That alone is useful.

### Letting friends see your choices

The owner probably wants friends to see the chosen looks. That needs one small message per choice.

**The channel: the co-op guard's in-game message** (`src/mods/coop_guard.cpp`). Our DLL already sends a world-checksum message (`NWorld::CCrcTestCmd`, vtable `0x00e43db0`) with check number −1 after the first segment. The game ignores check numbers below 1 (`0x007c9410`), so a PC without our DLL does nothing with it. Its 32-bit "checksum" carries our data, with a mark in the top byte (`G` for the co-op hello). Skins would use another mark (`K`):

As built (`hero_skins_logic.h`): a header first, then one message per choice. "player" is the sender's player number, whose heroes the choices are for; it saves mapping the network's PC numbers to players.

```
header  bits 31..24 'k'   bits 23..20 version   bits 19..16 player   bits 15..0 fingerprint of the hero list
choice  bits 31..24 'K'   bits 23..20 player    bits 19..10 hero     bits  9..0 look (0x3ff = back to its own)
```

Hero and look are positions in the game's list of all heroes (`HEROES_ANY`).

- **When:** after its second segment of a game (the co-op hello goes after the first), each PC sends one message for each look it has chosen; and again when its player changes a look during the game. A first message carries a fingerprint of the hero list (a hash of all the names), so PCs with different game data (another MMH5.5 version) ignore each other's skins and show the normal looks.
- **Who decides a hero's look:** the player who owns the hero. Every PC keeps the choices of each PC, and applies them to the heroes of that PC's player. A hero hired by someone else gets the new owner's choice (our DLL rebuilds the figure when the owner changes). Computer players' heroes keep their looks, unless the owner wants a local option for them (question 6).
- **The receiving hook is shared:** the co-op guard's handler (call site `0x007ca8d1`) takes every check −1 message today; it would pass `K` messages to the skins code. The sending should happen in the same place the hello is sent from (the transceiver's own step, `0x007ca990`), from a queue.
- **A PC without our DLL** ignores the messages and shows the normal looks. **A PC with the DLL but skins turned off** shows the normal looks too (question 3).
- Saves: no skin is saved in the game. When a saved game is loaded, every PC sends its choices again.

**Why not the lobby:** the faction mod's lobby hello (`rmg_factions.cpp`, a waiting-room request of type `0x4f48`, `NUI::CMPWaitChangeRequest`, vtable `0x00e39838`) goes from each client to the host only and carries one byte. The host could only pass things on inside the start message, as the faction mod does with the map's name. That is much more work for no gain: looks can change at any time, so the game's own message channel during the game is enough.

**Single player and hot seat:** no messages; the choices in the file apply to the human players' heroes.

## 5. The panel

### Where the player picks: the hero screen

The hero screen (`NUI::CHeroScreen2`, vtable `0x00e3584c`, files `UI/HeroScreen2/*.xdb`) is the natural place: it is about one hero, it opens from the map and from the town screen, and it shows the hero's battle figure in 3D. The town screen only shows portraits; the map has no room for a button.

- **A native button** "Apparence..." / "Look...", served in the hero screen's own UI files the way the random map screen got its "Template..." button (`rmg_screen.cpp`: the game's file with one more child, only when its bytes are the ones we know; otherwise an overlay button). Also a key, for example F8 (our DLL does not use it; to check that the game does not either).
- The panel is a **modal overlay panel** like the template builder (`overlay::OpenModal`): the game is dimmed and gets no input while it is open; Esc cancels.

### The panel (rough mockup)

Words in the game's language (French or English, `[ui] language`), the builder's textures and fonts.

```
+---------------------------- Apparence de Godric / Godric's look ----------------------------+
| [Origine] [Havre] [Académie] [Donjon] [Forteresse] [Inferno] [Nécropole] [Sylve] [Bastion]   |
|                                                                        [Extras (essai)]       |
|  +-------+ +-------+ +-------+ +-------+ +-------+ +-------+     +--------------------------+ |
|  |portr. | |portr. | |portr. | |portr. | |portr. | |portr. |     |      big portrait        | |
|  |Godric | |Knight | |Freyda | |Isabel | |Duncan | |Markal |     |        128 x 128         | |
|  |(own)  | | x11   | | x6    | | x2    | |       | | x7    |     +--------------------------+ |
|  +-------+ +-------+ +-------+ +-------+ +-------+ +-------+     Freyda, mounted             |
|  +-------+ +-------+ +-------+ +-------+ +-------+               Also worn by: Axel, Maeve,  |
|  |  ...  | |  ...  | |  ...  | |  ...  | |  ...  |               Nathaniel, Ving             |
|  +-------+ +-------+ +-------+ +-------+ +-------+               Banner in your colour: no   |
|                                                                  [x] Portrait too            |
|                                                                  Friends with open-homm5     |
|                                                                  see it too.                 |
|  [ Appliquer / Apply ]     [ Apparence d'origine / Original ]          [ Annuler / Cancel ]  |
+----------------------------------------------------------------------------------------------+
```

- **Each tile** is a look: the portrait of the hero it belongs to (read from the game's data with the overlay's `GameTexture`, as the player list does: `UI/.../Portrets128x128/....dds`), the name, and how many heroes wear it.
- **The preview** is the portrait and the names. **A 3D preview inside the panel is too hard:** it would mean drawing the game's 3D models ourselves (Granny models, skeletons, animations) or driving the game's 3D window into our panel. Instead, **Apply** closes the panel and the hero screen's own 3D figure shows the new look the next time the screen opens (a live refresh crashed: see "An open hero screen shows the new look the next time it opens").
- **Apply** changes the map figure at once, and sends the choice to the other PCs.

### How choices are remembered

A small file per PC, `bin\open-homm5-skins.ini`, keyed by the hero's internal name (the `InternalName` of its file, the same in every map). As built (no portrait, per the owner's answer 1; the switches went to `open-homm5.ini` `[skins]`: `skins=1`, `show_others=1`, `trace=0`):

```ini
[skins]
Godric=Freyda          ; Godric looks like Freyda (map, battle and hero screen figures)
Hero1=Kujin            ; Stronghold's first hero rides Kujin's beast
```

A choice follows the hero into every game (question 2).

**The house rule (original by default):** a look is a presentation change, so the original is the default. An empty file (as installed) changes nothing: the wrapped slots give the game's own answer for every hero without a choice, and nothing is rebuilt. Only a choice (yours, or a friend's when `show_others=1`) changes a figure. With no choice of its own and `show_others=0`, the DLL installs none of the skin hooks at all. The panel's **Original** button removes a choice.

## 6. Build steps (each one testable)

| Step | What | How it is checked | Sessions |
|---|---|---|---|
| 0 | Confirm what the static reading says: the per-hero files are read at start-up (`[audio] log_data_files`); calling `0x008df730` again replaces the map figure; no rules code calls the hero slots `0x248`/`0x24c` (a robot trace of their callers during an AI turn) | robot run, screenshots | 0.5 |
| 1 | The look catalogue (from `HEROES_ANY`), the wrapped slots and in-memory fields, the live rebuild, `open-homm5-skins.ini` read at start. No panel yet: looks are set by editing the file | robot: screenshots of a skinned hero on the map, in a battle, on the hero screen; a hero in a boat and in a town; LAN game with `mp_check_crc`, one PC skinned, checksums equal | 1.5 |
| 2 | The panel and the hero screen button, French and English | robot screenshots at 1024×768 and 1920×1080; the owner clicks through it | 1.5 |
| 3 | The multiplayer message (shared with the co-op guard's hook), owner rules, the fingerprint | robot LAN: the client sees the host's look; a client without the DLL sees the normal one; checksums equal | 1 |
| 4 | Portrait option; the extras (on-foot, cutscene and joke models), each checked in a battle before it is offered | robot screenshots in battle | 0.5 to 1 |

**Total: about 5 sessions** (4.5 to 6). Steps 0 and 1 give a working feature for one PC; step 3 is what friends need to see it.

## 7. Risks

- **Rebuilding a figure in an odd place.** The figure builder also sets the figure's state (`0x004b6560(2)`, probably "shown"). A hero in a boat, inside a town or in a garrison may pop up on the map. Rebuild only heroes standing on the map, or put their state back; the robot checks these cases (step 1).
- **Missing animations.** A figure without an animation the game asks for may freeze, or a battle may wait for an animation end that never comes. That is why the first version offers only real hero looks, with complete animation sets; extras come after a battle test each.
- **Size and framing.** A dragon is much bigger than a dwarf on a bear: on the map it may cover more of the tile, and on the hero screen it needs the look's camera (`+0x154`, swapped too).
- **Ownership is harder to see.** The unique looks (Freyda, Raelag, Kujin...) have no banner in the owner's colour; about half of MMH5.5's heroes already look like that today. A Haven player riding a Necropolis look may confuse friends. Question 5.
- **Reference counts.** Swapping references inside game data must count them right, or the game may crash at the end of a map or when it quits. Put the originals back when the game leaves a map.
- **Shared folders:** in MMH5.5 the campaign copies (`IsherC`...) share their hero's files; our per-hero answers keep them apart anyway.
- **MMH5.5 updates** may rename heroes or folders. The catalogue is built from the game's own list each time, and the network fingerprint keeps PCs with different data apart. RC19k changes nothing here.
- **Desync:** low, for the reasons of section 4, and checked by the robot at steps 1 and 3.
- **Linux (Proton):** nothing new (no new Windows functions); the panel shares the builder's input code, which still needs its own Linux check.

## 8. Questions for the owner

1. **What should a look change?** Only the 3D figures (map, battle, hero screen), or **the portrait too** (in every screen)? Proposed: the figures always, the portrait as a tick box.
2. **One choice per hero, for every game** (Godric always looks like Freyda on your PC), as proposed? Or per game?
3. **Should friends see your looks?** Proposed: yes, when their PC has the DLL (otherwise they see the normal look), with `show_others=0` for a player who prefers the normal looks. Should that be on or off by default?
4. **Which looks to offer:** only the ~40 real hero looks (proposed for the first version), or also the extras (on foot, cutscene models, the tank and the UFO) as "experimental"?
5. **Any faction on any hero** (a knight on Raelag's dragon), or only looks of the hero's own faction?
6. **Computer players' heroes:** keep their looks (proposed), or allow choices for them too (seen only on your PC)?
7. **The button on the hero screen:** where should it go? A quick sketch over a screenshot of your (French) hero screen would help.
8. Does the group ever play **without MMH5.5**? Vanilla works with the same code, but would need its own tests.

## Robot results (2026-11-01)

Test copy `C:\Games\H5_test` (MMH5.5 RC19d), map A1M4, each run under the robot lock, the test copy's `dinput8.dll` put back after each run. Choices files made by `tools/robot/make_skins_file.py`: every hero of a faction wears one look (file A: Freyda, Zehir, Raelag, Wulfstan, Agrael, Giovanni, Heam, Quroq; file B: Duncan, Cyrus, Thralsai, King Tolghar, Biara, Arantir, Diraya, Kujin).

**Step 0 (what the static reading said):**
- The game's list of all heroes (`HEROES_ANY`) holds **179** heroes (fingerprint `0518`), not all 203 hero files: MMH5.5's campaign copies are not in it. 163 of file A's 194 lines resolve; "Orlando" exists twice (Haven and Inferno), and the list's one wins.
- **Who calls the two figure slots** (`trace=1`, over a whole run with AI turns and a battle): slot `0x24c` only from the map figure builder (`0x008df758`, `0x008df773`), slot `0x248` only from the battle hero's figure (`0x00bc2303`, `0x00bc2324`, `0x00bc2367`). No rules code.
- **Calling the figure builder again replaces the map figure** (screenshots: the human player's hero takes the look; its portrait stays its own, as wanted).
- **But a rebuild is not invisible to the world:** it makes two new game objects (new object numbers from the world's counter) and marks four others as changed (`0x0050c020`). A LAN run where each PC rebuilt its own player's hero at its own moment had different world checksums from check 1 on (598 of 599). Hence the multiplayer design above.

**Steps 1 and 3:**

| Run | What happened | World checksums |
|---|---|---|
| Single player, file A, `trace=1` | The human's hero rebuilt with its look 0.7 s after the map showed; computer players' heroes kept theirs | (one PC) |
| Single player, file A, long run | The AI attacked the human's hero: the battle used the look's battle figure (slot `0x248`), auto-combat ran to the results screen, no error | (one PC) |
| LAN, host file A, client file B | Both PCs: "looks on for this game (all 2 PCs sent theirs)"; at segment 20 both rebuilt Nelech (host's hero, Agrael's look) and Tamika (client's hero, Arantir's look); the client's screenshot shows Tamika riding Arantir's horse with her own portrait; 43 AI steps | 793, all equal |
| LAN, host file A, client `skins=0` | Host: "no looks in this game: PC 1 sent none in time"; no rebuilds | 517, all equal |
| Single player, no file, the panel (step 2) | `skins open` for Ingvar (Fortress): 5 looks (his own, Wulfstan, Rolf +4, Ebba +3, King Tolghar); `pick 1`, `apply`: "Ingvar wears the look of Wulfstan: counts now", the map figure rebuilt at once (Ingvar on Wulfstan's mammoth, his own portrait); reopened: "now Wulfstan" | (one PC) |
| LAN, files A and B, the host's panel at ~75 s | Host: King Tolghar (Wulfstan's look) changed to Rolf's: "sent ... after this PC's report 422; they count at segment 432"; client: "a changed look for player 1 (counts at segment 432)"; both rebuilt King Tolghar at the same moment | 702, all equal |

Earlier LAN runs of the first version (rebuilds from frames, before the synchronized design) are the ones that showed the checksum split; they are kept in `build/robot/20261101-164742-lan` and `-165433-lan` of this worktree.

**Robot checks still to do** (each one run; commands as above with `--skins-file` / `--host-skins-file`):
1. **The hero screen:** open it and check the 3D figure takes the look. The robot has no step for it: `ui|NUI::CAdventureScreen hero_dbl_click` (the hero list's double click) crashed, caught, since it needs the clicked list item. A step that calls the hero screen's creation directly would need more research; checking by hand is quicker.
2. **A hero hired during a multiplayer game** after segment 20: both PCs build it with the look (checksums).
3. **A hero in a boat and in a town** when the looks come on (single player rebuilds from frames; multiplayer at segment 20).
4. **A battle in a multiplayer game** between a human with a look and the AI (checksums during the battle).
5. **Hot seat** (two humans on one PC): both humans' heroes take the file's looks.
6. **RC19k** (`--game C:\Games\H5_test_k`) and **Linux**.
7. **The game's own owner change** (a hero taken over by another player) in single player: rebuilt with the new owner's choice.

# Plan: volume range, the lobby's difficulty choice, ToE start bonuses, hero skills, map edges

Status: **research only** (2026-11-01). It is based on reading the vanilla 3.1 exe in Ghidra (`tools/inventory/dump.ps1`, `xrefs.ps1`; the addresses are also valid for `MMH55_Utility(_64).exe`) and the game and MMH5.5 data inside the paks (RC19d in the Steam copy, RC19k in `C:\Games\H5_test_k`), plus one screenshot from the owner. The game was not run, and neither was the robot (the owner was playing).

The owner asked for five things:
1. **Audio:** "even at the lowest setting, the sound is too loud. We need to allow more granular selection or make the slider a wider range."
2. **Lobby:** "Perks selection is a bit bugged right now (second and third option don't match HoMM 5.5 AI difficulties)."
3. **Lobby:** "Could we somehow bring back standard ToE perks selections alongside HoMM 5.5 difficulty?"
4. **Later:** "an additional setting (when starting a game) to select whether we use classes or standard HoMM 5 random skills."
5. **Map edges:** "could we modify the way the game displays the map borders? Right now it looks cut off with a violet square background; could we make it dark / foggy / look like a map?"

## The owner's answers (2026-11-01) and what is built

| Item | Answer | Status |
|---|---|---|
| 1. Volume | "Wider range": `[audio] volume_curve=wide`, off by default; no scale settings. | **Built and robot-checked:** `src/mods/audio_volume.cpp`. The owner's ears still to check. |
| 2. Lobby labels | (the fix is ours, no choice needed) | **Done and robot-checked:** 4 tooltips in the private packs; outdated-text check in `tools/lang/inventory.py`. |
| 3. ToE bonuses | "Each player picks": option A (a mode chooser in the lobby, the bonus arrows back to ToE). | **Built and robot-checked** (2026-11-01): host rules step 2, `game_mode` and `toe_bonuses` in the Game rules panel ([host-rules.md](host-rules.md), "Step 2"). |
| 4. Hero skills | "Per game in the lobby" (option 2). | Later. |
| 5. Map edges | "Black": `[camera] edge_sky=none`, off by default. | **Built and robot-checked:** `src/mods/edge_sky.cpp`. |
| The French pack in `data\` | The coordinator asks the owner. | Open. |

**What was built (2026-11-01):**

- **Volume** (`audio_volume.cpp`, setting `[audio] volume_curve=game|wide`, testing `log_volume=N`).
  - The two calls of `0x00b26e80` (`0x00d0439b` sounds, `0x00d047ac` music) are redirected, after checking each still calls `0x00b26e80`.
  - The volume is multiplied once more by its slider (`audiovol::Wide`, in `audio_volume.h`), and a sound that was audible stays at least at FMOD's quietest step.
  - The 2D and 3D per-sound factors (`0x00d043c0`, `0x00d045b0`) both include the sounds slider, so one factor fits every sound.
  - Unit test: `src/hook/test/audio_edge_test.cpp`. For example, at the lowest step FMOD gets 2/255 instead of 25/255, and the result is never louder than the game.
- **Edge sky** (`edge_sky.cpp`, setting `[camera] edge_sky=game|none`, testing `edge_sky_test_light=`).
  - It wraps the game's open-file function (slot 7, stacked with the music and language mods) and serves `Lights/_(AmbientLight)/Arena/Dusk_light_BoatArena01(VicTest).xdb` with an empty `<SkyDome/>`.
  - **Battle skies are unchanged:** 6 other light files in the game data (arena and scene lights) name this same sky dome, and they are not touched; neither is the dome itself. The dusk light is named only by the generator's list. Of the generator's other three lights, `0_Default_AmbientLight` is used by 21 battle maps (MMH5.5's bank arenas), which is one more reason they are left alone.
  - The same unit test covers the file edit (only the sky dome element changes; lights without one are left alone).
- **Lobby texts** (private, in `build/lang/`, never in git):
  - batch `gap-batches/b6-lobby-modes` (the 4 tooltips; `batch.py check`: 0 errors, 0 warnings);
  - `private-fr/open-homm5-fr-rc19d.pak` (183 + 4 texts, SHA-256 `633dd665…`) and `private-fr/open-homm5-fr.pak` (480 texts, 2 of them replaced, SHA-256 `a82628db…`), made with the new `batch.py add`. Their 4 new texts are dated 2026-11-01, so they win over the official French packs (dated 2025);
  - the ready zip `open-homm5-francais-RC19d.zip` (the same 3 files, the gap pack replaced).

  They were made in this worktree's `build/lang/` (the main checkout's `build/` was read only), so they still have to be copied over the main checkout's files.
- **Outdated-text check:** `inventory.py --fr-pack` now also writes `outdated.txt`, the translated files whose markup differs from the English text's. RC19i's pack on RC19k: 108 candidates, among them the gold and resources tooltips. `inventory.py --self-test` checks the comparison.

**Robot checks (2026-11-01, robot lock held; runs in this worktree's `build/robot/`):**

| Check | Run | Result |
|---|---|---|
| 1. Volume | `20261101-164014` (lowest step: `sound_sfxvolume = 0.1`, `sound_musicvolume = 0.1`) | The main menu music fades in from `game volume 1/255 -> 1/255` to `game volume 25/255 -> 2/255`. On the map, `audio: sound: slider 25/255, game volume 25/255 -> 2/255`. **Works**; the owner's ears still to check. |
| 2. Edge sky on A1M4 (test light `AC1M4.xdb`, far camera) | `20261101-162622` (none), `20261101-163308` (game) | With `game`, a pale sky glow above the far edge; with `none`, black there. The log has `edge sky: lights/_(ambientlight)/advmap/addon/ac1m4.xdb served without its sky dome`. A battle (an AI attack, underground battlefield) ran normally, and no other light was served. **Works.** |
| 3. Edge sky on a random map with the dusk light (the owner's `MAP_JoacJoz`) | `20261101-164014` (none, RC19d copy), `20261101-164319` (game, RC19k copy) | Same start view. With `game`, the flat violet and lavender of the owner's screenshot past the top-left edge; with `none`, black. The log has `edge sky: lights/_(ambientlight)/arena/dusk_light_boatarena01(victest).xdb served without its sky dome`. **Works.** |
| 4. LAN, host only with both settings (and `texts=fr`) | `20261101-164454-lan` | `lan_report.py`: 700 world checksums, all equal on both copies (no desync). The client logged the game's sky and sliders. |
| 5. French lobby tooltips (new RC19d packs in the test copy, removed afterwards) | `20261101-164454-lan`, host | The lobby reads the 4 tooltips when it builds its rows. All 4 came from `open-homm5-fr-rc19d.pak`, in French: Mode Par défaut, Mode Sauvage, Mode Cauchemar and the help text. **Works.** |

One fix came out of these runs. `log_volume` now logs only **changes** of an audible volume: a music fade-in had used up all the lines.

Screenshots of the edge (for the owner) are in the session's `scratchpad\edge-shots\`.

## Summary

| # | What we found | Recommended | Effort | Desync risk |
|---|---|---|---|---|
| 1 | Only two sliders exist (music, sounds). They have 11 steps and are linear. The lowest step is 10% of the loudness wave, only 20 dB quieter than full volume. | A setting `[audio] volume_curve=wide` plus per-channel `music_scale` and `sound_scale`, applied where the game hands every channel's volume to FMOD. The original stays the default. | **1 session** | None (sound is local only) |
| 2 | MMH5.5 turns the per-player "starting bonus" into a game-wide difficulty mode. MMH5.5's official **French pack** still has the old texts for the gold and resources choices, so with `texts=fr` they describe start bonuses instead of the Savage and Nightmare modes. The bug is not in our code. | Add 4 corrected texts to our private French gap packs (not in git). Add a check to our tools that flags outdated French texts. | **0.25 session** for the texts, 0.5 for the check | None (texts are local) |
| 3 | MMH5.5 reads the bonus each human got, decides the mode from it, then **takes the bonus back**. Only one control exists, so it cannot mean both things. | Not now. If wanted: first a host setting "ToE bonus for everyone" (B), later a game-wide mode chooser in the lobby (A). | B: 2 to 3 sessions; A: 5 to 6 | Yes: these change the world, so every PC needs the DLL |
| 4 | MMH5.5 has 24 hero classes (3 per faction) with their own skill chances, and assigns every hero to one. Nothing can switch them off: no setting, no script function. | First as a per-session setting (the DLL serves the vanilla class data at start-up), then per game from the lobby, using the channel the faction-terrain mod already uses. | 3 sessions (per session); 5 to 7 (per game) | Yes: skill offers are part of the world |
| 5 | The violet is a **sky dome**, a big textured shape drawn around the scene. MMH5.5 added it to one of the four lights the random map generator picks from. Its textures match the screenshot's two colours exactly. | A setting `[camera] edge_sky=game\|none\|dark`. `none` drops the sky dome from the random-map lights, so the edge shows black like unexplored land. `dark` swaps in a dark, hazy sky dome of our own. | **1 session** (none); +1 (dark) | None (presentation only) |

**Quick wins**, in order: the 4 French texts (item 2), Windows' own volume mixer today (item 1, no code), the volume curve (item 1), `edge_sky=none` (item 5).

---

## 1. The volume sliders

### What the game does

**The options screen** is data, not code. Its pages and options are in `UI/uiconsts.(UIGameConstsH5).xdb`. MMH5.5 ships its own copy in `MMH55-Index.pak`; the copies in `data.pak` and `a2p1-data.pak` have the same audio options. The audio page has:
- a checkbox `play_sounds`;
- **`music_volume`**, an `OPTION_SCROLLER` bound to the console variable `sound_musicvolume`;
- **`sound_volume`**, the same kind of slider, bound to `sound_sfxvolume`.

Each slider has 11 items, with values 0, 0.1, 0.2, ... 1.0. Their labels (`UI/Consts/GameOptions/AudioMusicVolume/Item00.txt` ... `Item10.txt`, and the same for `AudioSoundVolume`) read "no music", "10%" ... "100%". There is **no master slider and no ambient slider**: ambient sounds go through the sounds slider.

**The console variables** (registered in `0x00cf2f00`, each with a function the game calls when the value changes):

| Variable | Default | Saved in the profile | On change | Then |
|---|---|---|---|---|
| `sound_sfxvolume` | 0.75 | yes (type 2) | `0x00cefb80`: `int v = value × 255` | `0x00d03e90` → sound manager `[0x010dd560]`, `0x0049cb30`: stores `v` at `+0x18` and re-applies every playing sound |
| `sound_musicvolume` | 0.75 | yes (type 2) | `0x00cefbc0`: the same | `0x00d03f50` → music system `[0x010dd564]`, `0x00b2d560`: stores at `+0x18`, re-applies every stream |
| `sound_sfx3Dvolume` | 1.0 | no (type 0) | `0x00cefc00` | `0x00d03ed0` → `0x0049cb80` (`+0x1c`) |
| `sound_group_general_volume` | 1.0 | no | `0x00cefc40` | `0x00d04000(group 0)` → `0x0049cc20` |
| `sound_group_ambient_volume` | 1.0 | no | `0x00cefc50` | `0x00d04000(group 1)` |

- At start-up, `0x00cf0a30` initialises FMOD and applies the two saved values.
- The adventure map's small dialog screen lowers the music and the sound groups while it is open, then puts them back: `0x00611470` and `0x00610d10`, which restores the values it saved earlier.
- A cutscene screen (`0x006a2170`) saves the music volume and sets it to the sound volume for the video.

**From the variables to FMOD.** FMOD 3.75 (`bin\fmod.dll`) takes a channel volume from 0 to 255, on a straight (linear) scale of the sound wave's size.
- **Sounds:** `0x00d042e0` multiplies three factors: the sound's own volume (`+0x4c`), its group's volume, and a per-sound factor (virtual function `+0x28`; probably the 3D distance and the sounds master, not yet confirmed).
- **Music:** `0x00d04710` multiplies the music master (`[0x010dd564]+0x18`) by the stream's own volume (`+0x54`).
- **Both** call **`0x00b26e80`** (a float from 0 to 1, times 255, then `FSOUND_SetVolume(channel, v)`). It is the **only caller** of `FSOUND_SetVolume` (stub `0x00d8df10`, import slot `0x00e0a63c`). The two call sites are **`0x00d0439b`** (sounds) and **`0x00d047ac`** (music).
- `FSOUND_SetSFXMasterVolume` is used only to save FMOD's own master volume at start-up (`0x00d020e0`) and to restore it on exit (`0x00d03380`).

**Why it is too loud.** The lowest step is 0.1 of the full wave, which is only **−20 dB**. Most games put their lowest step at −40 dB or lower. The owner's profile (`Documents\My Games\Heroes of Might and Magic V - Tribes of the East\Profiles\Joz\user_a2.cfg`) already has `sound_musicvolume = 0.1` and `sound_sfxvolume = 0.1`, the lowest step that is not silent.

### Options

| Option | How | Verdict |
|---|---|---|
| a. More or different slider steps (data) | Serve a changed `uiconsts` with new `<Value>`s, or with 21 items | **No.** It is a large file that MMH5.5 changes in each version. The profile stores the value (0.1), so the slider may not find its position again. Labels and markers would need checking too. |
| b. Write a smaller value into the profile (`setvar sound_sfxvolume = 0.03`) | Edit `user_a2.cfg` by hand | **Fragile.** The options screen snaps the value back to a step when it is opened. The hidden group variables only work for sounds, not music, and are not saved. |
| c. **Rescale at the two call sites (code)** | `hookpatch::RedirectCall` at `0x00d0439b` and `0x00d047ac`. Each must call `0x00b26e80`. | **Recommended.** Nothing stored changes, so every save-and-restore path stays correct (dialogs, cutscenes, the profile). Turning the setting off gives back exactly the original. |
| d. The operating system's per-application volume | Windows: Volume Mixer, lower "Heroes V". Linux: `pavucontrol`, Applications tab. | **Today's workaround**, no code. It lowers everything together, and the in-game slider then works within that range. |

Hooking the volume setters themselves (`0x00d03e90` / `0x00d03f50`) was rejected. The dialog screen restores a value it read back from `+0x18`, so a curve put in there would be applied twice after every dialog, and the music would get quieter each time.

### Design (the original as the default)

```
[audio]
volume_curve=game   ; game: as the game (straight). wide: each slider step is a fixed number of decibels
music_scale=100     ; % on top of the music slider (100 = the game's)
sound_scale=100     ; % on top of the sounds slider
```

- **How:** at each of the two call sites, the replacement multiplies the float by `factor = curve(s) / s × scale`. Here `s` is the stored master (`[[0x010dd564]+0x18] / 255` for music, `[[0x010dd560]+0x18] / 255` for sounds), and `curve(s) = s²` for `wide`. When `s = 0` the sound is silent anyway.
- **`wide`, step by step:** 10% → −40 dB, 20% → −28, 30% → −21, 40% → −16, 50% → −12, 60% → −9, 70% → −6, 80% → −4, 90% → −2, 100% → 0 dB.
  - The steps are evenly spread on a loudness scale, and the lowest one is 10 times quieter than today.
  - FMOD's own resolution (1/255, about −48 dB) is the floor, which is why the curve is `s²` and not `s³` (with `s³`, the first step would round to silence).
- **Scales:** `music_scale=25` alone gives steps from 2.5% to 25%, for an owner who wants a lower ceiling rather than a different curve.
- **Where:** a new `src/mods/audio_volume.cpp` (about 120 lines). Settings in `src/mods/README.md` and the shipped ini. A log line like `audio: volume curve wide, music 100%, sounds 100% (call sites 0x00d0439b, 0x00d047ac hooked)`.

**Effort:** 1 session.

**Checks:**
- the robot: the log line, the game runs, a battle and a dialog work;
- the owner's ears (the robot cannot hear): the lowest step must be quiet but audible, and music and sounds must stay balanced;
- the same on Linux. Wine runs the same exe code, so no difference is expected.

**Risks:**
- The slider labels still say "10%" while the sound is at 1%. That is acceptable, or the labels could be served relabelled later.
- What virtual function `+0x28` returns does not change the design, since the factor is read from the stored master.

**Desync:** none. Volumes are not part of the game world and are never sent over the network.

---

## 2. The lobby's per-player choice shows wrong labels

### What MMH5.5 did with ToE's starting bonus

**The control.** In the multiplayer lobby, each player row has bonus arrows and an icon with a tooltip (`UI/MPWait/PlayersList/Item/BonusIcon.(WindowTextView).xdb`, `BonusChangeLeft/Right`). There are 4 values:
- 0, Random;
- 1, Random minor artifact;
- 2, Gold;
- 3, Resources.

Textures come from `Bonuses.(WindowRelatedTextures).xdb`; MMH5.5 repainted the four icons (`MMH55-Data.pak`, 2022). The exe picks the tooltip by name (`bonus_tooltip_random`, `_random_minor_artifact`, `_gold`, `_resource`; strings at `0x00e39db0`, `0x00e39d8c`, `0x00e39d78`, `0x00e39d60`, used by the player-row setup `0x00714f00`).

**MMH5.5's script** `scripts/H55-AdvMap.lua` (`MMH55-Index.pak`), function `H55_PrepareAdvMap`, block "StartingBonus" (lines 418 to 677 in RC19d; RC19k is the same) works like this:
- After the game has handed out the start bonuses, it looks at each **human** player's gold, resources and hero artifacts. It recognises which bonus the player got from the tables `H55_StartBonusGold`, `H55_StartBonusNormal`, ... in `H55-Core.lua` (lines 141 to 166).
- It **takes the bonus back** (resets gold and resources to the difficulty's amounts, removes the minor artifact). Then every human's starting hero gets MMH5.5's own bonus (`H55_ArtifactStartBonus`, plus a creature bonus).
- It sets **one mode for the whole game**: if any human picked the artifact, **Default**; otherwise, if any picked gold, **Savage** (AI heroes never flee; humans and AI on opposite teams); otherwise, if any picked resources, **Nightmare** (also: AI heroes cannot be looted). "Random" becomes a random one of these, which the help text warns about.
- Games without computer players always get Default. Scenarios get only Casual or Default, campaigns Default.
- **Casual** is decided elsewhere (`H55_UpdateGameMode`, from the AI players' gold). It belongs to the "equal resources" option (`EE_equal_resources`), which only the single-player 3.1j exe (`MMH55.exe`) has. As far as the scripts show, the co-op exe (`MMH55_Utility`) cannot select it.

So in MMH5.5 the choices mean: Random (a random mode), **Default**, **Savage**, **Nightmare**.

### The texts, layer by layer (tooltip paths `UI/MPWait/PlayersList/Item/bonus_tooltip_*.txt`)

| Layer | Random | Minor artifact | Gold | Resources |
|---|---|---|---|---|
| `data\texts.pak` (base game, French) | ToE meaning | ToE meaning | ToE meaning (a small gold bonus) | ToE meaning (a few resources) |
| `MMH55-Texts-EN.pak` (RC19d and RC19k) | MMH5.5's help text | **Default** mode | **Savage** mode | **Nightmare** mode |
| MMH5.5's official French pack, RC19d (`bin\open-homm5\lang\fr\MMH55-Texts-FR-RC19d.pak`, installed) and RC19i (`build\lang\fr-rc19i\`) | MMH5.5's text, **still in English** | Default, **still in English** | **outdated French**: a gold bonus of a few thousand | **outdated French**: extra creatures of tiers 2 to 4 |
| our gap pack for RC19d (`open-homm5-fr-rc19d.pak`, installed) | – | – | – | – |
| our gap pack for RC19k (`open-homm5-fr.pak`, not installed yet) | French | French | – | – |

### Why options 2 and 3 are wrong

The owner plays with `[lang] texts=fr`. For every text our French packs have, our mod hands the game that file, so the lobby shows the official French pack's four tooltips:
- **"Gold" (Savage) and "Resources" (Nightmare) describe start bonuses from an older MMH5.5**, before the modes existed. These are the owner's "second and third options" (counting Default as the first real choice).
- The Random and Default texts are correct, but in English.

The fault is in **MMH5.5's French pack** (the translators never updated these two texts). Our tools did not catch it either: they only translate texts that the French pack leaves in English, and these two look French. MMH5.5's English pack is right, and our language switch works as designed.

### A new finding: a French pack in the game's `data\` folder

Today (2026-11-01 14:02) a file `data\MMH55-Texts-FR.pak` appeared in the Steam copy. It is not ours and not in our package.
- Size 1,986,735 bytes, SHA-256 `aa1860885eda627e4a537d098346af1be0e0c1cbd16d7ba5f51eef010dcf7967`. It has 5,912 texts, with the same paths as the RC19i pack plus a few.
- Its files are **dated 2026-10-31**, newer than every other pack, so the game prefers it over `MMH55-Texts-EN.pak` for every path it has (see [../subsystems/texts.md](../subsystems/texts.md), "Which archive wins"). Anyone with `texts=game` and this file gets it.
- Its four bonus tooltips are the **base game's** (ToE meanings), so without our mod all four would be wrong.
- Its version text holds a placeholder, which explains our log line `languages: version text: ... the game "Version : "`.
- **Question for the owner:** where does it come from? If it is not wanted, take it out of `data\` (the owner's action; we never write there). A French pack belongs in `bin\open-homm5\lang\fr\`, where only our mod reads it.

### The fix that belongs to us

1. **Our private gap packs** (in `build\lang\private-fr\` and `gap-batches\`, never in git):
   - French versions of `bonus_tooltip_gold.txt` (Savage) and `bonus_tooltip_resource.txt` (Nightmare), translated from MMH5.5's English;
   - for the RC19d pack, also `bonus_tooltip_random.txt` and `bonus_tooltip_random_minor_artifact.txt`, which the RC19k gap pack already has.
   - Each entry dated after the official pack's files (2025-10-02), so it wins among our packs ("newest wins", `src/mods/languages.cpp`).
   - Then rebuild the packs with `tools/lang/build_pack.py` and the private zip, and the owner copies them into `bin\open-homm5\lang\fr\`.

   **0.25 session.**
2. **A check in our tools** (`tools/lang/inventory.py`, in git): flag the French pack's texts that are probably **outdated**, meaning their markup tags or placeholders differ from MMH5.5's English. Here the English has colour tags and line breaks, and the French has none. The candidates go into `build\lang\rc19k\mmh55-outdated.txt` for a translator to review. This will likely find more texts like these. **0.5 session.**
3. **No change to the DLL.**
4. **Upstream:** tell MMH5.5's French translators (the `MMH55-Texts-FR` repository). That is posting on the owner's behalf, so the owner must OK it.

---

## 3. ToE's starting bonuses alongside MMH5.5's modes

### Why it cannot simply be "both" today

- MMH5.5 needs the control's value as a **signal**. It reads it back from resources and artifacts, so a real ToE bonus and a mode choice cannot travel through the same value.
- The **mode is game-wide** (one value from all the humans' picks), while the **bonus is per player**.
- **Every choice in the lobby already reaches every PC.**
  - The game setup (`NWorld::CPlayersStartInfo`, vtable `0x00e92744`) is a list of slots, `0x8c` bytes each.
  - The host sends it in `NMP::CMPStart` when it starts the game (`0x0071feb0`, called at `0x007200da`).
  - Each PC creates its players from it (`0x00c9fa80`). See `src/mods/rmg_teams.cpp`.
  - The slot field that holds the bonus is not found yet; it is probably next to the team (`+0x1c`) or the faction (`+0x18`).
- **Anything that changes resources, artifacts or the mode changes the game world.** Every PC must do the same thing, or the game desyncs. So a PC without our DLL could not take part.

### Options

| Option | How | Effort | Notes |
|---|---|---|---|
| **A. A game-wide mode chooser in the lobby**, with the bonus arrows back to ToE's meaning | The host picks Casual/Default/Savage/Nightmare in a small panel our DLL draws in the lobby. The choice travels like the faction-terrain mod's (below). Every PC's DLL serves `scripts/H55-AdvMap.lua` with the "StartingBonus" block replaced: keep each player's ToE bonus, and take the mode from our value. | 5 to 6 sessions | The cleanest result. The script patch must match MMH5.5's exact text, and is refused for unknown versions. |
| **B. A host setting "ToE bonus for everyone"** | `[game] toe_bonus=off\|gold\|resources\|artifact\|random` on the host, carried to every PC. The served script gives every human that bonus **after** MMH5.5's own mode logic. The mode stays in the lobby control. | 2 to 3 sessions | No lobby UI. Loses the per-player choice. |
| C. More values on the same control (4 to 12 combinations) | Patch the lobby, the bonus code and the script | – | Rejected: too many exe patches. |

**How a per-game choice can safely reach every PC** (the same channel serves item 4). The faction-terrain mod already does this, in `src/mods/rmg_factions.cpp`:
- each client's DLL says hello to the host in the lobby, with a waiting-room request the game ignores (`0x00711580`, ignored in `0x0071f3d0`);
- the host then puts the decision **into the map's name** in the start message (`Maps/RMG/<guid>-OH1-<digits>/`);
- a PC without the mod cannot even find that map, so it can never start on a different world.

This works for random maps, which is what the group plays. Hand-made maps would need the VFS to serve them under a second name (not studied).

**Recommendation:** not now. Ask the owner which bonus they miss and whether per player matters.
- MMH5.5 already gives every starting hero a random minor artifact and a creature bonus in every mode. A ToE bonus on top makes the humans stronger, which changes MMH5.5's balance.
- If wanted, B first, then A.

**Risks:**
- desync if a PC differs (mitigated by the map-name channel);
- each MMH5.5 version may change the script block;
- balance.

---

## 4. Hero classes or standard random skills

### What MMH5.5 does

**The class table** is `GameMechanics/RefTables/HeroClass.xdb`. For each class it gives the chance of each skill being offered on level-up (`SkillsProbs`) and of each attribute going up (`AttributeProbs`).
- **Vanilla ToE** (`a2p1-data.pak`): 9 entries, "none" plus one class per faction (`HERO_CLASS_KNIGHT` for Haven, `RANGER` for Sylvan, and so on).
- **MMH5.5** (`MMH55-Index.pak`): 25 entries, "none" plus **24 classes, 3 per faction**, each with its own chances. For example, its Knight class offers skills a vanilla Knight never gets.

**Each hero's class** is the `<Class>` field of its `MapObjects/<faction>/<hero>.(AdvMapHeroShared).xdb`, and MMH5.5 reassigns them:
- 20 Haven heroes carry `HERO_CLASS_RANGER`, which is the Sylvan class in vanilla;
- `HERO_CLASS_NECROMANCER` is used by Fortress heroes, `HERO_CLASS_RUNEMAGE` by Necropolis heroes;
- `RANGER` appears in all 8 hero folders.

So a class name no longer tells you the faction.

**In the exe:**
- the table is loaded by `0x00ae2980` (string `/GameMechanics/RefTables/HeroClass.xdb`); its type is `NDb::STable_HeroClassDesc_HeroClass` (vtable `0x00e79348`);
- the readers of the `SkillsProbs` and `AttributeProbs` fields are `0x00ad8a50` and `0x00adec80`;
- MMH5.5 patches `0x00ae29e0` so the game accepts 74 classes instead of 9 ([mmh55-rc19k.md](mmh55-rc19k.md)).

MMH5.5's **scripts** keep their own copy of the classes: `H55-Tables.lua` (`H55_ClassesNames`, 24) and `H55_GetHeroClass` in `H55-Core.lua`, a per-hero list. They use it for witch huts and similar objects.

**No way to switch it off exists today:**
- `H55-Settings.lua` has no such setting, and neither has `MMH55Stats.xml`;
- the Lua functions have nothing for classes or skill chances (the exe has `GiveHeroSkill`, `ChangeHeroStat`, `LevelUpHero`, but no `SetHeroClass`), so a map script cannot do it;
- the 3.1j exe's custom-game options (`EE_*`) do not include it, and that exe is single player anyway.

**It is a world rule.** Level-up offers are worked out by the game world, on every PC, with the shared random numbers. If two PCs use different class tables, they offer different skills: a desync from the first level-up.

### Options

1. **Per session**, `[game] hero_skills=mmh55|standard`, the same on every PC.
   - When the game reads the data at start-up (during the splash screen; our file hook is in before, see [../subsystems/audio.md](../subsystems/audio.md), "Order seen in the robot"), the DLL serves:
     - each hero's xdb with `<Class>` set to its **faction's vanilla class**;
     - `HeroClass.xdb` with **vanilla chances for the 8 vanilla classes**, read at run time from `a2p1-data.pak` (never shipped). The 16 MMH5.5-only classes stay but are no longer used.
   - Every PC then says its setting in the co-op hello ([coop-timing-guard.md](coop-timing-guard.md)). The hello comes about 10 s into the game, so it can only **warn** ("hero_skills differs: the game will desync"), not prevent.
   - **About 3 sessions** (1 to find out how the game picks the offers and whether a hero's class is copied into the save, 2 to build).
2. **Per game, from the lobby.** The host picks; the choice travels in the map's name (item 3's channel). Every PC switches **in memory** at the start, before the first turn: the class table's chances, and each hero's class.
   - This needs to know where a live hero keeps its class, and whether saves store it. A loaded game must use the rule it was started with.
   - **2 to 4 sessions more** (5 to 7 in all).

**Risks:**
- **Desync** if PCs differ: the warning in option 1, the map-name channel in option 2.
- **Saves:** a save made with one rule, loaded with the other.
- **Balance:** MMH5.5's design assumes its classes.
- **Mixed behaviour:** MMH5.5's scripts (witch huts) still use their own classes.
- **New MMH5.5 versions** change hero files.
- The hero screen will show vanilla class names.

**Questions for the owner:** per session first, or per game only? Skills only, or attribute growth too (vanilla's is part of the class)?

---

## 5. The violet area beyond the map's edge

### What it is

The owner's screenshot (2026-11-01, the co-op exe, zoomed out with our camera mod) shows a hard cut at the map's edge, then a flat **dark violet** area, and a lighter **lavender** triangle at the top right. Measured on the screenshot: dark **(31, 27, 50)**, lavender **(86, 80, 114)**.

**It is a sky dome:**
- The dome is `/_(Model)/SkyDomes/SkyDome_Ramp_SphereCS.xdb` (`data.pak`). Its 6 materials use `Textures/auto-imported_______320/SkyDomes/ScyDome_Rampart_siege/sky1..sky6.tga`, the sky of the Sylvan siege battlefield.
- Measured from the textures themselves:
  - `sky5` and `sky6` are 32×32 and **one flat colour (31, 29, 51)**: the dark area;
  - `sky1` to `sky4` (1024×1024) shade from dark navy at the top to **(86, 80, 115) at their bottom row**: the lavender horizon in the corner.
- MMH5.5's HD texture pack does not replace these files. Its `_virtual` files have other names.

**Who adds the dome:**
- **The light.** The ambient light `Lights/_(AmbientLight)/Arena/Dusk_light_BoatArena01(VicTest).xdb` refers to this dome in **MMH5.5's copy only** (`MMH55-Data.pak`, 2016). The vanilla copy has no sky dome; it has clouds instead.
- **The random map generator** picks one of the lights listed under `GroundTerrainLights` in `RMG/Params/Default.xdb`:
  - **MMH5.5's list** (`MMH55-Index.pak`): `Day_light_AdvMap01(EugeneTest)`, **this dusk light**, `0_Default_AmbientLight`, `AdvMap/C1M1`;
  - **vanilla's list** (`a2p1-data.pak`) has a desert arena light in place of the dusk one.
- **The owner's current random map** (`data\RMGTemp\CurrentMap\map.xdb`) uses the dusk light.
- **The other three lights**, in MMH5.5's copies, use the Haven "virtual" dome `SkyDome_Heven_Virt.xdb`. Other random maps will show a different sky past the edge.
- **Hand-made maps** name their own light.

**What it is not:**
- **Not fog:** every one of the 302 ambient lights in the paks has a black fog colour, starting at distance 1000, far beyond the camera (at most about 210 with our zoom).
- **Not the light's `BackColor`:** it is black everywhere, and it is probably what the screen is cleared with. That is not confirmed; the exe has about 160 calls through the slot used by `IDirect3DDevice9::Clear`, too many to read statically.
- **Not a missing texture:** a missing texture would show bright magenta.

**The hard cut** is the end of the ground mesh. The map already darkens its last row (`<BorderSize>1</BorderSize>` in the map).

### Fix options (presentation only; the original as the default)

| Option | How | Effort | Look |
|---|---|---|---|
| **A. `[camera] edge_sky=none`** | When the game reads one of the random-map lights, our file hook (the one `original_music_xdb.cpp` uses to edit music definitions) serves it with an empty `<SkyDome/>`. Past the edge you then see the cleared screen, black. | **1 session** | Black past the edge, like unexplored land. The darkened border row makes the cut look like a fade into the dark. |
| **B. `edge_sky=dark`** | Serve the light pointing at a dome of our own: a made-up path holding a model that reuses the dome's shape (`SkyDome_Ramp_Sphere-geomCS.xdb`), with 6 materials pointing at small textures we generate in code (near black at the bottom, a dark blue-grey haze at the horizon). | +1 session | Dark and foggy |
| C. "Like a map" | Point the dome's materials at an existing parchment texture from the game's interface (no new art) | +0.5 session | Needs screenshots: a parchment on a sphere may look odd |
| D. A soft fade over the hard cut | Work out where the map's edge falls on screen each frame and draw a gradient between the 3D scene and the game's interface | 3 to 4 sessions | Not recommended (hard to draw under the interface) |

Before building, the first session checks two things:
- whether any battlefield uses the three other lights (the dusk light is referenced only by the generator's list in the paks);
- whether hand-made maps should be covered too (any light a map names could be edited the same way, but battles must keep theirs).

**Desync:** none expected. Lights and sky domes are drawn, not simulated, and are not part of the world checksum; each PC may choose, like the camera zoom. A LAN robot run with only the host using the setting will confirm it.

**What robot screenshots can confirm later** (with the robot lock, when the PC is free):
1. A random map that drew the dusk light, or any map after pointing our hook at its light, with `zoom_out=300` and a far start position: `edge_sky=game` must show the violet of the owner's screenshot (proving the source), and `none` must show black.
2. `[audio] log_data_files=N` (a testing setting in `original_music.cpp`) to log which light file each map opens.
3. A battle: its sky unchanged.
4. A LAN game with two copies, only the host with the setting: world checksums all equal.

---

## Order of work

1. **Item 2 texts** (0.25 session), then the tool check (0.5).
2. **Item 1, the volume curve** (1 session). Meanwhile the owner can use the system volume mixer.
3. **Item 5, `edge_sky=none`** (1 session), then `dark` if wanted.
4. **Item 4**, after the owner's answers: per session first (3 sessions).
5. **Item 3**, only if the owner still wants it after reading the trade-offs.

## Questions for the owner

1. `data\MMH55-Texts-FR.pak` appeared today in the game's `data\` folder: did you put it there? We suggest moving it out (into `bin\open-homm5\lang\fr\` if you want it).
2. Volume: is the "wide" curve (lowest step −40 dB) what you want, or a lower maximum (`music_scale`, `sound_scale`), or both?
3. ToE bonuses: which ones do you miss, and must each player choose their own, or is one host setting enough?
4. Hero skills: per session (simpler) or per game from the lobby? Skills only, or attribute growth too?
5. Map edges: black, dark haze, or parchment?

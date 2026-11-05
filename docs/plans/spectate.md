# Plan: follow a friend's view (spectating)

Status (2026-11-05): **step 1 built and robot-tested** (following a friend's view of the adventure map: `src/mods/spectate*.cpp`); waiting for a hand test in a real game. **Step 2 built and robot-tested** (watching a followed ally's battle live, with a button: below).

The wish (owner, 2026-11-05): "When you're waiting after passing your turn, a way to spectate your friends (using the player list maybe?): spectate their battle at worst, everything they do at best."

## In short

- **Step 1, the adventure map.** Click a friend's name in the player list: your camera follows theirs, gliding, until you click the name again or move the camera yourself. Only allies, and only what your own PC already shows (your own fog of war): nothing of the shared game world changes, so it cannot desync a game.
- **The friend sees it.** An eye next to your name in their player list, while you follow them. Each player can keep their view to themselves (`[coop] share_view=0`).
- **Step 2, battles.** To find out first: does every PC run a friend's battle (then it is "only" showing it), and what the game's hidden `mp_observer_mode_for_player2` does.

## What the game offers (found 2026-11-05)

Addresses in the vanilla 3.1 exe (the same in `MMH55_Utility`).

| What | Where | Notes |
|---|---|---|
| The adventure screen | `NUI::CAdventureScreen`, the overlay's top screen | |
| Its camera | screen `+0xa4`, a `CCamera` (vtable `0x00e44224`) | Slot `0x54` returns its position: 10 floats. |
| Its camera operator | screen `+0x5e4`, a `CCameraOperator` (vtable `0x00e4493c`) | Slot `0x50` (`0x007d4ce0`): set the position `(pos[10], apply)`. Slot `0x5c` (`0x007d6f40`): the object it keeps in view `(object, flag)`. |
| The floor shown | screen `+0xb4` (0 = surface, 1 = underground) | `0x00594440` (`__fastcall`, the screen): switch it, as the game's own button does. |
| The map scripts' `MoveCamera` | `0x005df150` → action `0x005f4f60` → `0x005f1fa0` | Where the offsets above come from: it reads the camera's position, changes the point looked at, and sets it through the operator. |

**A position** (10 floats): `[0]` distance (25 at the start), `[1]` tilt in radians (−0.925 = −53°), `[2]` turn (0.279 = 16°), `[3]` 0, `[4]` field of view (50), `[5]` `[6]` clip planes (1, 1000), `[7]` `[8]` `[9]` the point looked at: x, y in world units (a tile is 2 units), z the ground height.

**The operator keeps a hero in view.** After a hero is selected, the operator's slot `0x5c` object is that hero, and every frame it puts the camera back on it (state `+0x190` = 1). A position set from outside lasted one frame (robot, 2026-11-05, run `20261105-091300`). Releasing the object first (`slot 0x5c (nullptr, 0)`), then setting the position in small steps, moved the camera 30 units and it stayed (run `20261105-091758`). The game takes the object again when the player selects a hero.

## Design (step 1)

**Messages.** The same channel as hero looks and Undo End Turn: a world-checksum message with check number −1, which the game ignores, carrying 32 bits. The top byte is `'W'` (0x57). Three kinds:

| Kind | Sent by | Carries | When |
|---|---|---|---|
| watch | the follower | follower, followed, on/off | on a click, and every 5 s while following (an eye lapses after 12 s without one) |
| view | the followed | player, floor, x and y in tiles (9 bits each) | while an ally follows: when the point moved a tile, and every 2 s |
| status | the followed | player, screen (map, town, battle, hero screen, elsewhere) | while an ally follows: when it changes, and every 2 s |

They go out after this PC's own segment report, as the other mods' messages do, never from the drawing code.

**Who may follow whom.** Allies only (the same team number), both PCs check. A friend with `share_view=0`, an older add-on or none sends nothing: after 3 s the follower reads "does not share their view" after the friend's eye. (A friend who shares sends what it has on screen at once when a new follower comes, then its view whenever it is on the map: a friend followed in the middle of a battle sends no view until the battle ends, so the battle's button must not wait for a view; it did in 0.3.3, the owner's first real game.)

**Following.** Every frame on the adventure map: release the operator's object, then glide toward the friend's point (exponential, about 0.15 s; a jump when farther than 20 tiles), switching the floor when theirs differs. It stops when the player clicks the name again, moves the camera (the camera is more than 3 units from where we put it), or their own turn begins, or when the friend leaves the game.

**On screen.** In the player list: a click on an ally's row starts or stops following (only those rows take clicks: `overlay::BlockClicksIn`); an eye next to the followed friend, and next to whoever follows you. On the map, nothing in the middle (the owner's choice after the first real game, 2026-11-05: "the eye next to the player is enough to toggle"; 0.3.3 had a "Stop following Pierre" button and a line over the turn banner, and the two buttons moved under the mouse when the second came and went): right after the followed friend's eye, a short line when the friend is not on the map ("in a town", "in a menu"...) or, while the friend fights, a small "Watch the battle" button in the row's height (`PlayerListRowPlace`). Without the player list (F9), that button alone over the turn banner. A friend's status says "in a battle" while a battle screen is anywhere under the top screen (a spell book over it), but not on the results box and not while that friend only watches someone else's battle (its own inbox's old battle notice is not that battle). In a battle (the owner's choice: discreet): one small button at the bottom right, just over the creatures' bar, "Stop watching" (then "Back to the map"), with a caption "Watching Pierre" over it; and the battle's top-right buttons (auto combat, options, the white flag) greyed out with discs drawn over them (anchored to the right edge, scaled with the height: checked at 1008x729 and 1584x861).

## Step 2: watching a friend's battle (research, 2026-11-05)

**What the robot showed** (two copies over LAN; the robot can now click: `click|<x> <y>`, and centre the camera on a tile: `panel|spectate look <x> <y>`; on A1M4 the host's hero attacks the level-1 monster at tile 23,66: look, then two clicks at 504,460; `--quick-combat-state 1` for real battles, `--end-turn-every 0` so that nobody ends a turn; Enter ends the placement, A plays the battle by itself):

| Run | What | Result |
|---|---|---|
| `20261105-102409-lan` | The client fights, Enter, then A | Every action went out as a world command to every PC: `CMoveCreaturesOnBattlefield`, `CEndTactic`, `CHeroAttackCmd`, `CMoveAttackCreatureCmd`, `CShootCreatureCmd`, `CCastCombatSpellCmd`. 1864 world checks, all equal: the host ran the same battle, step by step, while it showed its map. |
| `20261105-104702-lan` | The host fights; both log the adventure screen's events | Only the fighting PC gets event `0x2a`, an `NWorld::CCreateCombatScreen` notice (its player in `[5]`, the local player as the handler's third argument); then its `NUI::CCombatArenaScreen` opens. The other PC gets no such event. |

**How the game opens the battle screen:**

- At a battle's start, every PC's world runs `0x008545b0`: for each side's player, an `NWorld::CCreateCombatScreen` notice (ctor `0x00856830`; a quick combat's is `NWorld::CCreateQuickCombatScreen`, `0x00856930`) is posted to that player (`0x00a7d0e0`).
- The adventure screen's frame (`0x0059d5f0`) reads only **its own player's** notices (the controller's `+0x3c`, player slot `0x1bc`: its list of notices) and hands each to its event handler `0x005a5d90` (called at `0x0059db0d`: `__thiscall(screen, notice, player)`; it switches on the notice's slot `0xc`).
- Event `0x2a` (case `0x28` of that switch) builds `NUI::CCreateCombatArenaScreen` (`0x005472b0`), whose slot 7 (`0x005529c0`) makes the `NUI::CCombatArenaScreen` (`0x0054c2f0`, 0x594 bytes) and pushes it.
- `mp_observer_mode_for_player2` is a developer's switch in the transceiver's "is this player one of this PC's?" (slot 11, `0x007c9050`), not a battle viewer.
- `NMP::CCreateCombatScreen` belongs to the multiplayer **duel** mode, not to map battles.
- Battle replays live in the world (`NWorld::CReplayBattleSaver`, `CReplayBattleContainer`); the results box has a "Revoir" (replay) button. A second road: watch a friend's battle as a replay once it ends.

**The plan:** on the follower's PC, when the followed friend's battle starts, take the friend's `CCreateCombatScreen` notice from the friend's own list (the world posted it there on every PC) and hand it to the adventure screen's handler: the game's own battle screen opens on that battle. While it is open: no input reaches it (the overlay's modal input), and every command this PC tries to send is dropped (the battle screen would act as the friend). It closes when the friend's battle ends. To check first: that the friend's list holds the notice on the follower's PC, and what the battle screen sends by itself.

### Built (2026-11-05): "Watch the battle"

- **Opening.** While this PC follows an ally whose own screen is a battle (their status message; the ally's inbox keeps old notices, so only then is its newest `0x2a` notice the battle going on), the banner has a second button, "Watch the battle". A click hands that notice to this PC's adventure screen handler (`0x005a5d90`), with the **ally** as the player: built for this PC's own player (not in the battle), the battle screen crashes at once (robot, `20261105-115543-lan`).
- **Watching.** Our modal panel over the battle screen (the game gets no input); every command this PC sends is dropped (`CMultiPlayerTransceiver` slot 0). The battle screen acts as the ally: it sends its start-up settings (`CSetVisualATBSizeCmd`, `CSetCombatTimeFactor`, `CSetArmyAutocombatOptions`, `CSetCombatCasterMana`) and, under the ally's auto-combat, moves for the ally's creatures (`CMoveAttackCreatureCmd`): all dropped. At the battle's end the results box opens on both PCs at once.
- **Closing.** The game's own "close the top screen" UI command (`0x00ac3590`), queued for the next frame (`0x00838c10`; the queue runs at the frame's start, `0x00838ac0`), once per battle screen on top: what the results box's OK does (`0x0057ff30`). Taking the battle screen off the stack at once (`0x008389e0`) left its message handlers alive: a crash in the next frame's message dispatch (`20261105-114629-lan`). The lock and the filter stay until the battle screens are gone.

| Run | What | Result |
|---|---|---|
| `20261105-111852-lan` | Watching from the start to the end (automatic, before the button) | The battle on both PCs, the same frames; the results box at the same moment on both. |
| `20261105-121427-lan` | Leaving mid-battle | Back on the map, still following ("Robot-host est en combat."); no command sent after; 4877 world checks, all equal. |
| `20261105-122038-lan` | Watching to the end, then "Back to the map" over the results | 2 screens closed, back on the map; 6475 world checks, all equal. |
| `20261105-123026-lan` | The button: joining mid-battle, leaving, joining again, leaving | The battle as it stands when joined; every command of the client during watching dropped; 3473 world checks, all equal. |

**Not tested yet:** Esc (the robot's keys do not reach the overlay's window messages), this PC's own battle starting while it watches, a real game between two PCs over Steam.

## Tests

- `test/spectate_test.cpp`: messages both ways, the glide, the "may follow" rule.
- Robot, two copies (LAN): the host follows the client, the client's camera moves (robot keys), the host's camera log follows; screenshots of the banner and the eye; world checksums equal.

Robot results (2026-11-05, test copy, A1M4, `teams=humans_vs_ai`, two copies over LAN):

| Run | What | Result |
|---|---|---|
| `20261105-093851-lan` | The host follows the client; the client's camera moves right, then down | The client's view arrives 0.1 s after the request; the host's camera goes from its own place (x 43) to the client's (83), then 107, 113, as the client's. Screenshots: the button "Arrêter de suivre Robot-client" and its line on the host, the eye after each name on both. 1865 world checks, all equal. |
| `20261105-094205-lan` | The client with `share_view=0` | The client: "Robot-host follows this PC's view (asked; not shared)"; the host's line: "Robot-client ne partage pas sa vue (...)"; the host's camera stays. |
| `20261105-094651-lan` | The host moves its camera (an arrow key) while following; then asks to follow itself | "stopped following Robot-client (the camera was moved)"; "Robot-host cannot be followed (this PC's own player)". |
| `20261105-094857-lan` | Following through the computer players' turns into the next day | Followed until "stopped following Robot-client (this PC's turn began)"; 29 AI steps; 2854 world checks, all equal. |

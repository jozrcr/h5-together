# Plan: follow a friend's view (spectating)

Status (2026-11-05): **step 1 built and robot-tested** (following a friend's view of the adventure map: `src/mods/spectate*.cpp`); waiting for a hand test in a real game. Step 2 (watching a friend's battle) is research: what we know is below.

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

**Who may follow whom.** Allies only (the same team number), both PCs check. A friend with `share_view=0`, an older add-on or none sends no view: the follower's banner says so after 3 s.

**Following.** Every frame on the adventure map: release the operator's object, then glide toward the friend's point (exponential, about 0.15 s; a jump when farther than 20 tiles), switching the floor when theirs differs. It stops when the player clicks the name again, moves the camera (the camera is more than 3 units from where we put it), or their own turn begins, or when the friend leaves the game.

**On screen.** In the player list: a click on an ally's row starts or stops following (only those rows take clicks: `overlay::BlockClicksIn`); an eye next to the followed friend, and next to whoever follows you. A banner at the top: "Following Pierre's view", with "Pierre is in a battle" and the like.

## Step 2: watching a friend's battle (research, 2026-11-05)

- **Every PC already runs every battle.** The battle classes are in `NWorld`, the world every PC runs in lockstep (`NWorld::CArena*`), and every battle action is a world command sent to every PC: `CMoveAttackCreatureCmd`, `CDefendCreatureCmd`, `CCastCombatSpellCmd`, `CHeroAttackCmd`, `CStepQuickCombatCmd`... So the battle's data is on the follower's PC too.
- **What is missing is the screen.** The fighting PC opens the battle screen (`NUI::CCombatArenaScreen`, made by `NMP::CCreateCombatScreen`); the others do not. Watching = opening it on another PC with no input, and closing it when the battle ends.
- **`mp_observer_mode_for_player2` is not a battle viewer.** It is read once, in `CMultiPlayerTransceiver` slot 11 (`0x007c9050`, "is this player one of this PC's?"): with it, a PC also counts player 2 as its own. A developer's tool to watch a game as player 2.
- **Other leads:** the game's battle replays (`NUI::CCreateReplayBattleScreen`, `CombatReplay_ShowStacksInfo`): a friend's battle could be watched just after it ends; the "ghost" combat results box (`NUI::CGhostCombatResultsBox`).
- **Next:** a battle in a two-copy robot game. The robot cannot click yet, so either teach it to click (a monster next to a start hero), or a short hand test (one PC attacks a monster, the other's log with `--trace-mp`-style tracing shows the battle's commands running there).

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

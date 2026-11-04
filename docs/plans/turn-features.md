# Turn features: undo End Turn, resume after a crash, parallel AI turns

Status: **study** (2026-11-04). Three ideas from the owner's games of 2026-11-03 and 04, in the order we plan to build them. Each one is a setting that is **off by default** (the original game), chosen by the host for the whole game. Addresses are in the vanilla 3.1 exe (the same in `MMH55_Utility(_64).exe`). What players, turn states and "pressed End Turn" are: [players.md](../subsystems/players.md). How the PCs stay in step: [multiplayer-sync.md](../subsystems/multiplayer-sync.md).

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

**Open questions (the first robot test answers them):**

- Does the adventure screen give the controls back when the state goes 1 → 0? (Its End Turn button and the "waiting for the other players" state may be set once, at the click. If not, the undo must also call the screen's own code for "your turn".)
- Does anything else happen at state 1 (`world +0x10c` records the day for state 2 only)?

**Test plan:** robot LAN game, 2 humans + 1 computer, `--simultaneous`; the host presses End Turn, then the robot's undo command; both PCs log the undo at the same segment; the host plays on (a hero moves), presses End Turn again; the day goes on; world checksums equal. Then the refused case: the undo sent just after the client's End Turn.

## 2. Resume after a crash

**The wish** (owner, 2026-11-04): a friend's game crashed; could they come back?

### What happens today (robot run `20261104-080739-lan`, two copies, the client's process killed in the middle of day 8)

- The host's lockstep stops: no segment for ~20 s (the client's reports no longer come).
- Then the host's transceiver **drops the player**: `0x007c8d10` (`CMultiPlayerTransceiver`, just before the pacing slot `0x007c8dd0`) clears the player's bit in the active players mask (`+0x58`), sets its reported segment to -1 and prints `Drop player 1` to the game's console.
- The host gets the game's box "Infos multijoueur: Robot-client a été déconnecté" (OK), and **the game goes on without that player**: from the next turn its letter is `N` (not in the game) in `GiveTurnToNextPlayers`, and our player list no longer shows it.

So a save made **after** the drop no longer has that player in the game: reloading it would not bring the friend back. Today the group can reload the **last autosave** (the start of the day, if autosaves are on), losing part of a day.

### Plan, in two steps

1. **Quick resume** (smaller): when the host drops a human, the host's add-on saves the game **at that moment**, before the player is taken out of the world, as "Reprise - <player>" ("Resume"). A banner says: "<player> left. Saved as Resume: Multiplayer, LAN, Create, Load to play on with them." Everyone (the friend included, after restarting the game) joins the loaded game; the loaded game's waiting room now has "Invite friends..." (0.3.1). To find: when exactly the world takes the player out (a command after the drop, or the next turn), and how to make one save on the host alone (the save code our autosave speed-up already knows: `autosave.cpp`, [save.md](../subsystems/save.md)). The world is the same on every PC, so the host's save is enough.
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

## Order

1. Undo End Turn: a robot test of the state change first (the open questions), then the feature.
2. Quick resume: find when the world removes a dropped player; then the host's save at the drop, the banner, the robot test (kill a copy, reload, rejoin).
3. Parallel AI turns: the experiment above, then a decision with the owner.

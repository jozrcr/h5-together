// Online play's auto-join (online_join.cpp; docs/plans/online-steam.md, step 4):
// after a Steam invite, the game opens Multiplayer > LAN by itself, waits for the
// invited host's game in its list, highlights it and presses Join, as the player
// would. Driven by the online UI (online_ui.cpp) on the game's main thread, once
// per frame. It acts only on the main menu and on the LAN screen.
#pragma once

#include <stdint.h>

#include <string>

#include "online_ui_logic.h"

namespace mods {
namespace autojoin {

// From hooks::InstallAll (with [online] mode=steam or loopback): the hook that
// learns the main menu's Multiplayer page (its vftable slot 0, called when the
// menu is built), whose "lan" command opens the LAN screen. False if the game's
// code is not as expected (then no auto-join, only the banner).
bool Install();

// A new invite: join the game of `host` (its SteamID; 0 while the lobby's data has
// not come, see Update). `lobby` names the invite; `name` is the friend's (for the
// LAN screen's line). Replaces an auto-join still waiting. No lobby and no host: the
// robot's test, the first host this PC hears of.
void Start(uint64_t lobby, uint64_t host, const char *name, const char *why);
// The invite's host became known (the lobby's data came).
void Update(uint64_t lobby, uint64_t host);
// Each frame: `top` is the RTTI name of the game's screen on top; `boxed`: a game
// message box or dialog is open over it (a common message box, or the LAN screen's
// name box NUI::CSimpleEditBox, a screen of its own): then the auto-join waits.
void Frame(const char *top, bool boxed);

// What the LAN screen says about it (no row: 20 s; failed: a message box, or no
// waiting room in 30 s), and the inviting friend's name.
online::JoinState State(std::string *name);

// Robot research: logs each row of the LAN list, its game data's class, and where a
// fake address (198.18.0.0/15) sits in it.
void LogGames();

// The game's message boxes (NUI::CCommonMsgBox and the boxes made from it, such as the
// LAN screen's "Timeout..." error): they are not screens of the game's screen list, so
// TopScreen does not see them. From hooks::InstallAll: a hook on the box's constructor
// (its call sites, 0x0067a730) remembers each box. False if the code is not as expected.
bool InstallBoxWatch();
// Is one of them shown now? (A box lives from its construction until its destructor
// changes its vftable; it is shown while its window fields say so: some boxes are made
// and never shown.) Once per frame, on the game's main thread.
bool GameBoxOpen();

}  // namespace autojoin
}  // namespace mods

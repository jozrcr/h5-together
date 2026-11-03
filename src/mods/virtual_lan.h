// Mod: the virtual LAN, the core of online play over Steam
// (docs/plans/online-steam.md; see virtual_lan.cpp).
#pragma once

#include <stdint.h>

#include <vector>

#include "online_ui_logic.h"

namespace mods {

// With the settings of h5-together.ini: [online] mode=off (the default: nothing
// changes) | steam (the game's LAN play over Steam, between Steam friends) |
// loopback (robot tests: two copies of the game on one PC). Puts the hooks on the
// exe's sendto, recvfrom and gethostbyname imports. Safe to call twice (the
// second call does nothing): hooks.cpp calls it before the robot's network log is
// installed, so that log sees the game's packets as the game sends them, and
// again from InstallAll for an exe that could not be recognized when the DLL was
// loaded.
void InstallVirtualLan();

// At the game's first DirectInput call (InstallAll), after InstallVirtualLan:
// mode=steam starts Steam here, on the game's thread. If Steam cannot start (no
// steam_api.dll, Steam not running), the hooks stand aside: the game's own LAN.
void StartVirtualLan();

// True while the virtual LAN carries the game's packets (with mode=steam: from the
// DLL's load until Steam failed to start, if it does).
bool VirtualLanOn();

// --- for the online UI (online_ui.cpp; step 4). Any thread; they take the core's lock. ---
// What Steam play looks like now: state kOff unless [online] mode=steam; else Steam's
// state, and with [online] ui_test=1 made-up friends added (online_ui_logic.h).
void OnlineStatus(online::Status *out);
// The invite picker is open (its list is kept fresh) or closed.
void OnlineWantFriends(bool on);
void OnlineFriends(std::vector<online::Friend> *out);
// Steam's own invite dialog (its overlay) for our lobby; false: no lobby, or ui_test.
bool OnlineInviteDialog();
// Invites one friend (ui_test: a made-up friend, nothing sent); false: no lobby.
bool OnlineInvite(uint64_t friend_id);
// How many times Steam's overlay opened so far (to see that the dialog came).
int OnlineOverlayOpened();
// ui_test: a made-up invite, as if from Steam (the robot's "panel|online test_invite").
void OnlineTestInvite();
// The auto-join after an invite (online_ui.cpp): the fake address a host's game is
// at for our game (the one the LAN list shows it from); false while not known. Also
// with mode=loopback (the robot's two copies).
bool OnlineHostAddress(uint64_t host, uint32_t *address);
// The first known host that is not us (0: none). For the robot's "test_autojoin".
uint64_t OnlineFirstHost();

}  // namespace mods

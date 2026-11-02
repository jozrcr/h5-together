// Mod: the virtual LAN, the core of online play over Steam
// (docs/plans/online-steam.md; see virtual_lan.cpp).
#pragma once

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

}  // namespace mods

// Mod: the online UI (step 4 of docs/plans/online-steam.md; see online_ui.cpp):
// Steam's lines on the game's LAN screen, "Invite friends..." in the waiting
// room, and what happens when a Steam invite comes. Only with [online] mode=steam.
#pragma once

namespace mods {

// From the DLL's load (hooks::InstallEarly): with mode=steam, the overlay's
// Direct3D import goes in before the game makes its device.
void InstallOnlineUiEarly();

// From hooks::InstallAll, after StartVirtualLan: the overlay client, and the
// handler of Steam's invites (the pop-up and the taskbar flash).
void ApplyOnlineUiFromSettings();

}  // namespace mods

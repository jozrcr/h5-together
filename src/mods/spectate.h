// Mod: follow a friend's view of the adventure map (see spectate.cpp and
// docs/plans/spectate.md).
#pragma once

namespace mods {

// From InstallAll.
void ApplySpectateFromSettings();
// The settings menu changed [coop] share_view (at once).
void ReloadSpectateSettings();

// For the player list (the game's main thread). Players are numbers 1..8.
int SpectateFollowing();                          // the player this PC follows, 0 = none
bool SpectateCanFollow(int player);               // an ally's row that takes a click
void SpectateToggle(int player, const char *how); // start following, or stop when it is the one followed
bool SpectateFollowsMe(int player);               // `player` follows this PC's view now (the eye)
void SpectateNoteName(int player, const char *name);  // the name the list shows (UTF-8)

}  // namespace mods

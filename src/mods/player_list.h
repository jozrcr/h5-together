// Mod: a list of the game's players drawn over the adventure map (see
// player_list.cpp). Display only: it reads the game, never changes it.
#pragma once

namespace mods {

// From hooks::InstallEarly (DLL load, a build recognized then): only the
// Direct3D import, so the game's first device is seen too. Reads the settings.
void InstallPlayerListEarly();

// From hooks::InstallAll: the rest, with [ui] player_list=1 (the default).
void ApplyPlayerListFromSettings();

// From the settings menu: its key and its place, at once.
void ReloadPlayerListSettings();

// Where a player's row was drawn on the last frame (pixels of the back buffer): `right` the
// end of the row (after its eye), `top` and `bottom` its pill's. False when the list is not
// shown (F9, not the adventure map) or the player has no row. (spectate.cpp draws beside it.)
bool PlayerListRowPlace(int player, float *right, float *top, float *bottom);

}  // namespace mods

// Mod: quicker autosaves in multiplayer games (see autosave.cpp).
#pragma once

namespace mods {

// With the settings of h5-together.ini, section [save]: trace_autosave=1,
// compression=1 (-1 = the game's level 6), autosave_every=1, fast_save_list=1
// (the defaults). Only after DetectGameBuild() said the build is hookable.
void ApplyAutosaveFromSettings();
// From the settings menu: compression and autosave_every again, at once.
void ReloadAutosaveSettings();
// Is this thread inside the game's look through the saves folder (one save's info read
// by 0x007e67b0, from the folder scans: an autosave's name lookups, the load screens'
// lists)? Every PC, whatever the settings. rmg_factions.cpp uses it: a listed save
// never rebuilds a map.
bool SaveListingOnStack();

}  // namespace mods

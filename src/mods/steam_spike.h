// Mod (test only): the Steam spike of docs/plans/online-steam.md.
#pragma once

namespace mods {

// [online] spike=1|host|join: start Steam inside the game under the game's own
// app id and log what Steam allows (lobbies, relays, messages). Off by default.
void ApplySteamSpikeFromSettings();

}  // namespace mods

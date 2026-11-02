// Mod: teams on maps without teams of their own, such as random maps (see rmg_teams.cpp).
#pragma once

namespace mods {

// With the settings of h5-together.ini: [rmg] teams= (empty: off; humans_vs_ai,
// pairs, or a list like 1,1,2,2), trace_teams=1. Only after DetectGameBuild()
// said the build is hookable.
void ApplyRmgTeamsFromSettings();

}  // namespace mods

// Mod: game rules chosen by the host for every player (see host_rules.cpp).
#pragma once

namespace mods {

// The panel's Direct3D import, before the game makes its device (as the
// other overlay panels).
void InstallHostRulesEarly();
// With the settings of open-homm5.ini [rules] (host_rules.cpp). Only after
// DetectGameBuild() said the build is hookable.
void ApplyHostRulesFromSettings();

}  // namespace mods

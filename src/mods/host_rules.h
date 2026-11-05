// Mod: game rules chosen by the host for every player (see host_rules.cpp).
#pragma once

namespace mods {

// The panel's Direct3D import, before the game makes its device (as the
// other overlay panels).
void InstallHostRulesEarly();
// With the settings of h5-together.ini [rules] (host_rules.cpp). Only after
// DetectGameBuild() said the build is hookable.
void ApplyHostRulesFromSettings();

// The computer bonus this PC would give as a host (MMH5.5's H55_AICheatMode, 0..6): its
// [rules] ai_cheat choice, else its own H55-Settings.lua's value, as the F2 panel shows it;
// -1 when the rules are off or that file was not read yet. (coop_features.cpp: ai_bonus.)
int HostComputerBonus();

}  // namespace mods

// Mod: a player who has lost every town keeps playing while a hero lives ([coop]
// keep_without_towns, the host's choice, off: the game's rule). keep_without_towns.cpp.
#pragma once

namespace mods {

// Hooks the start of a game's objectives (every PC, whatever its setting).
void InstallKeepWithoutTowns();

// This PC's [coop] keep_without_towns (as host, its choice for the next game).
bool KeepWithoutTownsWanted();

// For the game that starts next (host_rules.cpp, at each PC's start of a multiplayer game,
// with the host's choice from the start message; off for a single-player game).
void SetKeepWithoutTowns(bool on, const char *why);

}  // namespace mods

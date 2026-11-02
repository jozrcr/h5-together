// All hooks of the DLL. Only call InstallAll() after DetectGameBuild() said the
// build is hookable.
#pragma once

namespace hooks {

void InstallAll();

// For a build we do not know (Steam's 2025 H5_Game.exe, for example): no hooks,
// only what never touches the game's code. Today that is the Steam test
// ([online] spike, off by default), so it can run on a PC with the plain game.
void InstallUnknownBuild();

// When the DLL is loaded, before the game runs: the few hooks that must be in
// place before the game reads its data. Checks the build itself.
void InstallEarly();

}  // namespace hooks

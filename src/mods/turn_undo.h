// Mod: undo End Turn while the other humans still play (see turn_undo.cpp,
// docs/plans/turn-features.md, part 1). Experiment stage: only the robot uses it.
#pragma once

namespace mods {

// Installs the hooks in robot runs only (the experiment). Only after
// DetectGameBuild() said the build is hookable; after ApplyHeroSkinsFromSettings
// (it stacks on hero looks' segment hook).
void ApplyTurnUndoFromSettings();

}  // namespace mods

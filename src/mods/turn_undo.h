// Mod: undo End Turn while the other humans still play (see turn_undo.cpp,
// docs/plans/turn-features.md, part 1). [coop] undo_end_turn: the host's choice, off
// by default; every PC with this add-on takes part whatever its own setting.
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after
// ApplyHeroSkinsFromSettings (it stacks on hero looks' hooks).
void ApplyTurnUndoFromSettings();

}  // namespace mods

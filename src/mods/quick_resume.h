// Mod: a save at the moment a player's game drops out of a multiplayer game, so the
// group can load it and play on together (see quick_resume.cpp,
// docs/plans/turn-features.md, part 2). [coop] resume_save (the host's; on by default).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after
// ApplyAutosaveFromSettings (it stacks on the autosave's hook of the save command).
void ApplyQuickResumeFromSettings();

}  // namespace mods

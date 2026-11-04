// Mod: a save at the moment a player's game drops out of a multiplayer game, so the
// group can load it and play on together (see quick_resume.cpp,
// docs/plans/turn-features.md, part 2). [coop] resume_save (the host's; on by default).
#pragma once

#include <stddef.h>

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after
// ApplyAutosaveFromSettings (it stacks on the autosave's hook of the save command).
void ApplyQuickResumeFromSettings();

// The file of the last resume save this PC made (kept in h5-together-resume.txt, so a
// host that restarts its game still offers it); false: none. Any thread.
bool ResumeOfferFile(char *out, size_t size);
// This PC's saves folder (its profile's, with a final backslash), once the game read the
// profile's settings; false before.
bool ResumeSavesFolder(char *out, size_t size);

}  // namespace mods

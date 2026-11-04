// Mod (EXPERIMENT): the computer players take their turns at the same time (see
// parallel_ai.cpp, docs/plans/turn-features.md, part 3). [ai] parallel_turns=1, a
// hidden setting, off by default; every PC of a game must have the same.
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after ApplyAiPacingFromSettings.
void ApplyParallelAiFromSettings();

}  // namespace mods

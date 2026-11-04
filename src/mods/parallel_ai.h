// Mod: the computer players take their turns at the same time in multiplayer games (see
// parallel_ai.cpp, docs/plans/turn-features.md, part 3). [ai] parallel_turns: the host's
// choice, off by default; on only when every PC of the game knows it (coop_features.cpp).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after ApplyAiPacingFromSettings
// (and after or before the co-op features: it only asks them).
void ApplyParallelAiFromSettings();

}  // namespace mods

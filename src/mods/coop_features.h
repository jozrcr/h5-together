// Mod: which of our world-changing features a multiplayer game uses, agreed between
// the PCs at its start (see coop_features.cpp; the pure part is coop_features_logic.h).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after the turn undo (it stacks
// on the same hooks).
void ApplyCoopFeaturesFromSettings();

// Is `feature` (features::Feature) on in the multiplayer game being played? Decided at the
// start of segment features::kDecideAt, the same on every PC; false before that, and in a
// game where some PC does not know it or the host does not want it. It stays as decided
// after the game: callers act only inside InMultiplayerSegment().
bool CoopFeatureOn(int feature);

// True while the multiplayer segment runner runs (NWorld::CMultiPlayerTransceiver,
// 0x007c98e0): everything that changes a multiplayer game's world happens there, on
// every PC alike. False in single player.
bool InMultiplayerSegment();

// A number that changes with each multiplayer game's decision: to forget the last game.
unsigned CoopFeaturesGame();

}  // namespace mods

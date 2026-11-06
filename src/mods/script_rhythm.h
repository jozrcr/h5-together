// Mod: MMH5.5's scripts at their own pace in co-op games with our faster segments
// ([coop] script_rhythm, the host's, on by default; see script_rhythm.cpp).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after ApplyCoopFeaturesFromSettings.
void ApplyScriptRhythmFromSettings();

// Runs one line of Lua in the game's scripts (MMH5.5's), with the game's own "DoString"
// (the scripts' object slot 0, 0x00cab4a0), on the world of the last multiplayer update.
// Only where every PC runs it at the same point of the game (coop_features.cpp's
// decision). False when it could not run, or the scripts answered an error.
bool RunGameScript(const char *code);

}  // namespace mods

// Mod: MMH5.5's scripts at their own pace in co-op games with our faster segments
// ([coop] script_rhythm, the host's, on by default; see script_rhythm.cpp).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable; after ApplyCoopFeaturesFromSettings.
void ApplyScriptRhythmFromSettings();

}  // namespace mods

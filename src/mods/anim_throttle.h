// Mod: lighter animations when the map is slow ([camera] anim_saver=1, off by default;
// see anim_throttle.cpp).
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable.
void ApplyAnimThrottleFromSettings();

}  // namespace mods

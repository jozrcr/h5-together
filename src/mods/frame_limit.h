// Mod: a frame rate limit and the vertical sync ([video] fps_limit, vsync; see
// frame_limit.cpp).
#pragma once

namespace mods {

// Before the game makes its Direct3D device (from InstallAll).
void ApplyFrameLimitFromSettings();
// The settings menu changed fps_limit (at once).
void ReloadFrameLimitSettings();
// The limit in force (frames a second), 0 = none (for the lighter animations' thresholds).
int FrameLimit();

}  // namespace mods

// Mod: let the camera zoom out farther, on the adventure map and in battles (see camera.cpp).
#pragma once

namespace mods {

// With the settings of h5-together.ini, section [camera]: zoom_out=100 (the
// adventure map) and combat_zoom_out=100 (battles), in % of the game's own
// farthest zoom; 0, empty or 100 = the game's own (the default), and then
// nothing is installed. Only after DetectGameBuild() said the build is hookable.
void ApplyCameraFromSettings();

// From the settings menu: zoom_out, combat_zoom_out and start_zoom again, used from
// the next map or battle; the hooks go in if a limit now needs them.
void ReloadCameraSettings();
// The hooks the limits need (map: the adventure map's, battles: the battles').
void InstallCameraHooks(bool map, bool battles);

}  // namespace mods

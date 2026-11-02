// Mod: the mouse wheel's zoom under Wine / Proton, and a trace of the wheel's
// input (see wheel.cpp).
#pragma once

namespace mods {

// With the settings of h5-together.ini: [camera] wheel_fix=auto (under Wine only,
// once a check showed it is needed; 1 = from the first wheel event; 0 = off) and
// [debug] trace_wheel=0. Installs nothing on Windows unless trace_wheel=1. Only
// after DetectGameBuild() said the build is hookable, before the game makes its
// DirectInput object.
void ApplyWheelFromSettings();

}  // namespace mods

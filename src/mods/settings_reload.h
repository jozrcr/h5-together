// The settings menu's "at once" and "from the next game" settings: after
// Accept, the mod that uses a changed setting reads it again from
// h5-together.ini (settings_reload.cpp). Only the settings whose table entry
// says kLive or kNextGame come here; each mod's Reload...() changes plain
// values it reads when it uses them (a key, a place, a level), on the main
// thread, so nothing changes halfway through what the game is doing.
#pragma once

namespace mods {
namespace settings {

void Reload(const char *section, const char *key);

}  // namespace settings
}  // namespace mods

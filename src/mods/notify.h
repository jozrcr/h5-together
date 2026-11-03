// Mod: a "your turn" notification while the game is in the background (see notify.cpp).
#pragma once

#include <string>

namespace mods {

// With the settings of h5-together.ini, section [notify]: turn=1, sound=1,
// popup=1 (the defaults), trace=0. Only after DetectGameBuild() said the build
// is hookable.
void ApplyNotifyFromSettings();

// From the settings menu: sound and popup again, at once (the main thread).
void ReloadNotifySettings();

// Online play (online_ui.cpp): a Steam invite came. Any thread. When the game is
// not the window in front: the taskbar button flashes, the sound and the pop-up
// as [notify] sound= and popup= say (also with turn=0). In front: nothing here
// (the add-on draws its own banner in the game).
void NotifyInvite(const std::string &title, const std::string &text);
// The pop-up goes when the game is back in front (called every frame by the online UI).
void NotifyGameInFront();

}  // namespace mods

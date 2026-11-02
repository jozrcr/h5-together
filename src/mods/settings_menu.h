// Mod: the settings menu (docs/plans/community-release.md, part 2): an
// "H5 Together" category in the game's own Options screen (Options, H5
// Together), with the game's own rows, check boxes, lists, sliders, headers,
// tooltips and Accept / Default / Back buttons. See settings_menu.cpp.
#pragma once

namespace mods {

// From hooks::InstallEarly: the game's file system hook that adds our category
// to the Options screen's list (it must be in before the game reads that list).
void InstallSettingsMenuEarly();

// From hooks::InstallAll: Accept and Default write h5-together.ini; F3 opens the
// Options screen on our page; the line after Accept; the first-start question
// about the update check; the H5 Together version in the main menu.
void ApplySettingsMenuFromSettings();

}  // namespace mods

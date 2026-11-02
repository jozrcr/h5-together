// Mod: choose your hero's look (see hero_skins.cpp, docs/plans/hero-skins.md).
#pragma once

namespace mods {

// With the settings of h5-together.ini [skins] (skins=1, show_others=1,
// trace=0) and the choices of h5-together-skins.ini [skins] (Godric=Freyda).
// Only after DetectGameBuild() said the build is hookable.
void ApplyHeroSkinsFromSettings();

}  // namespace mods

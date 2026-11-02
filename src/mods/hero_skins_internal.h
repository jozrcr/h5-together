// Hero looks: what the panel (hero_skins_panel.cpp) asks of the engine
// (hero_skins.cpp). Game thread only (the overlay draws in it).
#pragma once

#include <string>
#include <vector>

namespace mods {
namespace skins {

// One look the panel offers: a group of heroes of the faction that show the
// same map and battle figures.
struct Look {
	int hero = -1;             // the hero whose name the choice is saved under (the group's best name)
	std::vector<int> members;  // every hero of the group (catalog positions)
	bool own = false;          // the panel's hero is in it: its own look
	std::string portrait;      // the .dds of the hero's portrait (data path), or ""
	std::string name_file;     // the text file of its name, or ""
	std::string internal;      // its InternalName
};

struct PanelData {
	int hero = -1;            // the hero the panel is for (catalog position)
	std::string internal;     // its InternalName
	std::string name_file;
	std::string portrait;
	int chosen = -1;          // the look chosen for it now (catalog position), -1 = its own
	std::vector<Look> looks;  // its own look first
	bool multiplayer = false;
	bool decided = false;     // multiplayer: the looks' rule of segment 20 has been applied
	bool agreed = false;      // multiplayer: looks on in this game
};

// The hero the panel is for: this PC's player's selected hero (on the map, and
// behind the hero screen it opens). False when there is none, or the looks are off.
bool PanelHero(PanelData *out);

// Saves a choice to h5-together-skins.ini and makes it count: at once in single
// player; in multiplayer it is sent and counts on every PC at the same segment
// (a few seconds), if this game's looks are on. Returns a line for the log.
enum Applied { kAppliedNow, kAppliedSoon, kSavedForNextGames, kNotApplied };
Applied ApplyLook(int hero, int look, std::string *note);

// The portrait (.dds) of the look chosen for the selected hero (its own if none),
// for the hero screen's button; false when there is no selected hero.
bool SelectedLookPortrait(std::string *dds);

// After these heroes' looks changed (catalog positions): at the next adventure
// frame, a hero screen on top showing one of them closes and opens again (its
// "next hero" with the same hero), so its 3D figure shows the look. Logs the model.
void RefreshHeroScreen(const std::vector<int> &heroes, const char *why);

// Robot mode only (the panel's robot commands): "hero_screen" (the adventure
// screen's "hero_attributes"), "town <n>" (its "input_select_town_<n+1>", then
// "adv_show_town"), "town_hero_screen" (the town screen's "hero_attributes").
// Done at the next adventure frame, by the game's own message handlers.
void RobotScreenCommand(const char *text);

// The catalog's InternalName of a position, and the text file of its name ("" if none).
std::string InternalName(int hero);
std::string NameFile(int hero);

}  // namespace skins

// The panel (hero_skins_panel.cpp): adds the overlay client. The key comes
// from h5-together.ini [skins] key (F7 by default).
void InstallHeroSkinsPanel();
void InstallHeroSkinsPanelEarly();

}  // namespace mods

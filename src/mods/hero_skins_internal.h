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

// Saves a choice to open-homm5-skins.ini and makes it count: at once in single
// player; in multiplayer it is sent and counts on every PC at the same segment
// (a few seconds), if this game's looks are on. Returns a line for the log.
enum Applied { kAppliedNow, kAppliedSoon, kSavedForNextGames, kNotApplied };
Applied ApplyLook(int hero, int look, std::string *note);

// The portrait (.dds) of the look chosen for the selected hero (its own if none),
// for the hero screen's button; false when there is no selected hero.
bool SelectedLookPortrait(std::string *dds);

// Asks the hero screen, if it is the screen on top at the next adventure frame,
// to refresh as when it shows another hero ("shown" +0x15a = 0, "needs refresh"
// +0x7d8 = 1; its update 0x006caee0 -> 0x006ca4e0), so its 3D figure is built
// again with the hero's look. Logs the model it then shows.
void RefreshHeroScreen(const char *why);

// The catalog's InternalName of a position, and the text file of its name ("" if none).
std::string InternalName(int hero);
std::string NameFile(int hero);

}  // namespace skins

// The panel (hero_skins_panel.cpp): adds the overlay client. The key comes
// from open-homm5.ini [skins] key (F7 by default).
void InstallHeroSkinsPanel();
void InstallHeroSkinsPanelEarly();

}  // namespace mods

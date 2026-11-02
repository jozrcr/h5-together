// Mod: the main menu's scene and its music, chosen by each player (see menu_scene.cpp
// and docs/plans/menu-scenes.md).
#pragma once
#include <functional>
#include <string>

namespace mods {

// With the setting of h5-together.ini [menu] scene=default|toe|homm5|random. Only after
// DetectGameBuild() said the build is hookable. With scene=default (or no [menu] section)
// nothing is installed: the menu is exactly what the installed game shows.
//
// InstallMenuSceneEarly() is called when the DLL is loaded: the game reads its root table
// of music (UI/UIGameRoot.(UIGameRoot).xdb) during its splash screen, before the other hooks
// go in. It only puts our function in the game's file system table; the paks are read when
// the game first opens one of the menu's files. ApplyMenuSceneFromSettings() then makes the
// pick (and the log line) if nobody has yet, and installs late if the early call could not.
void InstallMenuSceneEarly();
void ApplyMenuSceneFromSettings();

namespace menuscene {

// What the setting says.
enum class Choice {
	Default,  // "default": nothing changes
	Toe,      // "toe": the Tribes of the East scene and its theme
	Homm5,    // "homm5": the Heroes V (2006) scene and its theme
	Random,   // "random": one of the scenes this PC can show, drawn once per launch
};

// The scene to show.
enum class Scene {
	None,   // the game's own menu (nothing installed)
	Toe,
	Homm5,
};

// Any case; the empty text and anything else ("hof", a typo) is Default, with *known = false.
Choice ParseChoice(const std::string &text, bool *known);
const char *ChoiceName(Choice choice);
const char *SceneName(Scene scene);

// Which scenes this PC can show, and for the others, why not (a sentence for the log).
struct Availability {
	bool toe = false;
	bool homm5 = false;
	std::string toe_why;
	std::string homm5_why;
};

// "homm5, toe" (alphabetical, like the log's lists), or "none".
std::string AvailableList(const Availability &available);

// The scene to show for this setting. `seed` draws for Random (any 32 bits, a different one
// each launch); the draw is uniform among the scenes available. *why is one line for the log.
// An explicit scene this PC cannot show, and Random with none to show, give None.
Scene Pick(Choice choice, const Availability &available, unsigned seed, std::string *why);

// "/Arenas\Interface/MainMenu/MainMenu.(ArenaDesc).xdb#xpointer(/ArenaDesc)" ->
// "arenas/interface/mainmenu/mainmenu.(arenadesc).xdb": how the game compares paths.
std::string NormalizePath(const char *begin, const char *end);

// The scene an arena description (NDb::SArenaDesc, an .xdb) draws, from the name of the
// scene file it was exported from (its <ExpScene href="..."/>): Toe, Homm5, or None for any
// other (another mod's, or not an arena description).
Scene SceneOfArena(const std::string &arena_xdb);

// The first file that the arena description refers to (objects, effects, light, camera
// animation; not the .mb source) and that `exists` (a normalized path, as above) does not
// find. Empty when all are there. Relative names are in the arena's own folder.
std::string FirstMissing(const std::string &arena_xdb, const std::function<bool(const std::string &)> &exists);

// A music definition (Sounds/_(Music)/.../x.xdb): its track, "..\Music\a2\Main_Menu.ogg", or
// empty when it names none.
std::string TrackOf(const std::string &music_definition);

// A music definition as we serve it under our own name: the same text without the
// ObjectRecordID attribute of its first tag (a definition of the game's list has none).
std::string ServedDefinition(const std::string &music_definition);

// The root table of the game (UI/UIGameRoot.(UIGameRoot).xdb) with the main menu's music
// (the item whose <ID> is MAIN_MENU_MUSIC_ID) pointing at `new_path` instead of the
// definition it names; "#xpointer(/Music)" stays. False (nothing in *out) when that item is
// not there in the form we know. *old_path is the path it had.
bool WithMenuMusic(const std::string &root_xdb, const std::string &new_path, std::string *out, std::string *old_path);

}  // namespace menuscene
}  // namespace mods

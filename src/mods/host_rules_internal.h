// Host rules: what the panel (host_rules_panel.cpp) needs from the mod
// (host_rules.cpp).
#pragma once

#include <string>
#include <vector>

#include "host_rules_logic.h"

namespace mods {
namespace rules {

// What the panel shows, taken under the mod's lock.
struct View {
	enum Role { kNoGame, kSetup, kHost, kClient, kInGame } role = kNoGame;  // kSetup: the host's setup screen
	bool file_known = false;  // this PC's H55-Settings.lua was read
	Values file;              // its values (the host's defaults)
	Values chosen;            // host: its choices (empty = the file's)
	// client: what the host sent in the lobby; in game: the rules of this game
	bool shown_known = false;
	Values shown;
	bool will_apply = false;  // host and client: every player has open-homm5's rules (else the files' rules)
	int map_script = -1;      // host and client: the lobby's map has a map script (1), none (0: MMH5.5's scripts will not run), unknown (-1)
	std::vector<std::string> missing;  // host: the players without it (names)
	std::vector<std::string> differ;   // host: players whose MMH5.5 files differ ("Name: settings"...)
	bool this_differs = false;         // client: the host said this PC's files differ from its own
	bool in_game_rules = false;        // in game: the host's rules are used (else the files')
	// host: every PC can take the start rules (game mode, ToE bonuses), else why not;
	// client: the host said they can (false: MMH5.5's way for them)
	bool start_ok = true;
	std::string start_why;
	// host: every PC can take class_skills (the same class table, which can be
	// edited), else why not; client: the host said they can
	bool class_ok = true;
	std::string class_why;
	bool class_from_save = false;  // in game: this game's class skills came from a loaded save
};
// The view for this role (the panel decides it from the screen on top and LobbyRole()).
View GetView(View::Role role);

// Host: new choices (empty = the file's). Written to open-homm5.ini [rules] and
// sent to the players in the lobby.
void SetChoices(const Values &chosen);

// The panel's settings, read by the mod: its key (F2 by default) and the
// lobby button.
int PanelKey();
bool PanelButton();
// From ApplyHostRulesFromSettings: the overlay client (host_rules_panel.cpp).
bool InstallPanel();

// Is the waiting room on top, and this PC's role there (the logic seen in the
// last second)? kNoGame when not in a waiting room.
View::Role LobbyRole();

// --- the lobby's bonus tooltips (host_rules_bonus.cpp) ---
// What the waiting room's bonus tooltips should say. Not ours: the game's own
// (MMH5.5's texts: its modes). Ours: ToE's bonuses (toe), or that the choice
// gives nothing (MMH5.5's start artifact for everyone), and the game mode.
struct BonusTips {
	bool ours = false;
	bool toe = false;
	int mode = 0;  // ModeUsed(): 1 Default, 2 Savage, 3 Nightmare, 4 Random
};
// For these rule values (when every PC takes them).
BonusTips TipsFor(const Values &values);
// From the lobby logic's update (host or client, the main thread): each row's
// four bonus tooltips (its own copies of the texts) set to `tips`, and the shown
// one refreshed; each row's bonus pictures set to ToE's when tips.toe (when
// they were served, below), else to the game's (MMH5.5's); logs each row's
// bonus when it changes.
void SyncBonusTips(void *logic, const BonusTips &tips);

// --- ToE's bonus pictures for the lobby (host_rules_icons.cpp) ---
// From host_rules.cpp's wrap of the game's "open file" (any thread), for a .xdb
// or .dds path: the stream to give the game. `opened` is what the game opened
// (deleted when another one is given); our files and the bonus icon's texture
// list (with our four pictures added) are served, everything else is `opened`.
void *ServeBonusPicture(const char *begin, const char *end, void *opened);
// Was the icon's texture list served with ToE's pictures (their names are in it)?
bool ToePicturesServed();

// --- the class skill lock (host_rules_classes.cpp) ---
// This PC's class table, read from memory the first time it is asked for (the
// game loads it at start-up), with its structure checked.
struct ClassInfo {
	bool read = false;      // looked at
	bool editable = false;  // as expected: class_skills can be applied here
	uint32_t hash = 0;      // ClassTableHash (0 when not editable)
	std::string why;        // why not editable
	int classes = 0;        // the classes with a list
	int skills = 0;         // the skills some class is offered
};
ClassInfo ClassTableInfo();
// This game's class skills: mode "0" = MMH5.5's lock (the table as loaded).
struct ClassState {
	std::string mode = kClassLocked;
	std::string chance;      // the chance used when unlocked
	bool from_save = false;  // set by a loaded save
};
ClassState ClassSkillsNow();
// The class table for this game (main thread): "0" or "" = MMH5.5's lists back;
// "1" = unlocked at `chance` ("" = vanilla's 2). Logged with `why`. Nothing
// changes when the table cannot be edited (logged).
void SetClassSkills(const std::string &mode, const std::string &chance, bool from_save, const char *why);
// A save's world was read in the last `within_ms` (it set the class skills itself).
bool WorldReadRecently(unsigned long within_ms);
void ForgetWorldRead();
// From ApplyHostRulesFromSettings: the world's serializer (the rule in saves) and,
// with `trace`, the log of every new skill a level-up offers. False when saves
// cannot keep the rule.
bool InstallClassSkills(bool trace);

}  // namespace rules
}  // namespace mods

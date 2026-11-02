// Host rules: what the panel (host_rules_panel.cpp) needs from the mod
// (host_rules.cpp).
#pragma once

#include <string>
#include <vector>

#include "host_rules_logic.h"

namespace mods {
namespace rules {

// Why the start rules or class_skills cannot be used: the panel says it in the
// player's language (the rules.why.* texts, in this order); the technical
// detail is in the log.
struct Why {
	enum Code {
		kNone = -1,
		kStartNotRead,   // this PC's start script was not read yet
		kStartMissing,   // this PC has no start script
		kStartThisPc,    // this PC's start script is not one our edits fit
		kStartHost,      // the host's start script is not one our edits fit
		kStartDiffers,   // player's start script is not the host's
		kStartWaiting,   // player has not sent its start script's hash yet
		kClassThisPc,    // this PC's class table cannot be edited
		kClassHost,      // the host's class table cannot be edited
		kClassWaiting,   // player has not sent its class table's hash yet
		kClassCannot,    // player's class table cannot be edited
		kClassDiffers,   // player's class table is not the host's
		kCount
	} code = kNone;
	std::string player;
};

// A player whose MMH5.5 files differ from the host's.
struct Differ {
	std::string player;
	int what;  // 0 H55-Settings.lua besides the rules, 1 H55-Settings.lua, 2 MMH5.5's stats files
};

// What the panel shows, taken under the mod's lock.
struct View {
	enum Role { kNoGame, kSetup, kHost, kClient, kInGame } role = kNoGame;  // kSetup: the host's setup screen
	bool file_known = false;  // this PC's H55-Settings.lua was read
	Values file;              // its values (the host's defaults)
	Values chosen;            // host: its choices (empty = the file's)
	// client: what the host sent in the lobby; in game: the rules of this game
	bool shown_known = false;
	Values shown;
	bool will_apply = false;  // host and client: every player has H5 Together's rules (else the files' rules)
	int map_script = -1;      // host and client: the lobby's map has a map script (1), none (0: MMH5.5's scripts will not run), unknown (-1)
	std::vector<std::string> missing;  // host: the players without it (names)
	std::vector<Differ> differ;        // host: players whose MMH5.5 files differ
	bool this_differs = false;         // client: the host said this PC's files differ from its own
	bool in_game_rules = false;        // in game: the host's rules are used (else the files')
	// host: every PC can take the start rules (game mode, ToE bonuses), else why not;
	// client: the host said they can (false: MMH5.5's way for them)
	bool start_ok = true;
	Why start_why;
	// host: every PC can take class_skills (the same class table, which can be
	// edited), else why not; client: the host said they can
	bool class_ok = true;
	Why class_why;
	bool class_from_save = false;  // in game: this game's class skills came from a loaded save
};
// The view for this role (the panel decides it from the screen on top and LobbyRole()).
View GetView(View::Role role);

// Host: new choices (empty = the file's). Written to h5-together.ini [rules] and
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

// H5 Together's versions in the waiting room (host_rules_logic.h, "H5 Together's
// version in the waiting room"), for the line the panel draws there.
struct VersionsView {
	enum State {
		kNothing,    // nothing to say (yet): alone, or players just joined
		kSame,       // every player has this version and build
		kDiffer,     // `differ` have another one, an older one, or none
		kHostOlder,  // client: the host has H5 Together before 0.3.0 (its rules came, no list)
		kHostNone,   // client: the host has no H5 Together (or its game rules are off)
		kHostNewer,  // client: the host's list is of a later format: another (newer) version
	} state = kNothing;
	std::vector<Peer> differ;
	std::string mine;  // this PC's version ("0.3.0")
	int players = 0;   // in the list (host: itself included)
};
VersionsView GetVersions(View::Role role);

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

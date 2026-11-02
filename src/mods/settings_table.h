// Every setting of h5-together.ini in one table (docs/plans/community-release.md,
// part 2 and appendix A): its section, key and default, what kind of value it
// takes and its limits, who it matters to, when a change applies, and where it
// is in the settings menu (the "H5 Together" page of the game's Options screen,
// settings_menu.cpp) with the key of its words in src/mods/texts/*.txt.
//
// The menu is drawn from this table, and test/settings_test.cpp checks it
// against the shipped tools/package/files/h5-together.ini (the same settings,
// the same defaults), so the file and the menu cannot drift apart.
//
// No game and no Windows calls here: the test exe uses it as it is.
#pragma once

#include <string>

namespace mods {
namespace settings {

enum Kind {
	kBool,    // 1 or 0
	kInt,     // a whole number between low and high
	kKey,     // F1..F12 or none
	kChoice,  // one of `values` (words or numbers the mod knows)
	kText,    // free text (addresses, a list, a file name...)
};

enum Who {
	kPlayer,  // each player for themselves
	kHost,    // only the host's value counts
	kGroup,   // each PC, but everyone in a game should have the same
	kPanel,   // written by a panel or by the add-on itself
	kTester,  // logs, experiments, parts of a feature
};

enum When {
	kRestart,   // after a restart of the game (read once at start-up)
	kNextGame,  // from the next game (read when a game starts)
	kLive,      // at once (the camera: from the next map or battle)
	kWritten,   // not chosen by the player: written by a panel or the add-on
};

// Where a setting is in the menu. The game's Options screen has two pages per
// category, side by side; each page is a list with headers, like the game's own.
enum Group {
	kFileOnly,     // not in the menu: only in h5-together.ini
	kRulesPanel,   // in the game rules panel (F2), not in this menu
	kInterface,    // page 1
	kCamera,       // page 1
	kSound,        // page 1
	kLanguage,     // page 1
	kRandomMaps,   // page 1
	kHostGroup,    // page 2: only the host's value counts
	kAdvanced,     // page 2: everyone in a game should have the same
	kHelp,         // page 2, at the end
	kGroupCount
};
int PageOf(Group group);  // 1 or 2; 0 for the groups that are not in the menu

// One value the menu offers: what goes into the file, and its words (a text
// key; nullptr = the value itself is shown, "F9", or with the setting's unit, "150 %").
struct Value {
	const char *value;
	const char *text;
};

// How the menu shows a setting: the game's own controls.
enum Control {
	kCheck,   // a check box (kBool; or a kChoice of two values: ticked = the second)
	kList,    // a drop-down list
	kSlider,  // a slider (many steps of a number)
	kNone,    // not shown (file only, or a text the game's controls cannot edit)
};

struct Setting {
	const char *section, *key;
	const char *def;    // the default: what the add-on uses when the line is not there
	Kind kind;
	int low, high;      // kInt: the limits the mod accepts
	const Value *values;  // the menu's choices (kInt: its steps), in order; nullptr for kBool / kKey
	int value_count;
	const char *unit;   // a text key with {n} for numbers ("settings.unit.percent": "{n} %"), or nullptr
	Who who;
	When when;
	Group group;
	Control control;
	const char *text;   // the row's words, a text key ("settings.notify.sound"); nullptr if not in the menu
	const char *help;   // its explanation (the tooltip), a text key ("settings.notify.sound.help")
	bool shipped;       // in the shipped h5-together.ini (false: a hidden test setting read by the code)
};

extern const Setting kSettings[];
extern const int kSettingCount;

// The setting, or nullptr (section and key ignore upper/lower case, as Windows does).
const Setting *Find(const char *section, const char *key);

// The menu's choices of a key setting: F1..F12, then none.
extern const Value kKeyValues[];
extern const int kKeyValueCount;

// Is `value` acceptable for this setting (the same rules as its mod)?
bool Valid(const Setting &s, const std::string &value);
// The values of the file and of the menu are compared like this: "F9" = "f9",
// " 5" = "5", "" = the default when the default is "".
bool SameValue(const Setting &s, const std::string &a, const std::string &b);

// The index in s.values (or kKeyValues, or {0, 1} for kBool) of `value`; for a
// number not in the list, the nearest one; -1 when nothing fits.
int ChoiceIndex(const Setting &s, const std::string &value);
// The file's value of choice `index` (kBool: "0" / "1").
std::string ChoiceValue(const Setting &s, int index);
int ChoiceCount(const Setting &s);

// The co-op steps row: one choice for [coop] fast_segments and segment_ms
// ("50", "100", or "200" = the game's own steps: fast_segments=0).
extern const char kCoopStepsGame[];  // "200"

}  // namespace settings
}  // namespace mods

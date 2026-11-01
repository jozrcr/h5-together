// Host rules: the pure work, no game needed (host_rules_logic.cpp; tested by
// src/hook/test/host_rules_test.cpp). See host_rules.cpp for the whole mod.
//
//   - the rules: MMH5.5's switches of scripts/H55-Settings.lua that the host
//     may choose (docs/plans/mmh55-vs-vanilla.md, B.5);
//   - reading their values from that file, and the file with other values;
//   - the text that carries a set of values between PCs ("OHR1;governance=0;...");
//   - the lobby hello's number codes (the type and byte of a waiting-room request).
#pragma once

#include <stdint.h>

#include <utility>

#include <string>
#include <vector>

namespace mods {
namespace rules {

enum Kind {
	kBool,        // 0 or 1
	kInt,         // a whole number from low to high
	kChoice,      // a whole number from low to high, shown with names (AI cheat: Off, 40%...)
	kMultiplier,  // a decimal number above 0 (1 = MMH5.5's), offered from kMultipliers
};

enum Group { kTowns, kComputer, kItems, kRandomMaps, kGroupCount };

struct RuleDef {
	const char *key;  // open-homm5.ini [rules] and the carried text: "governance"
	const char *var;  // the variable in H55-Settings.lua: "H55_GovernanceEnabled"
	Kind kind;
	int low, high;  // kInt, kChoice
	Group group;
};

const int kRuleCount = 15;
extern const RuleDef kRules[kRuleCount];
int RuleIndex(const std::string &key);  // -1 when unknown

// The multipliers offered for kMultiplier rules ("1" is MMH5.5's).
const int kMultiplierCount = 7;
extern const char *const kMultipliers[kMultiplierCount];

// One value per rule, as the text written in the file ("0", "1", "1.25");
// empty = no value (the file's, or unknown).
typedef std::vector<std::string> Values;
Values NoValues();

// Is this text a value the rule may take? (a plain number: no sign, no
// exponent; at most 8 characters)
bool ValidValue(const RuleDef &rule, const std::string &value);

// The values of the rules' variables in a settings file, as Lua would leave
// them: the last plain assignment "NAME = number" at the start of a line, not in
// a comment. Missing (or not a plain number) = empty.
Values ParseSettings(const std::string &lua);

// The file with these values: every plain assignment of a rule's variable gets
// the new value (only the number's characters change); a variable that has
// none gets a line at the end. Empty values are left as they are. `changes`
// (optional) gets "governance 1->0" for each value that differs.
std::string Substitute(const std::string &lua, const Values &values, std::vector<std::string> *changes);

// FNV-1a, 32 bits.
uint32_t Fnv1a(const void *data, size_t size, uint32_t hash = 2166136261u);
// The file's hash with the rules' values left out (the host's values replace
// them on every PC, so only the rest must be the same everywhere).
uint32_t HashWithoutRuleValues(const std::string &lua);

// The text that carries a set of values: "OHR1" then ";key=value" for each
// rule that has one, in the table's order. At most kTextMax characters.
const int kTextMax = 480;
std::string Encode(const Values &values);
// False when the text is not ours, or has an unknown key, a bad value, or a
// key twice (a newer version: then nothing of it is used).
bool Decode(const std::string &text, Values *values);

// The host's lobby message to a client, short (it travels in 4-byte pieces):
// "L1|<flags>|" then every rule's value in the table's order, separated by ","
// (empty = none). Flags: kWillApply = every player has the mod (the rules will
// be used), kDiffers = this client's MMH5.5 files differ from the host's,
// kNoScript = the map has no map script (MMH5.5's scripts will not run).
const int kWillApply = 1, kDiffers = 2, kNoScript = 4;
std::string LobbyText(int flags, const Values &values);
bool ReadLobbyText(const std::string &text, int *flags, Values *values);

// Does this map.xdb name a map script (<MapScript href="..."/>)? MMH5.5's
// scripts run in a multiplayer game only on maps that do.
bool HasMapScript(const std::string &map_xdb);

// The pieces of a lobby text, each a (row, mask) pair of the game's "lock
// buttons" command (NUI::CMPWaitLockButtonsUICommand: a client ignores a row
// it does not have). Rows are kLobbyRowBase | seq << 8 | index: piece 0 holds
// the text's length, piece i its bytes 4(i-1) to 4i-1 (little end first).
const uint32_t kLobbyRowBase = 0x4f520000;
const int kLobbyTextMax = 4 * 250;
bool IsLobbyRow(uint32_t row);
std::vector<std::pair<uint32_t, uint32_t>> LobbyPieces(const std::string &text, uint8_t seq);
// Puts the pieces together (in any order, a newer seq starts over): true and
// the text once the last one is in.
struct LobbyAssembler {
	int seq = -1;
	size_t length = 0;
	bool have_length = false;
	std::vector<uint32_t> parts;
	std::vector<bool> got;
	bool Add(uint32_t row, uint32_t mask, std::string *text);
};

// --- the lobby hello (a waiting-room request the game ignores: rmg_factions.cpp) ---
// "This PC applies the host's rules, version N": type kHelloType, byte N.
const uint32_t kHelloType = 0x4f52;  // 'OR'
const uint8_t kProtocol = 1;
// A hash: the mark in the type's top byte, its low 24 bits in the rest, its top
// 8 bits in the byte. Marks: 'N' the settings file without the rules' values,
// 'S' the whole settings file, 'X' MMH5.5's stats files.
const char kMarkSettingsRest = 'N', kMarkSettingsWhole = 'S', kMarkStats = 'X';
void HashRequest(char mark, uint32_t hash, uint32_t *type, uint8_t *byte);
// True (and the mark and hash) when the type is one of those marks.
bool ReadHashRequest(uint32_t type, uint8_t byte, char *mark, uint32_t *hash);

}  // namespace rules
}  // namespace mods

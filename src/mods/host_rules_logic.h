// Host rules: the pure work, no game needed (host_rules_logic.cpp; tested by
// src/hook/test/host_rules_test.cpp). See host_rules.cpp for the whole mod.
//
//   - the rules: MMH5.5's switches of scripts/H55-Settings.lua that the host
//     may choose (docs/plans/mmh55-vs-vanilla.md, B.5), two rules of the
//     start (the game mode, ToE's start bonuses) that change MMH5.5's start
//     script scripts/H55-AdvMap.lua instead, and three the add-on applies itself:
//     MMH5.5's scripts on maps without a map script ("map_scripts"), and the
//     class skill lock ("class_skills", with its testing chance "class_chance":
//     the game's class table, edited in memory);
//   - reading their values from that file, and the file with other values;
//   - the start script with the start rules (checked text edits);
//   - the text that carries a set of values between PCs ("OHR4;governance=0;...");
//   - the class table's edit for class_skills, and the text a save keeps ("OHW1;...");
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

enum Group { kStart, kHeroes, kTowns, kComputer, kItems, kRandomMaps, kGroupCount };

struct RuleDef {
	const char *key;  // h5-together.ini [rules] and the carried text: "governance"
	const char *var;  // the variable in H55-Settings.lua: "H55_GovernanceEnabled";
	                  // nullptr = not in that file: a start rule (kStartRuleOff is MMH5.5's way)
	                  // or one the add-on applies itself (map_scripts, class_skills, class_chance)
	Kind kind;
	int low, high;  // kInt, kChoice
	Group group;
	bool hidden;    // not in the panel (a testing value, set in h5-together.ini only)
};

const int kRuleCount = 20;
extern const RuleDef kRules[kRuleCount];
int RuleIndex(const std::string &key);  // -1 when unknown
// map_scripts: after the start rules (the panel shows it first of its group, "Random maps")
const int kMapScripts = 17;

// The class skill lock (docs/plans/host-rules.md, "Step 3"): the last two of the table.
//   class_skills: 0 = MMH5.5's way (each class is offered only the skills of
//                 its list in GameMechanics/RefTables/HeroClass.xdb), 1 = unlocked
//                 (MMH5.5's classes and chances, and every other skill MMH5.5
//                 offers to some class at class_chance); a later 2 = vanilla classes;
//   class_chance: the chance of those other skills, as the class table counts
//                 (MMH5.5's lists add up to 100): 2 = vanilla ToE's rare skills.
//                 Hidden: only h5-together.ini sets another one (testing).
const int kClassSkills = 18, kClassChance = 19;
const char kClassLocked[] = "0", kClassUnlocked[] = "1";
const char kClassChanceVanilla[] = "2";
inline bool IsClassRule(int r)
{
	return r == kClassSkills || r == kClassChance;
}

// The start rules (scripts/H55-AdvMap.lua, docs/plans/host-rules.md "Game mode and
// start bonuses"): the two before map_scripts.
//   game_mode: 0 = MMH5.5's way (the lobby's bonus arrows choose it), 1 Default,
//              2 Savage, 3 Nightmare, 4 Random (one of the three, by the game's
//              shared random numbers);
//   toe_bonuses: 1 = each human keeps the ToE start bonus picked with the
//              arrows, and MMH5.5's start artifact is not given (0 = MMH5.5's way).
const int kGameModeRule = 15, kToeBonusRule = 16;
const char kStartRuleOff[] = "0";
inline bool IsStartRule(int r)
{
	return r == kGameModeRule || r == kToeBonusRule;
}

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
// a comment. Missing (or not a plain number) = empty; a rule without a variable
// (map_scripts) is always empty.
Values ParseSettings(const std::string &lua);

// The file with these values: every plain assignment of a rule's variable gets
// the new value (only the number's characters change); a variable that has
// none gets a line at the end. Empty values are left as they are, and so are
// the rules without a variable (map_scripts). `changes` (optional) gets
// "governance 1->0" for each value that differs.
std::string Substitute(const std::string &lua, const Values &values, std::vector<std::string> *changes);

// FNV-1a, 32 bits.
uint32_t Fnv1a(const void *data, size_t size, uint32_t hash = 2166136261u);
// The file's hash with the rules' values left out (the host's values replace
// them on every PC, so only the rest must be the same everywhere).
uint32_t HashWithoutRuleValues(const std::string &lua);

// The text that carries a set of values: "OHR4" then ";key=value" for each
// rule that has one, in the table's order. At most kTextMax characters.
const int kTextMax = 480;
std::string Encode(const Values &values);
// False when the text is not ours, or has an unknown key, a bad value, or a
// key twice (a newer version: then nothing of it is used).
bool Decode(const std::string &text, Values *values);

// The host's lobby message to a client, short (it travels in 4-byte pieces):
// "L4|<flags>|" then every rule's value in the table's order, separated by ","
// (empty = none). Flags: kWillApply = every player has the mod (the rules will
// be used), kDiffers = this client's MMH5.5 files differ from the host's,
// kNoScript = the map has no map script (MMH5.5's scripts run on it only with
// map_scripts on), kNoStart = the start rules cannot be used (MMH5.5's start
// script is not the same on every PC, or not as expected): MMH5.5's way for them,
// kNoClasses = class_skills cannot be used (a PC's class table is not the host's,
// or cannot be edited): MMH5.5's lock for everyone.
const int kWillApply = 1, kDiffers = 2, kNoScript = 4, kNoStart = 8, kNoClasses = 16;
std::string LobbyText(int flags, const Values &values);
bool ReadLobbyText(const std::string &text, int *flags, Values *values);

// Does this map.xdb name a map script (<MapScript href="..."/>)? In a
// multiplayer game MMH5.5's scripts run by themselves only on maps that do.
bool HasMapScript(const std::string &map_xdb);

// --- the rule map_scripts: MMH5.5's scripts on maps without a map script ---
// In a multiplayer game the world starts the map's scripts (0x00992d00:
// scripts/advmap-startup.lua, which loads MMH5.5's H55-Core.lua, then the map's
// own script) only when the map has a map script; its "no map script" jump, at
// 0x00992d2a, returns. A random map straight from the generator has none.
// MMH5.5 RC19k's 55u.dll changes that jump ("map script activation is now the
// default", its notes). What this PC's MMH5.5 does by itself, from the jump's 6
// bytes: 0 = the original jump (the original game, MMH5.5 RC19d: no scripts),
// 1 = changed (RC19k: the scripts run).
extern const unsigned char kNoScriptJump[6];
int OwnMapScripts(const unsigned char code[6]);
// What the add-on does when the world starts its scripts, for this game's value
// of map_scripts ("" = this game has no host's rules).
enum MapScriptAction {
	kAsIs,         // the game's own way: the map has its own script, or no host's rules
	kEmptyScript,  // the map has none: the world gets an empty one (as from MMH5.5's Mapmixer), so the scripts run
	kNoScripts,    // the map has none: no scripts, as in the original game (also where MMH5.5 would run them)
};
MapScriptAction MapScriptActionFor(bool map_has_script, const std::string &value);

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

// --- MMH5.5's start script (scripts/H55-AdvMap.lua) ---
// Its "StartingBonus" part (in H55_PrepareAdvMap, from H55_DEBUG = {102,
// "StartingBonus" to H55_DEBUG = {103,"StartingArmies"): after the game has
// handed out ToE's start bonuses, it finds which bonus each human got, takes it
// back, gives each human's first hero a random minor artifact of its own, and
// turns the humans' bonuses into one game mode (artifact = Default, gold =
// Savage, resources = Nightmare; Savage and Nightmare also team the computer
// players against the humans). False when the part is not found.
bool FindStartBlock(const std::string &lua, size_t *begin, size_t *end);
// FNV-1a of that part (0 when not found): every PC must have the same one.
uint32_t StartBlockHash(const std::string &lua);
// The game mode used: the rule's, or Default when the ToE bonuses are on and
// the rule says "the arrows" (the arrows then choose bonuses, not the mode).
int ModeUsed(int game_mode, bool toe_bonuses);
// The script with the start rules: three checked edits in that part
//   1. a line after "local amountai = ...": OH55_KeepToEBonus, OH55_Mode = <toe>, <mode>;
//   2. the humans' loop (take back, MMH5.5's artifact) gets "and OH55_KeepToEBonus ~= 1";
//   3. a line before the mode's decision (if H55_MapType == "Scenario") that sets
//      the "somebody chose" flags from OH55_Mode, so MMH5.5's own code (modes,
//      teams, AI heroes that never flee) runs for the host's mode;
// and with `trace`, a print of each player's resources and minor artifacts after
// the part (testing; it changes nothing in the game). With MMH5.5's way for both
// rules and no trace, the script as it is. False (and why; `out` untouched) when
// the expected text is not found exactly once: then nothing may change.
bool PatchStartScript(const std::string &lua, int game_mode, bool toe_bonuses, bool trace, std::string *out, std::string *why);

// --- the waiting room's bonus icons (host_rules_icons.cpp) ---
// The pictures of the bonus arrows, by bonus number (0 random, 1 minor
// artifact, 2 gold, 3 resources): texture records <name>.(Texture).xdb and their
// pictures <name>.(Texture).dds in Textures/Interface/MPWait/PlayersList/Item/
// Bonuses/. MMH5.5's pak repaints the pictures for its modes; ToE's are in the
// game's data.pak.
extern const char *const kBonusPictures[4];  // "Random", "RandomArtifact", "Gold", "Resource"
// The icon's list of related textures (the bonus icon finds its pictures by name in it).
const char kBonusTexturesPath[] = "ui/mpwait/playerslist/item/bonuses.(windowrelatedtextures).xdb";
// A picture's name in that list: the game's "bonus2", ours "oh_toe_bonus2" (ToE's).
std::string BonusTextureName(int bonus, bool toe);
// Paths, as NormalizePath gives them (lower case, '/'): the game's record or
// picture of a bonus (ours: "openhomm5-toe-gold.(texture).xdb" in the same folder).
std::string BonusPicturePath(int bonus, bool ours, bool dds);
// Which of our files is this path: its bonus (0..3) and whether it is the
// picture (.dds); -1 when none.
int OurBonusPicture(const std::string &key, bool *dds);
// The list with our four names after the game's, each naming our record of
// ToE's picture, in the file's layout. False (`out` untouched) when the list is
// not as expected: a "bonus0".."bonus3" not there exactly once, no single
// "</textures>", or our names already in.
bool BonusTexturesWithToe(const std::string &xdb, std::string *out);
// data.pak's record of a bonus with its picture (DestName) renamed to ours.
// False when the record has no single DestName naming that bonus's picture.
bool ToeTextureRecord(const std::string &xdb, int bonus, std::string *out);
// A .dds picture: "DDS " and at least its 128-byte header.
bool IsDds(const std::string &bytes);

// --- the class table (class_skills; host_rules_classes.cpp reads and edits it in memory) ---
// The game reads GameMechanics/RefTables/HeroClass.xdb once, at start-up, with
// the rest of its reference tables. Each class's SkillsProbs is a vector of
// 12-byte items (0x00875ba0 reads them): a word we leave alone, the skill, its
// chance. A new skill on level-up is drawn among the skills the hero lacks,
// weighted by these chances (0x00b50be0 -> 0x00b509b0: one draw of the world's
// shared random numbers in [0, sum - 1]); a skill the class does not list
// weighs 0, so it is never offered.
struct SkillProb {
	uint32_t head;  // the item's first word (the same in every item; copied into ours)
	int32_t skill;
	int32_t prob;
};
struct ClassSkillList {
	int id;  // the class's number (its place in the table)
	std::vector<SkillProb> probs;
};
typedef std::vector<ClassSkillList> ClassTable;  // the classes with a list, by number
// Plausible as the game's table? Every list 1..64 items, skills 1..1000, chances
// 0..1000 adding up to more than 0, no skill twice in a list, at least one class.
// "" = fine, else why not.
std::string CheckClassTable(const ClassTable &table);
// FNV-1a of the numbers, skills and chances (not the heads): every PC must have
// the same table for class_skills.
uint32_t ClassTableHash(const ClassTable &table);
// The skills some class can be offered (MMH5.5: 24; never the ones no class
// lists, such as vanilla's Barbarian Learning and Voice), from low to high.
std::vector<int> OfferedSkills(const ClassTable &table);
// A list with the rule: its own items, unchanged and in their order, then each
// skill of `skills` it lacks, at `chance` (head: the list's first item's).
std::vector<SkillProb> UnlockedList(const std::vector<SkillProb> &own, const std::vector<int> &skills, int chance);
// The table for these values of class_skills and class_chance: MMH5.5's for
// "0" or "" (false: nothing to change), every list unlocked for "1". False and
// why for a value it does not know (a later "2", vanilla classes) or a bad table.
bool ClassTableFor(const ClassTable &mmh55, const std::string &mode, const std::string &chance, ClassTable *out, std::string *why);
// The chance used: class_chance's value, or vanilla's 2 when it has none.
int ClassChance(const std::string &chance);

// --- what a save keeps: the rules the add-on applies in memory (class_skills) ---
// The world's own serializer (saves, and the multiplayer checksums) gets one more
// chunk when this game's class skills are unlocked: "OHW1;class_skills=1;
// class_chance=2" (the class rules that have a value). A save without it is
// MMH5.5's way. False when the text is not ours or has another key or a bad value.
std::string EncodeWorld(const Values &values);
bool DecodeWorld(const std::string &text, Values *values);

// --- the lobby hello (a waiting-room request the game ignores: rmg_factions.cpp) ---
// "This PC applies the host's rules, version N": type kHelloType, byte N.
const uint32_t kHelloType = 0x4f52;  // 'OR'
// 2: the start rules (game mode, ToE bonuses); 3: map_scripts; 4: class_skills.
// A PC with an older version gets no rules (as a PC without the add-on).
const uint8_t kProtocol = 4;
// A hash: the mark in the type's top byte, its low 24 bits in the rest, its top
// 8 bits in the byte. Marks: 'N' the settings file without the rules' values,
// 'S' the whole settings file, 'X' MMH5.5's stats files, 'A' the start script's
// "StartingBonus" part (StartBlockHash), 'C' the class table (ClassTableHash; 0 =
// this PC cannot edit its table).
const char kMarkSettingsRest = 'N', kMarkSettingsWhole = 'S', kMarkStats = 'X', kMarkStartScript = 'A', kMarkClassTable = 'C';
void HashRequest(char mark, uint32_t hash, uint32_t *type, uint8_t *byte);
// True (and the mark and hash) when the type is one of those marks.
bool ReadHashRequest(uint32_t type, uint8_t byte, char *mark, uint32_t *hash);

// --- H5 Together's version in the waiting room (0.3.0 on) ---
// Each client also tells the host its add-on's version and build, as two more of those
// requests: 'V' (PackVersion) and 'B' (the DLL's link time stamp: two builds of one
// version differ). A host before 0.3.0 does not know these marks: it ignores them (as
// the game does), and its clients see that no list comes (below).
// The host sends the list of every player's version, itself first, to each client that
// said its version, as one more lobby text (the same pieces as LobbyText):
//   "V1" then one line per player "\n<kind>|<version>|<build, 8 hex digits>|<name>".
// A later change of this text gets another first line ("V2"); a client that cannot
// read the list says "another version" (it never guesses). The rules' own hello
// ('OR', kProtocol) is unchanged: 0.2.7 and 0.3.0 still agree on the rules.
const char kMarkVersion = 'V', kMarkBuild = 'B';
// "0.3.0" -> 0x000300 (8 bits each: major, minor, patch); 0 when it is not 3 numbers 0..255.
uint32_t PackVersion(const char *version);
std::string UnpackVersion(uint32_t packed);  // "0.3.0"
struct Peer {
	enum Kind : char {
		kKnown = 'V',    // this H5 Together version and build
		kOlder = 'O',    // H5 Together before 0.3.0: its rules hello, but no version
		kNone = 'N',     // no hello: no H5 Together, or its game rules are off ([rules] enabled=0)
		kWaiting = 'W',  // joined a moment ago: not known yet
	};
	char kind = kNone;
	std::string version;  // kKnown: "0.3.0"
	uint32_t build = 0;   // kKnown: the link time stamp
	std::string name;     // the player's name (UTF-8)
	bool operator==(const Peer &o) const { return kind == o.kind && version == o.version && build == o.build && name == o.name; }
};
std::string VersionsText(const std::vector<Peer> &peers);
// False when the text is not a "V1" list, or a line of it is broken.
bool ReadVersionsText(const std::string &text, std::vector<Peer> *peers);
// Is this lobby text a version list (of any format: "V" and a digit)?
bool IsVersionsText(const std::string &text);
// The players whose add-on is not `mine` (another version or build, an older one, or
// none), in the list's order; the ones still waiting are left out.
std::vector<Peer> Differing(const std::vector<Peer> &peers, const Peer &mine);

}  // namespace rules
}  // namespace mods

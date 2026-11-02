// Test of the host rules' pure work (src/mods/host_rules_logic.h; no game
// needed): reading MMH5.5's settings file, serving it with other values (only
// the numbers change), the hash without the rules' values, the text carried in
// the start message and the lobby, the lobby hello's number codes, the
// class table's edit for class_skills (and what a save keeps of it), and the
// files served for the lobby's bonus icons.
//
// The settings text below is ours, written in the file's format (MMH5.5's own
// file is never copied into the repo).
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../mods/host_rules_logic.h"

using namespace mods::rules;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

int Index(const char *key)
{
	return RuleIndex(key);
}

// A settings file in MMH5.5's layout: CRLF lines, "NAME = number; --comment".
const char kSample[] =
	"-------------------------------------\r\n"
	"--\tA TEST SETTINGS FILE\r\n"
	"-------------------------------------\r\n"
	"\r\n"
	"H55_TownGateEnabled = 1; --town gate (0 = off).\r\n"
	"\r\n"
	"H55_TownConvEnabled = 1; --conversion\r\n"
	"H55_AmountExtraTownsDwellings = 5; --how many more\r\n"
	"H55_GovernanceEnabled = 1; --governors\r\n"
	"H55_GovernorExpCoef = 1; --not a rule of ours\r\n"
	"H55_MineControl = 1;\r\n"
	"H55_DimensionDoors = 1; --doors\r\n"
	"H55_DimensionDoorsOnThisMap = 0; --a longer name: not the doors\r\n"
	"--H55_AICheatMode = 6; --a comment, not a setting\r\n"
	"H55_AICheatMode = 4; --the AI bonus\r\n"
	"H55_GenerateMixedStacks = 1;  --two spaces before the comment\r\n"
	"H55_BanksDifficulty = 1; --multiplier\r\n"
	"H55_NeutralStackSize\t=\t1.2; --tabs, and a value not in the list\r\n"
	"H55_ManaObservatories = 1; H55_NoArtifactMerchantsInTowns = 0; --two on one line\r\n"
	"--[[ H55_NegativeArtifacts = 5;\r\n"
	"H55_NegativeArtifacts = 7; a block comment ]]\r\n"
	"H55_NegativeArtifacts = 1; --prank artifacts\r\n"
	"H55_MoonDiscQuest = 0; H55_MoonDiscOnARMGMaps = 1;\r\n"
	"H55_RPGPotions = 0; --potions\r\n"
	"H55_CompetitiveARMGTowns = 1;\r\n"
	"-------------------------------------\r\n";

void TestTable()
{
	Check(RuleIndex("governance") == 0 && RuleIndex("nothing") == -1, "rule keys are found by name");
	bool unique = true;
	for (int a = 0; a < kRuleCount; ++a)
		for (int b = a + 1; b < kRuleCount; ++b)
			if (strcmp(kRules[a].key, kRules[b].key) == 0 || (kRules[a].var && kRules[b].var && strcmp(kRules[a].var, kRules[b].var) == 0))
				unique = false;
	Check(unique, "every rule has its own key and variable");
	bool start = Index("game_mode") == kGameModeRule && Index("toe_bonuses") == kToeBonusRule;
	for (int r = 0; r < kRuleCount; ++r)
		start = start && (IsStartRule(r) || r == kMapScripts || IsClassRule(r)) == (kRules[r].var == nullptr) &&
		        (IsStartRule(r) == (kRules[r].group == kStart)) && (!IsStartRule(r) || ValidValue(kRules[r], kStartRuleOff));
	Check(start, "the start rules: no settings variable, their own group, \"0\" = MMH5.5's way");
	Check(ValidValue(kRules[kGameModeRule], "4") && !ValidValue(kRules[kGameModeRule], "5") &&
	          ValidValue(kRules[kToeBonusRule], "1") && !ValidValue(kRules[kToeBonusRule], "2"),
	      "game mode 0 to 4, ToE bonuses 0 or 1");
	Check(kMapScripts == Index("map_scripts") && kMapScripts == kToeBonusRule + 1 && !kRules[kMapScripts].var &&
	          kRules[kMapScripts].kind == kBool && kRules[kMapScripts].group == kRandomMaps && !IsStartRule(kMapScripts),
	      "map_scripts: after the start rules, no settings variable (the add-on applies it), on/off, with the random maps");
	Check(kClassSkills == Index("class_skills") && kClassChance == Index("class_chance") && kClassChance == kRuleCount - 1 &&
	          kClassSkills == kMapScripts + 1 && !kRules[kClassSkills].var && !kRules[kClassChance].var &&
	          kRules[kClassSkills].group == kHeroes && kRules[kClassChance].group == kHeroes && !kRules[kClassSkills].hidden &&
	          kRules[kClassChance].hidden && IsClassRule(kClassSkills) && IsClassRule(kClassChance) && !IsClassRule(kMapScripts),
	      "class_skills and class_chance: the table's last two, no settings variable, heroes; the chance is hidden");
	Check(ValidValue(kRules[kClassSkills], kClassLocked) && ValidValue(kRules[kClassSkills], kClassUnlocked) &&
	          !ValidValue(kRules[kClassSkills], "2") && ValidValue(kRules[kClassChance], kClassChanceVanilla) &&
	          ValidValue(kRules[kClassChance], "30") && ValidValue(kRules[kClassChance], "50") &&
	          !ValidValue(kRules[kClassChance], "0") && !ValidValue(kRules[kClassChance], "51"),
	      "class_skills 0 or 1 (vanilla classes, a later 2, is refused today), class_chance 1 to 50");
	bool hidden = true;
	for (int r = 0; r < kRuleCount; ++r)
		hidden = hidden && kRules[r].hidden == (r == kClassChance);
	Check(hidden, "only class_chance is hidden from the panel");
	Check(kProtocol == 4, "protocol 4 (class_skills): a PC with version 1 to 3 gets no rules");
	bool plain = true;
	for (int r = 0; r < kRuleCount; ++r)
		for (const char *c = kRules[r].key; *c; ++c)
			if (!((*c >= 'a' && *c <= 'z') || *c == '_'))
				plain = false;
	Check(plain, "rule keys are lower case letters and _ (ini keys, carried text)");
	bool multipliers = true;
	for (const char *m : kMultipliers)
		multipliers = multipliers && ValidValue(kRules[Index("neutral_stack_size")], m);
	Check(multipliers, "every offered multiplier is a valid value");
}

void TestValues()
{
	const RuleDef &gov = kRules[Index("governance")];
	const RuleDef &extra = kRules[Index("extra_conversions")];
	const RuleDef &ai = kRules[Index("ai_cheat")];
	const RuleDef &size = kRules[Index("neutral_stack_size")];
	Check(ValidValue(gov, "0") && ValidValue(gov, "1") && !ValidValue(gov, "2") && !ValidValue(gov, "") &&
	          !ValidValue(gov, "1.0") && !ValidValue(gov, "-1"),
	      "on/off rules take 0 or 1 only");
	Check(ValidValue(extra, "0") && ValidValue(extra, "10") && !ValidValue(extra, "11") && !ValidValue(extra, "05"),
	      "extra conversions: 0 to 10");
	Check(ValidValue(ai, "0") && ValidValue(ai, "6") && !ValidValue(ai, "7"), "AI bonus: 0 to 6");
	Check(ValidValue(size, "1") && ValidValue(size, "1.25") && ValidValue(size, "0.5") && !ValidValue(size, "0") &&
	          !ValidValue(size, "0.0") && !ValidValue(size, "11") && !ValidValue(size, "1.") && !ValidValue(size, ".5") &&
	          !ValidValue(size, "1e2") && !ValidValue(size, "1,5"),
	      "multipliers: a plain number above 0, at most 10");
}

void TestParse()
{
	const Values v = ParseSettings(kSample);
	Check(v[Index("town_gate")] == "1" && v[Index("extra_conversions")] == "5" && v[Index("ai_cheat")] == "4",
	      "values are read; a commented-out line is not");
	Check(v[Index("dimension_doors")] == "1", "a longer variable name (H55_DimensionDoorsOnThisMap) is another variable");
	Check(v[Index("neutral_stack_size")] == "1.2", "tabs around '=', a decimal value");
	Check(v[Index("mana_observatories")] == "1" && v[Index("moon_disc")] == "1", "two statements on one line");
	Check(v[Index("negative_artifacts")] == "1", "assignments inside a --[[ block comment ]] are not read");
	const Values none = ParseSettings("print(1)\nH55_AICheatMode = random(3);\n");
	Check(none[Index("ai_cheat")].empty(), "an expression is not a value we can read");
	const Values last = ParseSettings("H55_AICheatMode = 1;\nH55_AICheatMode = 3;\n");
	Check(last[Index("ai_cheat")] == "3", "the last assignment wins, as in Lua");
	Check(v[kMapScripts].empty() && ParseSettings("map_scripts = 1;\n")[kMapScripts].empty(),
	      "map_scripts is never read from the file");
}

// Every byte outside the changed numbers is the same.
bool OnlyNumbersChanged(const std::string &a, const std::string &b)
{
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		if (a[i] == b[j]) {
			++i, ++j;
			continue;
		}
		const auto num = [](char c) { return (c >= '0' && c <= '9') || c == '.'; };
		while (i < a.size() && num(a[i]))
			++i;
		while (j < b.size() && num(b[j]))
			++j;
		if (i < a.size() && j < b.size() && a[i] != b[j])
			return false;
	}
	return i == a.size() && j == b.size();
}

void TestSubstitute()
{
	const std::string file = kSample;
	Values v = NoValues();
	std::vector<std::string> changes;
	Check(Substitute(file, v, &changes) == file && changes.empty(), "no values: the file is served as it is");
	Check(Substitute(file, ParseSettings(file), &changes) == file && changes.empty(),
	      "the file's own values: byte for byte the same file");

	v[Index("governance")] = "0";
	v[Index("ai_cheat")] = "0";
	v[Index("mixed_stacks")] = "0";
	v[Index("neutral_stack_size")] = "1.25";
	v[Index("moon_disc")] = "0";
	changes.clear();
	const std::string out = Substitute(file, v, &changes);
	const Values back = ParseSettings(out);
	Check(back[Index("governance")] == "0" && back[Index("ai_cheat")] == "0" && back[Index("mixed_stacks")] == "0" &&
	          back[Index("neutral_stack_size")] == "1.25" && back[Index("moon_disc")] == "0",
	      "the served file has the new values");
	Check(back[Index("town_gate")] == "1" && back[Index("extra_conversions")] == "5", "the other values stay");
	Check(out.size() == file.size() + 1, "only the numbers' characters change (1.2 -> 1.25: one more)");
	Check(OnlyNumbersChanged(file, out), "every byte but the numbers is the same");
	Check(out.find("--H55_AICheatMode = 6;") != std::string::npos, "the commented-out line is untouched");
	Check(out.find("H55_DimensionDoorsOnThisMap = 0;") != std::string::npos, "the longer name is untouched");
	Check(out.find("--[[ H55_NegativeArtifacts = 5;") != std::string::npos &&
	          out.find("H55_NegativeArtifacts = 7; a block comment") != std::string::npos,
	      "the block comment is untouched");
	Check(changes.size() == 5, "five changes are reported");
	Check(!changes.empty() && changes[0] == "governance 1->0", "a change reads \"governance 1->0\"");
	Values start = NoValues();
	start[kGameModeRule] = "2";
	start[kToeBonusRule] = "1";
	changes.clear();
	Check(Substitute(file, start, &changes) == file && changes.empty() && ParseSettings(file)[kGameModeRule].empty(),
	      "the start rules never touch the settings file");

	// a rule the file does not have: a line at the end (with the file's line ends)
	Values only = NoValues();
	only[Index("rpg_potions")] = "1";
	only[Index("bank_difficulty")] = "2";
	const std::string small = "H55_RPGPotions = 0;\r\n";
	const std::string added = Substitute(small, only, nullptr);
	Check(added == "H55_RPGPotions = 1;\r\nH55_BanksDifficulty = 2; -- H5 Together: the host's rules\r\n",
	      "a missing variable gets a line at the end");
	Check(Substitute("H55_RPGPotions = 0;", only, nullptr) ==
	          "H55_RPGPotions = 1;\nH55_BanksDifficulty = 2; -- H5 Together: the host's rules\n",
	      "no line end at the end of the file: one is added first");
	// a variable set by an expression: our line at the end wins
	Values ai = NoValues();
	ai[Index("ai_cheat")] = "2";
	const std::string expr = Substitute("H55_AICheatMode = 1;\nH55_AICheatMode = random(3);\n", ai, nullptr);
	const std::string tail = "H55_AICheatMode = 2; -- H5 Together: the host's rules\n";
	Check(ParseSettings(expr)[Index("ai_cheat")].empty() && expr.size() > tail.size() &&
	          expr.compare(expr.size() - tail.size(), tail.size(), tail) == 0 && expr.find("H55_AICheatMode = 2;\n") == 0,
	      "a variable also set by an expression gets our line at the end");
	// map_scripts: not a variable of the file, so the file never changes for it
	Values scripts = NoValues();
	scripts[kMapScripts] = "1";
	changes.clear();
	Check(Substitute(file, scripts, &changes) == file && changes.empty(), "map_scripts alone: the file is served as it is");
	scripts[Index("governance")] = "0";
	changes.clear();
	const std::string with = Substitute(file, scripts, &changes);
	Check(ParseSettings(with)[Index("governance")] == "0" && with.size() == file.size() && changes.size() == 1 &&
	          with.find("map_scripts") == std::string::npos,
	      "map_scripts with another rule: only the other rule's number changes, no line for map_scripts");
}

void TestHash()
{
	const std::string file = kSample;
	Values v = NoValues();
	v[Index("governance")] = "0";
	v[Index("neutral_stack_size")] = "3";
	const std::string other_values = Substitute(file, v, nullptr);
	Check(HashWithoutRuleValues(file) == HashWithoutRuleValues(other_values), "the rest's hash ignores the rules' values");
	Check(Fnv1a(file.data(), file.size()) != Fnv1a(other_values.data(), other_values.size()), "the whole hash sees them");
	std::string other_rest = file;
	other_rest.replace(other_rest.find("H55_GovernorExpCoef = 1"), 23, "H55_GovernorExpCoef = 2");
	Check(HashWithoutRuleValues(file) != HashWithoutRuleValues(other_rest), "the rest's hash sees another setting");
	Check(Fnv1a("a", 1) == 0xe40c292cu, "FNV-1a of \"a\" is the standard value");
}

void TestText()
{
	Values v = NoValues();
	Check(Encode(v) == "OHR4", "no values: \"OHR4\"");
	v[Index("governance")] = "0";
	v[Index("ai_cheat")] = "0";
	v[Index("neutral_stack_size")] = "1.25";
	const std::string text = Encode(v);
	Check(text == "OHR4;governance=0;ai_cheat=0;neutral_stack_size=1.25", "values in the table's order");
	Values back;
	Check(Decode(text, &back) && back == v, "decoded back to the same values");
	Values with_start = v;
	with_start[kGameModeRule] = "3";
	with_start[kToeBonusRule] = "1";
	Check(Encode(with_start) == text + ";game_mode=3;toe_bonuses=1" && Decode(Encode(with_start), &back) && back == with_start,
	      "the start rules travel too");
	Check(!Decode("OHR1;governance=0", &back) && !Decode("OHR2;governance=0", &back) && !Decode("OHR3;governance=0", &back),
	      "another version (1, 2, 3) is refused");
	Check(!Decode("OHR4;governance=2", &back) && !Decode("OHR4;game_mode=5", &back), "a bad value is refused");
	Check(!Decode("OHR4;governance=0;governance=1", &back), "a key twice is refused");
	Check(!Decode("OHR4;new_rule=1", &back), "an unknown key is refused (a newer host: nothing is used)");
	Check(!Decode("OHR4;governance", &back) && !Decode("OHR4governance=0", &back) && !Decode("OHR4;", &back),
	      "broken texts are refused");
	Values bad = NoValues();
	bad[Index("governance")] = "7";
	Check(Encode(bad) == "OHR4", "invalid values are never sent");
	// the longest text: every rule with an 8-character value
	Values longest = NoValues();
	for (int r = 0; r < kRuleCount; ++r)
		longest[r] = kRules[r].kind == kMultiplier ? "9.999999" : kRules[r].kind == kBool ? "1" : std::to_string(kRules[r].high);
	const std::string all = Encode(longest);
	Check(all.size() < 480 && Decode(all, &back) && back == longest,
	      "every rule at its longest fits (start message chunk: 512 bytes with its zero)");
	Check(all.size() < 512 - 1, "the chunk keeps a final zero");

	Values scripts = v;
	scripts[kMapScripts] = "1";
	const std::string with_scripts = Encode(scripts);
	Check(with_scripts == text + ";map_scripts=1" && Decode(with_scripts, &back) && back == scripts,
	      "map_scripts travels in the start message like the others");
	Values all_three = with_start;
	all_three[kMapScripts] = "0";
	Check(Encode(all_three) == text + ";game_mode=3;toe_bonuses=1;map_scripts=0" && Decode(Encode(all_three), &back) &&
	          back == all_three,
	      "the start rules and map_scripts travel together");
	Check(!Decode("OHR4;map_scripts=2", &back), "map_scripts takes 0 or 1 only");
	Values classes = v;
	classes[kClassSkills] = kClassUnlocked;
	classes[kClassChance] = "30";
	Check(Encode(classes) == text + ";class_skills=1;class_chance=30" && Decode(Encode(classes), &back) && back == classes,
	      "class_skills and its chance travel in the start message, last");
	Check(!Decode("OHR4;class_skills=2", &back) && !Decode("OHR4;class_chance=0", &back) && !Decode("OHR4;class_chance=51", &back),
	      "class_skills 2 (a later version's vanilla classes) and a chance out of 1..50 are refused");
	Check(!Decode("OHR3;governance=0;class_skills=1", &back), "a version 3 text (no class rules) is refused");

	const std::string lobby = LobbyText(kWillApply | kDiffers, v);
	int flags = 0;
	Check(lobby == "L4|3|0,,,,,,0,,1.25,,,,,,,,,,," && ReadLobbyText(lobby, &flags, &back) && flags == 3 && back == v,
	      "the lobby text: L4|flags|every value in the table's order");
	Check(LobbyText(kWillApply | kNoStart, with_start) == "L4|9|0,,,,,,0,,1.25,,,,,,,3,1,,," &&
	          ReadLobbyText(LobbyText(kWillApply | kNoStart, with_start), &flags, &back) && flags == 9 && back == with_start,
	      "the start rules and their flag in the lobby text");
	Check(LobbyText(kNoScript, scripts) == "L4|4|0,,,,,,0,,1.25,,,,,,,,,1,," && ReadLobbyText(LobbyText(kNoScript, scripts), &flags, &back) &&
	          flags == 4 && back == scripts,
	      "map_scripts in the lobby text");
	Check(LobbyText(kWillApply | kNoClasses, classes) == "L4|17|0,,,,,,0,,1.25,,,,,,,,,,1,30" &&
	          ReadLobbyText(LobbyText(kWillApply | kNoClasses, classes), &flags, &back) && flags == 17 && back == classes,
	      "the class rules (the last two values) and their flag kNoClasses (16) in the lobby text");
	Check(!ReadLobbyText(text, &flags, &back) && !ReadLobbyText("L4|x|0,,,,,,0,,1.25,,,,,,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L4||0,,,,,,0,,1.25,,,,,,,,,,,", &flags, &back) && !ReadLobbyText("L3|3|0,,,,,,0,,1.25,,,,,,,,,", &flags, &back),
	      "a lobby text without flags, or of another version, is refused");
	Check(!ReadLobbyText("L4|1|0,,,,,,0,,1.25,,,,,,,,,,", &flags, &back) && !ReadLobbyText("L4|1|0,,,,,,0,,1.25,,,,,,,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L4|1|7,,,,,,0,,1.25,,,,,,,,,,,", &flags, &back),
	      "too few or too many values (a version 3 text has 18), or a bad one, is refused");
	Check(LobbyText(kWillApply, longest).size() < 100, "the lobby text stays short (it travels 4 bytes at a time)");
}

void TestPieces()
{
	Values v = NoValues();
	v[Index("governance")] = "0";
	v[Index("bank_difficulty")] = "1.5";
	const std::string text = LobbyText(kWillApply, v);
	const auto pieces = LobbyPieces(text, 7);
	Check(pieces.size() == 1 + (text.size() + 3) / 4, "a length piece, then 4 bytes per piece");
	bool rows = true;
	for (size_t i = 0; i < pieces.size(); ++i)
		rows = rows && IsLobbyRow(pieces[i].first) && pieces[i].first == (kLobbyRowBase | 7u << 8 | static_cast<uint32_t>(i));
	Check(rows, "rows are 0x4f52, the sequence number, the piece's number");
	Check(pieces[0].second == text.size(), "piece 0 holds the length");
	Check(!IsLobbyRow(0) && !IsLobbyRow(7) && !IsLobbyRow(0x4f530000u), "the game's own rows are not ours");

	LobbyAssembler a;
	std::string out;
	int completes = 0;
	size_t at = 0;
	for (size_t i = 0; i < pieces.size(); ++i)
		if (a.Add(pieces[i].first, pieces[i].second, &out))
			++completes, at = i;
	Check(completes == 1 && at + 1 == pieces.size() && out == text, "the pieces in order give the text back, at the last one");
	LobbyAssembler b;
	out.clear();
	bool done = false;
	for (size_t i = pieces.size(); i-- > 0;)
		done = b.Add(pieces[i].first, pieces[i].second, &out) || done;
	Check(done && out == text, "in reverse order too");
	// an older text's pieces, then a newer one: the newer wins
	const std::string newer = LobbyText(0, NoValues());
	const auto pieces2 = LobbyPieces(newer, 8);
	LobbyAssembler c;
	out.clear();
	c.Add(pieces[0].first, pieces[0].second, &out);
	c.Add(pieces[1].first, pieces[1].second, &out);
	done = false;
	for (const auto &p : pieces2)
		done = c.Add(p.first, p.second, &out) || done;
	Check(done && out == newer, "a new sequence number starts over");
	LobbyAssembler d;
	out.clear();
	done = false;
	for (size_t i = 0; i + 1 < pieces.size(); ++i)
		done = d.Add(pieces[i].first, pieces[i].second, &out) || done;
	Check(!done, "a missing piece: no text");
	Check(LobbyPieces(std::string(kLobbyTextMax + 1, 'x'), 1).empty() && LobbyPieces("", 1).empty(), "too long or empty: no pieces");
}

void TestMapScript()
{
	Check(HasMapScript("<AdvMapDesc>\n\t<MapScript href=\"MapScript.xdb#xpointer(/Script)\"/>\n</AdvMapDesc>"),
	      "a map script is found");
	Check(!HasMapScript("<AdvMapDesc>\n\t<MapScript/>\n\t<Script href=\"x\"/>\n</AdvMapDesc>"),
	      "<MapScript/> (a generated random map) is none, whatever follows");
	Check(!HasMapScript("<MapScript href=\"\"/>") && !HasMapScript("<AdvMapDesc/>"), "an empty link or no field is none");

	// what this PC's MMH5.5 does by itself: the "no map script" jump's bytes
	const unsigned char original[6] = {0x0f, 0x84, 0x4e, 0x01, 0x00, 0x00};
	const unsigned char nops[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
	const unsigned char elsewhere[6] = {0xe9, 0x10, 0x20, 0x30, 0x40, 0x90};
	Check(OwnMapScripts(original) == 0, "the original jump (the original game, RC19d): no scripts without a map script");
	Check(OwnMapScripts(nops) == 1 && OwnMapScripts(elsewhere) == 1, "a changed jump (RC19k's 55u.dll): the scripts run");

	// what the add-on does when the world starts its scripts
	Check(MapScriptActionFor(true, "") == kAsIs && MapScriptActionFor(true, "0") == kAsIs && MapScriptActionFor(true, "1") == kAsIs,
	      "a map with its own script: always the game's own way");
	Check(MapScriptActionFor(false, "") == kAsIs, "no host's rules: the game's own way (MMH5.5 decides)");
	Check(MapScriptActionFor(false, "1") == kEmptyScript, "no map script, map_scripts=1: an empty script, the scripts run");
	Check(MapScriptActionFor(false, "0") == kNoScripts, "no map script, map_scripts=0: no scripts, as in the original game");
}

// A start script in the layout of MMH5.5's H55-AdvMap.lua "StartingBonus" part
// (our own short text, not MMH5.5's): the flags, the computer players' count,
// the humans' loop that takes the bonus back and gives the start artifact, and
// the mode's decision. Line ends mixed, as in MMH5.5's file.
const char kStartSample[] =
	"function H55_PrepareAdvMap()\r\n"
	"\tH55_DEBUG = {101,\"ShamanPatterns\",1,\"NoHero\"};----\r\n"
	"\tH55_DEBUG = {102,\"StartingBonus\",1,\"NoHero\"};----\n"
	"\t----\n"
	"\n"
	"\tif H55_EnableModes == 1 then\n"
	"\t\tlocal somebodygold = 0;\n"
	"\t\tlocal somebodyres = 0;\t\t\t\n"
	"\t\tlocal somebodyartifact = 0;\n"
	"\t\tlocal amountai = table.containsamount(H55_PlayerStatus,1);\r\n"
	"\t\tif H55_MapType ~= \"Campaign\" then\n"
	"\t\t\tfor i=1,8 do\n"
	"\t\t\t\tif (GetPlayerState(i) == 1) and (H55_IsThisAIPlayer(i) ~= 1) then\n"
	"\t\t\t\t\tlocal heroes = GetPlayerHeroes(i);\n"
	"\t\t\t\t\tif GetPlayerResource(i,6) > 25000 then somebodygold = 1; SetPlayerResource(i,6,25000); end;\n"
	"\t\t\t\t\tif heroes ~= nil then RemoveArtefact(heroes[0],1); somebodyartifact = 1; end;\n"
	"\t\t\t\t\tif heroes ~= nil then\n"
	"\t\t\t\t\t\tH55_ArtifactStartBonus(heroes[0]);\n"
	"\t\t\t\t\tend;\n"
	"\t\t\t\tend;\n"
	"\t\t\tend;\n"
	"\t\tend;\n"
	"\t\tif H55_MapType == \"Scenario\" then\r\n"
	"\t\t\tH55_GameMode = 1;\n"
	"\t\telseif H55_MapType == \"Campaign\" then\n"
	"\t\t\tH55_GameMode = 1;\n"
	"\t\telse\n"
	"\t\t\tif H55_GameMode ~= 0 and somebodyartifact == 1 then H55_GameMode = 1;\n"
	"\t\t\telseif H55_GameMode ~= 0 and somebodygold == 1 then H55_GameMode = 2; H55_TeamHumansvsAI();\n"
	"\t\t\telseif H55_GameMode ~= 0 and somebodyres == 1 then H55_GameMode = 3; H55_TeamHumansvsAI();\n"
	"\t\t\tend;\n"
	"\t\tend;\n"
	"\tend;\n"
	"\t----\r\n"
	"\tH55_DEBUG = {103,\"StartingArmies\",1,\"NoHero\"};----\r\n"
	"end;\r\n";

// The script without our lines and with the loop's condition as it was: the original?
std::string WithoutOurs(std::string s)
{
	for (;;) {
		const size_t at = s.find("-- H5 Together");
		if (at == std::string::npos)
			break;
		size_t b = at, e = s.find('\n', at);
		while (b > 0 && s[b - 1] != '\n')
			--b;
		s.erase(b, e - b + 1);
	}
	const char ours[] = " and OH55_KeepToEBonus ~= 1";
	const size_t at = s.find(ours);
	if (at != std::string::npos)
		s.erase(at, strlen(ours));
	return s;
}

void TestStartScript()
{
	const std::string lua = kStartSample;
	size_t b = 0, e = 0;
	Check(FindStartBlock(lua, &b, &e) && b < e && lua.compare(e, 15, "H55_DEBUG = {10") == 0, "the StartingBonus part is found");
	Check(StartBlockHash(lua) != 0 && StartBlockHash("no such part") == 0, "its hash (0 when not found)");
	std::string outside = lua;
	outside.replace(outside.find("ShamanPatterns"), 6, "Witchy");
	std::string inside = lua;
	inside.replace(inside.find("25000); end"), 5, "20000");
	Check(StartBlockHash(outside) == StartBlockHash(lua) && StartBlockHash(inside) != StartBlockHash(lua),
	      "the hash sees the part only");
	Check(ModeUsed(0, false) == 0 && ModeUsed(0, true) == 1 && ModeUsed(2, true) == 2 && ModeUsed(4, false) == 4 &&
	          ModeUsed(7, false) == 0,
	      "the mode used: the rule's, or Default with ToE bonuses and \"the arrows\"");

	std::string out, why;
	Check(PatchStartScript(lua, 0, false, false, &out, &why) && out == lua, "MMH5.5's way for both: the script as it is");

	out.clear();
	Check(PatchStartScript(lua, 0, true, false, &out, &why), "ToE bonuses: patched");
	Check(out.find("\t\tlocal amountai = table.containsamount(H55_PlayerStatus,1);\r\n"
	               "\t\tlocal OH55_KeepToEBonus, OH55_Mode = 1, 1; -- H5 Together") != std::string::npos,
	      "edit 1: the values after the computer players' count, its indent and line end; mode Default");
	Check(out.find("\t\tif H55_MapType ~= \"Campaign\" and OH55_KeepToEBonus ~= 1 then\n\t\t\tfor i=1,8 do") != std::string::npos,
	      "edit 2: the humans' loop is skipped when the ToE bonuses are kept");
	const std::string decide = "\t\tif OH55_Mode ~= 0 then if OH55_Mode == 4 then OH55_Mode = random(3)+1 end; "
	                           "somebodyartifact = 0; somebodygold = 0; somebodyres = 0; "
	                           "if OH55_Mode == 1 then somebodyartifact = 1 elseif OH55_Mode == 2 then somebodygold = 1 else "
	                           "somebodyres = 1 end; end; -- H5 Together: the game mode is the host's rule, not the bonus arrows\r\n"
	                           "\t\tif H55_MapType == \"Scenario\" then\r\n";
	Check(out.find(decide) != std::string::npos, "edit 3: the mode's flags just before the decision (its indent and line end)");
	Check(WithoutOurs(out) == lua, "nothing else changes");
	Check(out.find("print(") == std::string::npos, "no trace line without trace");

	Check(PatchStartScript(lua, 2, false, false, &out, &why) && out.find("OH55_KeepToEBonus, OH55_Mode = 0, 2;") != std::string::npos &&
	          WithoutOurs(out) == lua,
	      "Savage without the ToE bonuses: the loop stays (MMH5.5 takes the bonus back), the mode is Savage");
	Check(PatchStartScript(lua, 4, true, false, &out, &why) && out.find("OH55_KeepToEBonus, OH55_Mode = 1, 4;") != std::string::npos,
	      "Random: decided in the script by the game's shared random numbers");
	Check(PatchStartScript(lua, 3, true, true, &out, &why) && WithoutOurs(out) == lua &&
	          out.find("print(\"H5 Together: start of player \"") != std::string::npos &&
	          out.find("-- H5 Together: [rules] trace=1\r\n\tH55_DEBUG = {103,") != std::string::npos,
	      "trace: one print line just before the next part");
	Check(PatchStartScript(lua, 0, false, true, &out, &why) && out.find("OH55_Mode") == std::string::npos && WithoutOurs(out) == lua,
	      "trace alone: only the print line");

	// MMH5.5's text not as expected: refused, nothing served
	const auto refused = [](const std::string &text) {
		std::string o = "untouched", w;
		return !PatchStartScript(text, 1, true, false, &o, &w) && o == "untouched" && !w.empty();
	};
	const auto without = [&lua](const char *what) {
		std::string s = lua;
		s.erase(s.find(what), strlen(what));
		return s;
	};
	const auto twice = [&lua](const char *what) {
		std::string s = lua;
		const size_t at = s.find(what);
		s.insert(at, std::string(what) + "\n");
		return s;
	};
	Check(refused(without("H55_DEBUG = {102,\"StartingBonus\",1,\"NoHero\"};")) &&
	          refused(without("H55_DEBUG = {103,\"StartingArmies\",1,\"NoHero\"};")),
	      "refused: a marker is missing");
	Check(refused(without("local amountai = table.containsamount(H55_PlayerStatus,1);")) &&
	          refused(twice("if H55_MapType == \"Scenario\" then")) && refused(twice("if H55_MapType ~= \"Campaign\" then")),
	      "refused: an edit's place is missing or there twice");
	Check(refused(without("H55_ArtifactStartBonus(heroes[0]);")) && refused(without("SetPlayerResource(i,6,25000);")) &&
	          refused(without("local somebodyres = 0;")),
	      "refused: the loop no longer takes the bonus back, or a flag is not a local of the part");
	Check(refused(without("H55_GameMode ~= 0 and somebodygold == 1")), "refused: the decision no longer reads a flag");
	std::string moved = lua;
	const std::string scenario = "\t\tif H55_MapType == \"Scenario\" then\r\n";
	moved.erase(moved.find(scenario), scenario.size());
	moved.insert(moved.find("\t\tif H55_MapType ~= \"Campaign\" then"), scenario);
	Check(refused(moved), "refused: the places in another order");
	std::string o = "untouched", w;
	Check(PatchStartScript(without("H55_ArtifactStartBonus(heroes[0]);"), 0, false, false, &o, &w) &&
	          o == without("H55_ArtifactStartBonus(heroes[0]);"),
	      "MMH5.5's way needs no edit: any script is fine");
}

void TestHello()
{
	uint32_t type = 0;
	uint8_t byte = 0;
	HashRequest(kMarkSettingsRest, 0xdeadbeefu, &type, &byte);
	char mark = 0;
	uint32_t hash = 0;
	Check(type == 0x4eadbeefu && byte == 0xde, "a hash: its mark in the type's top byte, its top byte in the byte");
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkSettingsRest && hash == 0xdeadbeefu, "and back");
	HashRequest(kMarkStats, 0x00000001u, &type, &byte);
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkStats && hash == 1, "a small hash too");
	Check(type >= 0x4e000000u, "hash types are far above the game's request types (1, 2, ...)");
	HashRequest(kMarkStartScript, 0x00345678u, &type, &byte);
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkStartScript && hash == 0x00345678u && type >= 0x41000000u,
	      "the start script's hash ('A') too, also far above the game's types");
	HashRequest(kMarkClassTable, 0x9abcdef0u, &type, &byte);
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkClassTable && hash == 0x9abcdef0u && type >= 0x43000000u,
	      "the class table's hash ('C')");
	HashRequest(kMarkClassTable, 0, &type, &byte);
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkClassTable && hash == 0,
	      "a class table that cannot be edited is sent as hash 0");
	Check(!ReadHashRequest(kHelloType, kProtocol, &mark, &hash) && !ReadHashRequest(0x4f48, 1, &mark, &hash) &&
	          !ReadHashRequest(2, 0, &mark, &hash),
	      "the hellos (ours, the faction mod's) and the game's types are not hashes");
}

// H5 Together's version in the waiting room: the requests, the host's list, who differs.
void TestVersions()
{
	Check(PackVersion("0.3.0") == 0x000300u && PackVersion("1.2.3") == 0x010203u && PackVersion("0.2.7-test") == 0x000207u &&
	          PackVersion("255.255.255") == 0xffffffu,
	      "versions: packed in 24 bits (8 each), a suffix left out");
	Check(PackVersion("") == 0 && PackVersion("0.3") == 0 && PackVersion("0.3.x") == 0 && PackVersion("0.256.0") == 0 &&
	          PackVersion("1.2.3.4") == 0 && PackVersion("v0.3.0") == 0,
	      "versions: anything else packs to 0");
	Check(UnpackVersion(0x000300u) == "0.3.0" && UnpackVersion(PackVersion("12.0.99")) == "12.0.99", "versions: and back");
	uint32_t type = 0, hash = 0;
	uint8_t byte = 0;
	char mark = 0;
	HashRequest(kMarkVersion, PackVersion("0.3.0"), &type, &byte);
	Check(type == 0x56000300u && byte == 0 && ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkVersion && hash == 0x000300u,
	      "versions: the version request ('V'), far above the game's types");
	HashRequest(kMarkBuild, 0x6a5b4c3du, &type, &byte);
	Check(ReadHashRequest(type, byte, &mark, &hash) && mark == kMarkBuild && hash == 0x6a5b4c3du, "versions: the build request ('B')");

	Peer host;
	host.kind = Peer::kKnown, host.version = "0.3.0", host.build = 0x6a5b4c3du, host.name = "Joz";
	Peer bob = host, ann = host, eve, max, new_one, old_build = host;
	bob.name = "Bob", bob.version = "0.3.1";
	ann.name = "Ann | the second";  // a name with the separator
	eve.kind = Peer::kOlder, eve.name = "Ève";
	max.kind = Peer::kNone, max.name = "Max";
	new_one.kind = Peer::kWaiting, new_one.name = "Neo";
	old_build.name = "Tim", old_build.build = 0x11111111u;
	const std::vector<Peer> list = {host, bob, ann, eve, max, new_one, old_build};
	const std::string text = VersionsText(list);
	Check(text.compare(0, 3, "V1\n") == 0 && text.find("\nV|0.3.1|6a5b4c3d|Bob") != std::string::npos &&
	          text.find("\nO||00000000|\xc3\x88ve") != std::string::npos && text.size() <= static_cast<size_t>(kLobbyTextMax),
	      "versions: the host's list, one line per player, the host first");
	std::vector<Peer> back;
	Check(ReadVersionsText(text, &back) && back == list, "versions: read back the same (a '|' in a name is kept)");
	Check(IsVersionsText(text) && IsVersionsText("V2\nanything") && !IsVersionsText(LobbyText(1, NoValues())) && !IsVersionsText("V"),
	      "versions: a list is told from the rules' lobby text, also a later format");
	Check(!ReadVersionsText("V2\nV|0.4.0|00000000|Joz", &back) && !ReadVersionsText("V1", &back) && !ReadVersionsText("V1\n", &back) &&
	          !ReadVersionsText("V1\nX|0.3.0|00000000|Joz", &back) && !ReadVersionsText("V1\nV||00000000|Joz", &back) &&
	          !ReadVersionsText("V1\nN|0.3.0|00000000|Joz", &back) && !ReadVersionsText("V1\nV|0.3|00000000|Joz", &back) &&
	          !ReadVersionsText("V1\nV|0.3.0|0000000|Joz", &back) && !ReadVersionsText("V1\nV|0.3.0|0000000g|Joz", &back) &&
	          !ReadVersionsText("V1\nV|0.3.0|00000000", &back),
	      "versions: a later format, an empty list or a broken line is not read");
	const std::vector<Peer> differ = Differing(list, host);
	Check(differ.size() == 4 && differ[0].name == "Bob" && differ[1].name == "\xc3\x88ve" && differ[2].name == "Max" && differ[3].name == "Tim",
	      "versions: who differs: another version, an older one, none, another build; not the same one, not one still waiting");
	Check(Differing({host, ann}, host).empty(), "versions: all the same: nobody differs");
	Peer guest = bob;
	Check(Differing(list, guest).size() == 5, "versions: seen from a client with another version: everyone else differs");
}

// A class table in MMH5.5's shape (our own numbers, not MMH5.5's): three
// classes of 4 skills each, the chances adding up to 100.
ClassTable SampleClasses()
{
	const uint32_t head = 0x00e7aa00u;
	ClassTable t;
	t.push_back(ClassSkillList{1, {{head, 4, 40}, {head, 6, 30}, {head, 7, 20}, {head, 11, 10}}});
	t.push_back(ClassSkillList{3, {{head, 1, 25}, {head, 8, 25}, {head, 9, 25}, {head, 10, 25}}});
	t.push_back(ClassSkillList{9, {{head, 6, 50}, {head, 191, 20}, {head, 13, 20}, {head, 4, 10}}});
	return t;
}

void TestClassTable()
{
	const ClassTable t = SampleClasses();
	Check(CheckClassTable(t).empty(), "a plain table passes the check");
	ClassTable bad = t;
	bad[1].probs[2].skill = bad[1].probs[0].skill;
	Check(!CheckClassTable(bad).empty(), "a skill twice in a list is refused");
	bad = t;
	bad[0].probs[0].prob = -1;
	Check(!CheckClassTable(bad).empty(), "a chance below 0 is refused");
	bad = t;
	bad[2].probs[1].skill = 0;
	Check(!CheckClassTable(bad).empty(), "skill 0 (none) is refused");
	bad = t;
	for (SkillProb &p : bad[1].probs)
		p.prob = 0;
	Check(!CheckClassTable(bad).empty(), "a class whose chances add up to 0 is refused");
	bad = t;
	std::swap(bad[0], bad[1]);
	Check(!CheckClassTable(bad).empty() && !CheckClassTable(ClassTable()).empty(), "classes out of order, or none, are refused");
	bad = t;
	bad[0].probs.clear();
	Check(!CheckClassTable(bad).empty(), "an empty list is refused (the reader skips classes without one)");

	const std::vector<int> skills = OfferedSkills(t);
	Check(skills == std::vector<int>({1, 4, 6, 7, 8, 9, 10, 11, 13, 191}),
	      "the skills some class is offered: each once, from low to high");
	Check(ClassTableHash(t) == ClassTableHash(SampleClasses()), "the same table, the same hash");
	bad = t;
	bad[2].probs[3].prob = 11;
	Check(ClassTableHash(bad) != ClassTableHash(t), "another chance, another hash");
	bad = t;
	bad[2].id = 10;
	Check(ClassTableHash(bad) != ClassTableHash(t), "another class number, another hash");
	bad = t;
	for (ClassSkillList &c : bad)
		for (SkillProb &p : c.probs)
			p.head = 0x1234u;
	Check(ClassTableHash(bad) == ClassTableHash(t), "the items' first word is not in the hash (a pointer may differ between PCs)");

	const std::vector<SkillProb> list = UnlockedList(t[0].probs, skills, 2);
	bool own_first = list.size() == 10;
	for (size_t i = 0; own_first && i < t[0].probs.size(); ++i)
		own_first = list[i].head == t[0].probs[i].head && list[i].skill == t[0].probs[i].skill && list[i].prob == t[0].probs[i].prob;
	Check(own_first, "unlocked: the class's own items first, unchanged and in their order (4 + 6 others = 10)");
	bool added = true;
	const int expect[] = {1, 8, 9, 10, 13, 191};
	for (size_t i = 0; added && i < 6; ++i)
		added = list[4 + i].skill == expect[i] && list[4 + i].prob == 2 && list[4 + i].head == t[0].probs[0].head;
	Check(added, "then each skill it lacks, at the chance, with the list's head");
	long sum = 0;
	for (const SkillProb &p : list)
		sum += p.prob;
	Check(sum == 112, "the chances add up to 100 + 6 x 2 (the draw is in [0, sum - 1]: MMH5.5's skills keep their weights)");

	ClassTable out;
	std::string why;
	Check(!ClassTableFor(t, kClassLocked, "", &out, &why) && !ClassTableFor(t, "", "", &out, &why) && why.empty(),
	      "MMH5.5's lock (0 or no value): nothing to change, no error");
	Check(ClassTableFor(t, kClassUnlocked, "", &out, &why) && out.size() == 3 && out[0].probs.size() == 10 &&
	          out[1].probs.size() == 10 && out[2].probs.size() == 10 && out[1].probs[4].prob == 2,
	      "unlocked: every class gets the other skills, at vanilla's 2 when the chance has no value");
	Check(ClassTableFor(t, kClassUnlocked, "30", &out, &why) && out[2].probs[9].prob == 30 && out[2].probs[3].prob == 10,
	      "the testing chance (30) for the added skills only");
	Check(ClassChance("") == 2 && ClassChance("30") == 30 && ClassChance("0") == 2 && ClassChance("99") == 2,
	      "the chance used: the rule's value, else vanilla's 2");
	bool unique = true;
	for (const ClassSkillList &c : out)
		for (size_t i = 0; i < c.probs.size(); ++i)
			for (size_t j = 0; j < i; ++j)
				unique = unique && c.probs[i].skill != c.probs[j].skill;
	Check(unique && CheckClassTable(out).empty(), "the unlocked table has no skill twice and passes the check itself");
	why.clear();
	Check(!ClassTableFor(t, "2", "", &out, &why) && !why.empty(), "class_skills=2 (vanilla classes) is not built: refused, with why");
	why.clear();
	bad = t;
	bad[0].probs[1].skill = bad[0].probs[0].skill;
	Check(!ClassTableFor(bad, kClassUnlocked, "", &out, &why) && !why.empty(), "a bad table is not unlocked");

	// what a save keeps
	Values v = NoValues();
	Check(EncodeWorld(v) == "OHW1", "no class rules: \"OHW1\"");
	v[kClassSkills] = kClassUnlocked;
	v[kClassChance] = "2";
	v[Index("governance")] = "0";
	const std::string world = EncodeWorld(v);
	Check(world == "OHW1;class_skills=1;class_chance=2", "a save keeps only the rules the add-on applies in memory");
	Values back;
	Values expect_back = NoValues();
	expect_back[kClassSkills] = kClassUnlocked;
	expect_back[kClassChance] = "2";
	Check(DecodeWorld(world, &back) && back == expect_back, "and reads them back");
	Check(world.size() < 64, "it fits the world's chunk (64 bytes with its zero)");
	Check(!DecodeWorld("OHW1;governance=0", &back) && !DecodeWorld("OHW1;class_skills=3", &back) &&
	          !DecodeWorld("OHW2;class_skills=1", &back) && !DecodeWorld("OHR4;class_skills=1", &back) &&
	          !DecodeWorld("OHW1;class_skills=1;class_skills=1", &back) && !DecodeWorld("", &back),
	      "another key, a bad value, another version, a key twice or nothing is refused");
}

// A list of related textures and a texture record in the game's layout (texts of
// ours: no game file is copied here).
const char kTextureList[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
	"<WindowRelatedTextures>\r\n"
	"\t<textures>\r\n"
	"\t\t<Item>\r\n\t\t\t<TextureName>bonus0</TextureName>\r\n\t\t\t<Texture href=\"/A/Zero.(Texture).xdb#xpointer(/Texture)\"/>\r\n\t\t</Item>\r\n"
	"\t\t<Item>\r\n\t\t\t<TextureName>bonus1</TextureName>\r\n\t\t\t<Texture href=\"/A/One.(Texture).xdb#xpointer(/Texture)\"/>\r\n\t\t</Item>\r\n"
	"\t\t<Item>\r\n\t\t\t<TextureName>bonus2</TextureName>\r\n\t\t\t<Texture href=\"/A/Two.(Texture).xdb#xpointer(/Texture)\"/>\r\n\t\t</Item>\r\n"
	"\t\t<Item>\r\n\t\t\t<TextureName>bonus3</TextureName>\r\n\t\t\t<Texture href=\"/A/Three.(Texture).xdb#xpointer(/Texture)\"/>\r\n\t\t</Item>\r\n"
	"\t</textures>\r\n"
	"</WindowRelatedTextures>\r\n";
const char kGoldRecord[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Texture>\n\t<SrcName href=\"/Some/Gold.tga\"/>\n"
	"\t<DestName href=\"Gold.(Texture).dds\"/>\n\t<Width>55</Width>\n</Texture>\n";

void TestBonusIcons()
{
	Check(BonusTextureName(2, false) == "bonus2" && BonusTextureName(0, true) == "oh_toe_bonus0", "the pictures' names in the list");
	Check(BonusPicturePath(1, false, true) == "textures/interface/mpwait/playerslist/item/bonuses/randomartifact.(texture).dds",
	      "the game's picture of the minor artifact");
	Check(BonusPicturePath(3, true, false) == "textures/interface/mpwait/playerslist/item/bonuses/openhomm5-toe-resource.(texture).xdb",
	      "our record of the resources' picture");
	bool dds = false;
	bool all = true;
	for (int b = 0; b < 4; ++b)
		for (int d = 0; d < 2; ++d)
			all = all && OurBonusPicture(BonusPicturePath(b, true, d == 1), &dds) == b && dds == (d == 1);
	Check(all, "each of our eight files is recognised (bonus, picture or record)");
	Check(OurBonusPicture(BonusPicturePath(2, false, true), &dds) == -1 && OurBonusPicture("textures/x.(texture).dds", &dds) == -1 &&
	          OurBonusPicture(BonusPicturePath(2, true, true) + "x", &dds) == -1,
	      "the game's own pictures and other paths are not ours");

	// the list
	const std::string list = kTextureList;
	std::string out;
	Check(BonusTexturesWithToe(list, &out), "the list takes our four names");
	const std::string item2 =
		"\t\t<Item>\r\n\t\t\t<TextureName>oh_toe_bonus2</TextureName>\r\n\t\t\t<Texture "
		"href=\"/Textures/Interface/MPWait/PlayersList/Item/Bonuses/OpenHomm5-ToE-Gold.(Texture).xdb#xpointer(/Texture)\"/>\r\n"
		"\t\t</Item>\r\n";
	Check(out.find(item2) != std::string::npos && out.find("OpenHomm5-ToE-Resource.(Texture).xdb#xpointer(/Texture)\"/>\r\n"
	                                                       "\t\t</Item>\r\n\t</textures>\r\n") != std::string::npos,
	      "an item in the file's layout (indent, CRLF), naming our record of ToE's gold; the last one before the list's end");
	Check(out.compare(0, list.find("\t</textures>"), list, 0, list.find("\t</textures>")) == 0 &&
	          out.compare(out.size() - strlen("\t</textures>\r\n</WindowRelatedTextures>\r\n"), std::string::npos,
	                      "\t</textures>\r\n</WindowRelatedTextures>\r\n") == 0,
	      "the game's items come first, unchanged; the end is unchanged");
	size_t ours = 0;
	for (size_t p = out.find("oh_toe_bonus"); p != std::string::npos; p = out.find("oh_toe_bonus", p + 1))
		++ours;
	Check(ours == 4 && out.size() > list.size() && out.find("oh_toe_bonus0") < out.find("oh_toe_bonus3"), "four names, in order");
	std::string untouched = "x";
	Check(!BonusTexturesWithToe(out, &untouched) && untouched == "x", "a list that has our names already is refused");
	std::string missing = list;
	missing.replace(missing.find("bonus3"), 6, "bonus4");
	std::string twice = list;
	twice.insert(twice.find("\t</textures>"), "\t\t<Item>\r\n\t\t\t<TextureName>bonus1</TextureName>\r\n\t\t</Item>\r\n");
	std::string no_end = list;
	no_end.erase(no_end.find("\t</textures>"), strlen("\t</textures>\r\n"));
	std::string same_line = list;
	same_line.replace(same_line.find("</Item>\r\n\t</textures>"), strlen("</Item>\r\n\t</textures>"), "</Item></textures>");
	Check(!BonusTexturesWithToe(missing, &untouched) && !BonusTexturesWithToe(twice, &untouched) &&
	          !BonusTexturesWithToe(no_end, &untouched) && !BonusTexturesWithToe(same_line, &untouched) && untouched == "x",
	      "a missing or doubled name, no end of the list, or the end not on its own line: refused");
	std::string lf = list;
	for (size_t p = lf.find("\r\n"); p != std::string::npos; p = lf.find("\r\n", p))
		lf.erase(p, 1);
	Check(BonusTexturesWithToe(lf, &out) && out.find('\r') == std::string::npos && out.find("oh_toe_bonus3") != std::string::npos,
	      "a list with LF line ends gets LF items");

	// the record
	std::string record;
	Check(ToeTextureRecord(kGoldRecord, 2, &record) &&
	          record == std::string(kGoldRecord).replace(std::string(kGoldRecord).find("Gold.(Texture).dds"), 0, "OpenHomm5-ToE-"),
	      "ToE's gold record names our picture, the rest unchanged");
	record = "x";
	std::string two = std::string(kGoldRecord) + "<DestName href=\"Gold.(Texture).dds\"/>";
	Check(!ToeTextureRecord(kGoldRecord, 3, &record) && !ToeTextureRecord(two, 2, &record) &&
	          !ToeTextureRecord("<DestName href=\"Gold.(Texture).dds\"/>", 2, &record) && record == "x",
	      "another bonus's record, two DestNames, or not a texture record: refused");
	std::string lower = kGoldRecord;
	lower.replace(lower.find("Gold.(Texture)"), 14, "gold.(texture)");
	Check(ToeTextureRecord(lower, 2, &record) && record.find("\"OpenHomm5-ToE-Gold.(Texture).dds\"") != std::string::npos,
	      "the picture's name in any case");

	Check(IsDds(std::string("DDS ") + std::string(124, '\0')) && !IsDds("DDS ") && !IsDds(std::string(200, 'x')),
	      "a picture is a .dds with its header");
}

}  // namespace

int main()
{
	TestTable();
	TestValues();
	TestParse();
	TestSubstitute();
	TestHash();
	TestText();
	TestPieces();
	TestMapScript();
	TestStartScript();
	TestHello();
	TestVersions();
	TestClassTable();
	TestBonusIcons();
	printf(g_failures ? "%d FAILED\n" : "all host rules tests passed\n", g_failures);
	return g_failures ? 1 : 0;
}

// Test of the host rules' pure work (src/mods/host_rules_logic.h; no game
// needed): reading MMH5.5's settings file, serving it with other values (only
// the numbers change), the hash without the rules' values, the text carried in
// the start message and the lobby, and the lobby hello's number codes.
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
		start = start && (IsStartRule(r) || r == kMapScripts) == (kRules[r].var == nullptr) &&
		        (IsStartRule(r) == (kRules[r].group == kStart)) && (!IsStartRule(r) || ValidValue(kRules[r], kStartRuleOff));
	Check(start, "the start rules: no settings variable, their own group, \"0\" = MMH5.5's way");
	Check(ValidValue(kRules[kGameModeRule], "4") && !ValidValue(kRules[kGameModeRule], "5") &&
	          ValidValue(kRules[kToeBonusRule], "1") && !ValidValue(kRules[kToeBonusRule], "2"),
	      "game mode 0 to 4, ToE bonuses 0 or 1");
	Check(kMapScripts == Index("map_scripts") && kMapScripts == kRuleCount - 1 && !kRules[kMapScripts].var &&
	          kRules[kMapScripts].kind == kBool && kRules[kMapScripts].group == kRandomMaps && !IsStartRule(kMapScripts),
	      "map_scripts: the table's last, no settings variable (the add-on applies it), on/off, with the random maps");
	Check(kProtocol == 3, "protocol 3 (map_scripts after the start rules): a PC with version 1 or 2 gets no rules");
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
	Check(added == "H55_RPGPotions = 1;\r\nH55_BanksDifficulty = 2; -- open-homm5: the host's rules\r\n",
	      "a missing variable gets a line at the end");
	Check(Substitute("H55_RPGPotions = 0;", only, nullptr) ==
	          "H55_RPGPotions = 1;\nH55_BanksDifficulty = 2; -- open-homm5: the host's rules\n",
	      "no line end at the end of the file: one is added first");
	// a variable set by an expression: our line at the end wins
	Values ai = NoValues();
	ai[Index("ai_cheat")] = "2";
	const std::string expr = Substitute("H55_AICheatMode = 1;\nH55_AICheatMode = random(3);\n", ai, nullptr);
	const std::string tail = "H55_AICheatMode = 2; -- open-homm5: the host's rules\n";
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
	Check(Encode(v) == "OHR3", "no values: \"OHR3\"");
	v[Index("governance")] = "0";
	v[Index("ai_cheat")] = "0";
	v[Index("neutral_stack_size")] = "1.25";
	const std::string text = Encode(v);
	Check(text == "OHR3;governance=0;ai_cheat=0;neutral_stack_size=1.25", "values in the table's order");
	Values back;
	Check(Decode(text, &back) && back == v, "decoded back to the same values");
	Values with_start = v;
	with_start[kGameModeRule] = "3";
	with_start[kToeBonusRule] = "1";
	Check(Encode(with_start) == text + ";game_mode=3;toe_bonuses=1" && Decode(Encode(with_start), &back) && back == with_start,
	      "the start rules travel too");
	Check(!Decode("OHR1;governance=0", &back) && !Decode("OHR2;governance=0", &back), "another version (1, 2) is refused");
	Check(!Decode("OHR3;governance=2", &back) && !Decode("OHR3;game_mode=5", &back), "a bad value is refused");
	Check(!Decode("OHR3;governance=0;governance=1", &back), "a key twice is refused");
	Check(!Decode("OHR3;new_rule=1", &back), "an unknown key is refused (a newer host: nothing is used)");
	Check(!Decode("OHR3;governance", &back) && !Decode("OHR3governance=0", &back) && !Decode("OHR3;", &back),
	      "broken texts are refused");
	Values bad = NoValues();
	bad[Index("governance")] = "7";
	Check(Encode(bad) == "OHR3", "invalid values are never sent");
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
	Check(!Decode("OHR3;map_scripts=2", &back), "map_scripts takes 0 or 1 only");
	Check(!Decode("OHR2;governance=0;game_mode=3", &back), "a version 2 text (no map_scripts) is refused");

	const std::string lobby = LobbyText(kWillApply | kDiffers, v);
	int flags = 0;
	Check(lobby == "L3|3|0,,,,,,0,,1.25,,,,,,,,," && ReadLobbyText(lobby, &flags, &back) && flags == 3 && back == v,
	      "the lobby text: L3|flags|every value in the table's order");
	Check(LobbyText(kWillApply | kNoStart, with_start) == "L3|9|0,,,,,,0,,1.25,,,,,,,3,1," &&
	          ReadLobbyText(LobbyText(kWillApply | kNoStart, with_start), &flags, &back) && flags == 9 && back == with_start,
	      "the start rules and their flag in the lobby text");
	Check(LobbyText(kNoScript, scripts) == "L3|4|0,,,,,,0,,1.25,,,,,,,,,1" && ReadLobbyText(LobbyText(kNoScript, scripts), &flags, &back) &&
	          flags == 4 && back == scripts,
	      "map_scripts in the lobby text (the last value)");
	Check(!ReadLobbyText(text, &flags, &back) && !ReadLobbyText("L3|x|0,,,,,,0,,1.25,,,,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L3||0,,,,,,0,,1.25,,,,,,,,,", &flags, &back) && !ReadLobbyText("L2|3|0,,,,,,0,,1.25,,,,,,,,", &flags, &back),
	      "a lobby text without flags, or of another version, is refused");
	Check(!ReadLobbyText("L3|1|0,,,,,,0,,1.25,,,,,,,,", &flags, &back) && !ReadLobbyText("L3|1|0,,,,,,0,,1.25,,,,,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L3|1|7,,,,,,0,,1.25,,,,,,,,,", &flags, &back),
	      "too few or too many values (a version 2 text has 17), or a bad one, is refused");
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
		const size_t at = s.find("-- open-homm5");
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
	               "\t\tlocal OH55_KeepToEBonus, OH55_Mode = 1, 1; -- open-homm5") != std::string::npos,
	      "edit 1: the values after the computer players' count, its indent and line end; mode Default");
	Check(out.find("\t\tif H55_MapType ~= \"Campaign\" and OH55_KeepToEBonus ~= 1 then\n\t\t\tfor i=1,8 do") != std::string::npos,
	      "edit 2: the humans' loop is skipped when the ToE bonuses are kept");
	const std::string decide = "\t\tif OH55_Mode ~= 0 then if OH55_Mode == 4 then OH55_Mode = random(3)+1 end; "
	                           "somebodyartifact = 0; somebodygold = 0; somebodyres = 0; "
	                           "if OH55_Mode == 1 then somebodyartifact = 1 elseif OH55_Mode == 2 then somebodygold = 1 else "
	                           "somebodyres = 1 end; end; -- open-homm5: the game mode is the host's rule, not the bonus arrows\r\n"
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
	          out.find("print(\"open-homm5: start of player \"") != std::string::npos &&
	          out.find("-- open-homm5: [rules] trace=1\r\n\tH55_DEBUG = {103,") != std::string::npos,
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
	Check(!ReadHashRequest(kHelloType, kProtocol, &mark, &hash) && !ReadHashRequest(0x4f48, 1, &mark, &hash) &&
	          !ReadHashRequest(2, 0, &mark, &hash),
	      "the hellos (ours, the faction mod's) and the game's types are not hashes");
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
	printf(g_failures ? "%d FAILED\n" : "all host rules tests passed\n", g_failures);
	return g_failures ? 1 : 0;
}

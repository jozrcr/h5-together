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
			if (strcmp(kRules[a].key, kRules[b].key) == 0 || strcmp(kRules[a].var, kRules[b].var) == 0)
				unique = false;
	Check(unique, "every rule has its own key and variable");
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
	Check(Encode(v) == "OHR1", "no values: \"OHR1\"");
	v[Index("governance")] = "0";
	v[Index("ai_cheat")] = "0";
	v[Index("neutral_stack_size")] = "1.25";
	const std::string text = Encode(v);
	Check(text == "OHR1;governance=0;ai_cheat=0;neutral_stack_size=1.25", "values in the table's order");
	Values back;
	Check(Decode(text, &back) && back == v, "decoded back to the same values");
	Check(!Decode("OHR2;governance=0", &back), "another version is refused");
	Check(!Decode("OHR1;governance=2", &back), "a bad value is refused");
	Check(!Decode("OHR1;governance=0;governance=1", &back), "a key twice is refused");
	Check(!Decode("OHR1;new_rule=1", &back), "an unknown key is refused (a newer host: nothing is used)");
	Check(!Decode("OHR1;governance", &back) && !Decode("OHR1governance=0", &back) && !Decode("OHR1;", &back),
	      "broken texts are refused");
	Values bad = NoValues();
	bad[Index("governance")] = "7";
	Check(Encode(bad) == "OHR1", "invalid values are never sent");
	// the longest text: every rule with an 8-character value
	Values longest = NoValues();
	for (int r = 0; r < kRuleCount; ++r)
		longest[r] = kRules[r].kind == kMultiplier ? "9.999999" : kRules[r].kind == kBool ? "1" : std::to_string(kRules[r].high);
	const std::string all = Encode(longest);
	Check(all.size() < 480 && Decode(all, &back) && back == longest,
	      "every rule at its longest fits (start message chunk: 512 bytes with its zero)");
	Check(all.size() < 512 - 1, "the chunk keeps a final zero");

	const std::string lobby = LobbyText(kWillApply | kDiffers, v);
	int flags = 0;
	Check(lobby == "L1|3|0,,,,,,0,,1.25,,,,,," && ReadLobbyText(lobby, &flags, &back) && flags == 3 && back == v,
	      "the lobby text: L1|flags|every value in the table's order");
	Check(!ReadLobbyText(text, &flags, &back) && !ReadLobbyText("L1|x|0,,,,,,0,,1.25,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L1||0,,,,,,0,,1.25,,,,,,", &flags, &back) && !ReadLobbyText("L2|3|0,,,,,,0,,1.25,,,,,,", &flags, &back),
	      "a lobby text without flags, or of another version, is refused");
	Check(!ReadLobbyText("L1|1|0,,,,,,0,,1.25,,,,,", &flags, &back) && !ReadLobbyText("L1|1|0,,,,,,0,,1.25,,,,,,,", &flags, &back) &&
	          !ReadLobbyText("L1|1|7,,,,,,0,,1.25,,,,,,", &flags, &back),
	      "too few or too many values, or a bad one, is refused");
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
	TestHello();
	printf(g_failures ? "%d FAILED\n" : "all host rules tests passed\n", g_failures);
	return g_failures ? 1 : 0;
}

// Host rules: the pure work (see host_rules_logic.h).
#include "host_rules_logic.h"

#include <stdlib.h>
#include <string.h>

namespace mods {
namespace rules {

const RuleDef kRules[kRuleCount] = {
	{"governance", "H55_GovernanceEnabled", kBool, 0, 1, kTowns},
	{"town_conversion", "H55_TownConvEnabled", kBool, 0, 1, kTowns},
	{"extra_conversions", "H55_AmountExtraTownsDwellings", kInt, 0, 10, kTowns},
	{"town_gate", "H55_TownGateEnabled", kBool, 0, 1, kTowns},
	{"mine_control", "H55_MineControl", kBool, 0, 1, kTowns},
	{"dimension_doors", "H55_DimensionDoors", kBool, 0, 1, kTowns},
	{"ai_cheat", "H55_AICheatMode", kChoice, 0, 6, kComputer},
	{"mixed_stacks", "H55_GenerateMixedStacks", kBool, 0, 1, kComputer},
	{"neutral_stack_size", "H55_NeutralStackSize", kMultiplier, 0, 0, kComputer},
	{"bank_difficulty", "H55_BanksDifficulty", kMultiplier, 0, 0, kComputer},
	{"negative_artifacts", "H55_NegativeArtifacts", kBool, 0, 1, kItems},
	{"rpg_potions", "H55_RPGPotions", kBool, 0, 1, kItems},
	{"competitive_towns", "H55_CompetitiveARMGTowns", kBool, 0, 1, kRandomMaps},
	{"mana_observatories", "H55_ManaObservatories", kBool, 0, 1, kRandomMaps},
	{"moon_disc", "H55_MoonDiscOnARMGMaps", kBool, 0, 1, kRandomMaps},
	{"game_mode", nullptr, kChoice, 0, 4, kStart},       // kGameModeRule
	{"toe_bonuses", nullptr, kBool, 0, 1, kStart},       // kToeBonusRule
	{"map_scripts", nullptr, kBool, 0, 1, kRandomMaps},  // kMapScripts: not in the file
};

const char *const kMultipliers[kMultiplierCount] = {"0.5", "0.75", "1", "1.25", "1.5", "2", "3"};

int RuleIndex(const std::string &key)
{
	for (int i = 0; i < kRuleCount; ++i)
		if (key == kRules[i].key)
			return i;
	return -1;
}

Values NoValues()
{
	return Values(kRuleCount);
}

namespace {

bool Digit(char c)
{
	return c >= '0' && c <= '9';
}

bool IdentStart(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool IdentChar(char c)
{
	return IdentStart(c) || Digit(c);
}

// "12", "1.25": digits, at most one dot with digits on both sides.
bool PlainNumber(const std::string &s)
{
	if (s.empty() || s.size() > 8 || !Digit(s[0]) || !Digit(s[s.size() - 1]))
		return false;
	int dots = 0;
	for (char c : s) {
		if (c == '.')
			++dots;
		else if (!Digit(c))
			return false;
	}
	return dots <= 1;
}

// The value of a plain number, in thousandths (1.25 -> 1250); -1 when too big.
long Milli(const std::string &s)
{
	long whole = 0, frac = 0, scale = 1000;
	size_t i = 0;
	for (; i < s.size() && s[i] != '.'; ++i) {
		whole = whole * 10 + (s[i] - '0');
		if (whole > 1000000)
			return -1;
	}
	for (++i; i < s.size(); ++i) {
		scale /= 10;
		frac += (s[i] - '0') * scale;
	}
	return whole * 1000 + frac;
}

// One plain assignment found in the file: the variable's rule and where its
// number is.
struct Spot {
	int rule;
	size_t at, len;
};

// Every statement "NAME = ..." at the start of a line (or after a ";" on it),
// outside comments: the plain ones (a number) as spots, and for each rule
// whether it also has another kind of assignment (then Lua's last word is
// not a number we can see, and Substitute adds a line at the end).
void Scan(const std::string &lua, std::vector<Spot> *spots, std::vector<bool> *other)
{
	other->assign(kRuleCount, false);
	const size_t n = lua.size();
	size_t i = 0;
	bool in_block = false;  // inside --[[ ... ]]
	while (i < n) {
		size_t eol = lua.find('\n', i);
		if (eol == std::string::npos)
			eol = n;
		size_t p = i;
		if (in_block) {
			const size_t close = lua.find("]]", p);
			if (close == std::string::npos || close >= eol) {
				i = eol + 1;
				continue;
			}
			in_block = false;
			p = close + 2;
		}
		for (;;) {
			while (p < eol && (lua[p] == ' ' || lua[p] == '\t'))
				++p;
			if (p + 1 < eol && lua[p] == '-' && lua[p + 1] == '-') {
				if (lua.compare(p, 4, "--[[") == 0) {
					const size_t close = lua.find("]]", p + 4);
					if (close == std::string::npos || close >= eol) {
						in_block = true;
						break;
					}
					p = close + 2;
					continue;
				}
				break;  // a line comment
			}
			if (p >= eol || !IdentStart(lua[p]))
				break;
			const size_t name_at = p;
			while (p < eol && IdentChar(lua[p]))
				++p;
			const std::string name = lua.substr(name_at, p - name_at);
			while (p < eol && (lua[p] == ' ' || lua[p] == '\t'))
				++p;
			if (p >= eol || lua[p] != '=' || (p + 1 < eol && lua[p + 1] == '='))
				break;  // not an assignment (a call, a keyword...): the rest of the line is left alone
			++p;
			while (p < eol && (lua[p] == ' ' || lua[p] == '\t'))
				++p;
			int rule = -1;
			for (int r = 0; r < kRuleCount; ++r)
				if (kRules[r].var && name == kRules[r].var)
					rule = r;
			const size_t value_at = p;
			while (p < eol && (Digit(lua[p]) || lua[p] == '.'))
				++p;
			const std::string value = lua.substr(value_at, p - value_at);
			const char next = p < eol ? lua[p] : '\n';
			const bool plain = PlainNumber(value) && (next == ' ' || next == '\t' || next == ';' || next == '\r' || next == '\n' ||
			                                          (next == '-' && p + 1 < eol && lua[p + 1] == '-'));
			if (rule >= 0) {
				if (plain)
					spots->push_back({rule, value_at, value.size()});
				else
					(*other)[rule] = true;
			}
			if (!plain)
				break;  // an expression: the rest of the line is not read
			while (p < eol && (lua[p] == ' ' || lua[p] == '\t'))
				++p;
			if (p < eol && lua[p] == ';')
				++p;
		}
		i = eol + 1;
	}
}

}  // namespace

bool ValidValue(const RuleDef &rule, const std::string &value)
{
	if (!PlainNumber(value))
		return false;
	if (rule.kind == kMultiplier) {
		const long m = Milli(value);
		return m > 0 && m <= 10000;
	}
	if (value.find('.') != std::string::npos || value.size() > 3)
		return false;
	const int v = atoi(value.c_str());
	return v >= rule.low && v <= rule.high && !(value.size() > 1 && value[0] == '0');
}

Values ParseSettings(const std::string &lua)
{
	std::vector<Spot> spots;
	std::vector<bool> other;
	Scan(lua, &spots, &other);
	Values values = NoValues();
	for (const Spot &s : spots)
		values[s.rule] = lua.substr(s.at, s.len);  // the last one wins, as in Lua
	for (int r = 0; r < kRuleCount; ++r)
		if (other[r])
			values[r].clear();  // another kind of assignment: not a value we can read
	return values;
}

std::string Substitute(const std::string &lua, const Values &values, std::vector<std::string> *changes)
{
	std::vector<Spot> spots;
	std::vector<bool> other;
	Scan(lua, &spots, &other);
	const Values before = ParseSettings(lua);
	std::string out;
	out.reserve(lua.size() + 256);
	size_t from = 0;
	std::vector<bool> seen(kRuleCount, false);
	for (const Spot &s : spots) {  // in file order
		seen[s.rule] = true;
		if (static_cast<size_t>(s.rule) >= values.size() || values[s.rule].empty())
			continue;
		out.append(lua, from, s.at - from);
		out += values[s.rule];
		from = s.at + s.len;
	}
	out.append(lua, from, std::string::npos);
	const char *eol = lua.find("\r\n") != std::string::npos ? "\r\n" : "\n";
	bool added = false;
	for (int r = 0; r < kRuleCount; ++r) {
		if (!kRules[r].var || static_cast<size_t>(r) >= values.size() || values[r].empty() || (seen[r] && !other[r]))
			continue;
		if (!added && !out.empty() && out[out.size() - 1] != '\n')
			out += eol;
		added = true;
		out += kRules[r].var;
		out += " = ";
		out += values[r];
		out += "; -- open-homm5: the host's rules";
		out += eol;
	}
	if (changes)
		for (int r = 0; r < kRuleCount; ++r)
			if (kRules[r].var && static_cast<size_t>(r) < values.size() && !values[r].empty() && values[r] != before[r])
				changes->push_back(std::string(kRules[r].key) + " " + (before[r].empty() ? "?" : before[r]) + "->" + values[r]);
	return out;
}

uint32_t Fnv1a(const void *data, size_t size, uint32_t hash)
{
	const unsigned char *p = static_cast<const unsigned char *>(data);
	for (size_t i = 0; i < size; ++i) {
		hash ^= p[i];
		hash *= 16777619u;
	}
	return hash;
}

uint32_t HashWithoutRuleValues(const std::string &lua)
{
	std::vector<Spot> spots;
	std::vector<bool> other;
	Scan(lua, &spots, &other);
	uint32_t hash = 2166136261u;
	size_t from = 0;
	for (const Spot &s : spots) {
		hash = Fnv1a(lua.data() + from, s.at - from, hash);
		from = s.at + s.len;
	}
	return Fnv1a(lua.data() + from, lua.size() - from, hash);
}

std::string Encode(const Values &values)
{
	std::string text = "OHR3";
	for (int r = 0; r < kRuleCount; ++r)
		if (static_cast<size_t>(r) < values.size() && ValidValue(kRules[r], values[r]))
			text += std::string(";") + kRules[r].key + "=" + values[r];
	return text.size() <= static_cast<size_t>(kTextMax) ? text : std::string("OHR3");
}

bool Decode(const std::string &text, Values *values)
{
	if (text.compare(0, 4, "OHR3") != 0 || text.size() > static_cast<size_t>(kTextMax))
		return false;
	Values v = NoValues();
	size_t p = 4;
	while (p < text.size()) {
		if (text[p] != ';')
			return false;
		const size_t end = text.find(';', p + 1);
		const std::string item = text.substr(p + 1, end == std::string::npos ? std::string::npos : end - p - 1);
		const size_t eq = item.find('=');
		if (eq == std::string::npos)
			return false;
		const int r = RuleIndex(item.substr(0, eq));
		const std::string value = item.substr(eq + 1);
		if (r < 0 || !v[r].empty() || !ValidValue(kRules[r], value))
			return false;
		v[r] = value;
		p = end == std::string::npos ? text.size() : end;
	}
	*values = v;
	return true;
}

std::string LobbyText(int flags, const Values &values)
{
	std::string text = "L3|" + std::to_string(flags & 0xff) + "|";
	for (int r = 0; r < kRuleCount; ++r) {
		if (r)
			text += ",";
		if (static_cast<size_t>(r) < values.size() && ValidValue(kRules[r], values[r]))
			text += values[r];
	}
	return text;
}

bool ReadLobbyText(const std::string &text, int *flags, Values *values)
{
	if (text.compare(0, 3, "L3|") != 0)
		return false;
	const size_t bar = text.find('|', 3);
	if (bar == std::string::npos || bar == 3 || bar > 6)
		return false;
	for (size_t i = 3; i < bar; ++i)
		if (!Digit(text[i]))
			return false;
	Values v = NoValues();
	size_t p = bar + 1;
	for (int r = 0; r < kRuleCount; ++r) {
		const size_t comma = text.find(',', p);
		const bool last = r == kRuleCount - 1;
		if (last != (comma == std::string::npos))
			return false;  // too few or too many values
		const std::string value = text.substr(p, last ? std::string::npos : comma - p);
		if (!value.empty() && !ValidValue(kRules[r], value))
			return false;
		v[r] = value;
		p = comma + 1;
	}
	*flags = atoi(text.c_str() + 3);
	*values = v;
	return true;
}

bool HasMapScript(const std::string &map_xdb)
{
	const size_t at = map_xdb.find("<MapScript");
	if (at == std::string::npos)
		return false;
	const size_t end = map_xdb.find('>', at);
	const size_t href = map_xdb.find("href=\"", at);
	return end != std::string::npos && href != std::string::npos && href < end && map_xdb.compare(href + 6, 1, "\"") != 0;
}

const unsigned char kNoScriptJump[6] = {0x0f, 0x84, 0x4e, 0x01, 0x00, 0x00};  // jz 0x00992e7e

int OwnMapScripts(const unsigned char code[6])
{
	return memcmp(code, kNoScriptJump, sizeof(kNoScriptJump)) == 0 ? 0 : 1;
}

MapScriptAction MapScriptActionFor(bool map_has_script, const std::string &value)
{
	if (map_has_script || value.empty())
		return kAsIs;
	return value == "1" ? kEmptyScript : kNoScripts;
}

// --- MMH5.5's start script -------------------------------------------------------------------------
namespace {

const char kStartMarker[] = "H55_DEBUG = {102,\"StartingBonus\",1,\"NoHero\"};";
const char kEndMarker[] = "H55_DEBUG = {103,\"StartingArmies\",1,\"NoHero\"};";
// The edits' places (each must be in the part exactly once).
const char kAmountAi[] = "local amountai = table.containsamount(H55_PlayerStatus,1);";
const char kHumansLoop[] = "if H55_MapType ~= \"Campaign\" then";
const char kModeDecision[] = "if H55_MapType == \"Scenario\" then";
// What the edits rely on: the three flags are locals of the part, declared
// before the computer players' count; the humans' loop (between edits 2 and 3)
// takes the bonus back and gives MMH5.5's artifact; the mode's decision (after
// edit 3) reads the three flags.
const char *const kFlags[] = {"local somebodygold = 0;", "local somebodyres = 0;", "local somebodyartifact = 0;"};
const char *const kInLoop[] = {"for i=1,8 do", "(H55_IsThisAIPlayer(i) ~= 1)", "SetPlayerResource(i,6,", "RemoveArtefact(heroes[0]",
                               "H55_ArtifactStartBonus(heroes[0]);"};
const char *const kInDecision[] = {"H55_GameMode ~= 0 and somebodyartifact == 1", "H55_GameMode ~= 0 and somebodygold == 1",
                                   "H55_GameMode ~= 0 and somebodyres == 1"};

// Where `what` is in [from, to) when it is there exactly once; npos otherwise.
size_t FindOnce(const std::string &s, const char *what, size_t from, size_t to)
{
	const size_t n = strlen(what);
	const size_t at = s.find(what, from);
	if (at == std::string::npos || at + n > to)
		return std::string::npos;
	const size_t again = s.find(what, at + 1);
	return again != std::string::npos && again + n <= to ? std::string::npos : at;
}

// Is `what` in [from, to)?
bool Within(const std::string &s, const char *what, size_t from, size_t to)
{
	const size_t at = s.find(what, from);
	return at != std::string::npos && at + strlen(what) <= to;
}

size_t LineStart(const std::string &s, size_t at)
{
	while (at > 0 && s[at - 1] != '\n')
		--at;
	return at;
}

// The line's own end, "\r\n" or "\n" (MMH5.5's file mixes both).
std::string LineEnd(const std::string &s, size_t at)
{
	const size_t nl = s.find('\n', at);
	return nl != std::string::npos && nl > 0 && s[nl - 1] == '\r' ? "\r\n" : "\n";
}

std::string Indent(const std::string &s, size_t line)
{
	size_t e = line;
	while (e < s.size() && (s[e] == ' ' || s[e] == '\t'))
		++e;
	return s.substr(line, e - line);
}

}  // namespace

bool FindStartBlock(const std::string &lua, size_t *begin, size_t *end)
{
	const size_t b = FindOnce(lua, kStartMarker, 0, lua.size());
	const size_t e = FindOnce(lua, kEndMarker, 0, lua.size());
	if (b == std::string::npos || e == std::string::npos || e <= b)
		return false;
	*begin = b;
	*end = e;
	return true;
}

uint32_t StartBlockHash(const std::string &lua)
{
	size_t b, e;
	if (!FindStartBlock(lua, &b, &e))
		return 0;
	const uint32_t h = Fnv1a(lua.data() + b, e - b);
	return h ? h : 1;
}

int ModeUsed(int game_mode, bool toe_bonuses)
{
	if (game_mode < 0 || game_mode > 4)
		return 0;
	return game_mode == 0 && toe_bonuses ? 1 : game_mode;
}

bool PatchStartScript(const std::string &lua, int game_mode, bool toe_bonuses, bool trace, std::string *out, std::string *why)
{
	const int mode = ModeUsed(game_mode, toe_bonuses);
	if (mode == 0 && !toe_bonuses && !trace) {
		*out = lua;
		return true;
	}
	size_t b, e;
	if (!FindStartBlock(lua, &b, &e)) {
		*why = "its StartingBonus part (from H55_DEBUG = {102,\"StartingBonus\" to {103,\"StartingArmies\") is not there once";
		return false;
	}
	const size_t amount = FindOnce(lua, kAmountAi, b, e);
	const size_t loop = FindOnce(lua, kHumansLoop, b, e);
	const size_t decision = FindOnce(lua, kModeDecision, b, e);
	if (amount == std::string::npos || loop == std::string::npos || decision == std::string::npos || !(amount < loop && loop < decision)) {
		*why = std::string("the lines \"") + kAmountAi + "\", \"" + kHumansLoop + "\" and \"" + kModeDecision +
		       "\" are not each there once, in this order";
		return false;
	}
	for (const char *flag : kFlags)
		if (FindOnce(lua, flag, b, amount) == std::string::npos) {
			*why = std::string("\"") + flag + "\" is not there once before the computer players' count";
			return false;
		}
	for (const char *need : kInLoop)
		if (!Within(lua, need, loop, decision)) {
			*why = std::string("the humans' loop has no \"") + need + "\"";
			return false;
		}
	for (const char *need : kInDecision)
		if (FindOnce(lua, need, decision, e) == std::string::npos) {
			*why = std::string("the mode's decision has no \"") + need + "\" (once)";
			return false;
		}
	if (lua.find('\n', amount) == std::string::npos || lua.find('\n', amount) > loop) {
		*why = "no line end after the computer players' count";
		return false;
	}
	// The edits, from the last place to the first: the earlier places stay where they are.
	std::string s = lua;
	if (trace) {
		const size_t line = LineStart(s, e);
		const std::string text =
			Indent(s, line) +
			"for i=1,8 do if GetPlayerState(i) == 1 then local h = GetPlayerHeroes(i); local a = \"\"; "
			"if h ~= nil and h[0] ~= nil then if HasArtefact(h[0],ARTIFACT_WAND_OF_X) == 1 then a = a..\" wand\" end; "
			"if HasArtefact(h[0],ARTIFACT_SCROLL_OF_SPELL_X) == 1 then a = a..\" scroll\" end; "
			"for j,m in H55_MinorArtifacts do if HasArtefact(h[0],m) == 1 then a = a..\" \"..m end end end; "
			"print(\"open-homm5: start of player \"..i..\" (computer \"..H55_IsThisAIPlayer(i)..\"): gold \"..GetPlayerResource(i,6)"
			"..\" wood \"..GetPlayerResource(i,0)..\" ore \"..GetPlayerResource(i,1)..\" mercury \"..GetPlayerResource(i,2)"
			"..\" crystal \"..GetPlayerResource(i,3)..\" sulfur \"..GetPlayerResource(i,4)..\" gems \"..GetPlayerResource(i,5)"
			"..\"; minor artifacts:\"..a..\"; game mode \"..H55_GameMode) end end; -- open-homm5: [rules] trace=1" +
			LineEnd(s, e);
		s.insert(line, text);
	}
	if (mode != 0 || toe_bonuses) {
		const size_t line = LineStart(s, decision);
		const std::string decide =
			Indent(s, line) +
			"if OH55_Mode ~= 0 then if OH55_Mode == 4 then OH55_Mode = random(3)+1 end; "
			"somebodyartifact = 0; somebodygold = 0; somebodyres = 0; "
			"if OH55_Mode == 1 then somebodyartifact = 1 elseif OH55_Mode == 2 then somebodygold = 1 else somebodyres = 1 end; "
			"end; -- open-homm5: the game mode is the host's rule, not the bonus arrows" +
			LineEnd(s, decision);
		s.insert(line, decide);
		s.replace(loop, strlen(kHumansLoop), "if H55_MapType ~= \"Campaign\" and OH55_KeepToEBonus ~= 1 then");
		const std::string values =
			Indent(s, LineStart(s, amount)) + "local OH55_KeepToEBonus, OH55_Mode = " + (toe_bonuses ? "1" : "0") + ", " +
			std::to_string(mode) +
			"; -- open-homm5: the host's rules (1 = each human keeps the ToE start bonus; mode 1 Default, 2 Savage, "
			"3 Nightmare, 4 Random, 0 = the bonus arrows)" +
			LineEnd(s, amount);
		s.insert(s.find('\n', amount) + 1, values);
	}
	*out = s;
	return true;
}

bool IsLobbyRow(uint32_t row)
{
	return (row & 0xffff0000u) == kLobbyRowBase;
}

std::vector<std::pair<uint32_t, uint32_t>> LobbyPieces(const std::string &text, uint8_t seq)
{
	std::vector<std::pair<uint32_t, uint32_t>> pieces;
	if (text.empty() || text.size() > static_cast<size_t>(kLobbyTextMax))
		return pieces;
	const uint32_t base = kLobbyRowBase | static_cast<uint32_t>(seq) << 8;
	pieces.push_back({base, static_cast<uint32_t>(text.size())});
	for (size_t i = 0; i < text.size(); i += 4) {
		uint32_t mask = 0;
		for (size_t b = 0; b < 4 && i + b < text.size(); ++b)
			mask |= static_cast<uint32_t>(static_cast<unsigned char>(text[i + b])) << (8 * b);
		pieces.push_back({base | static_cast<uint32_t>(i / 4 + 1), mask});
	}
	return pieces;
}

bool LobbyAssembler::Add(uint32_t row, uint32_t mask, std::string *text)
{
	if (!IsLobbyRow(row))
		return false;
	const int s = static_cast<int>((row >> 8) & 0xff);
	const size_t index = row & 0xff;
	if (s != seq) {  // a new text: start over
		seq = s;
		have_length = false;
		length = 0;
		parts.clear();
		got.clear();
	}
	if (index == 0) {
		if (mask == 0 || mask > static_cast<uint32_t>(kLobbyTextMax) || (mask + 3) / 4 > 254)
			return false;
		length = mask;
		have_length = true;
		parts.resize((length + 3) / 4, 0);
		got.resize(parts.size(), false);
	}
	else {
		if (index > 254)
			return false;
		if (parts.size() < index) {
			parts.resize(index, 0);
			got.resize(index, false);
		}
		parts[index - 1] = mask;
		got[index - 1] = true;
	}
	if (!have_length || parts.size() != (length + 3) / 4)
		return false;
	for (bool g : got)
		if (!g)
			return false;
	std::string t;
	for (size_t i = 0; i < length; ++i)
		t += static_cast<char>((parts[i / 4] >> (8 * (i % 4))) & 0xff);
	*text = t;
	seq = -1;  // used: a repeat of the same pieces starts over
	return true;
}

void HashRequest(char mark, uint32_t hash, uint32_t *type, uint8_t *byte)
{
	*type = static_cast<uint32_t>(static_cast<unsigned char>(mark)) << 24 | (hash & 0x00ffffffu);
	*byte = static_cast<uint8_t>(hash >> 24);
}

bool ReadHashRequest(uint32_t type, uint8_t byte, char *mark, uint32_t *hash)
{
	const char m = static_cast<char>(type >> 24);
	if (m != kMarkSettingsRest && m != kMarkSettingsWhole && m != kMarkStats && m != kMarkStartScript)
		return false;
	*mark = m;
	*hash = static_cast<uint32_t>(byte) << 24 | (type & 0x00ffffffu);
	return true;
}

}  // namespace rules
}  // namespace mods

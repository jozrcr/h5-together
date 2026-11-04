// Hero looks: the rules (see hero_skins_logic.h). No game access.
#include "hero_skins_logic.h"

#include <stdio.h>

namespace mods {
namespace skins {
namespace {

std::string Lower(const std::string &s)
{
	std::string out(s);
	for (char &c : out)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	return out;
}

std::string Trim(const std::string &s)
{
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t'))
		++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
		--e;
	return s.substr(b, e - b);
}

}  // namespace

int FindHero(const Catalog &catalog, const std::string &name)
{
	const std::string want = Lower(Trim(name));
	if (want.empty())
		return -1;
	for (size_t i = 0; i < catalog.size(); ++i)
		if (Lower(catalog[i].name) == want)
			return static_cast<int>(i);
	return -1;
}

bool LookAllowed(const Catalog &catalog, int hero, int look)
{
	const int n = static_cast<int>(catalog.size());
	return hero >= 0 && hero < n && look >= 0 && look < n && hero != look && catalog[hero].town == catalog[look].town;
}

Choices ParseChoices(const std::vector<std::pair<std::string, std::string>> &entries, const Catalog &catalog,
                     std::vector<std::string> *notes)
{
	Choices out;
	for (const auto &e : entries) {
		const std::string key = Trim(e.first);
		std::string value = Trim(e.second);
		const size_t semicolon = value.find(';');  // a comment after the value
		if (semicolon != std::string::npos)
			value = Trim(value.substr(0, semicolon));
		if (key.empty() || key[0] == ';')
			continue;
		const int hero = FindHero(catalog, key);
		const std::string lower = Lower(value);
		std::string note;
		if (hero < 0) {
			note = "\"" + key + "\": no hero of that name: left out";
		} else if (lower.empty() || lower == "original" || lower == "none") {
			note = catalog[hero].name + ": its own look";
		} else {
			const int look = FindHero(catalog, value);
			if (look < 0)
				note = catalog[hero].name + "=" + value + ": no hero of that name: left out";
			else if (look == hero)
				note = catalog[hero].name + ": its own look";
			else if (!LookAllowed(catalog, hero, look))
				note = catalog[hero].name + "=" + catalog[look].name + ": another faction: left out (same faction only)";
			else {
				out[hero] = look;
				note = catalog[hero].name + " wears the look of " + catalog[look].name;
			}
		}
		if (notes)
			notes->push_back(note);
	}
	return out;
}

unsigned Fingerprint(const Catalog &catalog)
{
	unsigned h = 2166136261u;  // FNV-1a
	auto mix = [&h](unsigned char c) {
		h ^= c;
		h *= 16777619u;
	};
	for (const Hero &hero : catalog) {
		for (char c : Lower(hero.name))
			mix(static_cast<unsigned char>(c));
		mix(0);
		for (int i = 0; i < 4; ++i)
			mix(static_cast<unsigned char>(hero.town >> (8 * i)));
	}
	return (h ^ (h >> 16)) & 0xffff;
}

unsigned EncodeHeader(int player, unsigned fingerprint)
{
	return kHeaderMark << 24 | (kVersion & 0xf) << 20 | (static_cast<unsigned>(player) & 0xf) << 16 | (fingerprint & 0xffff);
}

unsigned EncodeChoice(int player, int hero, int look)
{
	const unsigned l = look < 0 ? static_cast<unsigned>(kNoLook) : static_cast<unsigned>(look) & 0x3ff;
	return kChoiceMark << 24 | (static_cast<unsigned>(player) & 0xf) << 20 | (static_cast<unsigned>(hero) & 0x3ff) << 10 | l;
}

Message Decode(unsigned value)
{
	Message m;
	const unsigned mark = value >> 24;
	if (mark == kHeaderMark) {
		m.kind = Message::kHeader;
		m.version = (value >> 20) & 0xf;
		m.player = static_cast<int>((value >> 16) & 0xf);
		m.fingerprint = value & 0xffff;
	} else if (mark == kChoiceMark) {
		m.kind = Message::kChoice;
		m.player = static_cast<int>((value >> 20) & 0xf);
		m.hero = static_cast<int>((value >> 10) & 0x3ff);
		const int look = static_cast<int>(value & 0x3ff);
		m.look = look == kNoLook ? -1 : look;
	}
	return m;
}

unsigned ChoiceCountsAt(unsigned sender_reports)
{
	return sender_reports <= kSendBy ? 0 : sender_reports + kChangeDelay;
}

bool Receive(Remotes &remotes, const Message &m, const Catalog &catalog, unsigned fingerprint, std::string *note)
{
	char line[200];
	line[0] = 0;
	bool changed = false;
	if (m.kind == Message::kNone || m.player < 1 || m.player > kMaxPlayers) {
		snprintf(line, sizeof(line), "a message for player %d: ignored", m.player);
	} else if (m.kind == Message::kHeader) {
		Remote &r = remotes[m.player];
		changed = !r.choices.empty();
		r.choices.clear();  // a header starts a fresh list
		r.header = true;
		r.same_data = m.version == kVersion && m.fingerprint == fingerprint;
		if (r.same_data)
			snprintf(line, sizeof(line), "player %d sends its looks (same game data)", m.player);
		else
			snprintf(line, sizeof(line),
			         "player %d sends its looks, but with other game data (version %u, list %04x; here %u, %04x): "
			         "its heroes keep their own looks", m.player, m.version, m.fingerprint, kVersion, fingerprint);
	} else {
		Remote &r = remotes[m.player];
		const int n = static_cast<int>(catalog.size());
		if (!r.header || !r.same_data) {
			snprintf(line, sizeof(line), "player %d: a look without a matching header: ignored", m.player);
		} else if (m.hero < 0 || m.hero >= n) {
			snprintf(line, sizeof(line), "player %d: hero %d is not in the list: ignored", m.player, m.hero);
		} else if (m.look < 0) {
			changed = r.choices.erase(m.hero) != 0;
			snprintf(line, sizeof(line), "player %d: %s wears its own look", m.player, catalog[m.hero].name.c_str());
		} else if (!LookAllowed(catalog, m.hero, m.look)) {
			snprintf(line, sizeof(line), "player %d: %s cannot wear look %d (same faction only): ignored", m.player,
			         catalog[m.hero].name.c_str(), m.look);
		} else {
			auto it = r.choices.find(m.hero);
			changed = it == r.choices.end() || it->second != m.look;
			r.choices[m.hero] = m.look;
			snprintf(line, sizeof(line), "player %d: %s wears the look of %s", m.player, catalog[m.hero].name.c_str(),
			         catalog[m.look].name.c_str());
		}
	}
	if (note)
		*note = line;
	return changed;
}

int Decide(int hero, const Owner &owner, const Situation &s, const Choices &local, const Remotes &remotes,
           const Catalog &catalog)
{
	if (hero < 0 || !s.known || !owner.present || !owner.human || owner.number < 1 || owner.number > kMaxPlayers)
		return -1;
	const Choices *choices = nullptr;
	if (!s.multiplayer)
		choices = &local;
	else if (remotes[owner.number].same_data)  // this PC's own player too: its choices as it sent them
		choices = &remotes[owner.number].choices;
	if (!choices)
		return -1;
	auto it = choices->find(hero);
	if (it == choices->end() || !LookAllowed(catalog, hero, it->second))
		return -1;
	return it->second;
}

}  // namespace skins
}  // namespace mods

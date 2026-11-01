// Test of the hero looks' rules (src/mods/hero_skins_logic.cpp): the choices
// file, the messages between PCs and who decides a hero's look. No game needed.
#include <stdio.h>

#include <string>
#include <utility>
#include <vector>

#include "../../mods/hero_skins_logic.h"

using namespace mods::skins;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

const unsigned kHaven = 1, kNecropolis = 5;

Catalog MakeCatalog()
{
	return {{"Godric", kHaven}, {"Freyda", kHaven}, {"Axel", kHaven}, {"Giovanni", kNecropolis}, {"Aberrar", kNecropolis}};
}

}  // namespace

int main()
{
	const Catalog cat = MakeCatalog();
	const int godric = 0, freyda = 1, axel = 2, giovanni = 3, aberrar = 4;

	// --- names and the faction rule ---
	Check(FindHero(cat, "godric") == godric && FindHero(cat, " FREYDA ") == freyda, "names are found in any case");
	Check(FindHero(cat, "Nobody") == -1 && FindHero(cat, "") == -1, "unknown and empty names are not");
	Check(LookAllowed(cat, godric, freyda), "a Haven hero may wear a Haven look");
	Check(!LookAllowed(cat, godric, giovanni), "but not a Necropolis one");
	Check(!LookAllowed(cat, godric, godric) && !LookAllowed(cat, -1, 1) && !LookAllowed(cat, 0, 99), "nor its own, nor bad positions");

	// --- the choices file ---
	std::vector<std::pair<std::string, std::string>> entries = {
		{"Godric", "Freyda"}, {"axel", "giovanni"}, {"Giovanni", "Aberrar ; the lich"}, {"Nobody", "Freyda"},
		{"Aberrar", "original"}, {"Freyda", "Unknown"}, {"; comment", "x"}};
	std::vector<std::string> notes;
	const Choices local = ParseChoices(entries, cat, &notes);
	Check(local.size() == 2 && local.at(godric) == freyda && local.at(giovanni) == aberrar,
	      "choices: two kept (a comment after the value is cut)");
	Check(local.count(axel) == 0, "choices: another faction's look is left out");
	Check(notes.size() == 6, "choices: one note per entry (comments skipped)");
	for (const std::string &n : notes)
		printf("      note: %s\n", n.c_str());

	// --- the fingerprint ---
	Catalog other = cat;
	other[2].name = "Axel2";
	Catalog moved = cat;
	moved[4].town = kHaven;
	Check(Fingerprint(cat) == Fingerprint(MakeCatalog()), "fingerprint: the same list gives the same value");
	Check(Fingerprint(cat) != Fingerprint(other) && Fingerprint(cat) != Fingerprint(moved), "fingerprint: a name or a faction changes it");
	Check(Fingerprint(cat) <= 0xffff, "fingerprint: 16 bits");

	// --- messages ---
	const unsigned fp = Fingerprint(cat);
	Message h = Decode(EncodeHeader(3, fp));
	Check(h.kind == Message::kHeader && h.player == 3 && h.version == kVersion && h.fingerprint == fp, "header round trip");
	Message c = Decode(EncodeChoice(8, 1000, 1001));
	Check(c.kind == Message::kChoice && c.player == 8 && c.hero == 1000 && c.look == 1001, "choice round trip (10-bit positions)");
	Message n = Decode(EncodeChoice(2, godric, -1));
	Check(n.kind == Message::kChoice && n.look == -1, "choice: back to the hero's own look");
	Check((EncodeHeader(1, 0) >> 24) == 'k' && (EncodeChoice(1, 0, 0) >> 24) == 'K', "the marks");
	Check(Decode(0x47010032).kind == Message::kNone, "the co-op guard's hello ('G') is not ours");
	Check(Decode(0x12345678).kind == Message::kNone, "a real checksum is not ours");

	// --- receiving ---
	Remotes remotes;
	std::string note;
	Check(!Receive(remotes, Decode(EncodeChoice(2, godric, freyda)), cat, fp, &note) && remotes[2].choices.empty(),
	      "a choice before the header is ignored");
	Receive(remotes, Decode(EncodeHeader(2, fp)), cat, fp, &note);
	Check(remotes[2].header && remotes[2].same_data, "a header with our list: same data");
	Check(Receive(remotes, Decode(EncodeChoice(2, godric, freyda)), cat, fp, &note) && remotes[2].choices[godric] == freyda,
	      "a choice is taken");
	Check(!Receive(remotes, Decode(EncodeChoice(2, godric, freyda)), cat, fp, &note), "the same choice again changes nothing");
	Check(!Receive(remotes, Decode(EncodeChoice(2, axel, giovanni)), cat, fp, &note) && remotes[2].choices.count(axel) == 0,
	      "another faction's look is ignored");
	Check(!Receive(remotes, Decode(EncodeChoice(2, 900, freyda)), cat, fp, &note), "a hero outside the list is ignored");
	Check(Receive(remotes, Decode(EncodeChoice(2, godric, -1)), cat, fp, &note) && remotes[2].choices.empty(),
	      "back to the own look");
	Receive(remotes, Decode(EncodeChoice(2, godric, axel)), cat, fp, &note);
	Check(Receive(remotes, Decode(EncodeHeader(2, fp)), cat, fp, &note) && remotes[2].choices.empty(), "a new header starts afresh");
	Receive(remotes, Decode(EncodeHeader(4, fp ^ 1)), cat, fp, &note);
	Check(remotes[4].header && !remotes[4].same_data, "a header with another list: not the same data");
	Check(!Receive(remotes, Decode(EncodeChoice(4, godric, freyda)), cat, fp, &note) && remotes[4].choices.empty(),
	      "then its choices are ignored");
	printf("      note: %s\n", note.c_str());
	Check(!Receive(remotes, Decode(EncodeHeader(0, fp)), cat, fp, &note), "player 0 is ignored");

	// --- deciding ---
	Remotes rs;
	Receive(rs, Decode(EncodeHeader(2, fp)), cat, fp, nullptr);
	Receive(rs, Decode(EncodeChoice(2, axel, godric)), cat, fp, nullptr);
	Owner human1{true, true, 1}, human2{true, true, 2}, ai3{true, false, 3}, nobody;
	Situation unknown;
	Situation single;
	single.known = true;
	Situation multi;
	multi.known = true;
	multi.multiplayer = true;
	multi.local_player = 1;
	Check(Decide(godric, human1, unknown, local, rs, cat) == -1, "decide: nothing while the kind of game is unknown");
	Check(Decide(godric, human1, single, local, rs, cat) == freyda, "single player: this PC's choice");
	Check(Decide(godric, human2, single, local, rs, cat) == freyda, "hot seat: this PC's choice for every human");
	Check(Decide(godric, ai3, single, local, rs, cat) == -1, "a computer player's hero keeps its look");
	Check(Decide(godric, nobody, single, local, rs, cat) == -1, "a hero without an owner keeps its look");
	Check(Decide(godric, human1, multi, local, rs, cat) == freyda, "multiplayer: this PC's choice for its own player");
	Check(Decide(godric, human2, multi, local, rs, cat) == -1, "multiplayer: not for another player");
	Check(Decide(axel, human2, multi, local, rs, cat) == godric, "multiplayer: the other player's own choice");
	Check(Decide(axel, human1, multi, local, rs, cat) == -1, "multiplayer: its choice is not for this PC's heroes");
	Situation hidden = multi;
	hidden.show_others = false;
	Check(Decide(axel, human2, hidden, local, rs, cat) == -1, "show_others=0: friends' looks not shown");
	Owner human4{true, true, 4};
	Check(Decide(godric, human4, multi, local, remotes, cat) == -1, "a player with other game data: own looks");
	Check(Decide(-1, human1, single, local, rs, cat) == -1, "a hero outside the list keeps its look");

	if (g_failures) {
		printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	printf("all hero skins checks passed\n");
	return 0;
}

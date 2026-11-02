// Hero looks ("skins"): the rules, with no game access (testable on their own).
// The game side is hero_skins.cpp; the plan is docs/plans/hero-skins.md.
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mods {
namespace skins {

// One hero of the game's list of all heroes (HEROES_ANY), in the list's order.
struct Hero {
	std::string name;  // its InternalName, as in its data file ("Godric", "Hero1")
	unsigned town;     // its TownType (the faction)
};
typedef std::vector<Hero> Catalog;

// hero -> look: both are positions in the catalog. A look is "the figures of
// that hero".
typedef std::map<int, int> Choices;

// The position of a hero by its name (any case), or -1.
int FindHero(const Catalog &catalog, const std::string &name);

// May `hero` wear `look`? Two different heroes of the same faction (the
// owner's rule, 2026-11-01: same faction only).
bool LookAllowed(const Catalog &catalog, int hero, int look);

// The choices of h5-together-skins.ini ([skins] section: hero=look, by
// InternalName). Unknown names, other factions and "original" / empty values
// are left out; `notes` gets one line for each entry (for the log).
Choices ParseChoices(const std::vector<std::pair<std::string, std::string>> &entries, const Catalog &catalog,
                     std::vector<std::string> *notes);

// A 16-bit fingerprint of the catalog (names and factions, in order): PCs with
// different game data (another MMH5.5 version) have different ones.
unsigned Fingerprint(const Catalog &catalog);

// --- the messages between PCs -------------------------------------------------
// Each is the 32-bit "checksum" of a world-checksum message with check number
// -1, which the game ignores (see hero_skins.cpp). The top byte is the mark.
//   header: 'k', version (4 bits), player (4 bits), fingerprint (16 bits)
//   choice: 'K', player (4 bits), hero (10 bits), look (10 bits; 0x3ff = none)
// "player" is the sender's player number (1..8), whose heroes the choices are for.
const unsigned kHeaderMark = 0x6b;  // 'k'
const unsigned kChoiceMark = 0x4b;  // 'K'
const unsigned kVersion = 1;
const int kNoLook = 0x3ff;
const int kMaxCatalog = 0x3ff;  // heroes 0..0x3fe fit in 10 bits

unsigned EncodeHeader(int player, unsigned fingerprint);
unsigned EncodeChoice(int player, int hero, int look);  // look -1 = back to the hero's own look

struct Message {
	enum Kind { kNone, kHeader, kChoice } kind = kNone;
	int player = 0;
	unsigned version = 0;
	unsigned fingerprint = 0;
	int hero = -1;
	int look = -1;  // -1 = none
};
// kNone when the value is not one of ours.
Message Decode(unsigned value);

// What this PC knows of another player's choices.
struct Remote {
	bool header = false;     // its header arrived
	bool same_data = false;  // with our fingerprint and version
	Choices choices;
};
const int kMaxPlayers = 8;
typedef Remote Remotes[kMaxPlayers + 1];  // by player number 1..8

// Takes in a received message. Returns true when it changed a choice (then
// figures may need rebuilding). `note` gets a line for the log (may be empty).
bool Receive(Remotes &remotes, const Message &m, const Catalog &catalog, unsigned fingerprint, std::string *note);

// --- the decision ---------------------------------------------------------------
struct Owner {
	bool present = false;  // the hero has an owner
	bool human = false;
	int number = 0;        // 1..8
};
struct Situation {
	bool known = false;        // the kind of game is known (single, or multiplayer with looks on)
	bool multiplayer = false;
};
// The look `hero` wears now, or -1 for its own. Only a human owner's choice
// counts: in single player and hot seat, the choices of this PC's file for
// every human; in multiplayer, each player's choices as that player's PC sent
// them (this PC's own included, so every PC decides alike). Computer players'
// heroes keep their looks.
int Decide(int hero, const Owner &owner, const Situation &s, const Choices &local, const Remotes &remotes,
           const Catalog &catalog);

}  // namespace skins
}  // namespace mods

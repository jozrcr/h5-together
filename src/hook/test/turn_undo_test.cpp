// Test of undo End Turn's rules (src/mods/turn_undo_logic.cpp): the messages between
// PCs, when they count, who decides, and when an undo is allowed. No game needed.
#include <stdio.h>

#include "../../mods/turn_undo_logic.h"

using namespace undo;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

Player P(int number, bool human, int state, bool pressed, bool in_game = true)
{
	Player p;
	p.number = number;
	p.human = human;
	p.state = state;
	p.pressed = pressed;
	p.in_game = in_game;
	return p;
}

}  // namespace

int main()
{
	// --- messages ---
	Message m = Decode(EncodeHello(true));
	Check(m.kind == Message::kHello && m.version == kVersion && m.allow, "hello: allow, our version");
	m = Decode(EncodeHello(false));
	Check(m.kind == Message::kHello && !m.allow, "hello: not allowed");
	for (int p = 1; p <= 8; ++p) {
		m = Decode(EncodeRequest(p));
		if (m.kind != Message::kRequest || m.player != p)
			Check(false, "request: every player number comes back");
	}
	Check(Decode(EncodeRequest(3)).player == 3, "request: player 3");
	Check(Decode(EncodeRequest(0)).kind == Message::kNone && Decode(EncodeRequest(9)).kind == Message::kNone,
	      "request: players 0 and 9 are not ours");
	Check(Decode(0).kind == Message::kNone && Decode(0x6b012345).kind == Message::kNone &&
	          Decode(0x4b012345).kind == Message::kNone && Decode(0x47010032).kind == Message::kNone,
	      "others' values (zero, hero looks 'k' 'K', the co-op guard 'G') are not ours");
	Check(Decode(0x55300001).kind == Message::kNone, "an unknown kind of ours is ignored");
	Check(Decode(EncodeHello(true) | 0x100).kind == Message::kNone && Decode(EncodeRequest(2) | 0x100).kind == Message::kNone,
	      "stray bits: ignored");
	Check(EncodeHello(true) >> 24 == 0x55 && EncodeRequest(1) >> 24 == 0x55, "the mark is 'U'");

	// --- timing ---
	Check(RequestCountsAt(25) == 35 && RequestCountsAt(0) == kDelay, "a request counts kDelay segments after its sender's report");
	Check(kSendAt < kSendBy && kSendBy + 1 < kDecideAt && kDecideAt <= RequestCountsAt(kSendBy + 1),
	      "hellos come before the decision, which comes before any request");

	// --- the decision ---
	const unsigned all[3] = {1, 1, 1}, missing[3] = {1, 0, 1}, newer[2] = {2, 1};
	Check(Decide(all, 3, true), "on: every PC said hello, the host allows");
	Check(!Decide(all, 3, false), "off: the host does not allow");
	Check(!Decide(missing, 3, true), "off: a PC sent no hello (no add-on, an old one)");
	Check(Decide(newer, 2, true), "a newer version still knows it");
	Check(!Decide(all, 1, true) && !Decide(all, 0, true), "off: a game of one PC");

	// --- when an undo is allowed ---
	const char *why = nullptr;
	Player game[3] = {P(1, true, 1, true), P(2, true, 0, false), P(3, false, 1, false)};
	Check(Allowed(game, 3, true, 1, &why), "allowed: player 1 ended (state 1), player 2 still plays");
	game[0] = P(1, true, 0, true);
	Check(Allowed(game, 3, true, 1, &why), "allowed: player 1 pressed End Turn, before the tick gave it state 1");
	Check(!Allowed(game, 3, false, 1, &why), "refused: no simultaneous turns");
	Check(!Allowed(game, 3, true, 2, &why), "refused: player 2 has not ended its turn");
	Check(!Allowed(game, 3, true, 3, &why), "refused: player 3 is a computer");
	Check(!Allowed(game, 3, true, 5, &why), "refused: no player 5");
	game[1] = P(2, true, 1, true);
	Check(!Allowed(game, 3, true, 1, &why), "refused: the other human ended too");
	game[1] = P(2, true, 0, true);
	Check(!Allowed(game, 3, true, 1, &why), "refused: the other human pressed End Turn (the next turn starts at the tick)");
	game[1] = P(2, true, 0, false, false);
	Check(!Allowed(game, 3, true, 1, &why), "refused: the other human left the game");
	Player three[4] = {P(1, true, 1, true), P(2, true, 1, true), P(3, true, 0, false), P(4, false, 1, false)};
	Check(Allowed(three, 4, true, 2, &why), "allowed: 3 humans, one still plays");
	Check(Allowed(three, 4, true, 1, nullptr), "the reason may be left out");
	game[0] = P(1, true, 2, true);
	game[1] = P(2, true, 0, false);
	Check(!Allowed(game, 3, true, 1, &why), "refused: state 2 (another kind of waiting)");

	printf("%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
	return g_failures ? 1 : 0;
}

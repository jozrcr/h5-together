// Undo End Turn's pure work (see turn_undo_logic.h).
#include "turn_undo_logic.h"

namespace undo {
namespace {

// The value: kMark in bits 24-31, the kind in bits 20-23, then the kind's own bits.
const unsigned kHelloKind = 1, kRequestKind = 2;

}  // namespace

uint32_t EncodeHello(bool allow)
{
	return kMark << 24 | kHelloKind << 20 | (kVersion & 0xf) << 16 | (allow ? 1u : 0u);
}

uint32_t EncodeRequest(int player)
{
	return kMark << 24 | kRequestKind << 20 | (static_cast<unsigned>(player) & 0xf);
}

Message Decode(uint32_t value)
{
	Message m;
	if (value >> 24 != kMark)
		return m;
	const unsigned kind = (value >> 20) & 0xf;
	if (kind == kHelloKind && (value & 0xfffe) == 0) {
		m.kind = Message::kHello;
		m.version = (value >> 16) & 0xf;
		m.allow = (value & 1) != 0;
	}
	else if (kind == kRequestKind && (value & 0xffff0) == 0) {
		const int player = static_cast<int>(value & 0xf);
		if (player >= 1 && player <= 8) {
			m.kind = Message::kRequest;
			m.player = player;
		}
	}
	return m;
}

unsigned RequestCountsAt(unsigned sender_reports)
{
	return sender_reports + kDelay;
}

bool Decide(const unsigned *hellos, int pcs, bool host_allows)
{
	if (pcs < 2 || !host_allows)
		return false;  // (a game of one PC has nobody to wait for)
	for (int pc = 0; pc < pcs; ++pc)
		if (hellos[pc] < 1)
			return false;
	return true;
}

bool Allowed(const Player *players, int count, bool simultaneous, int number, const char **why)
{
	const char *unused;
	if (!why)
		why = &unused;
	if (!simultaneous) {
		*why = "no simultaneous turns";
		return false;
	}
	const Player *target = nullptr;
	bool other_playing = false;
	for (int i = 0; i < count; ++i) {
		const Player &p = players[i];
		if (!p.human || !p.in_game)
			continue;
		if (p.number == number)
			target = &p;
		else if (p.state == 0 && !p.pressed)
			other_playing = true;
	}
	if (!target) {
		*why = "no such human player in the game";
		return false;
	}
	if (!target->pressed || (target->state != 0 && target->state != 1)) {
		*why = "the player has not ended its turn";
		return false;
	}
	if (!other_playing) {
		*why = "no other human still plays";
		return false;
	}
	*why = "";
	return true;
}

}  // namespace undo

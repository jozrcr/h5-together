// Undo End Turn's pure work (no game), unit-tested by src/hook/test/turn_undo_test.cpp.
// The mod itself is turn_undo.cpp; the plan is docs/plans/turn-features.md, part 1.
//
// In plain words: the PCs tell each other two things, each as one 32-bit value sent
// next to the world checksums (as hero looks and the co-op guard do):
//   - a hello at the start of each game: "this PC knows the undo" (and, from the
//     host, whether the host allows it);
//   - a request: "undo the End Turn of player P".
// Every PC takes a request in at the same segment, counted from its sender's segment
// reports, and decides there, on the same world, whether the undo is still allowed.
#pragma once

#include <stdint.h>

namespace undo {

const unsigned kMark = 0x55;    // 'U' in the top byte (hero looks use 'k' and 'K', the co-op guard 'G')
const unsigned kVersion = 1;    // what a hello says this PC knows

// When things count, in segments (every PC counts the same segments).
const unsigned kSendAt = 2;     // a PC sends its hello after its own report kSendAt...
const unsigned kSendBy = 10;    // ...and a hello counts only if it came by its sender's report kSendBy
const unsigned kDecideAt = 20;  // the segment at whose start every PC decides whether the undo is on
const unsigned kDelay = 10;     // a request counts at its sender's report + this

struct Message {
	enum Kind { kNone, kHello, kRequest } kind = kNone;
	unsigned version = 0;  // hello
	bool allow = false;    // hello: the sender allows the undo ([coop] undo_end_turn=1; only the host's counts)
	int player = 0;        // request: the player (1..8) whose End Turn is undone
};

uint32_t EncodeHello(bool allow);
uint32_t EncodeRequest(int player);
// kNone when the value is not one of ours.
Message Decode(uint32_t value);

// The segment at whose start a request counts, from its sender's report count when it
// was sent (the sender) or received (the others: the same number, as messages arrive
// in the order they were sent, after that report).
unsigned RequestCountsAt(unsigned sender_reports);

// The decision at kDecideAt: on only if every PC sent a hello in time with a version
// that knows the undo, and the host (PC 0) allows it. `hellos[pc]` is 0 when none came,
// else the version; `host_allows` is what the host's hello said.
bool Decide(const unsigned *hellos, int pcs, bool host_allows);

// A player, as the undo needs to see it.
struct Player {
	int number = 0;      // 1..8
	bool human = false;
	bool in_game = false;
	int state = 0;       // 0: its turn now; 1: done for today (simultaneous turns)
	bool pressed = false;  // pressed End Turn
};

// May `number` undo its End Turn now? Simultaneous turns on; the player human, in the
// game, has pressed End Turn and is at state 0 or 1; another human in the game is at
// state 0 and has not pressed it. `why` (if not null) says why not.
bool Allowed(const Player *players, int count, bool simultaneous, int number, const char **why);

}  // namespace undo

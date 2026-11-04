// Mod (EXPERIMENT): the computer players take their turns at the same time
// ([ai] parallel_turns=1, hidden, off by default; docs/plans/turn-features.md, part 3).
//
// Why: an AI turn is mostly waiting. After each move the AI waits until the move has
// run (ai-turn.md: the AI manager asks the world "is this player still busy as of 400 ms
// of game time ago?", world slot 0xa4, for that player only). With several computer
// players one after another, those waits add up; side by side, they overlap.
//
// How the game runs turns (players.md, turn-features.md):
//   - the next turn (0x00998060) gives the turn (state 0) to one computer player at a
//     time, in player order, after the humans; it finds "the computer player playing
//     now" with 0x0098d480 (players, dl = 0, 1), which answers NOTHING when two are at
//     state 0;
//   - the AI manager (the adventure controller's frame, 0x006082f0) drives the AI of
//     that one player (0x0098d480 at 0x00608658), one step per frame;
//   - a computer player's End Turn (the world's slot 0xd0, 0x00998760) calls the next
//     turn at once; the world's tick (0x00998520, from 0x0099892e) moves the day on
//     only when no player at state 0 is still playing.
// What this does (every hook acts only while the setting is on):
//   1. the world's tick, before it runs: when one computer player has just got the turn
//      and no human plays (the computer players' part of the day begins), every other
//      computer player in the game gets state 0 too. They make up this "phase";
//   2. the AI manager's "which computer player plays?": during a phase, the next one in
//      the phase (in turn, one per frame) that has not ended its turn;
//   3. a computer player's End Turn during a phase: it is only marked done, unless it is
//      the last of the phase. Then the game's own End Turn runs, and with it the next
//      turn, for every player of the phase still at state 0;
//   4. inside that next turn (its call at 0x009980a9): "the computer player playing
//      now" is the last of the phase in player order, so the day moves on as it does
//      after the last computer player.
// Hooks 1, 3 and 4 change the world: every PC of a multiplayer game must have the same
// setting (an experiment: no agreement between PCs yet).
#include "parallel_ai.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kFindPlaying = 0x0098d480;     // __fastcall(players vector, dl = human?, [esp+4] in game only): the one at state 0, or 0
const DWORD kManagerFindCall = 0x00608658; // in the AI manager 0x006082f0
const DWORD kNextTurnFindCall = 0x009980a9;// in the next turn 0x00998060
const DWORD kTick = 0x00998520;            // the world's turn manager, __thiscall(world)
const DWORD kTickCall = 0x0099892e;        // in the world's update 0x009987b0
const DWORD kEndTurnSlot = 0x00e6260c + 0xd0;  // NWorld::CWorld slot 0xd0
const DWORD kEndTurn = 0x00998760;         // __thiscall(world, IPlayer *), ret 4
const DWORD kPlayers = 0x24, kPlayersEnd = 0x28;
// IPlayer slots (byte offsets in its vtable 0x00e1a09c)
const DWORD kNumber = 0x2c, kState = 0x44, kSetState = 0x48, kInGame = 0x6c, kHuman = 0x184;

struct Code {
	DWORD address;
	BYTE bytes[8];
	int size;
};
const Code kCode[] = {
	{kFindPlaying, {0x51, 0x53, 0x55, 0x8b, 0xe9, 0x8b, 0x45, 0x00}, 8},
	{kTick, {0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50}, 8},
	{kEndTurn, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x57}, 8},
	{0x00720e90, {0x8b, 0x41, 0x78, 0xc3}, 4},   // slot 0x2c: the number at +0x78
	{0x00500c10, {0x8b, 0x81, 0x34, 0x06, 0x00, 0x00}, 6},  // slot 0x44: the state
	{0x00500d60, {0x33, 0xc0, 0x39, 0x41, 0x74, 0x0f}, 6},  // slot 0x184: human
};

typedef void *(__fastcall *FindFn)(void *players, int want_human, int in_game_only);
typedef void(__fastcall *TickFn)(void *world, void *edx);
typedef void(__fastcall *EndTurnFn)(void *world, void *edx, void *player);
typedef int(__fastcall *GetFn)(void *self, void *edx);
typedef void(__fastcall *SetFn)(void *self, void *edx, int value);

FindFn g_manager_find_next = nullptr;
FindFn g_next_turn_find_next = nullptr;
TickFn g_tick_next = nullptr;
EndTurnFn g_end_turn_next = nullptr;

// The phase: which computer players (by number, 1..8) play side by side, which are done.
bool g_phase = false;
bool g_member[9], g_done[9];
int g_rotation = 0;            // the AI manager's next pick
DWORD g_phase_started = 0;     // GetTickCount(), for the log
int g_phase_count = 0;
LONG g_picks[9];               // the AI manager's picks per player in this phase (research)
LONG g_busy[9], g_free[9];     // research: the world's "is this player busy?" answers per player
LONG g_reached[9];             // research: the AI manager got past every check for this player
LONG g_state44[16];            // research: the controller's +0x44 at our picks
typedef void *(__fastcall *AiOfFn)(void *manager, void *edx, void *player);
AiOfFn g_ai_of_next = nullptr;
const DWORD kAiOfCall = 0x006086d2, kAiOf = 0x00608180;
typedef bool(__fastcall *BusyFn)(void *world, void *edx, DWORD time, void *player);
BusyFn g_busy_next = nullptr;
const DWORD kBusySlot = 0x00e6260c + 0xa4, kBusy = 0x0098e1c0;


DWORD Read(const void *p, DWORD off)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off);
}

int Get(void *player, DWORD slot)
{
	return reinterpret_cast<GetFn>(Read(reinterpret_cast<void *>(Read(player, 0)), slot))(player, nullptr);
}

bool Dying(const void *object)
{
	const BYTE *o = static_cast<const BYTE *>(object);
	const DWORD vbtable = Read(o, 4);
	const LONG base = *reinterpret_cast<const LONG *>(vbtable + 4);
	return (o[base + 0xb] & 0x80) != 0;
}

bool __fastcall BusyHook(void *world, void *edx, DWORD time, void *player)
{
	const bool busy = g_busy_next(world, edx, time, player);
	if (g_phase && player && !Dying(player)) {
		const int n = Get(player, kNumber);
		if (n >= 1 && n <= 8)
			++(busy ? g_busy : g_free)[n];
	}
	return busy;
}

void *__fastcall AiOfHook(void *manager, void *edx, void *player)
{
	if (g_phase && player && !Dying(player)) {
		const int n = Get(player, kNumber);
		if (n >= 1 && n <= 8) {
			++g_reached[n];
			++g_rotation;  // driven now: the next pick starts after it
		}
	}
	return g_ai_of_next(manager, edx, player);
}

struct P {
	void *player;
	int number;
	bool human, in_game;
	int state;
};

// The players of a players vector ({begin, end} of IPlayer *), alive ones only.
int ReadPlayers(const void *vector, P *out, int max)
{
	void **p = reinterpret_cast<void **>(Read(vector, 0));
	void **end = reinterpret_cast<void **>(Read(vector, 4));
	int n = 0;
	for (; p && p < end && n < max; ++p) {
		if (!*p || Dying(*p))
			continue;
		P &q = out[n++];
		q.player = *p;
		q.number = Get(*p, kNumber);
		q.human = (Get(*p, kHuman) & 0xff) != 0;
		q.in_game = Get(*p, kInGame) == 1;
		q.state = Get(*p, kState);
	}
	return n;
}

std::string Members()
{
	std::string out;
	for (int i = 1; i <= 8; ++i)
		if (g_member[i]) {
			char item[8];
			_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%d", out.empty() ? "" : ",", i);
			out += item;
		}
	return out;
}

// 1. Before the world's tick: the computer players' part of the day begins.
void __fastcall TickHook(void *world, void *edx)
{
	if (!g_phase) {
		P players[8];
		const int n = ReadPlayers(static_cast<const BYTE *>(world) + kPlayers, players, 8);
		int playing_ai = 0, playing_human = 0, other_ai = 0;
		for (int i = 0; i < n; ++i) {
			if (!players[i].in_game)
				continue;
			if (players[i].state == 0)
				(players[i].human ? playing_human : playing_ai) += 1;
			else if (!players[i].human)
				++other_ai;
		}
		if (playing_ai == 1 && playing_human == 0 && other_ai > 0) {
			memset(g_member, 0, sizeof(g_member));
			memset(g_done, 0, sizeof(g_done));
			for (int i = 0; i < n; ++i) {
				if (!players[i].in_game || players[i].human || players[i].number < 1 || players[i].number > 8)
					continue;
				g_member[players[i].number] = true;
				if (players[i].state != 0)
					reinterpret_cast<SetFn>(Read(reinterpret_cast<void *>(Read(players[i].player, 0)), kSetState))(players[i].player,
					                                                                                              nullptr, 0);
			}
			g_phase = true;
			memset(g_picks, 0, sizeof(g_picks));
			memset(g_busy, 0, sizeof(g_busy));
			memset(g_reached, 0, sizeof(g_reached));
			memset(g_free, 0, sizeof(g_free));
			g_phase_started = GetTickCount();
			++g_phase_count;
			hooklog::Write("parallel ai: phase %d: computer players %s play at the same time", g_phase_count, Members().c_str());
		}
	}
	g_tick_next(world, edx);
}

// 2. The AI manager: the next computer player of the phase not done yet.
void *__fastcall ManagerFindHook(void *players_vector, int want_human, int in_game_only)
{
	if (!g_phase || (want_human & 0xff))
		return g_manager_find_next(players_vector, want_human, in_game_only);
	P players[8];
	const int n = ReadPlayers(players_vector, players, 8);
	for (int tries = 0; tries < n; ++tries) {
		const P &p = players[(g_rotation + tries) % n];
		if (p.number >= 1 && p.number <= 8 && g_member[p.number] && !g_done[p.number] && p.state == 0 && p.in_game) {
			// (the rotation moves on only when this one is driven, AiOfHook: a pass the manager
			// skips, right after another AI's move, must not cost this AI its turn)
			g_rotation = (g_rotation + tries) % n;
			++g_picks[p.number];
			return p.player;
		}
	}
	return nullptr;
}

// 3. A player's End Turn.
void __fastcall EndTurnHook(void *world, void *edx, void *player)
{
	if (g_phase && player && !Dying(player)) {
		const int number = Get(player, kNumber);
		if (number >= 1 && number <= 8 && g_member[number] && !(Get(player, kHuman) & 0xff) && Get(player, kState) == 0) {
			g_done[number] = true;
			bool last = true;
			for (int i = 1; i <= 8; ++i)
				if (g_member[i] && !g_done[i])
					last = false;
			if (!last) {
				hooklog::Write("parallel ai: computer player %d ended its turn (%lu ms into the phase); others still play", number,
				               GetTickCount() - g_phase_started);
				return;  // only marked done: the turn moves on with the last one
			}
			std::string picks;
			for (int i = 1; i <= 8; ++i)
				if (g_member[i]) {
					char item[24];
					_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%d:%ld/%ld/%ld/%ld", picks.empty() ? "" : " ", i, g_picks[i], g_busy[i], g_free[i], g_reached[i]);
					picks += item;
				}
			hooklog::Write("parallel ai: computer player %d ended its turn, the last of phase %d (%lu ms): the day moves on (the "
			               "AI manager's picks/busy/free/reached: %s)",
			               number, g_phase_count, GetTickCount() - g_phase_started, picks.c_str());
			g_end_turn_next(world, edx, player);  // the game's End Turn, and its next turn (hook 4 inside)
			g_phase = false;
			return;
		}
	}
	g_end_turn_next(world, edx, player);
}

// 4. Inside the next turn: "the computer player playing now".
void *__fastcall NextTurnFindHook(void *players_vector, int want_human, int in_game_only)
{
	if (!g_phase || (want_human & 0xff))
		return g_next_turn_find_next(players_vector, want_human, in_game_only);
	P players[8];
	const int n = ReadPlayers(players_vector, players, 8);
	void *last = nullptr;
	for (int i = 0; i < n; ++i)
		if (players[i].number >= 1 && players[i].number <= 8 && g_member[players[i].number] && players[i].state == 0 && players[i].in_game)
			last = players[i].player;
	return last ? last : g_next_turn_find_next(players_vector, want_human, in_game_only);
}

bool CodeAsExpected()
{
	for (const Code &c : kCode)
		if (memcmp(reinterpret_cast<const void *>(c.address), c.bytes, c.size) != 0) {
			hooklog::Write("parallel ai: the game's code at %08lx is not as expected: off", c.address);
			return false;
		}
	return true;
}

}  // namespace

void ApplyParallelAiFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	if (GetPrivateProfileIntW(L"ai", L"parallel_turns", 0, path) == 0 || !CodeAsExpected())
		return;
	g_manager_find_next = reinterpret_cast<FindFn>(hookpatch::RedirectCall(kManagerFindCall, kFindPlaying, &ManagerFindHook));
	g_next_turn_find_next = reinterpret_cast<FindFn>(hookpatch::RedirectCall(kNextTurnFindCall, kFindPlaying, &NextTurnFindHook));
	g_tick_next = reinterpret_cast<TickFn>(hookpatch::RedirectCall(kTickCall, kTick, &TickHook));
	g_end_turn_next = reinterpret_cast<EndTurnFn>(hookpatch::PatchPointer(kEndTurnSlot, kEndTurn, &EndTurnHook));
	g_busy_next = reinterpret_cast<BusyFn>(hookpatch::PatchPointer(kBusySlot, kBusy, &BusyHook));  // research
	g_ai_of_next = reinterpret_cast<AiOfFn>(hookpatch::RedirectCall(kAiOfCall, kAiOf, &AiOfHook));  // research
	if (!g_manager_find_next || !g_next_turn_find_next || !g_tick_next || !g_end_turn_next) {
		hooklog::Write("parallel ai: part of the game's code is not as expected (manager %d, next turn %d, tick %d, end turn %d): "
		               "the hooks that went in pass on",
		               g_manager_find_next != nullptr, g_next_turn_find_next != nullptr, g_tick_next != nullptr, g_end_turn_next != nullptr);
		return;
	}
	hooklog::Write("parallel ai: ON (EXPERIMENT): the computer players take their turns at the same time; every PC of a "
	               "multiplayer game must have [ai] parallel_turns=1");
}

}  // namespace mods

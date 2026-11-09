// Mod: a player who has lost every town keeps playing while a hero lives ([coop]
// keep_without_towns, the host's choice, off: the game's rule). The owner's request,
// 2026-11-09: "we missed on a great fight cuz ai got depopped after days without a city".
//
// The game's rule. A map says, in its objectives, whether a player without towns loses
// after a week: NDb SAdvMapDesc +0x12c "Objectives", its "Primary" block (+4) has a
// "Common" set (+4 in the block) and a set per player ("PlayerSpecific", a vector at +0x18,
// 0x14 bytes each), and each set has "DieInWeekWithoutTowns" at +0x10 (the serializers
// 0x00adc810 / 0x00adf9b0). Every random map has it on, for everyone. When a game starts,
// the objectives system (NWorld::CObjectivesSystem, its start from the map 0x00aff210)
// calls 0x00afebd0 with the Primary block; for each player 1..8 whose Common or own set
// has the flag, that adds a hidden primary objective of kind 9, "OBJ_CAPTURE_ANY_TOWN_IN_
// WEEK": once the player has no town, it counts 7 days, then the player is out, heroes or
// not. (A player with neither town nor hero is out at once, by another rule of the game.)
//
// With keep_without_towns, around that call (at 0x00aff269), the block's flags read 0 (as
// if the map's maker had unticked them), then get their values back: the objective is not
// made, so a player without towns stays while a hero lives. This acts on the world, so
// every PC must do the same: the host decides at its start (every PC has H5 Together
// 0.3.7 or later) and its start message carries the choice (host_rules.cpp, a chunk of its
// own that older versions skip); each PC sets it when its game starts. The objectives are
// part of the world, so a saved game keeps the rule it started with.
#include "keep_without_towns.h"

#include <windows.h>
#include <stdio.h>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kSetupCall = 0x00aff269;  // in CObjectivesSystem's start from the map (0x00aff210)
const DWORD kSetup = 0x00afebd0;      // the players' objectives from the map's Primary block
// What 0x00afebd0 reads: the Common set's flag (block +4 +0x10), the PlayerSpecific vector
// (block +0x18 / +0x1c), each set's flag (+0x10, sets of 0x14 bytes); it loops on 8 players.
const int kCommonFlag = 0x14, kSpecificBegin = 0x18, kSpecificEnd = 0x1c, kSetSize = 0x14, kSetFlag = 0x10;
const int kMaxSets = 16;

typedef void(__fastcall *SetupFn)(void *system, void *edx, void *players, BYTE *block);
SetupFn g_next = nullptr;
volatile LONG g_on = 0;

// The flags, read (and cleared when `clear`): false when the block cannot be read.
bool Flags(BYTE *block, bool clear, BYTE *common, BYTE *sets, int *count)
{
	__try {
		*common = block[kCommonFlag];
		BYTE *begin = *reinterpret_cast<BYTE **>(block + kSpecificBegin);
		BYTE *end = *reinterpret_cast<BYTE **>(block + kSpecificEnd);
		int n = begin && end > begin ? static_cast<int>((end - begin) / kSetSize) : 0;
		if (n > kMaxSets)
			n = kMaxSets;
		for (int i = 0; i < n; ++i)
			sets[i] = begin[i * kSetSize + kSetFlag];
		*count = n;
		if (clear) {
			block[kCommonFlag] = 0;
			for (int i = 0; i < n; ++i)
				begin[i * kSetSize + kSetFlag] = 0;
		}
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool Restore(BYTE *block, BYTE common, const BYTE *sets, int count)
{
	__try {
		block[kCommonFlag] = common;
		BYTE *begin = *reinterpret_cast<BYTE **>(block + kSpecificBegin);
		for (int i = 0; i < count; ++i)
			begin[i * kSetSize + kSetFlag] = sets[i];
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// "common 1, players 1 1 1 1 1 1 1 1"
void Describe(BYTE common, const BYTE *sets, int count, char *out, size_t size)
{
	int n = _snprintf_s(out, size, _TRUNCATE, "common %d, players", common ? 1 : 0);
	for (int i = 0; i < count && n > 0 && static_cast<size_t>(n) < size; ++i)
		n += _snprintf_s(out + n, size - n, _TRUNCATE, " %d", sets[i] ? 1 : 0);
}

void __fastcall SetupHook(void *system, void *edx, void *players, BYTE *block)
{
	const bool on = InterlockedCompareExchange(&g_on, 0, 0) != 0;
	BYTE common = 0, sets[kMaxSets] = {};
	int count = 0;
	char what[96] = "";
	const bool read = block && Flags(block, on, &common, sets, &count);
	if (read)
		Describe(common, sets, count, what, sizeof(what));
	if (!on || !read) {
		hooklog::Write("keep without towns: a game's objectives start: the map's rule, a player without towns is out after "
		               "a week (%s)",
		               read ? what : "the map's flags could not be read");
		g_next(system, edx, players, block);
		return;
	}
	g_next(system, edx, players, block);
	const bool back = Restore(block, common, sets, count);
	hooklog::Write("keep without towns: a game's objectives start: ON, a player without towns stays while a hero lives "
	               "(the map's \"lose after a week without towns\": %s; not made in this game%s)",
	               what, back ? "" : "; its flags could not be put back");
}

}  // namespace

bool KeepWithoutTownsWanted()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	return GetPrivateProfileIntW(L"coop", L"keep_without_towns", 0, path) != 0;
}

void InstallKeepWithoutTowns()
{
	g_next = reinterpret_cast<SetupFn>(hookpatch::RedirectCall(kSetupCall, kSetup, reinterpret_cast<const void *>(&SetupHook)));
	if (!g_next) {
		hooklog::Write("keep without towns: the call at %08lx is not as expected (another build?): not available", kSetupCall);
		return;
	}
	hooklog::Write("keep without towns: ready (the host's [coop] keep_without_towns decides for each new multiplayer game; "
	               "this PC's: %d)",
	               KeepWithoutTownsWanted() ? 1 : 0);
}

void SetKeepWithoutTowns(bool on, const char *why)
{
	InterlockedExchange(&g_on, on ? 1 : 0);
	hooklog::Write("keep without towns: %s (%s)", on ? "ON for this game" : "off for this game (the game's rule)", why);
}

}  // namespace mods

// Mod: more time for a PC that joins a multiplayer game ([net] join_timeout_s).
//
// In plain words: the game gives up on a PC that sends nothing for net_timeout seconds (20),
// "timeout 0.18.0" on the joining PC. A PC that joins a random map game first builds the map
// itself (rmg_join.cpp: the game's own way), and while it builds, its game sends nothing:
// with a big map on a slower PC (the owner's 6-player Impossible map under Wine, 2026-11-06:
// about 15 s to build, the box 22 s after Join) the 20 seconds run out. The same limit drops a
// player whose game crashed in a running game, so it is not raised for good: while players
// join (the waiting room) the network manager gets join_timeout_s (90 by default), and at the
// first segment of the game (every PC at the same point) the game's own value again.
//
// How: the game makes its network manager with 0x0047ef60 (new 0x150, then the constructor
// 0x0047ee50 at 0x0047ef7b, __thiscall(manager, const DWORD settings[2], BYTE)), the settings
// being { net_timeout (whole seconds), 20.0f }; the constructor keeps them at +0x0c and +0x10.
// The call is wrapped: the new manager is remembered (with its vtable 0x00e131e4, checked
// before each write), and its +0x0c is set. Robot (LAN, the 6-player Impossible recipe map,
// 2026-11-06): the host makes this manager when it starts hosting, the joining PC only after
// it built the map; the join took 20.1 s of the host's 20 (Join at 31.3 s, the map built at
// 47.1 s, in at 51.4 s): the host's limit is the one that counts. The game also makes a
// manager with 60 s at start-up (not net_timeout's): left alone.
#include "join_time.h"

#include <windows.h>

#include <string>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kCtorCall = 0x0047ef7b;
const DWORD kCtor = 0x0047ee50;
const DWORD kManagerVftable = 0x00e131e4;
const DWORD kTimeoutOffset = 0x0c;
const DWORD kNetTimeoutVar = 0x00f27394;  // the console variable net_timeout (20)

typedef void *(__fastcall *CtorFn)(void *self, void *edx, const DWORD *settings, BYTE flag);
CtorFn g_ctor_next = nullptr;
DWORD g_join_s = 90;           // 0: off (the game's own)
void *g_manager = nullptr;     // the newest network manager
DWORD g_game_s = 0;            // its time limit as the game made it
int g_made = 0;

bool IsManager(void *p)
{
	__try {
		return p && *static_cast<const DWORD *>(p) == kManagerVftable;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool SetLimit(void *manager, DWORD seconds)
{
	if (!IsManager(manager))
		return false;
	__try {
		*reinterpret_cast<DWORD *>(static_cast<BYTE *>(manager) + kTimeoutOffset) = seconds;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void *__fastcall CtorHook(void *self, void *edx, const DWORD *settings, BYTE flag)
{
	void *result = g_ctor_next(self, edx, settings, flag);
	const DWORD limit = settings ? settings[0] : 0;
	// only the manager of a game's players (the game's net_timeout): the game also makes one
	// with its own 60 s at start-up, left as it is
	const bool players = limit == *reinterpret_cast<const DWORD *>(kNetTimeoutVar);
	++g_made;
	bool raised = false;
	if (players) {
		g_manager = self;
		g_game_s = limit;
		raised = g_join_s > limit && SetLimit(self, g_join_s);
	}
	hooklog::Write("join time: the game made a network manager (#%d, its time limit %lu s%s)", g_made, limit,
	               raised ? (": " + std::to_string(g_join_s) + " s while players join").c_str()
	               : players ? "" : ", not the players' one: left as it is");
	return result;
}

}  // namespace

void JoinTimeGameStarted()
{
	if (!g_manager || !g_join_s || g_game_s >= g_join_s)
		return;
	const bool ok = SetLimit(g_manager, g_game_s);
	hooklog::Write("join time: the game starts: the network manager's time limit is the game's %lu s again%s", g_game_s,
	               ok ? "" : " (FAILED: the manager is gone)");
	g_manager = nullptr;
}

void ApplyJoinTimeFromSettings()
{
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);
	g_join_s = static_cast<DWORD>(GetPrivateProfileIntW(L"net", L"join_timeout_s", 90, ini));
	if (g_join_s > 600)
		g_join_s = 600;
	g_ctor_next = reinterpret_cast<CtorFn>(hookpatch::RedirectCall(kCtorCall, kCtor, reinterpret_cast<const void *>(&CtorHook)));
	hooklog::Write(g_ctor_next ? (g_join_s ? "join time: on: %lu s for a PC that joins (the game's 20 s again once the game runs)"
	                                       : "join time: off: the game's own time limit (join_timeout_s=0)%.0lu")
	                           : "join time: the game's code is not as expected: off%.0lu",
	               g_join_s);
}

}  // namespace mods

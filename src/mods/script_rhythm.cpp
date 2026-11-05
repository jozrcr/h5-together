// Mod: MMH5.5's scripts at their own pace in co-op games with our faster segments
// ([coop] script_rhythm: the host's choice, on by default; docs/plans/performance.md).
//
// In plain words: map scripts (MMH5.5's above all) wait with sleep(n), where n counts
// "ticks" of the game's script scheduler, and the game ticks it once per update of the
// world: once per segment in a multiplayer game. MMH5.5 was made for the game's 200 ms
// segments: its main loop (H55_ContinuesActivator in H55-Core.lua) checks every hero of
// the players whose turn it is every 2 ticks, 0.4 s. With our 50 ms co-op segments that
// is every 0.1 s: four times the work (robot, 2026-11-04: GetPlayerState 80 calls a second
// instead of 19), for a choppier map. With this on, the scheduler ticks once per 200 ms of
// game time, as with the game's segments, so every script keeps its original pace: MMH5.5's
// loop, and its crash check too (H55_CrashProtection waits 2 ticks each new day for the loop
// to have seen the day; with fewer loop passes and as many ticks it would think the loop had
// crashed and start one more each day: robot, 2026-11-04, with H55_CycleSpeed = 8).
//
// How:
//   - the world's update (0x009987b0, __thiscall(world, DWORD game time), ret 4) ends with
//     the script scheduler's tick: a virtual call (0x00998968) to the scripts' object at
//     world +0x1d0 +0x0c, slot 2 (0x00cab4f0, cell 0x00e93768), which goes on to the
//     scheduler 0x004c0720 (every sleeping script thread: one tick less; at 0, it runs on);
//   - in a multiplayer game the world is updated by NWorld::CSetWorldTime's Execute
//     (0x00999070: world->Update(its time), the call at 0x009990bd), inside the segment
//     runner (coop_features: InMultiplayerSegment);
//   - so: while such an update runs, and the game's co-op features include script_rhythm
//     (agreed between the PCs at the start: coop_features.cpp), the tick goes through only
//     when the update's game time is in a new 200 ms step of game time (time / 200 changed).
//     Every PC runs the same updates with the same times: the same ticks on every PC.
// With the game's 200 ms segments every update is in a new step: nothing changes.
// Single player, and every other caller of the tick (the map's scripts starting,
// 0x00992e43): untouched.
#include "script_rhythm.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "coop_features.h"
#include "coop_features_logic.h"

#include "../hook/log.h"
#include "../hook/patch.h"
#include "../hook/robot.h"

namespace mods {
namespace {

const DWORD kUpdate = 0x009987b0;        // the world's update, __thiscall(world, DWORD game time), ret 4
const DWORD kSetTimeUpdateCall = 0x009990bd;  // in NWorld::CSetWorldTime's Execute 0x00999070
const DWORD kTickCell = 0x00e93768;      // the scripts' object's vtable 0x00e93760, slot 2
const DWORD kTick = 0x00cab4f0;          // mov ecx, [ecx+0x1c]; jmp 0x004c0720
const DWORD kStepMs = 200;               // the game's segment: MMH5.5's tick
const DWORD kWorldScripts = 0x1d0;       // world +0x1d0: its scripts' holder; +0x0c of it: the scripts' object
const DWORD kScriptsVftable = 0x00e93760;
const DWORD kDoString = 0x00cab4a0;      // slot 0: __thiscall(scripts, const char *code), ret 4: false when the line
                                         // ran without an error (robot 2026-11-05: its print came, it answered 0)

struct Code {
	DWORD address;
	BYTE bytes[9];
	int size;
};
const Code kCode[] = {
	{kTick, {0x8b, 0x49, 0x1c, 0xe9, 0x28, 0x52, 0x81, 0xff}, 8},
	{0x00998963, {0x8b, 0x4e, 0x0c, 0x8b, 0x01, 0xff, 0x50, 0x08}, 8},  // the update's tick: [esi+0xc], slot 2
	{0x009990b4, {0x8b, 0x4e, 0x10, 0x83, 0xc4, 0x14, 0x51, 0x8b, 0xc8}, 9},  // push [this+0x10] (the time), ecx = world
};

typedef void(__fastcall *UpdateFn)(void *world, void *edx, DWORD time);
typedef int(__fastcall *TickFn)(void *scripts, void *edx);
UpdateFn g_update_next = nullptr;
TickFn g_tick_next = nullptr;

// All on the game's main thread (the world's update), except the counters.
void *g_world = nullptr;  // the world of the last multiplayer update (RunGameScript)
int g_in_update = 0;      // inside a multiplayer world update (CSetWorldTime, in the segment runner)
DWORD g_update_time = 0;  // its game time
unsigned g_game = 0;      // the co-op features' game these belong to
bool g_have_last = false;
DWORD g_last_time = 0, g_last_step = 0;
int g_logged_times = 0;
char g_times[200];

void __fastcall SetTimeUpdateHook(void *world, void *edx, DWORD time)
{
	if (!InMultiplayerSegment()) {
		g_update_next(world, edx, time);
		return;
	}
	g_world = world;
	const DWORD outer = g_update_time;
	g_update_time = time;
	++g_in_update;
	g_update_next(world, edx, time);
	--g_in_update;
	g_update_time = outer;
}

int __fastcall TickHook(void *scripts, void *edx)
{
	if (g_in_update > 0 && CoopFeatureOn(features::kScriptRhythm)) {
		if (g_game != CoopFeaturesGame()) {
			g_game = CoopFeaturesGame();
			g_have_last = false;
			g_logged_times = 0;
			g_times[0] = 0;
			hooklog::Write("script rhythm: on for this game: the scripts tick once per %lu ms of game time", kStepMs);
		}
		const DWORD time = g_update_time, step = time / kStepMs;
		const bool hold = g_have_last && time >= g_last_time && step == g_last_step;
		if (g_logged_times < 12) {  // the first updates' game times, and which ones ticked (*)
			char item[24];
			_snprintf_s(item, sizeof(item), _TRUNCATE, " %lu%s", time, hold ? "" : "*");
			strncat_s(g_times, sizeof(g_times), item, _TRUNCATE);
			if (++g_logged_times == 12)
				hooklog::Write("script rhythm: the first updates' game times (* = the scripts ticked):%s", g_times);
		}
		if (hold) {
			robot::CountScriptTick(true);
			return 0;  // (the update's callers do not use what it returns)
		}
		g_have_last = true;
		g_last_time = time;
		g_last_step = step;
	}
	robot::CountScriptTick(false);
	return g_tick_next(scripts, edx);
}

bool CodeAsExpected()
{
	for (const Code &c : kCode)
		if (memcmp(reinterpret_cast<const void *>(c.address), c.bytes, c.size) != 0) {
			hooklog::Write("script rhythm: the game's code at %08lx is not as expected: off", c.address);
			return false;
		}
	return true;
}

bool RunGameScriptUnsafe(const char *code)
{
	const BYTE *holder = *reinterpret_cast<const BYTE *const *>(static_cast<const BYTE *>(g_world) + kWorldScripts);
	if (!holder)
		return false;
	void *scripts = *reinterpret_cast<void *const *>(holder + 0x0c);
	if (!scripts || *static_cast<const DWORD *>(scripts) != kScriptsVftable)
		return false;
	typedef bool(__fastcall * DoStringFn)(void *scripts, void *edx, const char *code);
	const bool error = reinterpret_cast<DoStringFn>(kDoString)(scripts, nullptr, code);
	if (error)
		hooklog::Write("script rhythm: the game's scripts answered an error for: %s", code);
	return !error;
}

}  // namespace

bool RunGameScript(const char *code)
{
	if (!g_world || !code)
		return false;
	__try {
		return RunGameScriptUnsafe(code);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::Write("script rhythm: running a line of script crashed (caught)");
		return false;
	}
}

void ApplyScriptRhythmFromSettings()
{
	// Every PC hooks it, whatever its own setting: the host's [coop] script_rhythm decides
	// for each game (coop_features.cpp).
	if (!CodeAsExpected())
		return;
	g_update_next = reinterpret_cast<UpdateFn>(hookpatch::RedirectCall(kSetTimeUpdateCall, kUpdate, &SetTimeUpdateHook));
	g_tick_next = reinterpret_cast<TickFn>(hookpatch::PatchPointer(kTickCell, kTick, &TickHook));
	if (!g_update_next || !g_tick_next) {
		hooklog::Write("script rhythm: part of the game's code is not as expected (update %d, tick %d): off",
		               g_update_next != nullptr, g_tick_next != nullptr);
		return;  // (a lone hook only passes on: g_in_update needs the update's, the gate the tick's)
	}
	hooklog::Write("script rhythm: ready (the host's [coop] script_rhythm decides at each game's start)");
}

}  // namespace mods

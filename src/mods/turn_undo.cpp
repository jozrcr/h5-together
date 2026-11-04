// Mod: undo End Turn while the other humans still play (docs/plans/turn-features.md,
// part 1). EXPERIMENT: only in robot runs, driven by robot steps; no setting, no
// button yet. It answers the plan's open questions: does the game give a player the
// controls back when its turn state goes from 1 ("done for today") to 0, and does
// the day go on as before (world checksums equal on every PC)?
//
// How a human's turn ends (players.md, turn-features.md): End Turn sets the player's
// "pressed End Turn" (+0x178, IPlayer slot 0x50); with simultaneous turns, the world's
// turn manager (0x00998520, every world tick) then gives state 1 to each player at
// state 0 who pressed it, as long as some player at state 0 has not; when none is
// left, the next turn starts (0x00998060) in the same tick.
//
// The undo: at the start of a segment (the segment runner's call 0x007c9a67, before
// that segment's commands: the same point of the game on every PC), if the undo is
// still allowed on this world (simultaneous turns on; the player human, in the game,
// pressed End Turn, at state 0 or 1; another human at state 0 who has not pressed
// it), the player's own setters put "pressed" to 0 and the state to 0. Every PC
// decides on the same world, so they all do it, or none.
//
// Robot steps (both copies get the same ones):
//   panel|turn undo <player> <segment>   undo for player <player> (1..8) at the first
//                                         segment from <segment> on where it is allowed
//   panel|turn states                     log every player's turn state
#include "turn_undo.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "../hook/log.h"
#include "../hook/patch.h"
#include "../hook/robot.h"

namespace mods {
namespace {

const DWORD kWorldVftable = 0x00e6260c;  // NWorld::CWorld
const DWORD kWorldDaySlot = kWorldVftable + 0xa8;
const DWORD kWorldDay = 0x00c61ab0;      // lea eax, [ecx+0x48]; ret (the adventure screen asks every frame)
const DWORD kSegmentCall = 0x007c9a67;   // in the segment runner 0x007c98e0, before a segment's commands
const DWORD kSegmentTime = 0x007cc680;   // __thiscall(transceiver + 8, DWORD game time), ret 4
// The world: its players (vector of IPlayer *), simultaneous turns (byte)
const DWORD kPlayers = 0x24, kPlayersEnd = 0x28, kSimultaneous = 0x90;
// IPlayer slots (byte offsets in its vtable 0x00e1a09c)
const DWORD kNumber = 0x2c, kState = 0x44, kSetState = 0x48, kPressed = 0x4c, kSetPressed = 0x50, kInGame = 0x6c,
            kHuman = 0x184;

struct Code {
	DWORD address;
	BYTE bytes[12];
	int size;
};
// The functions behind the slots above (players.md), checked before anything is changed.
const Code kCode[] = {
	{kWorldDay, {0x8d, 0x41, 0x48, 0xc3}, 4},
	{0x00720e90, {0x8b, 0x41, 0x78, 0xc3}, 4},                            // slot 0x2c: the number at +0x78
	{0x00500c10, {0x8b, 0x81, 0x34, 0x06, 0x00, 0x00}, 6},                // slot 0x44: the state at +0x634
	{0x00500c40, {0x8a, 0x81, 0x78, 0x01, 0x00, 0x00}, 6},                // slot 0x4c: pressed at +0x178
	{0x00500c50, {0x8a, 0x44, 0x24, 0x04, 0x38, 0x81, 0x78, 0x01}, 8},   // slot 0x50: set it
	{0x00500d60, {0x33, 0xc0, 0x39, 0x41, 0x74, 0x0f, 0x94, 0xc0, 0xc3}, 9},  // slot 0x184: human
};

typedef const void *(__fastcall *WorldDayFn)(const void *world, void *edx);
typedef void(__fastcall *SegmentFn)(void *transceiver8, void *edx, DWORD time);
typedef int(__fastcall *GetFn)(void *self, void *edx);
typedef void(__fastcall *SetFn)(void *self, void *edx, int value);

WorldDayFn g_world_day_next = nullptr;
SegmentFn g_segment_next = nullptr;
void *volatile g_world = nullptr;
const void *g_transceiver = nullptr;
DWORD g_segments = 0;  // segments started in this game
int g_want_player = 0;  // a robot undo waiting: its player and first segment
DWORD g_want_from = 0;
// The robot's pictures: the undo waits kShotGap segments after it is first allowed (a
// picture of the "ended" screen), and another picture comes kShotGap segments after it.
// The same segments on every PC: each decides on the same world.
const DWORD kShotGap = 30;
DWORD g_allowed_at = 0, g_after_shot_at = 0;

DWORD Read(const void *p, DWORD off)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off);
}

int Get(void *player, DWORD slot)
{
	return reinterpret_cast<GetFn>(Read(reinterpret_cast<void *>(Read(player, 0)), slot))(player, nullptr);
}

void Set(void *player, DWORD slot, int value)
{
	reinterpret_cast<SetFn>(Read(reinterpret_cast<void *>(Read(player, 0)), slot))(player, nullptr, value);
}

// The game's "is this object being destroyed" test (players.md).
bool Dying(const void *object)
{
	const BYTE *o = static_cast<const BYTE *>(object);
	const DWORD vbtable = Read(o, 4);
	const LONG base = *reinterpret_cast<const LONG *>(vbtable + 4);
	return (o[base + 0xb] & 0x80) != 0;
}

const void *__fastcall WorldDayHook(const void *world, void *edx)
{
	g_world = const_cast<void *>(world);
	return g_world_day_next(world, edx);
}

// "1h:0/1 2h:1/0 3ai:..." : number, human or computer, state / pressed End Turn.
std::string States(void *world)
{
	std::string out;
	void **p = reinterpret_cast<void **>(Read(world, kPlayers));
	void **end = reinterpret_cast<void **>(Read(world, kPlayersEnd));
	for (; p && p < end; ++p) {
		if (!*p || Dying(*p) || Get(*p, kInGame) != 1)
			continue;
		char item[48];
		_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%d%s:%d/%d", out.empty() ? "" : " ", Get(*p, kNumber),
		            (Get(*p, kHuman) & 0xff) ? "h" : "ai", Get(*p, kState), Get(*p, kPressed) & 0xff);
		out += item;
	}
	return out;
}

// Is the undo for `number` allowed on this world now (then, with `apply`, it is done)?
// `why` says why not.
bool TryUndo(void *world, int number, const char **why, bool apply)
{
	if (!*(static_cast<const BYTE *>(world) + kSimultaneous)) {
		*why = "no simultaneous turns";
		return false;
	}
	void *target = nullptr;
	bool other_playing = false;
	void **p = reinterpret_cast<void **>(Read(world, kPlayers));
	void **end = reinterpret_cast<void **>(Read(world, kPlayersEnd));
	for (; p && p < end; ++p) {
		if (!*p || Dying(*p) || Get(*p, kInGame) != 1 || !(Get(*p, kHuman) & 0xff))
			continue;
		if (Get(*p, kNumber) == number)
			target = *p;
		else if (Get(*p, kState) == 0 && !(Get(*p, kPressed) & 0xff))
			other_playing = true;
	}
	if (!target) {
		*why = "no such human player in the game";
		return false;
	}
	const int state = Get(target, kState);
	if (!(Get(target, kPressed) & 0xff) || (state != 0 && state != 1)) {
		*why = "the player has not ended its turn";
		return false;
	}
	if (!other_playing) {
		*why = "no other human still plays";
		return false;
	}
	if (apply) {
		Set(target, kSetPressed, 0);
		Set(target, kSetState, 0);
	}
	return true;
}

void RobotCommand(const char *text)
{
	if (strncmp(text, "undo ", 5) == 0) {
		char *rest = nullptr;
		g_want_player = static_cast<int>(strtol(text + 5, &rest, 10));
		g_want_from = rest ? strtoul(rest, nullptr, 10) : 0;
		hooklog::Write("turn undo: robot: undo for player %d from segment %lu on", g_want_player, g_want_from);
	}
	else if (strcmp(text, "states") == 0) {
		void *world = g_world;
		hooklog::Write("turn undo: segment %lu: %s", g_segments, world ? States(world).c_str() : "(no world)");
	}
	else
		hooklog::Write("turn undo: robot command \"%s\" not known", text);
}

void UndoStep(void *world)
{
	const std::string before = States(world);
	const char *why = "";
	if (!g_allowed_at) {  // first time allowed: a picture now, the undo kShotGap segments later
		if (!TryUndo(world, g_want_player, &why, false)) {
			if (g_segments % 20 == 0)
				hooklog::Write("turn undo: segment %lu: not for player %d yet: %s (%s)", g_segments, g_want_player, why, before.c_str());
			return;
		}
		g_allowed_at = g_segments;
		robot::RequestScreenshot("turn-ended");
		hooklog::Write("turn undo: segment %lu: allowed for player %d (%s); a picture, the undo at segment %lu", g_segments,
		               g_want_player, before.c_str(), g_segments + kShotGap);
		return;
	}
	if (g_segments < g_allowed_at + kShotGap)
		return;
	g_allowed_at = 0;
	if (TryUndo(world, g_want_player, &why, true)) {
		g_after_shot_at = g_segments + kShotGap;
		hooklog::Write("turn undo: segment %lu: player %d plays again (before: %s; after: %s)", g_segments, g_want_player,
		               before.c_str(), States(world).c_str());
		g_want_player = 0;
	}
	else
		hooklog::Write("turn undo: segment %lu: no longer allowed for player %d: %s (%s)", g_segments, g_want_player, why, before.c_str());
}

void SafeUndoStep(void *world)
{
	__try {
		UndoStep(world);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::Write("turn undo: crashed reading the players (caught): off");
		g_want_player = 0;
	}
}

void __fastcall SegmentHook(void *transceiver8, void *edx, DWORD time)
{
	const void *t = static_cast<const BYTE *>(transceiver8) - 8;
	if (t != g_transceiver) {
		g_transceiver = t;
		g_segments = 0;
	}
	++g_segments;
	char command[64];
	while (robot::TakePanelCommand(command, sizeof(command), "turn "))
		RobotCommand(command);
	void *world = g_world;
	if (g_want_player && g_segments >= g_want_from && world)
		SafeUndoStep(world);
	if (g_after_shot_at && g_segments >= g_after_shot_at) {
		g_after_shot_at = 0;
		robot::RequestScreenshot("turn-undone");
		if (world)
			hooklog::Write("turn undo: segment %lu: a picture after the undo (%s)", g_segments, States(world).c_str());
	}
	g_segment_next(transceiver8, edx, time);
}

bool CodeAsExpected()
{
	for (const Code &c : kCode)
		if (memcmp(reinterpret_cast<const void *>(c.address), c.bytes, c.size) != 0) {
			hooklog::Write("turn undo: the game's code at %08lx is not as expected: off", c.address);
			return false;
		}
	return true;
}

}  // namespace

void ApplyTurnUndoFromSettings()
{
	if (!robot::Active() || !CodeAsExpected())
		return;
	g_world_day_next = reinterpret_cast<WorldDayFn>(hookpatch::PatchPointer(kWorldDaySlot, kWorldDay, &WorldDayHook));
	g_segment_next = reinterpret_cast<SegmentFn>(hookpatch::RedirectCall(kSegmentCall, kSegmentTime, &SegmentHook));
	if (!g_world_day_next || !g_segment_next) {
		hooklog::Write("turn undo: the game's code is not as expected: off");
		return;
	}
	hooklog::Write("turn undo: on (robot experiment: panel|turn undo <player> <segment>, panel|turn states)");
}

}  // namespace mods

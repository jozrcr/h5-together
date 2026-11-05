// Mod: follow a friend's view of the adventure map (docs/plans/spectate.md).
//
// In plain words: while a player waits (or any time), a click on an ally's name in the
// player list makes this PC's camera follow that ally's camera, gliding, until the name
// is clicked again, the player moves the camera, or this PC's turn begins. The ally's
// PC sends where its camera looks (a tile, a few times a second) only while an ally
// follows it, and shows an eye next to the follower's name. Nothing of the game world
// changes: each PC only moves its own camera, so this cannot desync a game.
//
// 1. Messages: world-checksum messages with check number -1 (the game ignores them),
//    32 bits, mark 'W' (spectate_logic.h): "watch" (follower -> followed, on/off),
//    "view" (the followed's floor and tile) and "status" (what it has on screen).
//    They go out after this PC's own segment report (the transceiver's step), as the
//    other mods' messages do; they come in through the message pump's checksum call.
// 2. The camera (found 2026-11-05, robot): the adventure screen (NUI::CAdventureScreen)
//    has its CCamera at +0xa4 (its position at +0x70, 10 floats: [7] [8] the point
//    looked at, in world units, 2 per tile) and its CCameraOperator at +0x5e4. The
//    operator's slot 0x5c (object, flag) is the object it keeps in view (the selected
//    hero: it puts the camera back on it every frame), slot 0x50 (pos, apply) sets the
//    position. The floor shown is the screen's +0xb4; 0x00594440 switches it, as the
//    game's own button does.
// 3. Every frame (after it is shown, in a Present callback, on the game's thread): as the
//    followed, read the camera and queue what changed; as the follower, let go of the
//    operator's object and glide toward the ally's point.
// 4. The banner (an overlay client): "Stop following Pierre", a button at the top of the
//    adventure map, and a line under it ("Pierre is in a battle", ...).
//
// Research probe ([debug] spectate_probe=1, robot runs): the camera's position, the floor
// and the operator's object, every 2 s.
#include "spectate.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "rmg_builder_widgets.h"
#include "spectate_logic.h"

#include "../hook/d3d9_hook.h"
#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/patch.h"
#include "../hook/robot.h"
#include "../hook/texts.h"
#include "../hook/third_party/imgui/imgui.h"

namespace mods {
namespace {

namespace ui = builder_ui;

const char kName[] = "spectate";
const char kAdventureScreen[] = ".?AVCAdventureScreen@NUI@@";

// --- the game (vanilla 3.1 addresses, the same in MMH55_Utility) -----------------------------
const DWORD kUpdateCall = 0x0060895e;      // in 0x00608930: call 0x006082f0 (the adventure controller's frame)
const DWORD kUpdate = 0x006082f0;          // __thiscall, 5 stack arguments
const DWORD kControllerWorld = 0x18, kControllerLocal = 0x3c;  // NWorld world, this PC's IPlayer *
const DWORD kMpStepCell = 0x00e43f1c + 0x10;  // NWorld::CMultiPlayerTransceiver slot 4 (its own step: reports)
const DWORD kMpStep = 0x007ca990;
const DWORD kCheckCall = 0x007ca8d1;       // in the message pump 0x007ca7c0: call 0x007c9410 (a checksum came)
const DWORD kCheckHandler = 0x007c9410;    // __thiscall(int pc, CCrcTestCmd *), ret 8; ignores check -1
const DWORD kCrcCmdVftable = 0x00e43db0;   // NWorld::CCrcTestCmd, 0x2c bytes
const DWORD kGameNew = 0x00878fd0;
const DWORD kReleaseStrong = 0x008c61d0;   // __thiscall(object, mask)
const DWORD kNet = 0x34;                   // the transceiver's network
// The world's players (vector of IPlayer *); an IPlayer (vtable 0x00e1a09c, players.md)
const DWORD kPlayers = 0x24, kPlayersEnd = 0x28;
const DWORD kPlayerVftable = 0x00e1a09c;
const DWORD kHuman = 0x74, kNumber = 0x78, kInGame = 0x7c, kPressed = 0x178, kTeam = 0x1cc, kTurn = 0x634;
// The adventure screen, its camera and operator
const DWORD kScreenCamera = 0xa4, kScreenFloor = 0xb4, kScreenOperator = 0x5e4;
const DWORD kCameraVftable = 0x00e44224, kCameraPos = 0x70;  // slot 0x54 = 0x004f8be0: lea eax,[ecx+70h]
const DWORD kOperatorVftable = 0x00e4493c;
const DWORD kOperatorSet = 0x007d4ce0, kOperatorFollow = 0x007d6f40;  // slots 0x50, 0x5c
const DWORD kOperatorObject = 0x188;      // the object it keeps in view (slot 0x5c sets it)
const DWORD kSwitchFloor = 0x00594440;     // __fastcall(screen)

struct Code {
	DWORD address;
	BYTE bytes[10];
	int size;
};
// Checked before anything is changed (the hooked code as the other mods check it, the
// players' fields behind their slots, the camera's functions).
const Code kCode[] = {
	{0x00608959, {0x51, 0x52, 0x50, 0x8b, 0xce, 0xe8}, 6},                // push ecx/edx/eax; mov ecx,esi; call (the frame)
	{kMpStep, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x1c, 0x21, 0x00}, 8},
	{kCheckHandler, {0x83, 0xec, 0x60, 0x33, 0xc0, 0x39, 0x44, 0x24}, 8},
	{0x00720e90, {0x8b, 0x41, 0x78, 0xc3}, 4},                            // IPlayer slot 0x2c: the number at +0x78
	{0x00500c10, {0x8b, 0x81, 0x34, 0x06, 0x00, 0x00}, 6},                // slot 0x44: the turn state at +0x634
	{0x00500c40, {0x8a, 0x81, 0x78, 0x01, 0x00, 0x00}, 6},                // slot 0x4c: End Turn pressed at +0x178
	{0x00500bc0, {0x8b, 0x81, 0xcc, 0x01, 0x00, 0x00, 0xc3}, 7},          // slot 0x14: the team at +0x1cc
	{0x00500d60, {0x33, 0xc0, 0x39, 0x41, 0x74, 0x0f, 0x94, 0xc0, 0xc3}, 9},  // slot 0x184: human (+0x74 == 0)
	{0x004f8be0, {0x8d, 0x41, 0x70, 0xc3}, 4},                            // CCamera slot 0x54: its position at +0x70
	{kOperatorSet, {0x53, 0x55, 0x8b, 0xd9, 0x8b, 0x03, 0x56, 0x8b, 0x74, 0x24}, 10},
	{kOperatorFollow, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xec, 0x0c, 0x85, 0xc0, 0x56}, 10},
	{kSwitchFloor, {0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0xf4, 0x05, 0x00, 0x00, 0xe8}, 10},
};
struct Slot {
	DWORD vtable, offset, function;
};
const Slot kSlots[] = {
	{kCameraVftable, 0x54, 0x004f8be0},
	{kOperatorVftable, 0x50, kOperatorSet},
	{kOperatorVftable, 0x5c, kOperatorFollow},
	{kPlayerVftable, 0x2c, 0x00720e90},
	{kPlayerVftable, 0x14, 0x00500bc0},
};

typedef void(__fastcall *UpdateFn)(void *controller, void *edx, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);
typedef void(__fastcall *StepFn)(void *transceiver, void *edx);
typedef void(__fastcall *CheckFn)(void *transceiver, void *edx, int pc, const BYTE *cmd);
typedef void(__fastcall *SetPosFn)(void *op, void *edx, const float *pos, int apply);
typedef void(__fastcall *FollowFn)(void *op, void *edx, void *object, int flag);
typedef void(__fastcall *SwitchFloorFn)(void *screen, void *edx);

UpdateFn g_update_next = nullptr;
StepFn g_mp_step_next = nullptr;
CheckFn g_check_next = nullptr;
bool g_ready = false;  // the hooks are in and the game is as expected

// --- settings ----------------------------------------------------------------------------------
bool g_share = true;  // [coop] share_view: allies may follow this PC's view
int g_probe = 0;      // [debug] spectate_probe

// --- state: all on the game's main thread --------------------------------------------------------
void *g_controller = nullptr;         // the adventure controller (world, this PC's player)
const void *g_transceiver = nullptr;  // the multiplayer transceiver, while a multiplayer game runs
DWORD g_transceiver_seen = 0;         // when its step last ran (GetTickCount): none for 3 s = no game

struct PlayerInfo {
	bool known, human, in_game;
	int team;
	DWORD turn;    // 0 = playing its turn
	bool pressed;  // End Turn pressed
};
PlayerInfo g_players[9];  // by number 1..8, read every frame
char g_names[9][64];      // by number: the player list's names ("" = not told)
int g_me = 0;             // this PC's player number

// Messages waiting for this PC's next step
DWORD g_queue[16];
int g_queued = 0;

// As the followed: who follows, and what was last sent
spectate::Watchers g_watchers;
bool g_watchers_told[9];
int g_sent_x = -1, g_sent_y = -1, g_sent_floor = -1, g_sent_screen = -1;
DWORD g_sent_view_at = 0, g_sent_status_at = 0;

// As the follower
int g_following = 0;  // the player this PC follows, 0 = none
DWORD g_follow_since = 0;
DWORD g_watch_sent_at = 0;
bool g_have_view = false;
int g_view_x = 0, g_view_y = 0, g_view_floor = 0;
int g_their_screen = spectate::kOnMap;
bool g_set = false;  // we put the camera somewhere in the frame before
float g_set_x = 0, g_set_y = 0;
bool g_my_turn_before = false;
LARGE_INTEGER g_last_frame = {};

DWORD Read(const void *p, DWORD off)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off);
}

BYTE ReadByte(const void *p, DWORD off)
{
	return *(static_cast<const BYTE *>(p) + off);
}

void Queue(DWORD value)
{
	if (g_queued < static_cast<int>(sizeof(g_queue) / sizeof(g_queue[0])))
		g_queue[g_queued++] = value;
}

// A world-checksum message with check number -1 and our value, to the other PCs (as
// Undo End Turn and hero looks send theirs; the game ignores check -1).
void SendValue(const BYTE *transceiver, DWORD value)
{
	typedef void *(__cdecl * NewFn)(unsigned size);
	typedef void(__fastcall * SendFn)(void *net, void *edx, void *cmd);
	typedef void(__fastcall * StrongReleaseFn)(void *cmd, void *edx, DWORD mask);
	BYTE *cmd = static_cast<BYTE *>(reinterpret_cast<NewFn>(kGameNew)(0x2c));
	if (!cmd)
		return;
	memset(cmd, 0, 0x2c);
	*reinterpret_cast<DWORD *>(cmd) = kCrcCmdVftable;
	*reinterpret_cast<LONG *>(cmd + 0x0c) = -1;
	*reinterpret_cast<DWORD *>(cmd + 0x10) = value;
	*reinterpret_cast<DWORD *>(cmd + 4) += 1;
	void *net = reinterpret_cast<void *>(Read(transceiver, kNet));
	(*reinterpret_cast<SendFn *const *>(net))[7](net, nullptr, cmd);
	*reinterpret_cast<DWORD *>(cmd + 4) -= 1;
	if ((*reinterpret_cast<DWORD *>(cmd + 4) & 0xfffff) == 0)
		reinterpret_cast<StrongReleaseFn>(kReleaseStrong)(cmd, nullptr, 0xfffff);
}

bool InMultiplayer()
{
	return g_transceiver && GetTickCount() - g_transceiver_seen < 3000;
}

// --- the players -------------------------------------------------------------------------------
void ReadPlayersUnsafe()
{
	PlayerInfo fresh[9];
	memset(fresh, 0, sizeof(fresh));
	int me = 0;
	if (g_controller) {
		const void *world = reinterpret_cast<const void *>(Read(g_controller, kControllerWorld));
		const void *local = reinterpret_cast<const void *>(Read(g_controller, kControllerLocal));
		if (world) {
			const DWORD *p = reinterpret_cast<const DWORD *>(Read(world, kPlayers));
			const DWORD *end = reinterpret_cast<const DWORD *>(Read(world, kPlayersEnd));
			for (int i = 0; p && p + i < end && i < 16; ++i) {
				const void *player = reinterpret_cast<const void *>(p[i]);
				if (!player || Read(player, 0) != kPlayerVftable)
					continue;
				const int number = static_cast<int>(Read(player, kNumber));
				if (number < 1 || number > 8)
					continue;
				PlayerInfo &info = fresh[number];
				info.known = true;
				info.human = Read(player, kHuman) == 0;
				info.in_game = Read(player, kInGame) == 1;
				info.team = static_cast<int>(Read(player, kTeam));
				info.turn = Read(player, kTurn);
				info.pressed = ReadByte(player, kPressed) != 0;
				if (player == local)
					me = number;
			}
		}
	}
	memcpy(g_players, fresh, sizeof(g_players));
	g_me = me;
}

void ReadPlayers()
{
	__try {
		ReadPlayersUnsafe();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		memset(g_players, 0, sizeof(g_players));
		g_me = 0;
	}
}

bool Followable(int player)
{
	if (!g_ready || !InMultiplayer() || g_me == 0 || player < 1 || player > 8)
		return false;
	const PlayerInfo &a = g_players[g_me], &b = g_players[player];
	return a.known && b.known && b.human && b.in_game && spectate::MayFollow(g_me, a.team, player, b.team);
}

// "Pierre", or "Player 2" when the player list has not told us the name.
std::string NameOf(int player)
{
	if (player >= 1 && player <= 8 && g_names[player][0])
		return g_names[player];
	return texts::Fill(texts::Get("spectate.player"), {{"n", std::to_string(player)}});
}

// --- the camera ----------------------------------------------------------------------------------
// The adventure screen when it is the screen on top, else nullptr.
BYTE *Screen()
{
	char top[96] = "";
	if (!overlay::TopScreen(top, sizeof(top)) || strcmp(top, kAdventureScreen) != 0)
		return nullptr;
	return static_cast<BYTE *>(overlay::TopScreenObject());
}

int ScreenCode()
{
	char top[96] = "";
	if (!overlay::TopScreen(top, sizeof(top)))
		return spectate::kElsewhere;
	if (strstr(top, "CAdventure"))
		return spectate::kOnMap;
	if (strstr(top, "Combat"))
		return spectate::kInBattle;
	if (strstr(top, "Town"))
		return spectate::kInTown;
	if (strstr(top, "Hero"))
		return spectate::kInHeroScreen;
	return spectate::kElsewhere;
}

// The screen's camera position (10 floats), or nullptr when it is not as expected.
float *CameraPos(BYTE *screen)
{
	BYTE *camera = reinterpret_cast<BYTE *>(Read(screen, kScreenCamera));
	if (!camera || Read(camera, 0) != kCameraVftable)
		return nullptr;
	return reinterpret_cast<float *>(camera + kCameraPos);
}

void *Operator(BYTE *screen)
{
	void *op = reinterpret_cast<void *>(Read(screen, kScreenOperator));
	return op && Read(op, 0) == kOperatorVftable ? op : nullptr;
}

// --- as the followed: tell the followers where this PC looks ---------------------------------
void ShareView(DWORD now)
{
	bool any = false;
	for (int p = 1; p <= 8; ++p) {
		const bool active = g_watchers.Active(p, now);
		if (active != g_watchers_told[p]) {
			g_watchers_told[p] = active;
			hooklog::Write("spectate: %s %s this PC's view%s", NameOf(p).c_str(), active ? "follows" : "stopped following",
			               active && !g_share       ? " (asked; not shared: share_view=0)"
			               : active && !Followable(p) ? " (asked; not shared: not an ally)"
			                                          : "");
		}
		any = any || (active && g_share && Followable(p));
	}
	if (!any || g_me == 0)
		return;
	const int screen_code = ScreenCode();
	if (screen_code != g_sent_screen || now - g_sent_status_at >= 2000) {
		Queue(spectate::EncodeStatus(g_me, screen_code));
		g_sent_screen = screen_code;
		g_sent_status_at = now;
	}
	BYTE *screen = Screen();
	if (!screen)
		return;
	const float *pos = CameraPos(screen);
	if (!pos)
		return;
	const int x = spectate::ToTile(pos[7]), y = spectate::ToTile(pos[8]);
	const int floor = Read(screen, kScreenFloor) ? 1 : 0;
	const bool moved = x != g_sent_x || y != g_sent_y || floor != g_sent_floor;
	if (moved ? now - g_sent_view_at >= 200 : now - g_sent_view_at >= 2000) {
		Queue(spectate::EncodeView(g_me, floor, x, y));
		g_sent_x = x;
		g_sent_y = y;
		g_sent_floor = floor;
		g_sent_view_at = now;
	}
}

// --- as the follower -------------------------------------------------------------------------------
void Stop(const char *why)
{
	if (!g_following)
		return;
	hooklog::Write("spectate: stopped following %s (%s)", NameOf(g_following).c_str(), why);
	if (g_me)
		Queue(spectate::EncodeWatch(g_me, g_following, false));
	g_following = 0;
	g_have_view = false;
	g_set = false;
}

void Start(int player, const char *how)
{
	if (player == g_me || !Followable(player)) {
		hooklog::Write("spectate: %s cannot be followed (%s)", NameOf(player).c_str(),
		               !InMultiplayer() ? "not in a multiplayer game"
		               : player == g_me ? "this PC's own player"
		                                : "not an ally, not a human or not in the game");
		return;
	}
	Stop("another player chosen");
	g_following = player;
	g_follow_since = GetTickCount();
	g_watch_sent_at = g_follow_since;
	g_have_view = false;
	g_their_screen = spectate::kOnMap;
	g_set = false;
	Queue(spectate::EncodeWatch(g_me, player, true));
	hooklog::Write("spectate: following %s's view (%s)", NameOf(player).c_str(), how);
}

// One frame of following: let go of the operator's object, glide toward the ally's point.
void FollowFrame(BYTE *screen, float dt)
{
	float *pos = CameraPos(screen);
	void *op = Operator(screen);
	if (!pos || !op)
		return;
	if (g_set && spectate::TookOver(pos[7], pos[8], g_set_x, g_set_y)) {
		Stop("the camera was moved");
		return;
	}
	if (!g_have_view) {  // nothing to follow yet (or a private view): where the camera is, to see the player move it
		if (!g_set) {
			g_set = true;
			g_set_x = pos[7];
			g_set_y = pos[8];
		}
		return;
	}
	const int floor = Read(screen, kScreenFloor) ? 1 : 0;
	if (floor != g_view_floor) {
		reinterpret_cast<SwitchFloorFn>(kSwitchFloor)(screen, nullptr);
		g_set = false;
		return;
	}
	if (Read(op, kOperatorObject))  // it keeps an object (a hero) in view: let go
		reinterpret_cast<FollowFn>(kOperatorFollow)(op, nullptr, nullptr, 0);
	float wanted[10];
	memcpy(wanted, pos, sizeof(wanted));
	spectate::Glide(&wanted[7], &wanted[8], spectate::FromTile(g_view_x), spectate::FromTile(g_view_y), dt);
	reinterpret_cast<SetPosFn>(kOperatorSet)(op, nullptr, wanted, 1);
	g_set = true;
	g_set_x = pos[7];
	g_set_y = pos[8];
}

void Frame(DWORD now, float dt)
{
	ReadPlayers();
	if (!InMultiplayer() || g_me == 0) {
		if (g_following)
			Stop("not in a multiplayer game");
		g_watchers.Clear();
		return;
	}
	ShareView(now);
	// this PC's turn begins: stop following (the player plays again)
	const PlayerInfo &me = g_players[g_me];
	const bool my_turn = me.known && me.turn == 0 && !me.pressed;
	if (my_turn && !g_my_turn_before && g_following)
		Stop("this PC's turn began");
	g_my_turn_before = my_turn;
	if (!g_following)
		return;
	if (!Followable(g_following)) {
		Stop("no longer an ally in the game");
		return;
	}
	if (now - g_watch_sent_at >= spectate::kWatchRepeat) {
		Queue(spectate::EncodeWatch(g_me, g_following, true));
		g_watch_sent_at = now;
	}
	BYTE *screen = Screen();
	if (!screen) {
		g_set = false;
		return;
	}
	FollowFrame(screen, dt);
}

void Probe(DWORD now)
{
	static char last_top[96] = "";
	char top[96] = "";
	overlay::TopScreen(top, sizeof(top));
	if (strcmp(top, last_top) != 0) {
		char names[12][96];
		const int n = overlay::ScreenNames(names, 12);
		std::string all;
		for (int i = 0; i < n; ++i)
			all += std::string(i ? ", " : "") + names[i];
		hooklog::Write("spectate probe: the screen on top is now %s (all, bottom first: %s)", top[0] ? top : "none", all.c_str());
		lstrcpynA(last_top, top, sizeof(last_top));
	}
	static DWORD last = 0;
	if (now - last < 2000)
		return;
	last = now;
	BYTE *screen = Screen();
	if (!screen)
		return;
	const float *p = CameraPos(screen);
	void *op = Operator(screen);
	if (p)
		hooklog::Write("spectate probe: floor %lu, camera %.2f %.3f %.3f | %.2f %.2f %.2f, the operator keeps %08lx (state %ld)",
		               Read(screen, kScreenFloor), p[0], p[1], p[2], p[7], p[8], p[9], op ? Read(op, kOperatorObject) : 0,
		               op ? static_cast<long>(Read(op, 0x190)) : -1L);
}

// Robot tests: the camera on a tile (to click what stands there at the screen's centre).
void LookAt(int x, int y)
{
	BYTE *screen = Screen();
	float *pos = screen ? CameraPos(screen) : nullptr;
	void *op = screen ? Operator(screen) : nullptr;
	if (!pos || !op) {
		hooklog::Write("spectate: robot: look at %d, %d: not on the adventure map", x, y);
		return;
	}
	if (Read(op, kOperatorObject))
		reinterpret_cast<FollowFn>(kOperatorFollow)(op, nullptr, nullptr, 0);
	float wanted[10];
	memcpy(wanted, pos, sizeof(wanted));
	wanted[7] = spectate::FromTile(x);
	wanted[8] = spectate::FromTile(y);
	reinterpret_cast<SetPosFn>(kOperatorSet)(op, nullptr, wanted, 1);
	hooklog::Write("spectate: robot: the camera looks at tile %d, %d (%.1f, %.1f; distance %.1f)", x, y, pos[7], pos[8], pos[0]);
}

void RobotCommand(const char *text)
{
	// robot steps "panel|spectate follow 2", "panel|spectate stop", "panel|spectate look 23 66"
	// (the prefix is taken off)
	int player = 0, x = 0, y = 0;
	if (sscanf_s(text, "follow %d", &player) == 1)
		Start(player, "robot");
	else if (sscanf_s(text, "look %d %d", &x, &y) == 2)
		LookAt(x, y);
	else if (strcmp(text, "stop") == 0)
		Stop("robot");
	else
		hooklog::Write("spectate: robot command \"%s\" not known", text);
}

void OnPresent(IDirect3DDevice9 *)
{
	if (!g_ready)
		return;
	LARGE_INTEGER t, f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency(&f);
	float dt = g_last_frame.QuadPart ? static_cast<float>(t.QuadPart - g_last_frame.QuadPart) / static_cast<float>(f.QuadPart) : 0.0f;
	if (dt > 0.25f)
		dt = 0.25f;
	g_last_frame = t;
	const DWORD now = GetTickCount();
	char command[64];
	while (robot::TakePanelCommand(command, sizeof(command), "spectate "))
		RobotCommand(command);
	__try {
		Frame(now, dt);
		if (g_probe)
			Probe(now);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		static bool told = false;
		if (!told) {
			told = true;
			hooklog::Write("spectate: a frame crashed (caught): following stops");
		}
		g_following = 0;
		g_set = false;
	}
}

// --- the game's hooks ------------------------------------------------------------------------------
void __fastcall UpdateHook(void *controller, void *edx, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
	g_controller = controller;
	g_update_next(controller, edx, a, b, c, d, e);
}

// After this PC's own step (its segment report went out): our messages.
void __fastcall MpStepHook(void *self, void *edx)
{
	g_mp_step_next(self, edx);
	if (self != g_transceiver) {
		g_transceiver = self;
		g_watchers.Clear();
		memset(g_watchers_told, 0, sizeof(g_watchers_told));
		g_sent_x = g_sent_y = g_sent_floor = g_sent_screen = -1;
		g_following = 0;
		g_queued = 0;
	}
	g_transceiver_seen = GetTickCount();
	for (int i = 0; i < g_queued; ++i)
		SendValue(static_cast<const BYTE *>(self), g_queue[i]);
	g_queued = 0;
}

void __fastcall CheckHook(void *self, void *edx, int pc, const BYTE *cmd)
{
	if (g_ready && cmd && *reinterpret_cast<const LONG *>(cmd + 0x0c) == -1) {
		const DWORD value = *reinterpret_cast<const DWORD *>(cmd + 0x10);
		if (value >> 24 == spectate::kMark) {
			const spectate::Message m = spectate::Decode(value);
			const DWORD now = GetTickCount();
			if (m.valid && m.kind == spectate::kWatch && m.target == g_me && g_me) {
				if (m.on)
					g_watchers.On(m.player, now);
				else
					g_watchers.Off(m.player);
			}
			else if (m.valid && m.kind == spectate::kView && g_following && m.player == g_following) {
				if (!g_have_view)
					hooklog::Write("spectate: %s's view arrives (%.1f s after the request)", NameOf(m.player).c_str(),
					               (now - g_follow_since) / 1000.0);
				g_have_view = true;
				g_view_x = m.x;
				g_view_y = m.y;
				g_view_floor = m.floor;
			}
			else if (m.valid && m.kind == spectate::kStatus && g_following && m.player == g_following)
				g_their_screen = m.screen;
			return;  // ours: the game never sees it
		}
	}
	g_check_next(self, edx, pc, cmd);
}

// --- the banner ------------------------------------------------------------------------------------
const float kRefH = 768.0f, kScreenW = 1024.0f;
const float kButtonX = 362.0f, kButtonY = 46.0f, kButtonW = 300.0f, kButtonH = 34.0f, kButtonFont = 15.0f, kLineFont = 14.0f;
float g_back_w = 1024.0f, g_back_h = 768.0f;
bool g_shown = false, g_down = false, g_hovered = false;

ImVec2 ScreenAt(float x, float y, float w, float h)
{
	const float k = h / kRefH;
	const float x0 = floorf((w - kScreenW * k) / 2 + 0.5f);
	return ImVec2(x0 + x * k, y * k);
}

void ButtonRect(float w, float h, ImVec2 *p0, ImVec2 *p1)
{
	*p0 = ScreenAt(kButtonX, kButtonY, w, h);
	*p1 = ScreenAt(kButtonX + kButtonW, kButtonY + kButtonH, w, h);
}

bool Wanted()
{
	g_shown = g_following != 0 && Screen() != nullptr;
	if (!g_shown) {
		g_down = g_hovered = false;
		return false;
	}
	ImVec2 p0, p1;
	ButtonRect(g_back_w, g_back_h, &p0, &p1);
	float x, y;
	HWND window = overlay::GameWindow();
	const bool front = window && GetAncestor(GetForegroundWindow(), GA_ROOTOWNER) == GetAncestor(window, GA_ROOTOWNER);
	g_hovered = front && overlay::CursorInBackBuffer(g_back_w, g_back_h, &x, &y) && x >= p0.x && x < p1.x && y >= p0.y && y < p1.y;
	const bool down = front && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	if (down && !g_down && g_hovered)
		g_down = true;
	else if (!down && g_down) {
		g_down = false;
		if (g_hovered)
			Stop("its button");
	}
	return g_following != 0;
}

void Prepare(const overlay::Frame &frame)
{
	ui::LoadPictures(frame.device);
	overlay::WantFont(floorf(kButtonFont * frame.height / kRefH + 0.5f));
	overlay::WantFont(floorf(kLineFont * frame.height / kRefH + 0.5f));
}

// The line under the button: what the ally does, or why nothing comes.
std::string StatusLine(DWORD now)
{
	const std::string name = NameOf(g_following);
	if (!g_have_view && now - g_follow_since > 3000)
		return texts::Fill(texts::Get("spectate.no_view"), {{"name", name}});
	switch (g_their_screen) {
	case spectate::kInBattle:
		return texts::Fill(texts::Get("spectate.in_battle"), {{"name", name}});
	case spectate::kInTown:
		return texts::Fill(texts::Get("spectate.in_town"), {{"name", name}});
	case spectate::kInHeroScreen:
		return texts::Fill(texts::Get("spectate.in_hero_screen"), {{"name", name}});
	case spectate::kElsewhere:
		return texts::Fill(texts::Get("spectate.elsewhere"), {{"name", name}});
	default:
		return texts::Get("spectate.hint");
	}
}

void Draw(const overlay::Frame &frame)
{
	g_back_w = frame.width;
	g_back_h = frame.height;
	if (!g_shown)
		return;
	const float k = frame.height / kRefH;
	ImVec2 p0, p1;
	ButtonRect(frame.width, frame.height, &p0, &p1);
	overlay::BlockClicksIn(kName, p0.x, p0.y, p1.x, p1.y, frame.width, frame.height);
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	const bool pressed = g_down && g_hovered;
	if (ImTextureID picture = ui::Tex(pressed ? ui::kButtonClick : ui::kButtonNormal))
		ui::NineSlice(draw, picture, p0, p1, ImVec2(122.0f, 34.0f), 12.0f, 12.0f * k);
	else
		draw->AddRectFilled(p0, p1, pressed ? IM_COL32(70, 52, 30, 255) : IM_COL32(96, 72, 40, 255), 4.0f * k);
	if (g_hovered && !pressed) {
		if (ImTextureID highlight = ui::Tex(ui::kButtonHighlight))
			ui::NineSlice(draw, highlight, p0, p1, ImVec2(122.0f, 34.0f), 12.0f, 12.0f * k);
		else
			draw->AddRect(p0, p1, ui::kGold, 4.0f * k, 0, 1.5f * k);
	}
	const float px = floorf(kButtonFont * k + 0.5f);
	ImFont *font = overlay::Font(px);
	const std::string label = texts::Fill(texts::Get("spectate.stop_button"), {{"name", NameOf(g_following)}});
	const std::string text = ui::Fit(font, px, label.c_str(), p1.x - p0.x - 16.0f * k);
	const ImVec2 size = font->CalcTextSizeA(px, 10000.0f, 0.0f, text.c_str());
	ui::ShadowText(draw, font, px, ImVec2(floorf((p0.x + p1.x - size.x) / 2), floorf((p0.y + p1.y - size.y) / 2 + (pressed ? k : 0.0f))),
	               IM_COL32(255, 236, 190, 255), text.c_str());
	const float lpx = floorf(kLineFont * k + 0.5f);
	ImFont *lfont = overlay::Font(lpx);
	const std::string line = StatusLine(GetTickCount());
	const std::string fitted = ui::Fit(lfont, lpx, line.c_str(), 600.0f * k);
	const ImVec2 lsize = lfont->CalcTextSizeA(lpx, 10000.0f, 0.0f, fitted.c_str());
	ui::ShadowText(draw, lfont, lpx, ImVec2(floorf((p0.x + p1.x - lsize.x) / 2), floorf(p1.y + 6.0f * k)), ui::kParchment, fitted.c_str());
}

bool CodeAsExpected()
{
	for (const Code &c : kCode)
		if (memcmp(reinterpret_cast<const void *>(c.address), c.bytes, c.size) != 0) {
			hooklog::Write("spectate: the game's code at %08lx is not as expected: off", c.address);
			return false;
		}
	for (const Slot &s : kSlots)
		if (*reinterpret_cast<const DWORD *>(s.vtable + s.offset) != s.function) {
			hooklog::Write("spectate: the table %08lx slot %lx is not as expected: off", s.vtable, s.offset);
			return false;
		}
	return true;
}

void ReadSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	g_share = GetPrivateProfileIntW(L"coop", L"share_view", 1, path) != 0;
	g_probe = static_cast<int>(GetPrivateProfileIntW(L"debug", L"spectate_probe", 0, path));
}

}  // namespace

// --- for the player list -----------------------------------------------------------------------
int SpectateFollowing()
{
	return g_following;
}

bool SpectateCanFollow(int player)
{
	return player != g_me && Followable(player);
}

void SpectateToggle(int player, const char *how)
{
	if (g_following == player)
		Stop(how);
	else
		Start(player, how);
}

bool SpectateFollowsMe(int player)
{
	return g_ready && InMultiplayer() && g_share && g_watchers.Active(player, GetTickCount()) && Followable(player);
}

void SpectateNoteName(int player, const char *name)
{
	if (player >= 1 && player <= 8 && name)
		lstrcpynA(g_names[player], name, sizeof(g_names[player]));
}

void ReloadSpectateSettings()
{
	ReadSettings();
	hooklog::Write("spectate: from the settings menu: allies may follow this PC's view: %s", g_share ? "yes" : "no");
}

void ApplySpectateFromSettings()
{
	ReadSettings();
	if (!CodeAsExpected())
		return;
	g_update_next = reinterpret_cast<UpdateFn>(hookpatch::RedirectCall(kUpdateCall, kUpdate, &UpdateHook));
	g_mp_step_next = reinterpret_cast<StepFn>(hookpatch::PatchPointer(kMpStepCell, kMpStep, &MpStepHook));
	g_check_next = reinterpret_cast<CheckFn>(hookpatch::RedirectCall(kCheckCall, kCheckHandler, &CheckHook));
	if (!g_update_next || !g_mp_step_next || !g_check_next) {
		hooklog::Write("spectate: part of the game's code is not as expected (frame %d, step %d, messages %d): off "
		               "(the hooks that went in only pass on)",
		               g_update_next != nullptr, g_mp_step_next != nullptr, g_check_next != nullptr);
		return;
	}
	g_ready = true;
	d3dhook::AddPresentCallback(&OnPresent, 900);
	const overlay::Client client = {kName, &Wanted, &Prepare, &Draw};
	if (!overlay::AddClient(client))
		hooklog::Write("spectate: no overlay in this game: no button to stop following (a click on the name still stops it)");
	else
		overlay::EnableModalInput();  // (its click zones: the game does not get the button's clicks)
	hooklog::Write("spectate: ready (a click on an ally's name in the player list follows their view; allies may follow "
	               "this PC's view: %s)%s",
	               g_share ? "yes" : "no, share_view=0", g_probe ? "; research probe on" : "");
}

}  // namespace mods

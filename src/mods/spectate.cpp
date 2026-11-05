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
// 4. On screen (an overlay client), beside the followed ally's row in the player list, after
//    its eye: a short line when the ally is not on the map ("in a town"), or, while the ally
//    fights, a small button "Watch the battle". Nothing in the middle of the map: a click on
//    the name or the eye starts and stops following (the owner's choice, 2026-11-05).
//
// Research probe ([debug] spectate_probe=1, robot runs): the camera's position, the floor
// and the operator's object, every 2 s.
#include "spectate.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "player_list.h"
#include "player_list_internal.h"
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
	{0x005a5d90, {0x81, 0xec, 0xfc, 0x02, 0x00, 0x00, 0x53, 0x55, 0x56, 0x57}, 10},  // the adventure screen's notice handler
	{0x00ac3590, {0x6a, 0x10, 0xe8, 0x39, 0x5a, 0xdb, 0xff, 0x33, 0xc9, 0x83}, 10},  // a new "close the top screen" command
	{0x00838c10, {0x56, 0x8b, 0xf1, 0x85, 0xf6, 0x74, 0x04, 0x83, 0x46, 0x04}, 10},  // queue a UI command for the next frame
	{0x007c9810, {0x55, 0x8b, 0x6c, 0x24, 0x08, 0x85, 0xed, 0x56, 0x8b, 0xf1}, 10},  // the transceiver's send
	{0x004fbe80, {0x8b, 0x81, 0x08, 0x06, 0x00, 0x00, 0xc3}, 7},              // IPlayer slot 0x1bc: its inbox at +0x608
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
	{kPlayerVftable, 0x1bc, 0x004fbe80},
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
	const void *object;  // its IPlayer
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
bool g_heard = false;  // a view or a status came from the followed ally (they share)
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
				info.object = player;
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

bool WatchingBattle();

int ScreenCode()
{
	char names[12][96];
	const int n = overlay::ScreenNames(names, 12);
	if (n <= 0)
		return spectate::kElsewhere;
	const char *top = names[n - 1];
	if (strstr(top, "CAdventure"))
		return spectate::kOnMap;
	// In a battle: its screen anywhere under the top one (a spell book or the battle's options
	// over it are still the battle), but not its results box, and not a battle this PC only
	// watches (its own inbox's old battle is not that one).
	if (strstr(top, "CombatResults") || WatchingBattle())
		return spectate::kElsewhere;
	for (int i = 0; i < n; ++i)
		if (strstr(names[i], "CombatArena"))
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
	bool any = false, fresh = false;
	for (int p = 1; p <= 8; ++p) {
		const bool active = g_watchers.Active(p, now);
		if (active != g_watchers_told[p]) {
			g_watchers_told[p] = active;
			fresh = fresh || active;  // a new follower: what this PC shows and where it looks, at once
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
	if (fresh || screen_code != g_sent_screen || now - g_sent_status_at >= 2000) {
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
	if (fresh || (moved ? now - g_sent_view_at >= 200 : now - g_sent_view_at >= 2000)) {
		Queue(spectate::EncodeView(g_me, floor, x, y));
		g_sent_x = x;
		g_sent_y = y;
		g_sent_floor = floor;
		g_sent_view_at = now;
	}
}

// --- as the follower -------------------------------------------------------------------------------
void StopWatching(const char *why);

void Stop(const char *why)
{
	if (!g_following)
		return;
	StopWatching(why);
	hooklog::Write("spectate: stopped following %s (%s)", NameOf(g_following).c_str(), why);
	if (g_me)
		Queue(spectate::EncodeWatch(g_me, g_following, false));
	g_following = 0;
	g_have_view = false;
	g_heard = false;
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
	g_heard = false;
	g_their_screen = spectate::kOnMap;
	g_set = false;
	Queue(spectate::EncodeWatch(g_me, player, true));
	hooklog::Write("spectate: following %s's view (%s)", NameOf(player).c_str(), how);
}

int NoticeType(const void *notice);
const char *RttiName(const void *object);

// For the log.
const char *ScreenWords(int screen)
{
	switch (screen) {
	case spectate::kOnMap:
		return "the map";
	case spectate::kInTown:
		return "a town";
	case spectate::kInBattle:
		return "a battle";
	case spectate::kInHeroScreen:
		return "the hero screen";
	default:
		return "something else (a menu, a box)";
	}
}

// --- watching the followed ally's battle ([coop] watch_battles) ----------------------------------
// At a battle's start every PC's world posts "open the battle screen" (NWorld::CCreateCombatScreen,
// notice type 0x2a) to each fighting human's inbox (IPlayer +0x608, slot 0x1bc; 0x008545b0 and
// 0x00a7d0e0), but each PC's adventure screen reads only its own player's (0x0059d5f0). While
// this PC follows an ally, the ally's notice goes to this PC's adventure screen too, through the
// same handler (0x005a5d90) when the player clicks "Watch the battle" (a second button while the
// followed ally fights; also mid-battle): the game's own battle screen opens on the ally's
// battle, which every PC runs anyway (its actions are world commands). While it is open:
//   - the game gets no mouse or keyboard input (our modal panel: the button to stop watching);
//   - every command this PC tries to send is dropped: that battle screen acts as if it were the
//     ally (its start-up settings, auto-combat moves...), and the world must stay the same on
//     every PC (robot, 2026-11-05: 1373 world checks equal while about 40 were dropped).
// It ends with the button, Esc, the ally's results box closed by the button, this PC's own
// battle starting, or the end of following. Closing queues the game's own "close the top
// screen" UI command once per battle screen on top (the results box, the battle): the game
// runs them at the start of the next frame; the ally's battle goes on.
const DWORD kScreenEvent = 0x005a5d90;     // __thiscall(screen, notice, player)
const DWORD kSendCell = 0x00e43f1c;        // CMultiPlayerTransceiver slot 0: send a command from this PC
const DWORD kSend = 0x007c9810;            // __thiscall(transceiver, command), ret 4
const DWORD kNewCloseCommand = 0x00ac3590; // (): the game's "close the top screen" UI command (a new one)
const DWORD kQueueUiCommand = 0x00838c10;  // __fastcall(command): run it at the start of the next frame
const DWORD kInbox = 0x608;                // IPlayer: its notices (a list at +0xc: [0] next, [2] time, [3] notice)
const int kBattleNoticeType = 0x2a;        // NWorld::CCreateCombatScreen
typedef void(__fastcall *SendFn)(void *transceiver, void *edx, void *command);
typedef DWORD(__fastcall *HandlerFn)(void *screen, void *edx, const void *notice, const void *player);
typedef void *(__cdecl *NewCommandFn)();
typedef void(__fastcall *QueueFn)(void *command, void *edx);
SendFn g_send_next = nullptr;
bool g_watch_battles = true;            // [coop] watch_battles
const void *g_battle_notice = nullptr;  // the ally's last battle notice (not opened twice)
bool g_watching_battle = false;
DWORD g_watch_since = 0;
char g_watch_top[96] = "";              // the battle screen our modal panel was opened over
LONG g_dropped = 0;

void __fastcall SendHook(void *transceiver, void *edx, void *command)
{
	if (g_watching_battle) {
		if (InterlockedIncrement(&g_dropped) <= 3)
			hooklog::Write("spectate: watching a battle: this PC's command %s dropped (and every other while it watches)",
			               command ? RttiName(command) : "-");
		return;
	}
	g_send_next(transceiver, edx, command);
}

// The newest "open the battle screen" notice in the player's inbox, or nullptr.
const void *BattleNoticeUnsafe(const void *player)
{
	const void *inbox = reinterpret_cast<const void *>(Read(player, kInbox));
	if (!inbox)
		return nullptr;
	const DWORD *head = reinterpret_cast<const DWORD *>(Read(inbox, 0xc));
	const void *found = nullptr;
	int count = 0;
	for (const DWORD *node = reinterpret_cast<const DWORD *>(head[0]); node && node != head && count < 5000;
	     node = reinterpret_cast<const DWORD *>(node[0]), ++count) {
		const void *notice = reinterpret_cast<const void *>(node[3]);
		if (notice && NoticeType(notice) == kBattleNoticeType)
			found = notice;
	}
	return found;
}

const void *BattleNotice(const void *player)
{
	if (!player)
		return nullptr;
	__try {
		return BattleNoticeUnsafe(player);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool BattleOnTop(char *top, int size)
{
	top[0] = 0;
	overlay::TopScreen(top, size);
	return strstr(top, "Combat") != nullptr;
}

// Close the top screen the game's own way: its "close the top screen" UI command, run at the
// start of the next frame (0x00838ac0), as the results box's OK does (0x0057ff30). (Taking it off
// the stack at once, from here, left the battle screen's message handlers alive: a crash.)
bool QueueClose()
{
	__try {
		void *command = reinterpret_cast<NewCommandFn>(kNewCloseCommand)();
		if (!command)
			return false;
		reinterpret_cast<QueueFn>(kQueueUiCommand)(command, nullptr);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::Write("spectate: closing a battle screen crashed (caught)");
		return false;
	}
}

bool g_closing = false;  // the close of the battle screens is queued: watching ends once they are gone
DWORD g_closing_since = 0;

bool WatchingBattle()
{
	return g_watching_battle;
}

void StopWatching(const char *why)
{
	if (!g_watching_battle || g_closing)
		return;
	// the screens on top that belong to the battle (the results box over the battle screen)
	char names[12][96];
	const int n = overlay::ScreenNames(names, 12);
	int battle_screens = 0;
	for (int i = n - 1; i >= 0 && strstr(names[i], "Combat"); --i)
		++battle_screens;
	int queued = 0;
	for (int i = 0; i < battle_screens; ++i)
		queued += QueueClose() ? 1 : 0;
	g_closing = true;
	g_closing_since = GetTickCount();
	// The lock and the filter stay until the battle screens are gone (WatchFrame).
	hooklog::Write("spectate: stop watching %s's battle (%s): %d battle screen(s), %d close(s) queued", NameOf(g_following).c_str(),
	               why, battle_screens, queued);
}

// The battle screens are gone: the end of watching.
void EndWatching(const char *top)
{
	overlay::CloseModal(kName);
	g_watching_battle = false;
	g_closing = false;
	g_watch_top[0] = 0;
	hooklog::Write("spectate: stopped watching %s's battle (now %s; %ld command(s) of this PC dropped while watching)",
	               NameOf(g_following).c_str(), top[0] ? top : "none", g_dropped);
}

void StartWatching(BYTE *screen, const void *notice)
{
	const PlayerInfo &ally = g_players[g_following];
	g_battle_notice = notice;
	g_watching_battle = true;  // (before: the battle screen sends its settings at once)
	g_closing = false;
	g_watch_since = GetTickCount();
	g_watch_top[0] = 0;
	g_dropped = 0;
	DWORD result = 0;
	__try {
		// (built for the ally, as on the ally's PC: built for this PC's own player, who is not in
		// the battle, the battle screen crashes at once; robot, 2026-11-05)
		result = reinterpret_cast<HandlerFn>(kScreenEvent)(screen, nullptr, notice, ally.object);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::Write("spectate: opening the ally's battle crashed (caught)");
		g_watching_battle = false;
		return;
	}
	hooklog::Write("spectate: the followed ally's battle starts: this PC watches it (the game's battle screen, no input, "
	               "no command sent; %lu)",
	               result);
}

// Every frame while watching: the input lock, and the ways it ends by itself.
void WatchFrame(DWORD now)
{
	char top[96];
	if (!BattleOnTop(top, sizeof(top))) {
		if (g_closing || now - g_watch_since > 3000)  // (the battle screen comes a frame or two after the notice)
			EndWatching(top);
		return;
	}
	if (g_closing) {
		if (now - g_closing_since > 5000) {
			hooklog::Write("spectate: the battle screen %s did not close: this PC keeps watching", top);
			g_closing = false;  // (the button can ask again)
		}
		return;
	}
	if (BattleNotice(g_players[g_me].object)) {
		StopWatching("this PC's own battle starts");
		return;
	}
	if (!overlay::ModalOpen(kName)) {
		if (g_watch_top[0] && strcmp(top, g_watch_top) == 0) {  // closed on the same screen: Esc
			StopWatching("Esc");
			return;
		}
		if (overlay::OpenModal(kName))  // (again when the results box comes on top)
			lstrcpynA(g_watch_top, top, sizeof(g_watch_top));
	}
}

// The ally's battle notice when this PC may watch it now: the ally's own screen is a battle
// (their status message), this PC is on its map, its own battle does not wait, watch_battles
// is on. (The ally's inbox keeps old notices: the newest is the battle going on only while the
// ally's screen is that battle.) No view of the ally's map is needed: an ally followed while
// already in a battle sends no view until it ends (the owner's game, 2026-11-05).
const void *WatchableNotice()
{
	if (!g_watch_battles || !g_send_next || g_watching_battle || !g_following || !g_heard ||
	    g_their_screen != spectate::kInBattle)
		return nullptr;
	if (!Screen() || BattleNotice(g_players[g_me].object))
		return nullptr;
	return BattleNotice(g_players[g_following].object);
}

void WatchNow(const char *how)
{
	const void *notice = WatchableNotice();
	BYTE *screen = Screen();
	if (!notice || !screen) {
		hooklog::Write("spectate: nothing to watch now (%s)", how);
		return;
	}
	hooklog::Write("spectate: watch %s's battle (%s)", NameOf(g_following).c_str(), how);
	StartWatching(screen, notice);
}

// The banner's clicks come during the drawing: they are handled in the next frame's step.
bool g_request_watch = false, g_request_stop_watching = false;
const char *g_request_how = "";

// One frame of following: let go of the operator's object, glide toward the ally's point.
void FollowFrame(BYTE *screen, float dt)
{
	float *pos = CameraPos(screen);
	void *op = Operator(screen);
	if (!pos || !op)
		return;
	// The player moved the camera (keys, the screen's edge, the minimap): it is away from where
	// we put it, and the operator keeps no object in view. (When the game itself shows a hero
	// walking, it gives the operator that hero: not the player's doing.)
	const bool game_moved_it = Read(op, kOperatorObject) != 0;
	if (g_set && !game_moved_it && spectate::TookOver(pos[7], pos[8], g_set_x, g_set_y)) {
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
	if (g_watching_battle) {
		if (g_request_stop_watching) {
			g_request_stop_watching = false;
			StopWatching(g_request_how);
		}
		WatchFrame(now);
		return;
	}
	BYTE *screen = Screen();
	if (!screen) {
		g_set = false;
		return;
	}
	if (g_request_watch) {
		g_request_watch = false;
		WatchNow(g_request_how);
		if (g_watching_battle)
			return;
	}
	FollowFrame(screen, dt);
}

// Research: the notices waiting in other humans' inboxes (IPlayer +0x608: a list at +0xc,
// each node: [0] next, [2] time, [3] the notice), each new one once.
const void *g_seen_notices[512];
int g_seen_next = 0;

int NoticeType(const void *notice);
const char *RttiName(const void *object);

void ProbeInboxesUnsafe()
{
	for (int p = 1; p <= 8; ++p) {
		const PlayerInfo &info = g_players[p];
		if (!info.known || !info.human || p == g_me || !info.object)
			continue;
		const void *inbox = reinterpret_cast<const void *>(Read(info.object, 0x608));
		if (!inbox)
			continue;
		const DWORD *head = reinterpret_cast<const DWORD *>(Read(inbox, 0xc));
		int count = 0;
		for (const DWORD *node = reinterpret_cast<const DWORD *>(head[0]); node && node != head && count < 200;
		     node = reinterpret_cast<const DWORD *>(node[0]), ++count) {
			const void *notice = reinterpret_cast<const void *>(node[3]);
			bool seen = false;
			for (int i = 0; i < 512 && !seen; ++i)
				seen = g_seen_notices[i] == notice;
			if (seen)
				continue;
			g_seen_notices[g_seen_next] = notice;
			g_seen_next = (g_seen_next + 1) % 512;
			if (NoticeType(notice) == 0x39)  // (the map changed: many)
				continue;
			hooklog::Write("spectate probe: player %d's inbox (not this PC's): notice type 0x%x %s at time %lu (%d waiting)", p,
			               NoticeType(notice), RttiName(notice), node[2], count + 1);
		}
	}
}

void ProbeInboxes()
{
	__try {
		ProbeInboxesUnsafe();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		static bool told = false;
		if (!told) {
			told = true;
			hooklog::Write("spectate probe: reading an inbox crashed (caught)");
		}
	}
}

void Probe(DWORD now)
{
	ProbeInboxes();
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
	else if (strcmp(text, "stop_watching") == 0)
		StopWatching("robot");
	else if (strcmp(text, "watch") == 0)
		WatchNow("robot");
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

// --- research (spectate_probe): the events the adventure screen gets (0x005a5d90 via 0x0059db0d) ---
typedef DWORD(__fastcall *ScreenEventFn)(void *screen, void *edx, const DWORD *event, void *third);
ScreenEventFn g_screen_event_next = nullptr;
LONG g_events_logged = 0;

const char *RttiName(const void *object)
{
	__try {
		const DWORD vtable = Read(object, 0);
		if (vtable < 0x00401000 || vtable > 0x01200000)
			return "?";
		const DWORD locator = Read(reinterpret_cast<const void *>(vtable), static_cast<DWORD>(-4));
		if (locator < 0x00401000 || locator > 0x01200000)
			return "?";
		const DWORD type = Read(reinterpret_cast<const void *>(locator), 12);
		if (type < 0x00401000 || type > 0x01200000)
			return "?";
		return reinterpret_cast<const char *>(type + 8);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return "!";
	}
}

int EventType(const DWORD *event)
{
	__try {
		typedef int(__fastcall * TypeFn)(const DWORD *event, void *edx);
		return reinterpret_cast<TypeFn>(Read(reinterpret_cast<const void *>(event[0]), 0xc))(event, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return -1;
	}
}

int NoticeType(const void *notice)
{
	return notice ? EventType(static_cast<const DWORD *>(notice)) : -1;
}

DWORD __fastcall ScreenEventHook(void *screen, void *edx, const DWORD *event, void *third)
{
	const int type = event ? EventType(event) : -1;
	const DWORD result = g_screen_event_next(screen, edx, event, third);
	if (type != 0x10 && type != 0x11 && InterlockedIncrement(&g_events_logged) <= 400) {
		const void *a = event ? reinterpret_cast<const void *>(event[5]) : nullptr;
		const void *b = event ? reinterpret_cast<const void *>(event[6]) : nullptr;
		hooklog::Write("spectate probe: event 0x%x (%s) [5]=%p %s [6]=%p %s third=%p %s -> %lu", type,
		               event ? RttiName(event) : "-", a, a ? RttiName(a) : "", b, b ? RttiName(b) : "", third,
		               third ? RttiName(third) : "", result);
	}
	return result;
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
				g_heard = true;
				g_view_x = m.x;
				g_view_y = m.y;
				g_view_floor = m.floor;
			}
			else if (m.valid && m.kind == spectate::kStatus && g_following && m.player == g_following) {
				if (!g_heard || m.screen != g_their_screen)
					hooklog::Write("spectate: %s's screen: %s", NameOf(m.player).c_str(), ScreenWords(m.screen));
				g_heard = true;
				g_their_screen = m.screen;
			}
			return;  // ours: the game never sees it
		}
	}
	g_check_next(self, edx, pc, cmd);
}

// --- on screen -------------------------------------------------------------------------------------
// On the map, beside the followed ally's row in the player list, just after its eye (nothing
// in the middle of the map: the name or the eye starts and stops following; the owner's
// choice, 2026-11-05): a short line when the ally is not on the map ("in a town"), or, while
// the ally fights, a small button "Watch the battle" in the row's height. Without the list
// (F9), that button alone over the game's turn banner. In the battle: one small button at the
// bottom right ("Stop watching", then "Back to the map") with a caption over it ("Watching
// Pierre"), the owner's choice: discreet.
// Places in the game's 1024x768 layout, scaled with the screen's height.
const float kRefH = 768.0f, kScreenW = 1024.0f;
const float kRowGap = 8.0f, kRowPad = 14.0f, kRowButtonMinW = 110.0f;
const float kFallbackW = 240.0f, kFallbackY = 497.0f, kFallbackH = 34.0f, kFallbackFont = 15.0f;  // (the turn banner: from about 539)
float g_back_w = 1024.0f, g_back_h = 768.0f;
bool g_shown = false;  // something to draw this frame
bool g_offer = false;  // "Watch the battle" shows
bool g_row = false;    // the followed ally's row is in the player list: drawn beside it
float g_row_right = 0, g_row_top = 0, g_row_bottom = 0;
float g_offer_w = 0;   // the button's width on the last frame (from its words)
struct Button {
	bool down, hovered;
};
Button g_main = {}, g_watch = {};

ImVec2 ScreenAt(float x, float y, float w, float h)
{
	const float k = h / kRefH;
	const float x0 = floorf((w - kScreenW * k) / 2 + 0.5f);
	return ImVec2(x0 + x * k, y * k);
}

// In a battle: one small button at the bottom right, just over the creatures' bar and beside the
// game's own buttons there (anchored to the right and bottom edges, scaled with the height).
const float kBattleRight = 80.0f, kBattleBottom = 92.0f, kBattleW = 172.0f, kBattleH = 28.0f, kBattleFont = 13.0f,
            kCaptionFont = 12.0f;

// The row's words and button: the player list's name size.
float RowFont(float h)
{
	float regular = 0, bold = 0;
	plist::FontSizes(h, &regular, &bold);
	return regular;
}

// A button's place: which = 0 the battle's button (while watching), 1 "Watch the battle".
void ButtonRect(int which, float w, float h, ImVec2 *p0, ImVec2 *p1)
{
	const float k = h / kRefH;
	if (which == 0) {
		*p1 = ImVec2(floorf(w - kBattleRight * k), floorf(h - kBattleBottom * k));
		*p0 = ImVec2(floorf(p1->x - kBattleW * k), floorf(p1->y - kBattleH * k));
		return;
	}
	if (g_row) {
		const float bw = g_offer_w > kRowButtonMinW * k ? g_offer_w : kRowButtonMinW * k;
		*p0 = ImVec2(floorf(g_row_right + kRowGap * k), floorf(g_row_top));
		*p1 = ImVec2(floorf(p0->x + bw), floorf(g_row_bottom));
		return;
	}
	*p0 = ScreenAt((kScreenW - kFallbackW) / 2, kFallbackY, w, h);
	*p1 = ScreenAt((kScreenW + kFallbackW) / 2, kFallbackY + kFallbackH, w, h);
}

// A click on a button: true once, when the mouse button is let go over it.
bool Clicked(Button *b, int which, bool front)
{
	ImVec2 p0, p1;
	ButtonRect(which, g_back_w, g_back_h, &p0, &p1);
	float x = 0, cy = 0;
	b->hovered = front && overlay::CursorInBackBuffer(g_back_w, g_back_h, &x, &cy) && x >= p0.x && x < p1.x && cy >= p0.y && cy < p1.y;
	const bool down = front && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	if (down && !b->down && b->hovered)
		b->down = true;
	else if (!down && b->down) {
		b->down = false;
		return b->hovered;
	}
	return false;
}

// The line beside the row: what the ally has on screen when it is not the map, or why
// nothing comes ("" = nothing to say).
std::string RowLine(DWORD now)
{
	if (!g_heard)
		return now - g_follow_since > 3000 ? std::string(texts::Get("spectate.row_no_view")) : std::string();
	switch (g_their_screen) {
	case spectate::kInBattle:
		return texts::Get("spectate.row_in_battle");
	case spectate::kInTown:
		return texts::Get("spectate.row_in_town");
	case spectate::kInHeroScreen:
		return texts::Get("spectate.row_in_hero_screen");
	case spectate::kElsewhere:
		return texts::Get("spectate.row_elsewhere");
	default:
		return std::string();
	}
}

bool Wanted()
{
	const bool on_map = g_following != 0 && !g_watching_battle && Screen() != nullptr;
	g_row = on_map && PlayerListRowPlace(g_following, &g_row_right, &g_row_top, &g_row_bottom);
	g_offer = on_map && WatchableNotice() != nullptr;
	g_shown = g_watching_battle || g_offer || (g_row && !RowLine(GetTickCount()).empty());
	HWND window = overlay::GameWindow();
	const bool front = window && GetAncestor(GetForegroundWindow(), GA_ROOTOWNER) == GetAncestor(window, GA_ROOTOWNER);
	if (g_watching_battle) {
		if (Clicked(&g_main, 0, front)) {
			g_request_how = "its button";
			g_request_stop_watching = true;
		}
	}
	else
		g_main = Button();
	if (g_offer) {
		if (Clicked(&g_watch, 1, front)) {
			g_request_how = "its button";
			g_request_watch = true;
		}
	}
	else
		g_watch = Button();
	return g_shown;
}

void Prepare(const overlay::Frame &frame)
{
	ui::LoadPictures(frame.device);
	overlay::WantFont(RowFont(frame.height));
	overlay::WantFont(floorf(kFallbackFont * frame.height / kRefH + 0.5f));
	overlay::WantFont(floorf(kBattleFont * frame.height / kRefH + 0.5f));
	overlay::WantFont(floorf(kCaptionFont * frame.height / kRefH + 0.5f));
}

// While watching a battle: is it over (the results box on top)?
bool ResultsOnTop()
{
	char top[96] = "";
	overlay::TopScreen(top, sizeof(top));
	return strstr(top, "CombatResults") != nullptr;
}

// The battle's button's words.
std::string ButtonLabel()
{
	return ResultsOnTop() ? std::string(texts::Get("spectate.back_to_map")) : std::string(texts::Get("spectate.stop_watching"));
}

// The caption over it.
std::string Caption()
{
	return texts::Fill(texts::Get(ResultsOnTop() ? "spectate.battle_over" : "spectate.watching"), {{"name", NameOf(g_following)}});
}

float ButtonFont(int which, float h)
{
	if (which == 0)
		return floorf(kBattleFont * h / kRefH + 0.5f);
	return g_row ? RowFont(h) : floorf(kFallbackFont * h / kRefH + 0.5f);
}

void DrawButton(ImDrawList *draw, const Button &b, int which, const std::string &label, float k)
{
	ImVec2 p0, p1;
	ButtonRect(which, g_back_w, g_back_h, &p0, &p1);
	const bool pressed = b.down && b.hovered;
	// the game's 122x34 button, its ends kept (in the row's height, they shrink with it)
	const float border = (p1.y - p0.y) < 34.0f * k ? 12.0f * (p1.y - p0.y) / 34.0f : 12.0f * k;
	if (ImTextureID picture = ui::Tex(pressed ? ui::kButtonClick : ui::kButtonNormal))
		ui::NineSlice(draw, picture, p0, p1, ImVec2(122.0f, 34.0f), 12.0f, border);
	else
		draw->AddRectFilled(p0, p1, pressed ? IM_COL32(70, 52, 30, 255) : IM_COL32(96, 72, 40, 255), 4.0f * k);
	if (b.hovered && !pressed) {
		if (ImTextureID highlight = ui::Tex(ui::kButtonHighlight))
			ui::NineSlice(draw, highlight, p0, p1, ImVec2(122.0f, 34.0f), 12.0f, border);
		else
			draw->AddRect(p0, p1, ui::kGold, 4.0f * k, 0, 1.5f * k);
	}
	const float px = ButtonFont(which, g_back_h);
	ImFont *font = overlay::Font(px);
	const std::string text = ui::Fit(font, px, label.c_str(), p1.x - p0.x - 12.0f * k);
	const ImVec2 size = font->CalcTextSizeA(px, 10000.0f, 0.0f, text.c_str());
	ui::ShadowText(draw, font, px, ImVec2(floorf((p0.x + p1.x - size.x) / 2), floorf((p0.y + p1.y - size.y) / 2 + (pressed ? k : 0.0f))),
	               IM_COL32(255, 236, 190, 255), text.c_str());
}

// While watching, the battle screen's buttons at the top right (auto combat, the battle's options,
// the white flag: retreat or surrender) look greyed out, as the game greys the ones below when it
// is not the player's turn: they do nothing for a watcher (the game gets no input then). Their
// places in the game's layout: anchored to the right edge, scaled with the screen's height
// (measured on the robot's 1008x729 screenshots, 2026-11-05).
void GreyBattleButtons(ImDrawList *draw, float width, float k)
{
	struct Disc {
		float from_right, y, r;
	};
	static const Disc kDiscs[] = {{22.1f, 61.1f, 20.0f}, {35.8f, 109.0f, 31.5f}, {22.1f, 171.7f, 20.0f}};
	for (const Disc &d : kDiscs) {
		const ImVec2 c(width - d.from_right * k, d.y * k);
		draw->AddCircleFilled(c, d.r * k, IM_COL32(0, 0, 0, 115), 40);
		draw->AddCircleFilled(c, d.r * k, IM_COL32(105, 105, 105, 115), 40);
	}
}

void Draw(const overlay::Frame &frame)
{
	g_back_w = frame.width;
	g_back_h = frame.height;
	if (!g_shown)
		return;
	const float k = frame.height / kRefH;
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	if (g_watching_battle) {
		ImVec2 p0, p1;
		ButtonRect(0, frame.width, frame.height, &p0, &p1);
		overlay::BlockClicksIn(kName, p0.x, p0.y, p1.x, p1.y, frame.width, frame.height);
		if (!ResultsOnTop())
			GreyBattleButtons(draw, frame.width, k);
		DrawButton(draw, g_main, 0, ButtonLabel(), k);
		// a small caption over the small button, centred on it
		const float cpx = floorf(kCaptionFont * k + 0.5f);
		ImFont *cfont = overlay::Font(cpx);
		const std::string caption = ui::Fit(cfont, cpx, Caption().c_str(), 260.0f * k);
		const ImVec2 csize = cfont->CalcTextSizeA(cpx, 10000.0f, 0.0f, caption.c_str());
		ui::ShadowText(draw, cfont, cpx, ImVec2(floorf((p0.x + p1.x - csize.x) / 2), floorf(p0.y - 4.0f * k - csize.y)), ui::kParchment,
		               caption.c_str());
		return;
	}
	if (g_offer) {
		const char *label = texts::Get("spectate.watch_button");
		const float px = ButtonFont(1, frame.height);
		g_offer_w = ceilf(overlay::Font(px)->CalcTextSizeA(px, 10000.0f, 0.0f, label).x + 2 * kRowPad * k);
		ImVec2 w0, w1;
		ButtonRect(1, frame.width, frame.height, &w0, &w1);
		overlay::BlockClicksIn(kName, w0.x, w0.y, w1.x, w1.y, frame.width, frame.height);
		DrawButton(draw, g_watch, 1, label, k);
		return;
	}
	const std::string line = RowLine(GetTickCount());
	if (!g_row || line.empty())
		return;
	const float px = RowFont(frame.height);
	ImFont *font = overlay::Font(px);
	const ImVec2 size = font->CalcTextSizeA(px, 10000.0f, 0.0f, line.c_str());
	ui::ShadowText(draw, font, px, ImVec2(floorf(g_row_right + kRowGap * k), floorf((g_row_top + g_row_bottom - size.y) / 2)),
	               ui::kParchment, line.c_str());
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
	g_watch_battles = GetPrivateProfileIntW(L"coop", L"watch_battles", 1, path) != 0;
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
	static const BYTE kEventCall[] = {0x57, 0x53, 0x8d, 0x8e, 0xbc, 0xf7, 0xff, 0xff, 0xe8};  // push edi; push ebx; lea ecx,[esi-844h]; call
	if (g_probe && memcmp(reinterpret_cast<const void *>(0x0059db05), kEventCall, sizeof(kEventCall)) == 0)
		g_screen_event_next = reinterpret_cast<ScreenEventFn>(hookpatch::RedirectCall(0x0059db0d, 0x005a5d90, &ScreenEventHook));
	if (g_probe)
	{
		const BYTE *c = reinterpret_cast<const BYTE *>(0x0059db05);
		hooklog::Write("spectate probe: the adventure screen's events %s (bytes %02x %02x %02x %02x %02x %02x %02x %02x %02x)",
		               g_screen_event_next ? "are logged" : "cannot be logged (code not as expected)", c[0], c[1], c[2], c[3], c[4],
		               c[5], c[6], c[7], c[8]);
	}
	g_send_next = reinterpret_cast<SendFn>(hookpatch::PatchPointer(kSendCell, kSend, &SendHook));
	if (!g_send_next)
		hooklog::Write("spectate: the game's send of commands is not as expected: no watching of battles (following the "
		               "map still works)");
	d3dhook::AddPresentCallback(&OnPresent, 900);
	const overlay::Client client = {kName, &Wanted, &Prepare, &Draw};
	if (!overlay::AddClient(client))
		hooklog::Write("spectate: no overlay in this game: no button to watch a battle (a click on the name still follows)");
	else
		overlay::EnableModalInput();  // (its click zones: the game does not get the button's clicks)
	hooklog::Write("spectate: ready (a click on an ally's name in the player list follows their view; allies may follow "
	               "this PC's view: %s)%s",
	               g_share ? "yes" : "no, share_view=0", g_probe ? "; research probe on" : "");
}

}  // namespace mods

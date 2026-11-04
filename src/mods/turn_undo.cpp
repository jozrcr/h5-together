// Mod: undo End Turn while the other humans still play ([coop] undo_end_turn: the
// host's choice, off by default; docs/plans/turn-features.md, part 1).
//
// In plain words: with simultaneous turns, a player who pressed End Turn too early
// gets a button "Resume my turn" under the game's own "Please wait for the other
// players..." banner, for as long as another human still plays. A click undoes the
// End Turn on every PC, at the same moment of the game.
//
// 1. The game's turns (players.md, turn-features.md): End Turn sets the player's
//    "pressed End Turn" (+0x178, IPlayer slot 0x50); with simultaneous turns the
//    world's turn manager (0x00998520, every world tick) gives state 1 to each player
//    at state 0 who pressed it, as long as some player at state 0 has not; when none
//    is left, the next turn starts (0x00998060) in the same tick. The undo puts
//    "pressed" and the state back to 0 with the player's own setters (slots 0x50 and
//    0x48): the game's banner and player list follow by themselves (robot, 2026-11-04).
// 2. Who takes part, decided once per game (the hero looks' rule, hero_skins.cpp 5):
//    each PC's DLL sends a hello after its own segment report kSendAt, which counts if
//    it came by its sender's report kSendBy; the host's hello says whether the host
//    allows the undo. At the start of segment kDecideAt every PC decides the same:
//    on only if every PC sent its hello and the host allows it (undo::Decide).
// 3. A click (or the robot's "panel|turn request"): this PC sends "undo player P"
//    after its next segment report R. Every PC, the sender too, takes it in at the
//    start of segment R + kDelay (the segment runner's call 0x007c9a67, before that
//    segment's commands) and decides there, on the same world, whether it is still
//    allowed (undo::Allowed); a request that comes after the last other human ended
//    is refused the same way everywhere.
// 4. The button: an overlay client on the adventure screen, in the game's own button
//    art, shown only while this PC's player could undo; the overlay holds back the
//    game's clicks under it (overlay::BlockClicksIn), so the click does not reach the map.
//
// Robot steps: "panel|turn request" (as a click on the button), "panel|turn
// force_request" (a request even when the button would not show: to test that every
// PC refuses it the same way), "panel|turn states" (every player's state in the log).
#include "turn_undo.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "rmg_builder_widgets.h"
#include "turn_undo_logic.h"

#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/patch.h"
#include "../hook/robot.h"
#include "../hook/texts.h"
#include "../hook/third_party/imgui/imgui.h"

namespace mods {
namespace {

namespace ui = builder_ui;

const char kName[] = "turn_undo";
const char kAdventureScreen[] = ".?AVCAdventureScreen@NUI@@";

// --- the game (vanilla 3.1 addresses, the same in MMH55_Utility) -----------------------------
const DWORD kUpdateCall = 0x0060895e;      // in 0x00608930: call 0x006082f0 (the adventure controller's frame)
const DWORD kUpdate = 0x006082f0;          // __thiscall, 5 stack arguments
const DWORD kControllerWorld = 0x18, kControllerLocal = 0x3c;  // NWorld world, this PC's IPlayer *
const DWORD kMpStepCell = 0x00e43f1c + 0x10;  // NWorld::CMultiPlayerTransceiver slot 4 (its own step: reports)
const DWORD kMpStep = 0x007ca990;
const DWORD kCheckCall = 0x007ca8d1;       // in the message pump 0x007ca7c0: call 0x007c9410 (a checksum came)
const DWORD kCheckHandler = 0x007c9410;    // __thiscall(int pc, CCrcTestCmd *), ret 8; ignores check -1
const DWORD kSegmentCall = 0x007c9a67;     // in the segment runner 0x007c98e0, before a segment's commands
const DWORD kSegmentTime = 0x007cc680;     // __thiscall(transceiver + 8, DWORD game time), ret 4
const DWORD kCrcCmdVftable = 0x00e43db0;   // NWorld::CCrcTestCmd, 0x2c bytes
const DWORD kGameNew = 0x00878fd0;
const DWORD kReleaseStrong = 0x008c61d0;   // __thiscall(object, mask)
// The transceiver (as the co-op guard and hero looks read it): the network, the
// players' segment reports (one per PC), this PC's index (the host is 0)
const DWORD kNet = 0x34, kReports = 0x3c, kReportsEnd = 0x40, kMe = 0x54;
const int kMaxPcs = 8;
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
// The functions behind the slots above (players.md) and the hooked code (as hero looks
// checks it), checked before anything is changed.
const Code kCode[] = {
	{0x00720e90, {0x8b, 0x41, 0x78, 0xc3}, 4},                            // slot 0x2c: the number at +0x78
	{0x00500c10, {0x8b, 0x81, 0x34, 0x06, 0x00, 0x00}, 6},                // slot 0x44: the state at +0x634
	{0x00500c20, {0x8b, 0x44, 0x24, 0x04, 0x39, 0x81, 0x34, 0x06}, 8},   // slot 0x48: set it
	{0x00500c40, {0x8a, 0x81, 0x78, 0x01, 0x00, 0x00}, 6},                // slot 0x4c: pressed at +0x178
	{0x00500c50, {0x8a, 0x44, 0x24, 0x04, 0x38, 0x81, 0x78, 0x01}, 8},   // slot 0x50: set it
	{0x00500d60, {0x33, 0xc0, 0x39, 0x41, 0x74, 0x0f, 0x94, 0xc0, 0xc3}, 9},  // slot 0x184: human
	{kMpStep, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x1c, 0x21, 0x00}, 8},
	{kCheckHandler, {0x83, 0xec, 0x60, 0x33, 0xc0, 0x39, 0x44, 0x24}, 8},
	{0x007c9a5f, {0x8d, 0x4e, 0x08, 0x53, 0x89, 0x4c, 0x24, 0x20}, 8},    // lea ecx,[esi+8]; push ebx; ... (then the call)
	{kSegmentTime, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x57, 0x55, 0x8b, 0xd9}, 10},
};

// --- layout: the game's 1024x768 screens, centred and scaled with the height ---------------
// The button: under the game's "Veuillez attendre les autres joueurs..." banner
// (robot run 20261104-082900-lan: the banner's middle at x 532, its bottom at y 575).
const float kRefH = 768.0f, kScreenW = 1024.0f;
const float kButtonX = 422.0f, kButtonY = 582.0f, kButtonW = 220.0f, kButtonH = 34.0f, kButtonFont = 15.0f;

typedef void(__fastcall *UpdateFn)(void *controller, void *edx, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);
typedef void(__fastcall *StepFn)(void *transceiver, void *edx);
typedef void(__fastcall *CheckFn)(void *transceiver, void *edx, int pc, const BYTE *cmd);
typedef void(__fastcall *SegmentFn)(void *transceiver8, void *edx, DWORD time);
typedef int(__fastcall *GetFn)(void *self, void *edx);
typedef void(__fastcall *SetFn)(void *self, void *edx, int value);

UpdateFn g_update_next = nullptr;
bool g_disabled = false;  // part of the game's code was not as expected: the hooks only pass on
StepFn g_mp_step_next = nullptr;
CheckFn g_check_next = nullptr;
SegmentFn g_segment_next = nullptr;

// --- state: all on the game's main thread (its frame, the message pump, the segment
// runner and the transceiver's step), except the overlay's draw, which reads g_shown ---
void *g_controller = nullptr;     // the adventure controller (world, this PC's player)
const void *g_transceiver = nullptr;
DWORD g_segments = 0;             // segments started in this game
DWORD g_last_reports = 0;
bool g_hello_sent = false;
unsigned g_hellos[kMaxPcs];       // each PC's hello: 0 none, else its version
bool g_host_allows = false;
bool g_decided = false, g_on = false;
int g_request = 0;                // a click: the player to undo, sent after this PC's next report
struct Pending {
	DWORD at;
	int player;
	int from_pc;
};
std::vector<Pending> g_pending;
DWORD g_requested_at = 0;         // this PC's last request counts at this segment (no button until then)
// the button
bool g_shown = false, g_down = false, g_hovered = false;
float g_back_w = 1024.0f, g_back_h = 768.0f;

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

void *World()
{
	return g_controller ? reinterpret_cast<void *>(Read(g_controller, kControllerWorld)) : nullptr;
}

// The players as undo::Allowed sees them, and their IPlayer pointers (same order).
int ReadPlayers(void *world, undo::Player *out, void **pointers, int max)
{
	int n = 0;
	void **p = reinterpret_cast<void **>(Read(world, kPlayers));
	void **end = reinterpret_cast<void **>(Read(world, kPlayersEnd));
	for (; p && p < end && n < max; ++p) {
		if (!*p || Dying(*p))
			continue;
		undo::Player &u = out[n];
		u.number = Get(*p, kNumber);
		u.human = (Get(*p, kHuman) & 0xff) != 0;
		u.in_game = Get(*p, kInGame) == 1;
		u.state = Get(*p, kState);
		u.pressed = (Get(*p, kPressed) & 0xff) != 0;
		pointers[n++] = *p;
	}
	return n;
}

bool Simultaneous(void *world)
{
	return *(static_cast<const BYTE *>(world) + kSimultaneous) != 0;
}

// "1h:0/1 2h:1/0 3ai:..." : number, human or computer, state / pressed End Turn.
std::string States(void *world)
{
	undo::Player players[8];
	void *pointers[8];
	const int n = ReadPlayers(world, players, pointers, 8);
	std::string out;
	for (int i = 0; i < n; ++i) {
		if (!players[i].in_game)
			continue;
		char item[32];
		_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%d%s:%d/%d", out.empty() ? "" : " ", players[i].number,
		            players[i].human ? "h" : "ai", players[i].state, players[i].pressed ? 1 : 0);
		out += item;
	}
	return out;
}

// May `number` undo its End Turn on this world now?
bool AllowedNow(void *world, int number, const char **why)
{
	undo::Player players[8];
	void *pointers[8];
	const int n = ReadPlayers(world, players, pointers, 8);
	return undo::Allowed(players, n, Simultaneous(world), number, why);
}

// The undo, at the start of a segment on every PC: done if still allowed.
void Apply(void *world, int number, int from_pc)
{
	undo::Player players[8];
	void *pointers[8];
	const int n = ReadPlayers(world, players, pointers, 8);
	const std::string before = States(world);
	const char *why = "";
	if (!undo::Allowed(players, n, Simultaneous(world), number, &why)) {
		hooklog::Write("turn undo: segment %lu: PC %d's request for player %d refused: %s (%s)", g_segments, from_pc, number, why,
		               before.c_str());
		return;
	}
	for (int i = 0; i < n; ++i)
		if (players[i].number == number) {
			Set(pointers[i], kSetPressed, 0);
			Set(pointers[i], kSetState, 0);
		}
	hooklog::Write("turn undo: segment %lu: player %d plays again (PC %d asked; before: %s; after: %s)", g_segments, number,
	               from_pc, before.c_str(), States(world).c_str());
}

int LocalNumber()
{
	if (!g_controller)
		return 0;
	void *local = reinterpret_cast<void *>(Read(g_controller, kControllerLocal));
	return local && !Dying(local) ? Get(local, kNumber) : 0;
}

// --- the game's hooks --------------------------------------------------------------------------
void NewGame(const void *transceiver)
{
	g_transceiver = transceiver;
	g_segments = 0;
	g_last_reports = 0;
	g_hello_sent = false;
	memset(g_hellos, 0, sizeof(g_hellos));
	g_host_allows = false;
	g_decided = false;
	g_on = false;
	g_request = 0;
	g_pending.clear();
	g_requested_at = 0;
}

bool Pcs(const BYTE *t, int *me, int *pcs)
{
	*me = static_cast<int>(Read(t, kMe));
	*pcs = static_cast<int>((Read(t, kReportsEnd) - Read(t, kReports)) / 4);
	return *pcs > 0 && *pcs <= kMaxPcs && *me >= 0 && *me < *pcs;
}

DWORD Reports(const BYTE *t, int pc)
{
	return reinterpret_cast<const DWORD *>(Read(t, kReports))[pc];
}

// A world-checksum message with check number -1 and our value, to the other PCs (as
// hero looks and the co-op guard send theirs; the game ignores check -1).
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

bool HostSetting()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	return GetPrivateProfileIntW(L"coop", L"undo_end_turn", 0, path) != 0;
}

void __fastcall UpdateHook(void *controller, void *edx, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
	g_controller = controller;
	g_update_next(controller, edx, a, b, c, d, e);
}

// After this PC's own step (its segment report went out): the hello, and a request.
void __fastcall MpStepHook(void *self, void *edx)
{
	g_mp_step_next(self, edx);
	if (g_disabled)
		return;
	const BYTE *t = static_cast<const BYTE *>(self);
	int me = 0, pcs = 0;
	if (!Pcs(t, &me, &pcs))
		return;
	const DWORD reports = Reports(t, me);
	if (t != g_transceiver || reports < g_last_reports)
		NewGame(t);
	g_last_reports = reports;
	if (!g_hello_sent && reports >= undo::kSendAt && reports <= undo::kSendBy) {
		g_hello_sent = true;
		const bool allow = me == 0 && HostSetting();
		g_hellos[me] = undo::kVersion;
		if (me == 0)
			g_host_allows = allow;
		SendValue(t, undo::EncodeHello(allow));
		hooklog::Write("turn undo: hello sent (PC %d of %d%s)", me, pcs, me == 0 ? (allow ? ", the host allows the undo" : ", the host does not allow the undo ([coop] undo_end_turn=0)") : "");
	}
	if (g_request && g_on) {
		const DWORD at = undo::RequestCountsAt(reports);
		SendValue(t, undo::EncodeRequest(g_request));
		g_pending.push_back({at, g_request, me});
		g_requested_at = at;
		hooklog::Write("turn undo: request sent for player %d (after this PC's report %lu; it counts at segment %lu)", g_request,
		               reports, at);
	}
	g_request = 0;
}

void __fastcall CheckHook(void *self, void *edx, int pc, const BYTE *cmd)
{
	if (!g_disabled && cmd && *reinterpret_cast<const LONG *>(cmd + 0x0c) == -1) {
		const undo::Message m = undo::Decode(*reinterpret_cast<const DWORD *>(cmd + 0x10));
		if (m.kind != undo::Message::kNone) {
			const BYTE *t = static_cast<const BYTE *>(self);
			int me = 0, pcs = 0;
			if (t != g_transceiver)
				NewGame(t);
			if (!Pcs(t, &me, &pcs) || pc < 0 || pc >= pcs)
				return;
			const DWORD sender_reports = Reports(t, pc);
			if (m.kind == undo::Message::kHello) {
				// counted only if it came by its sender's report kSendBy: the same hellos count on every PC
				if (sender_reports <= undo::kSendBy && !g_decided) {
					g_hellos[pc] = m.version;
					if (pc == 0)
						g_host_allows = m.allow;
					hooklog::Write("turn undo: hello from PC %d (version %u%s)", pc, m.version,
					               pc == 0 ? (m.allow ? ", the host allows the undo" : ", the host does not allow the undo") : "");
				}
				else
					hooklog::Write("turn undo: hello from PC %d after its report %lu: too late, not counted", pc, sender_reports);
			}
			else if (g_on) {
				const DWORD at = undo::RequestCountsAt(sender_reports);
				g_pending.push_back({at, m.player, pc});
				hooklog::Write("turn undo: PC %d asks to undo player %d's End Turn (counts at segment %lu)", pc, m.player, at);
			}
			return;  // ours: the game never sees it
		}
	}
	g_check_next(self, edx, pc, cmd);
}

void SafeApply(void *world, int number, int from_pc)
{
	__try {
		Apply(world, number, from_pc);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::Write("turn undo: crashed reading the players (caught)");
	}
}

void RobotCommand(const char *text);

// The start of each segment's commands (every PC runs the same segments in the same
// order): the decision at kDecideAt, then the requests that count at this segment.
void __fastcall SegmentHook(void *transceiver8, void *edx, DWORD time)
{
	if (g_disabled) {
		g_segment_next(transceiver8, edx, time);
		return;
	}
	const BYTE *t = static_cast<const BYTE *>(transceiver8) - 8;
	if (t != g_transceiver)
		NewGame(t);
	++g_segments;
	char command[64];
	while (robot::TakePanelCommand(command, sizeof(command), "turn "))
		RobotCommand(command);
	int me = 0, pcs = 0;
	if (g_segments == undo::kDecideAt && !g_decided && Pcs(t, &me, &pcs)) {
		g_decided = true;
		g_on = undo::Decide(g_hellos, pcs, g_host_allows);
		if (g_on)
			hooklog::Write("turn undo: on for this game (all %d PCs know it, the host allows it)", pcs);
		else {
			int missing = -1;
			for (int pc = 0; pc < pcs && missing < 0; ++pc)
				if (!g_hellos[pc])
					missing = pc;
			hooklog::Write("turn undo: off for this game (%s)", !g_host_allows ? "the host does not allow it" : missing >= 0 ? "a PC sent no hello: no add-on, or an older one" : "a game of one PC");
		}
	}
	void *world = World();
	for (size_t i = 0; i < g_pending.size();) {
		if (g_pending[i].at > g_segments) {
			++i;
			continue;
		}
		if (g_pending[i].at < g_segments)
			hooklog::Write("turn undo: a request for segment %lu taken at segment %lu (late!)", g_pending[i].at, g_segments);
		if (world)
			SafeApply(world, g_pending[i].player, g_pending[i].from_pc);
		g_pending.erase(g_pending.begin() + i);
	}
	g_segment_next(transceiver8, edx, time);
}

// --- the button -----------------------------------------------------------------------------------
// May this PC's player undo now (the button shows)?
bool CanUndoHere()
{
	if (!g_on || !g_controller || (g_requested_at && g_segments < g_requested_at))
		return false;
	void *world = World();
	const int me = LocalNumber();
	const char *why;
	return world && me > 0 && AllowedNow(world, me, &why);
}

bool SafeCanUndoHere()
{
	__try {
		return CanUndoHere();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void Clicked(const char *how)
{
	const int me = LocalNumber();
	if (me <= 0)
		return;
	g_request = me;  // sent after this PC's next segment report
	hooklog::Write("turn undo: %s: undo player %d's End Turn", how, me);
}

void RobotCommand(const char *text)
{
	if (strcmp(text, "request") == 0) {
		if (SafeCanUndoHere())
			Clicked("robot");
		else
			hooklog::Write("turn undo: robot: no button now (%s)", g_on ? "the undo is not allowed now" : "the undo is off in this game");
	}
	else if (strcmp(text, "force_request") == 0) {
		// a request even when the button would not show: every PC must refuse it the same way
		if (g_on)
			Clicked("robot (forced, to test a refusal)");
		else
			hooklog::Write("turn undo: robot: the undo is off in this game");
	}
	else if (strcmp(text, "states") == 0) {
		void *world = World();
		hooklog::Write("turn undo: segment %lu: %s", g_segments, world ? States(world).c_str() : "(no world)");
	}
	else
		hooklog::Write("turn undo: robot command \"%s\" not known", text);
}

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
	char top[96] = "";
	overlay::TopScreen(top, sizeof(top));
	g_shown = strcmp(top, kAdventureScreen) == 0 && SafeCanUndoHere();
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
			Clicked("its button");
	}
	return true;
}

void Prepare(const overlay::Frame &frame)
{
	ui::LoadPictures(frame.device);
	overlay::WantFont(floorf(kButtonFont * frame.height / kRefH + 0.5f));
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
	// The game's 122x34 button, wider: its ends kept, its middle stretched (as online_ui.cpp's).
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
	const std::string text = ui::Fit(font, px, texts::Get("turn.undo_button"), p1.x - p0.x - 12.0f * k);
	const ImVec2 size = font->CalcTextSizeA(px, 10000.0f, 0.0f, text.c_str());
	ui::ShadowText(draw, font, px, ImVec2(floorf((p0.x + p1.x - size.x) / 2), floorf((p0.y + p1.y - size.y) / 2 + (pressed ? k : 0.0f))),
	               IM_COL32(255, 236, 190, 255), text.c_str());
	static bool told = false;
	if (!told) {
		told = true;
		hooklog::Write("turn undo: the button is shown (the first time in this run)");
	}
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
	// Every PC takes part (its hello, others' requests), whatever its own setting: only
	// the host's [coop] undo_end_turn decides, at each game's start.
	if (!CodeAsExpected())
		return;
	g_update_next = reinterpret_cast<UpdateFn>(hookpatch::RedirectCall(kUpdateCall, kUpdate, &UpdateHook));
	g_mp_step_next = reinterpret_cast<StepFn>(hookpatch::PatchPointer(kMpStepCell, kMpStep, &MpStepHook));
	g_check_next = reinterpret_cast<CheckFn>(hookpatch::RedirectCall(kCheckCall, kCheckHandler, &CheckHook));
	g_segment_next = reinterpret_cast<SegmentFn>(hookpatch::RedirectCall(kSegmentCall, kSegmentTime, &SegmentHook));
	if (!g_update_next || !g_mp_step_next || !g_check_next || !g_segment_next) {
		hooklog::Write("turn undo: part of the game's code is not as expected (frame %d, step %d, messages %d, segments %d): "
		               "off (the hooks that went in only pass on)",
		               g_update_next != nullptr, g_mp_step_next != nullptr, g_check_next != nullptr, g_segment_next != nullptr);
		g_disabled = true;
		return;
	}
	const overlay::Client client = {kName, &Wanted, &Prepare, &Draw};
	if (!overlay::AddClient(client))
		hooklog::Write("turn undo: no overlay in this game: no button (the requests of others still count)");
	else
		overlay::EnableModalInput();  // (its click zones: the game does not get the button's clicks)
	hooklog::Write("turn undo: ready (the host's [coop] undo_end_turn decides at each game's start; this PC's: %d)",
	               HostSetting() ? 1 : 0);
}

}  // namespace mods

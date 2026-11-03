// Online play's auto-join (see online_join.h).
//
// The steps, each on the game's main thread from the online UI's frame callback
// (as the settings menu's F3 calls the game's own code), each only on the screen
// it belongs to:
//   1. main menu: the Multiplayer page's own command "lan" (what its "Réseau local"
//      button sends): NUI::CMultiPlayerSubmenu::OnCommand, 0x00699730, slot 1 of
//      its vftable 0x00e318b8. The page object is learned by a hook on slot 0
//      (SetOwner, 0x006994a0), which the game calls when it builds the main menu
//      (the robot learns it the same way, robot_menu.cpp; the two hooks stack);
//   2. LAN screen (NUI::CLobbyScreen): every 250 ms, the rows of its games list
//      (robot_lobby.cpp's PickGame: NUI::CGamesView2 at +0x1dc, its list widget's
//      IListControl part at view+8, vftable 0x00e94870; the rows at +0x20; a row's
//      data, IListControlItem slot 1, is the game's NUI::CGameData). The invited
//      host's row is the one whose game data holds the host's fake address (the
//      virtual LAN gives the game only fake addresses, 198.18.0.0/15, one per
//      player: the host's game description carries it, docs/subsystems/
//      multiplayer-sync.md "Addresses inside the packets");
//   3. that row is highlighted (IListControl slot 10, 0x00cba6a0) and the screen
//      gets the command "join", as from its Join button (its
//      IProgrammedReactionsAndChecks part, found through the object's RTTI;
//      React = 0x006fd4f0, the LAN screen's own, which passes "join" to its base's 0x006f7a00);
//   4. done when the waiting room (NUI::CMPWaitScreen) is on top. If the row does
//      not come in 20 s, or Join does not lead there (a message box, or 30 s), the
//      player stays on the LAN screen and a line there says so.
// Never in a game, the waiting room, or any screen but these two; never while a
// message box is open; once per invite; an invite older than 10 minutes is dropped.
#include "online_join.h"

#include <winsock2.h>
#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

#include "online_ui_logic.h"
#include "virtual_lan.h"
#include "virtual_lan_logic.h"

#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/patch.h"
#include "../hook/robot.h"

namespace mods {
namespace autojoin {
namespace {

// --- the game (vanilla 3.1 addresses, the same in MMH55_Utility) ----------------------------
const DWORD kMultiSlot0 = 0x00e318b8;       // NUI::CMultiPlayerSubmenu's vftable, slot 0
const DWORD kMultiSetOwner = 0x006994a0;    // SetOwner(owner, ?): __thiscall, ret 8
const DWORD kMultiOnCommand = 0x00699730;   // slot 1: OnCommand(?, const string *, ?), ret 0Ch: "hot_seat", "lan"...
// The LAN screen's React(?, const string *, ?): NUI::CLobbyScreen's own (vftable cell 0x00e382dc:
// "duelmode", then its base's), and the base's (NUI::CLobbyScreenBase: "join", "create", "changename"...)
const DWORD kLobbyReact = 0x006fd4f0, kLobbyBaseReact = 0x006f7a00;
const DWORD kListVftable = 0x00e94870;      // CWindowListCtrl's IListControl part
const DWORD kDynamicCast = 0x00d119fc;      // the game's __RTDynamicCast
const DWORD kLobbyView = 0x1dc, kViewList = 8, kListRows = 0x20;
const char kLobbyScreen[] = ".?AVCLobbyScreen@NUI@@";
const char kWaitScreen[] = ".?AVCMPWaitScreen@NUI@@";
const char kMainMenu[] = ".?AVCMainMenuScreen@NUI@@";
const char kReactions[] = ".?AUIProgrammedReactionsAndChecks@@";
const DWORD kKeepMs = 10 * 60000, kOpenMs = 10000, kRowMs = 20000, kJoinMs = 30000, kScanMs = 250, kPickMs = 600;
const size_t kDataBytes = 0x400;  // how much of a row's game data is searched for the address
// NUI::CGameData +0x14: the host's sockaddr_in (the join's 0x006fb9a0 returns data + 0xc), its
// address at +0x18 (robot run 20261103-191003-lan: the host's 198.18.161.134 there)
const int kDataServerAddress = 0x18;

typedef void(__fastcall *SetOwnerFn)(void *self, void *edx, void *a, void *b);
typedef bool(__fastcall *CommandFn)(void *self, void *edx, void *a, robot::GameString *command, void *c);
typedef void *(__cdecl *DynamicCastFn)(void *object, LONG vf_delta, const void *from, const void *to, BOOL is_ref);
typedef void *(__fastcall *GetFn)(void *self, void *edx);
typedef void(__fastcall *SelectFn)(void *self, void *edx, void *data);

SetOwnerFn g_set_owner = nullptr;
void *volatile g_multi_page = nullptr;

void __fastcall SetOwnerHook(void *self, void *edx, void *a, void *b)
{
	g_multi_page = self;  // (the main menu is being built: its Multiplayer page)
	g_set_owner(self, edx, a, b);
}

// MSVC's RTTI records (32-bit).
struct TypeDescriptor {
	DWORD vftable, spare;
	char name[1];
};
struct BaseClassDescriptor {
	const TypeDescriptor *type;
	DWORD contained;
	int mdisp, pdisp, vdisp;
	DWORD attributes;
};
struct Hierarchy {
	DWORD signature, attributes, count;
	const BaseClassDescriptor *const *bases;
};
struct Locator {
	DWORD signature, offset, cd_offset;
	const TypeDescriptor *type;
	const Hierarchy *hierarchy;
};

const char *SafeClassName(const void *object)
{
	__try {
		const DWORD *vt = *static_cast<const DWORD *const *>(object);
		const Locator *l = reinterpret_cast<const Locator *>(vt[-1]);
		return l && l->type ? l->type->name : "?";
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return "?";
	}
}

// The object's part that takes button commands (IProgrammedReactionsAndChecks), or null.
void *SafeReactions(void *object)
{
	__try {
		const DWORD *vt = *static_cast<const DWORD *const *>(object);
		const Locator *l = reinterpret_cast<const Locator *>(vt[-1]);
		if (!l || !l->hierarchy || l->hierarchy->count > 64)
			return nullptr;
		for (DWORD i = 0; i < l->hierarchy->count; ++i) {
			const BaseClassDescriptor *b = l->hierarchy->bases[i];
			if (b && b->type && b->pdisp == -1 && strcmp(b->type->name, kReactions) == 0)
				return static_cast<BYTE *>(object) + b->mdisp;
		}
		return nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool SafeCommand(CommandFn fn, void *self, const char *text)
{
	const size_t n = strlen(text);
	robot::GameString s = {text, text + n, text + n};
	__try {
		return fn(self, nullptr, nullptr, &s, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// Where `net` (an address in network byte order) is in the `n` bytes at `p`: its
// offset, or -1. Also looks one pointer deep from the first 0x100 bytes (+0x1000 +
// the offset inside).
int SafeFind(const BYTE *p, size_t n, DWORD net)
{
	__try {
		for (size_t i = 0; i + 4 <= n; i += 2)
			if (*reinterpret_cast<const DWORD UNALIGNED *>(p + i) == net)
				return static_cast<int>(i);
		for (size_t i = 0; i + 4 <= 0x100 && i + 4 <= n; i += 4) {
			const BYTE *q = *reinterpret_cast<const BYTE *const *>(p + i);
			MEMORY_BASIC_INFORMATION mbi;
			if (reinterpret_cast<DWORD>(q) < 0x10000 || !VirtualQuery(q, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
			    (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)))
				continue;
			const size_t room = static_cast<size_t>(static_cast<const BYTE *>(mbi.BaseAddress) + mbi.RegionSize - q);
			const size_t m = room < 0x100 ? room : 0x100;
			for (size_t j = 0; j + 4 <= m; j += 2)
				if (*reinterpret_cast<const DWORD UNALIGNED *>(q + j) == net)
					return static_cast<int>(0x1000 + i * 0x100 + j);
		}
		return -1;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return -1;
	}
}

const void *g_window_type = nullptr;
const void *g_item_type = nullptr;

bool Types()
{
	if (!g_window_type)
		g_window_type = robot::TypeDescriptorOf("IWindow");
	if (!g_item_type)
		g_item_type = robot::TypeDescriptorOf("IListControlItem");
	return g_window_type && g_item_type;
}

// The LAN screen's games list: its rows' game data (at most `max`); their count, -1 if
// the screen is not as expected.
int SafeRows(void *lobby, void **out, int max, BYTE **list_out)
{
	const DynamicCastFn cast = reinterpret_cast<DynamicCastFn>(kDynamicCast);
	__try {
		BYTE *view = *reinterpret_cast<BYTE **>(static_cast<BYTE *>(lobby) + kLobbyView);
		BYTE *list = view ? *reinterpret_cast<BYTE **>(view + kViewList) : nullptr;
		if (!list || *reinterpret_cast<DWORD *>(list) != kListVftable)
			return -1;
		*list_out = list;
		const DWORD *head = *reinterpret_cast<const DWORD **>(list + kListRows);
		int count = 0;
		for (const DWORD *node = reinterpret_cast<const DWORD *>(head[0]); node != head && count < max;
		     node = reinterpret_cast<const DWORD *>(node[0])) {
			void *item = cast(reinterpret_cast<void *>(node[2]), 0, g_window_type, g_item_type, FALSE);
			out[count++] = item ? reinterpret_cast<GetFn>((*static_cast<void ***>(item))[1])(item, nullptr) : nullptr;
		}
		return count;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return -1;
	}
}

// The highlighted row's game data, or null.
void *SafeHighlightedData(BYTE *list)
{
	__try {
		void **vt = *reinterpret_cast<void ***>(list);
		void *item = reinterpret_cast<GetFn>(vt[9])(list, nullptr);
		return item ? reinterpret_cast<GetFn>((*static_cast<void ***>(item))[1])(item, nullptr) : nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool SafeHighlight(BYTE *list, void *data)
{
	__try {
		void **vt = *reinterpret_cast<void ***>(list);
		reinterpret_cast<SelectFn>(vt[10])(list, nullptr, data);
		return reinterpret_cast<GetFn>(vt[9])(list, nullptr) != nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// --- the job --------------------------------------------------------------------------------------
enum class Step { kIdle, kWaitMenu, kOpening, kWaitRow, kJoining, kNoRow, kFailed };
struct Job {
	Step step = Step::kIdle;
	uint64_t lobby = 0, host = 0;
	char name[online::kNameSize] = "";
	DWORD since = 0, step_at = 0, scanned_at = 0;
	bool saw_box = false, said_address = false;
	bool any_host = false;  // the robot's test: the first host this PC hears of (no Steam invite)
	void *picked = nullptr;  // the host's row, highlighted at picked_at
	DWORD picked_at = 0;
	const void *said_elsewhere = nullptr;
	int rows_seen = -1;
};
Job g;
bool g_said_no_page = false;

const char *StepName(Step s)
{
	switch (s) {
	case Step::kWaitMenu: return "waiting for the main menu";
	case Step::kOpening: return "opening the LAN screen";
	case Step::kWaitRow: return "waiting for the host's game in the LAN list";
	case Step::kJoining: return "joining";
	case Step::kNoRow: return "the host's game did not show";
	case Step::kFailed: return "the join did not work";
	default: return "idle";
	}
}

void Go(Step step, DWORD now, const char *why)
{
	g.step = step;
	g.step_at = now;
	g.scanned_at = 0;
	g.saw_box = false;
	hooklog::Write("online: auto-join: %s (%s)", StepName(step), why);
}

void Stop(const char *why)
{
	if (g.step == Step::kIdle)
		return;
	hooklog::Write("online: auto-join: stopped (%s)", why);
	g.step = Step::kIdle;
}

// Looks for the host's row; highlights it and presses Join. True when Join was pressed.
bool TryJoin(void *lobby)
{
	uint32_t address = 0;
	if (!g.host && g.any_host)
		g.host = OnlineFirstHost();  // (the loopback hears of hosts only once the LAN screen has its sockets)
	if (!g.host || !OnlineHostAddress(g.host, &address))
		return false;  // (its lobby's data has not come yet, or its game not heard from)
	if (!g.said_address) {
		g.said_address = true;
		char ip[20];
		vlan::FormatIp(address, ip, sizeof(ip));
		hooklog::Write("online: auto-join: the host's game is at %s for our game", ip);
	}
	if (!Types())
		return false;
	void *rows[64];
	BYTE *list = nullptr;
	const int n = SafeRows(lobby, rows, 64, &list);
	if (n != g.rows_seen) {
		g.rows_seen = n;
		hooklog::Write("online: auto-join: the LAN list has %d game(s)%s", n, n < 0 ? " (its widget is not as expected)" : "");
	}
	const DWORD net = htonl(address);
	for (int i = 0; i < n; ++i) {
		if (!rows[i])
			continue;
		const int at = SafeFind(static_cast<const BYTE *>(rows[i]), kDataBytes, net);
		if (at < 0)
			continue;
		if (at != kDataServerAddress) {  // (the address as a player of another game: not its host)
			if (g.said_elsewhere != rows[i])
				hooklog::Write("online: auto-join: game %d holds the host's address at +0x%x, not as its server: not that one", i, at);
			g.said_elsewhere = rows[i];
			continue;
		}
		// First highlight the row, as a click would; Join only on a later look (the screen
		// takes the highlighted row as its choice in its own update: pressed in the same
		// frame, Join did nothing)
		const DWORD now = GetTickCount();
		const void *shown = SafeHighlightedData(list);
		if (!shown || SafeFind(static_cast<const BYTE *>(shown), kDataBytes, net) < 0) {
			if (!SafeHighlight(list, rows[i])) {
				hooklog::Write("online: auto-join: game %d of the list is the host's (its data +0x%x) but could not be highlighted", i,
				               at);
				return false;
			}
			hooklog::Write("online: auto-join: game %d of %d is the host's (its data +0x%x): highlighted", i, n, at);
			g.picked = rows[i];
			g.picked_at = now;
			return false;
		}
		if (!g.picked) {  // (the player highlighted it)
			g.picked = rows[i];
			g.picked_at = now;
		}
		if (now - g.picked_at < kPickMs)
			return false;
		void *reactions = SafeReactions(lobby);
		const DWORD react = reactions ? **reinterpret_cast<DWORD **>(reactions) : 0;
		if (react != kLobbyReact && react != kLobbyBaseReact) {
			hooklog::Write("online: auto-join: the LAN screen's buttons are not as expected (%08lx): Join not pressed", react);
			g.step = Step::kFailed;
			return false;
		}
		const bool handled = SafeCommand(reinterpret_cast<CommandFn>(react), reactions, "join");
		hooklog::Write("online: auto-join: Join pressed on game %d -> %s", i, handled ? "handled" : "not handled");
		return handled;
	}
	return false;
}

// --- the game's message boxes --------------------------------------------------------------------
// NUI::CCommonMsgBox's constructor (__thiscall, one argument, ret 4; it also builds the base of
// NUI::CCommonMsgBoxWithUpdatableText, 0x006f22f0, the "connecting"/"timeout" box), and its call sites
// in the exe (the same in vanilla 3.1 and MMH55_Utility). Its vftable is 0x00e3091c (0x00e37564 for
// the one with updatable text); when a box is destroyed, its base's destructor changes that vftable.
// Not every box is shown: the robot's run 20261103-193021-lan had a box made (at 0x0067af1b) and never
// shown. Its window fields told the two apart (the shown "Timeout..." box first): +0x14 1 / 0, +0x18
// a pointer / 0 (its parent window, it seems), +0x104 the LAN screen object / 0. A box counts as open
// while +0x14 and +0x18 are set.
const DWORD kBoxCtor = 0x0067a730;
const DWORD kBoxCtorSites[] = {0x0067919d, 0x0067925d, 0x0067931a, 0x006793d8, 0x0067ae55,
                               0x0067af1b, 0x006f23a7, 0x0077f597, 0x0077fb47};
const DWORD kBoxShown = 0x14, kBoxParent = 0x18;
const int kMaxBoxes = 8;
typedef void *(__fastcall *BoxCtorFn)(void *self, void *edx, void *arg);
BoxCtorFn g_box_ctor = nullptr;
struct Box {
	void *object;
	DWORD vftable;  // read on the first look after its constructor (the derived class sets its own)
	DWORD site;     // the call that made it (for the log)
	bool shown;
};
Box g_boxes[kMaxBoxes] = {};
CRITICAL_SECTION g_box_lock;
int g_box_open = 0;

void *__fastcall BoxCtorHook(void *self, void *edx, void *arg)
{
	const DWORD site = reinterpret_cast<DWORD>(_ReturnAddress()) - 5;
	void *r = g_box_ctor(self, edx, arg);
	EnterCriticalSection(&g_box_lock);
	int slot = -1;
	for (int i = 0; i < kMaxBoxes && slot < 0; ++i)
		if (!g_boxes[i].object || g_boxes[i].object == self)
			slot = i;
	if (slot >= 0)
		g_boxes[slot] = {self, 0, site, false};
	LeaveCriticalSection(&g_box_lock);
	return r;
}

DWORD SafeVftable(const void *object)
{
	__try {
		return *static_cast<const DWORD *>(object);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

bool SafeShown(const void *object)
{
	__try {
		const BYTE *p = static_cast<const BYTE *>(object);
		return *reinterpret_cast<const DWORD *>(p + kBoxShown) != 0 && *reinterpret_cast<const DWORD *>(p + kBoxParent) != 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

}  // namespace

bool InstallBoxWatch()
{
	if (g_box_ctor)
		return true;
	InitializeCriticalSection(&g_box_lock);
	g_box_ctor = reinterpret_cast<BoxCtorFn>(hookpatch::RedirectCalls(
		kBoxCtorSites, static_cast<int>(sizeof(kBoxCtorSites) / sizeof(kBoxCtorSites[0])), kBoxCtor, &BoxCtorHook));
	if (!g_box_ctor)
		hooklog::Write("online: the game's message box code is not as expected: our lines may show over its boxes");
	return g_box_ctor != nullptr;
}

bool GameBoxOpen()
{
	if (!g_box_ctor)
		return false;
	EnterCriticalSection(&g_box_lock);
	int open = 0;
	char said[160] = "";
	for (Box &b : g_boxes) {
		if (!b.object)
			continue;
		const DWORD vft = SafeVftable(b.object);
		if (!b.vftable && vft) {
			b.vftable = vft;
			char one[96];
			_snprintf_s(one, sizeof(one), _TRUNCATE, "; new: %s, made at %08lx", SafeClassName(b.object), b.site);
			strncat_s(said, sizeof(said), one, _TRUNCATE);
		}
		if (!vft || vft != b.vftable) {
			strncat_s(said, sizeof(said), "; one gone", _TRUNCATE);
			b = Box();  // destroyed
			continue;
		}
		const bool shown = SafeShown(b.object);
		if (shown != b.shown) {
			b.shown = shown;
			strncat_s(said, sizeof(said), shown ? "; one shown" : "; one hidden", _TRUNCATE);
		}
		open += shown;
	}
	LeaveCriticalSection(&g_box_lock);
	if (open != g_box_open || said[0]) {
		hooklog::Write("online: game message boxes shown: %d%s", open, said);
		g_box_open = open;
	}
	return open > 0;
}

bool Install()
{
	if (g_set_owner)
		return true;
	g_set_owner = reinterpret_cast<SetOwnerFn>(hookpatch::PatchPointer(kMultiSlot0, kMultiSetOwner, &SetOwnerHook));
	if (!g_set_owner || *reinterpret_cast<const DWORD *>(kMultiSlot0 + 4) != kMultiOnCommand) {
		hooklog::Write("online: auto-join: the main menu's Multiplayer page is not as expected: invites show a banner only");
		return false;
	}
	return true;
}

void Start(uint64_t lobby, uint64_t host, const char *name, const char *why)
{
	g = Job();
	g.lobby = lobby;
	g.host = host;
	g.any_host = !lobby && !host;
	online::CopyName(name, g.name, sizeof(g.name));
	g.since = GetTickCount();
	if (!g_set_owner) {
		hooklog::Write("online: auto-join: not installed: the banner only");
		return;
	}
	Go(Step::kWaitMenu, g.since, why);
}

void Update(uint64_t lobby, uint64_t host)
{
	if (g.step != Step::kIdle && g.lobby == lobby && host && g.host != host) {
		g.host = host;
		hooklog::Write("online: auto-join: the invite's lobby data came: its host is known");
	}
}

void Frame(const char *top, bool boxed)
{
	if (g.step == Step::kIdle)
		return;
	const DWORD now = GetTickCount();
	const bool lan = strcmp(top, kLobbyScreen) == 0, menu = strcmp(top, kMainMenu) == 0, wait = strcmp(top, kWaitScreen) == 0;
	switch (g.step) {
	case Step::kWaitMenu:
		if (now - g.since > kKeepMs) {
			Stop("the invite is older than 10 minutes");
			return;
		}
		if (boxed)
			return;
		if (lan) {
			Go(Step::kWaitRow, now, "the LAN screen is open already");
			return;
		}
		if (!menu)
			return;  // (a game, the waiting room, Options...: never there)
		if (!g_multi_page) {
			if (!g_said_no_page)
				hooklog::Write("online: auto-join: the main menu's Multiplayer page was not seen: the banner only");
			g_said_no_page = true;
			Stop("no Multiplayer page");
			return;
		}
		if (!SafeCommand(reinterpret_cast<CommandFn>(kMultiOnCommand), g_multi_page, "lan")) {
			Stop("the Multiplayer page's \"lan\" was not handled");
			return;
		}
		Go(Step::kOpening, now, "Multiplayer > LAN, as its button");
		return;
	case Step::kOpening:
		if (lan)
			Go(Step::kWaitRow, now, "the LAN screen is open");
		else if (now - g.step_at > kOpenMs)
			Stop("the LAN screen did not open in 10 s");
		return;
	case Step::kWaitRow:
		if (!lan) {
			if (!boxed)
				Stop("the player left the LAN screen");
			return;
		}
		if (boxed)
			return;
		if (now - g.scanned_at >= kScanMs) {
			g.scanned_at = now;
			if (TryJoin(overlay::TopScreenObject())) {
				Go(Step::kJoining, now, "Join pressed");
				return;
			}
		}
		if (g.step == Step::kWaitRow && now - g.step_at > kRowMs)
			Go(Step::kNoRow, now, "no row of the host's game in 20 s");
		return;
	case Step::kJoining:
		if (wait) {
			hooklog::Write("online: auto-join: done, the waiting room is open (%lu s after the invite)", (now - g.since) / 1000);
			g.step = Step::kIdle;
			return;
		}
		if (boxed)
			g.saw_box = true;
		if (lan && !boxed && (g.saw_box || now - g.step_at > kJoinMs))
			Go(Step::kFailed, now, g.saw_box ? "a message box, then the LAN screen again" : "no waiting room in 30 s");
		else if (!lan && !boxed && now - g.step_at > kJoinMs)
			Stop("another screen");
		return;
	case Step::kNoRow:
	case Step::kFailed:
		if (!lan && !boxed)
			Stop("the player left the LAN screen");
		return;
	default:
		return;
	}
}

online::JoinState State(std::string *name)
{
	*name = g.name;
	switch (g.step) {
	case Step::kWaitRow: return online::JoinState::kWaiting;
	case Step::kJoining: return online::JoinState::kJoining;
	case Step::kNoRow: return online::JoinState::kNoRow;
	case Step::kFailed: return online::JoinState::kFailed;
	default: return online::JoinState::kNone;
	}
}

void LogGames()
{
	void *lobby = overlay::TopScreenObject();
	char top[96] = "";
	overlay::TopScreen(top, sizeof(top));
	if (!lobby || strcmp(top, kLobbyScreen) != 0 || !Types()) {
		hooklog::Write("online: robot: games: not on the LAN screen (%s)", top[0] ? top : "no screen");
		return;
	}
	void *rows[64];
	BYTE *list = nullptr;
	const int n = SafeRows(lobby, rows, 64, &list);
	hooklog::Write("online: robot: games: %d row(s)", n);
	for (int i = 0; i < n; ++i) {
		char found[256] = "";
		// every fake address (198.18.0.0/15, network order: c6 12 or c6 13 ...) in the data
		const BYTE *p = static_cast<const BYTE *>(rows[i]);
		for (uint32_t net_hi = 0; net_hi < 2 && p; ++net_hi) {
			__try {
				for (size_t j = 0; j + 4 <= kDataBytes; ++j)
					if (p[j] == 0xc6 && p[j + 1] == 0x12 + net_hi) {
						char one[40];
						_snprintf_s(one, sizeof(one), _TRUNCATE, " +0x%x=198.%u.%u.%u", static_cast<unsigned>(j), p[j + 1],
						            p[j + 2], p[j + 3]);
						strncat_s(found, sizeof(found), one, _TRUNCATE);
					}
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				strncat_s(found, sizeof(found), " (unreadable)", _TRUNCATE);
			}
		}
		hooklog::Write("online: robot: game %d: data %p %s; fake addresses at:%s", i, rows[i], rows[i] ? SafeClassName(rows[i]) : "-",
		               found[0] ? found : " none");
	}
}

}  // namespace autojoin
}  // namespace mods

// Mod: game rules chosen by the host for every player, with no file to swap
// (docs/plans/host-rules.md). Step 1: MMH5.5's own switches of
// scripts/H55-Settings.lua (governors, town conversion, town gate, mine guards,
// AI bonus, mixed neutral stacks, prank artifacts, random map extras...).
//
// How a game gets its rules:
//   1. Lobby hello (every PC with this mod, any map). Each client's DLL sends the
//      host, now and then, waiting-room requests the game ignores (as the faction
//      mod's hello, rmg_factions.cpp: CMPWaitChangeRequest, type unknown to the
//      host's handler 0x0071f3d0): "I apply the host's rules, version 1" (type
//      'OR'), and three hashes of its MMH5.5 files (types 'N', 'S', 'X' in the
//      top byte: H55-Settings.lua without the rules' values, the whole of it,
//      and MMH5.5's stats paks, which 55u.dll reads itself). The host logs (and
//      the panel shows) a player whose files differ from its own.
//   2. Lobby display (host -> clients). The host sends each client that said
//      hello its rules (a short text, "L1|1|0,1,..."), 4 bytes at a time, as the
//      game's "lock buttons" commands (CMPWaitLockButtonsUICommand, 0x007111d0,
//      sent to one PC like the game's own, transport slot 8) for rows that do not
//      exist (0x4f52xxxx): a client without this mod ignores them (0x0071ef10
//      checks the row), ours puts them together and shows the rules read only.
//      (A hero-name command, which holds a string, never reached the client: the
//      game does not send that class over the network.)
//   3. The start (binding). When the host starts (0x0071feb0) and every human
//      player said hello, the start message (NMP::CMPStart) carries one more
//      chunk, 0x4f, after the game's own fields (its serializer, vtable slot 3
//      0x007208a0, through the IBinSaver: a chunk with an unknown number is
//      skipped by the game's reader, 0x00cb6370): "OHR1;governance=0;...", the
//      value of every rule (the host's file, with its choices). The host keeps
//      the same values. If a player lacks the mod (no hello), no chunk is sent
//      and every PC keeps its own file: the game as without the mod (logged).
//   4. Each PC, when its game starts (NUI::CRunMPGame, slot 7 0x0071d660),
//      takes those values for this game. A single-player start
//      (NUI::CStartSingleAdventureMap, slot 7 0x007e1030) drops them.
//   5. When the map's scripts read scripts/H55-Settings.lua (advmap-startup.lua
//      -> H55-Core.lua, on every PC), our wrap of the game's "open file"
//      (NVFS::CWinVFS slot 7, stacked with the other mods) serves the file with
//      those values: only the numbers change (host_rules_logic.cpp). Logged.
// Saves: the scripts' variables are part of the save (docs/plans/host-rules.md,
// "Saves"), so a loaded game keeps the rules it started with.
//
// The panel (host_rules_panel.cpp): the host chooses in the waiting room; the
// clients see the host's rules read-only; in a game, everyone sees this game's.
//
// Settings, open-homm5.ini [rules]:
//   enabled=1      0 = take no part (no hello, no rules served), as without the DLL
//   key=F2         the panel's key (lobby and adventure map); none = no key
//   button=1       our "Rules..." button in the waiting room (0 = the key only)
//   <rule>=        the host's choices (written by the panel); empty = MMH5.5's file
//   trace=0        1 = log every script file the game opens (research)
#include "host_rules.h"
#include "host_rules_internal.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/patch.h"

namespace mods {
namespace {

using rules::kRuleCount;
using rules::kRules;
using rules::Values;

// --- the game (vanilla 3.1 layout, also MMH55_Utility) -------------------------------------
const DWORD kWinVfsOpenSlot = 0x00e6651c;  // NVFS::CWinVFS vftable 0x00e66500, slot 7: OpenFile
const DWORD kWinVfsOpen = 0x009e0020;      // __thiscall(CWinVFS *, const nstl::string *path), ret 4
const DWORD kMemoryStreamVftable = 0x00e0a980;
const DWORD kMemoryStreamCtor = 0x004034d0;
const DWORD kStreamSetSize = 0x008c7610;
const DWORD kGameNew = 0x00878fd0;

const DWORD kHostUpdateSlot = 0x00e3a5dc;     // CMPUbiComWaitHostLogic slot 0
const DWORD kHostUpdate = 0x00720080;         // __thiscall(logic)
const DWORD kHostRequestSlot = 0x00e3a5f0;    // slot 5: a message from a PC
const DWORD kHostRequest = 0x0071f3d0;        // __thiscall(logic, int sender, message *), ret 8
const DWORD kClientUpdateSlot = 0x00e3a544;   // CMPUbiComWaitClientLogic slot 0
const DWORD kClientUpdate = 0x0071df50;
const DWORD kClientMessageSlot = 0x00e3a558;  // slot 5: a message from the host
const DWORD kClientMessage = 0x0071ef10;      // __thiscall(logic, int sender, message *), ret 8
const DWORD kHostStartCall = 0x007200da;      // in the host logic's update
const DWORD kHostStart = 0x0071feb0;          // __thiscall(logic): builds and sends NMP::CMPStart
const DWORD kStartSerializeSlot = 0x00e3a4e8; // NMP::CMPStart (vftable 0x00e3a4dc) slot 3
const DWORD kStartSerialize = 0x007208a0;     // __thiscall(message, IBinSaver *), ret 4, returns 0
const DWORD kRunMpSlot = 0x00e3a498;          // NUI::CRunMPGame (vftable 0x00e3a47c) slot 7
const DWORD kRunMp = 0x0071d660;              // __thiscall(this): every PC's world starts
const DWORD kRunSingleSlot = 0x00e454dc;      // NUI::CStartSingleAdventureMap (vftable 0x00e454c0) slot 7
const DWORD kRunSingle = 0x007e1030;

const DWORD kRequestVftable = 0x00e39838;      // NUI::CMPWaitChangeRequest: +0xc row, +0x10 type, +0x14 byte
const DWORD kLockCommandVftable = 0x00e397d0;  // NUI::CMPWaitLockButtonsUICommand: +0xc row, +0x10 mask
const DWORD kNewLockCommand = 0x007111d0;      // __fastcall(int row, const DWORD *mask): new command, count 0
const DWORD kReleaseObject = 0x008c61d0;       // __thiscall(object, 0xfffff): when its count reached 0
const DWORD kStartInfoVftable = 0x00e92744;
const DWORD kSlotSize = 0x8c;
const BYTE kChunk = 0x4f;          // our chunk in the start message (the game's are 2..7 inside 3)
const int kChunkSize = 512;        // fixed: a reader copies this many bytes (a new size needs a new chunk number)

const char kSettingsPath[] = "scripts/h55-settings.lua";

struct GameString {
	const char *begin;
	const char *end;
	const char *cap;
};
struct GameWString {
	const wchar_t *begin;
	const wchar_t *end;
	const wchar_t *cap;
};
struct GameStream {  // CMemoryStream (see original_music.cpp)
	void **vftable;
	char *begin;
	char *cap;
	char *end;
	char *pos;
	DWORD flags;  // bit 0: broken
};

typedef GameStream *(__fastcall *OpenFn)(void *vfs, void *edx, const GameString *path);
typedef GameStream *(__fastcall *CtorFn)(GameStream *self, void *edx);
typedef void(__fastcall *SetSizeFn)(GameStream *self, void *edx, int size);
typedef void(__fastcall *DeleteFn)(GameStream *self, void *edx, int flags);
typedef void *(__cdecl *NewFn)(size_t size);
typedef void(__fastcall *LogicFn)(void *logic, void *edx);
typedef int(__fastcall *MessageFn)(void *logic, void *edx, int sender, BYTE *message);
typedef int(__fastcall *SerializeFn)(BYTE *message, void *edx, void *saver);
typedef void(__fastcall *RunFn)(void *self, void *edx);
typedef BYTE *(__fastcall *NewLockCommandFn)(int row, const DWORD *mask);
typedef void(__fastcall *ReleaseFn)(void *object, void *edx, int mask);
typedef void(__fastcall *SendToFn)(void *net, void *edx, int pc, void *message);
typedef char(__fastcall *IsReadingFn)(void *saver, void *edx);
typedef void(__fastcall *DataChunkFn)(void *saver, void *edx, int id, void *data, int size, int number);

OpenFn g_open_next = nullptr;
LogicFn g_host_update_next = nullptr;
MessageFn g_host_request_next = nullptr;
LogicFn g_client_update_next = nullptr;
MessageFn g_client_message_next = nullptr;
LogicFn g_host_start_next = nullptr;
SerializeFn g_serialize_next = nullptr;
RunFn g_run_mp_next = nullptr;
RunFn g_run_single_next = nullptr;

bool g_enabled = true;
bool g_trace = false;
int g_key = VK_F2;
bool g_button = true;

// --- state (under g_lock) --------------------------------------------------------------------
CRITICAL_SECTION g_lock;
struct Lock {
	Lock() { EnterCriticalSection(&g_lock); }
	~Lock() { LeaveCriticalSection(&g_lock); }
};

void *volatile g_vfs = nullptr;
bool g_file_known = false;  // this PC's settings file, as installed
std::string g_file;
Values g_file_values = rules::NoValues();
uint32_t g_hash_rest = 0, g_hash_whole = 0, g_hash_stats = 0;
Values g_chosen = rules::NoValues();  // host: [rules] <key>= (valid values only)

struct Pc {
	int sender;
	BYTE version;  // 0 = no hello yet
	bool has[3];   // rest, whole, stats
	uint32_t hash[3];
	bool told[3];  // the difference was logged
	std::string sent;  // the last lobby text sent to it
	BYTE seq;          // of the last text's pieces
	DWORD hello_at;    // when its last hello came (a client says it every ~4 s)
	std::string name;  // its player's name then (another PC may get its number later)
};
std::vector<Pc> g_pcs;
const void *g_pcs_logic = nullptr;  // the host lobby they belong to
DWORD g_host_seen = 0, g_client_seen = 0;  // the last update of each lobby logic (tick)
bool g_will_apply = false;
std::string g_map_path;   // host: the lobby's map
int g_map_script = -1;    // host: does it have a map script (-1 unknown)
std::vector<std::string> g_missing, g_differ;

bool g_lobby_known = false;  // client: what the host sent in the lobby
Values g_lobby_values = rules::NoValues();
int g_lobby_flags = 0;
rules::LobbyAssembler g_assembler;  // the host's lobby text, piece by piece

std::string g_start_text;  // host: the chunk of its start message ("" = none)
bool g_armed = false;      // this PC's next multiplayer game gets these rules
Values g_armed_values = rules::NoValues();
bool g_serving = false;  // this game's rules (served when the scripts read the file)
Values g_serving_values = rules::NoValues();
bool g_in_game = false;  // a game started since the last lobby
LONG g_served = 0;

// --- small helpers ---------------------------------------------------------------------------
bool Readable(const void *p, size_t n)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (!p || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
		return false;
	return static_cast<const BYTE *>(p) + n <= static_cast<const BYTE *>(mbi.BaseAddress) + mbi.RegionSize;
}

DWORD Read(const void *base, DWORD offset)
{
	const BYTE *p = static_cast<const BYTE *>(base) + offset;
	return Readable(p, 4) ? *reinterpret_cast<const DWORD *>(p) : 0;
}

std::wstring IniPath()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"open-homm5", L".ini", path, MAX_PATH);
	return path;
}

std::string Narrow(const std::wstring &w, UINT cp = CP_UTF8)
{
	const int n = WideCharToMultiByte(cp, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0)
		WideCharToMultiByte(cp, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
	return s;
}

std::string ValueList(const Values &v)
{
	std::string s;
	for (int r = 0; r < kRuleCount; ++r)
		if (!v[r].empty())
			s += std::string(s.empty() ? "" : " ") + kRules[r].key + "=" + v[r];
	return s.empty() ? "(none)" : s;
}

// "scripts\H55-Settings.lua" or "/scripts/h55-settings.lua" -> "scripts/h55-settings.lua"
std::string NormalizePath(const char *b, const char *e)
{
	while (b < e && (*b == '/' || *b == '\\'))
		++b;
	std::string s(b, e);
	for (char &c : s)
		c = c == '\\' ? '/' : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	return s;
}

// --- the files ---------------------------------------------------------------------------------
// A stream with this content instead of the one the game opened (as edge_sky.cpp).
GameStream *WithContent(GameStream *s, const std::string &data)
{
	const SetSizeFn set_size = reinterpret_cast<SetSizeFn>(kStreamSetSize);
	const auto ok = [&data](const GameStream *m) {
		return !(m->flags & 1) && m->begin && static_cast<size_t>(m->end - m->begin) == data.size();
	};
	if (s->vftable == reinterpret_cast<void **>(kMemoryStreamVftable)) {
		set_size(s, nullptr, static_cast<int>(data.size()));
		if (ok(s))
			memcpy(s->begin, data.data(), data.size());
		return s;
	}
	GameStream *m = static_cast<GameStream *>(reinterpret_cast<NewFn>(kGameNew)(sizeof(GameStream)));
	if (!m)
		return s;
	reinterpret_cast<CtorFn>(kMemoryStreamCtor)(m, nullptr);
	set_size(m, nullptr, static_cast<int>(data.size()));
	GameStream *unused = m;
	if (ok(m)) {
		memcpy(m->begin, data.data(), data.size());
		unused = s;
	}
	reinterpret_cast<DeleteFn>(unused->vftable[0])(unused, nullptr, 1);
	return unused == s ? m : s;
}

// A file of the game's data as installed (below our wrap: never with our values).
bool ReadInstalled(const char *path, std::string *out)
{
	void *vfs = g_vfs;
	if (!vfs || !g_open_next)
		return false;
	const size_t n = strlen(path);
	const GameString name = {path, path + n, path + n};
	GameStream *s = g_open_next(vfs, nullptr, &name);
	if (!s)
		return false;
	const bool ok = !(s->flags & 1) && s->begin && s->end > s->begin;
	if (ok)
		out->assign(s->begin, s->end);
	reinterpret_cast<DeleteFn>(s->vftable[0])(s, nullptr, 1);
	return ok;
}

// MMH5.5's stats files, as 55u.dll looks for them next to the exe (its own
// list: UserMODs\MMH55-CustomStats.h5u, data\MMH55-Stats.pak, ...Stats1..9.pak):
// one hash of the names and bytes of those that exist.
uint32_t StatsHash()
{
	wchar_t exe[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	while (n > 0 && exe[n - 1] != L'\\')
		--n;
	exe[n] = 0;
	std::vector<std::wstring> names = {L"..\\UserMODs\\MMH55-CustomStats.h5u", L"..\\data\\MMH55-Stats.pak"};
	for (int i = 1; i <= 9; ++i)
		names.push_back(L"..\\data\\MMH55-Stats" + std::to_wstring(i) + L".pak");
	uint32_t hash = 2166136261u;
	std::string found;
	for (const std::wstring &name : names) {
		HANDLE f = CreateFileW((std::wstring(exe) + name).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		                       OPEN_EXISTING, 0, nullptr);
		if (f == INVALID_HANDLE_VALUE)
			continue;
		const std::string narrow = Narrow(name);
		hash = rules::Fnv1a(narrow.data(), narrow.size(), hash);
		std::vector<char> buf(65536);
		DWORD got = 0;
		while (ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) && got)
			hash = rules::Fnv1a(buf.data(), got, hash);
		CloseHandle(f);
		found += (found.empty() ? "" : ", ") + narrow;
	}
	hooklog::Write("rules: MMH5.5's stats files here: %s (hash %08x)", found.empty() ? "none" : found.c_str(), hash);
	return hash;
}

// This PC's settings file and its hashes, once (from the lobby, when the game's
// file system is up).
void LoadFiles()
{
	{
		Lock lock;
		if (g_file_known)
			return;
	}
	std::string file;
	if (!ReadInstalled(kSettingsPath, &file))
		return;
	const Values values = rules::ParseSettings(file);
	const uint32_t rest = rules::HashWithoutRuleValues(file);
	const uint32_t whole = rules::Fnv1a(file.data(), file.size());
	const uint32_t stats = StatsHash();
	int missing = 0;
	for (int r = 0; r < kRuleCount; ++r)
		missing += values[r].empty() ? 1 : 0;
	hooklog::Write("rules: this PC's scripts/H55-Settings.lua: %u bytes, hash %08x (without the rules' values %08x); %s%s",
	               static_cast<unsigned>(file.size()), whole, rest, ValueList(values).c_str(),
	               missing ? " (some rules are not in the file: MMH5.5's scripts use their own defaults)" : "");
	Lock lock;
	g_file = file;
	g_file_values = values;
	g_hash_rest = rest;
	g_hash_whole = whole;
	g_hash_stats = stats;
	g_file_known = true;
}

// The host's values: its file's, with its choices.
Values Effective()
{
	Values v = g_file_values;
	for (int r = 0; r < kRuleCount; ++r)
		if (!g_chosen[r].empty())
			v[r] = g_chosen[r];
	return v;
}

GameStream *__fastcall OpenFileHook(void *vfs, void *edx, const GameString *path)
{
	g_vfs = vfs;
	GameStream *s = g_open_next(vfs, edx, path);
	if (!path || !path->begin || path->end <= path->begin)
		return s;
	const size_t length = static_cast<size_t>(path->end - path->begin);
	if (length < 8 || length > 300 || _strnicmp(path->end - 4, ".lua", 4) != 0)
		return s;  // (cheap test first: the game opens thousands of files)
	const std::string key = NormalizePath(path->begin, path->end);
	if (g_trace) {
		static LONG traced = 0;
		if (InterlockedIncrement(&traced) <= 200)
			hooklog::Write("rules: the game opens %s: %s", key.c_str(), s && !(s->flags & 1) ? "found" : "not found");
	}
	if (key != kSettingsPath || !s || (s->flags & 1) || !s->begin || s->end < s->begin || s->pos != s->begin)
		return s;
	Values values;
	{
		Lock lock;
		if (!g_serving) {
			hooklog::Write("rules: the scripts read scripts/H55-Settings.lua: this game uses the file's rules");
			return s;
		}
		values = g_serving_values;
	}
	const std::string file(s->begin, s->end);
	std::vector<std::string> changes;
	const std::string served = rules::Substitute(file, values, &changes);
	std::string list;
	for (const std::string &c : changes)
		list += (list.empty() ? "" : ", ") + c;
	if (served != file)
		s = WithContent(s, served);
	const bool ok = !(s->flags & 1) && static_cast<size_t>(s->end - s->begin) == served.size() &&
	                memcmp(s->begin, served.data(), served.size()) == 0;
	if (InterlockedIncrement(&g_served) <= 50)
		hooklog::Write("rules: the scripts read scripts/H55-Settings.lua: served with the host's rules (%s; %s); %u -> %u bytes, "
		               "hash %08x%s",
		               list.empty() ? "no value differs from this PC's file" : list.c_str(), ValueList(values).c_str(),
		               static_cast<unsigned>(file.size()), static_cast<unsigned>(served.size()),
		               rules::Fnv1a(served.data(), served.size()), ok ? "" : " - FAILED: the game got the file as installed");
	return s;
}

// --- the lobby: who is who ------------------------------------------------------------------
int SlotCount(const BYTE *info)
{
	if (!info || Read(info, 0) != kStartInfoVftable)
		return 0;
	const DWORD b = Read(info, 0x10), e = Read(info, 0x14);
	const int n = e > b ? static_cast<int>((e - b) / kSlotSize) : 0;
	return n <= 8 ? n : 0;
}

const BYTE *SlotAt(const BYTE *info, int i)
{
	return reinterpret_cast<const BYTE *>(Read(info, 0x10) + i * kSlotSize);
}

std::string SlotName(const BYTE *slot)
{
	const GameWString *w = reinterpret_cast<const GameWString *>(slot + 0xc);
	if (!Readable(w, sizeof(*w)) || !w->begin || w->end <= w->begin || w->end - w->begin > 64 || !Readable(w->begin, (w->end - w->begin) * 2))
		return std::string();
	return Narrow(std::wstring(w->begin, w->end));
}

std::string PcName(const BYTE *info, int pc)
{
	for (int i = 0; i < SlotCount(info); ++i) {
		const BYTE *s = SlotAt(info, i);
		if (static_cast<int>(Read(s, 0x24)) == pc) {
			const std::string name = SlotName(s);
			if (!name.empty())
				return name;
		}
	}
	char text[24];
	_snprintf_s(text, sizeof(text), _TRUNCATE, "PC %d", pc);
	return text;
}

Pc *FindPc(int sender)
{
	for (Pc &p : g_pcs)
		if (p.sender == sender)
			return &p;
	return nullptr;
}

// A new host lobby: forget the last one's players.
void HostLobby(const void *logic)
{
	if (g_pcs_logic == logic)
		return;
	g_pcs_logic = logic;
	g_pcs.clear();
	g_start_text.clear();
	g_in_game = false;
}

// The human players' PCs other than the host's (slot state 2 = human).
std::vector<int> HumanClients(const void *logic, const BYTE *info)
{
	std::vector<int> pcs;
	const int host_pc = static_cast<int>(Read(logic, 0x34));
	for (int i = 0; i < SlotCount(info); ++i) {
		const BYTE *s = SlotAt(info, i);
		const int pc = static_cast<int>(Read(s, 0x24));
		if (Read(s, 0) == 2 && pc != host_pc && pc != -1)
			pcs.push_back(pc);
	}
	return pcs;
}

// Host, under the lock: who lacks the mod, whose files differ (for the log, the
// panel and the start).
void CheckPlayers(const void *logic, const BYTE *info, bool log)
{
	g_missing.clear();
	g_differ.clear();
	static const char *const kWhat[3] = {"H55-Settings.lua (besides the rules)", "H55-Settings.lua", "MMH5.5's stats files"};
	for (int pc : HumanClients(logic, info)) {
		Pc *p = FindPc(pc);
		const std::string name = PcName(info, pc);
		// a hello of this version, fresh, from this player (not from a PC that left before it joined)
		if (!p || p->version != rules::kProtocol || GetTickCount() - p->hello_at > 15000 || p->name != name) {
			g_missing.push_back(name);
			continue;
		}
		const uint32_t mine[3] = {g_hash_rest, g_hash_whole, g_hash_stats};
		for (int k = 0; k < 3; ++k) {
			if (!g_file_known || !p->has[k] || p->hash[k] == mine[k])
				continue;
			if (k == 1 && p->has[0] && p->hash[0] == mine[0])
				continue;  // only the rules' values differ: the host's replace them
			g_differ.push_back(name + ": " + kWhat[k]);
			if (log && !p->told[k]) {
				p->told[k] = true;
				hooklog::Write("rules: CHECK: %s (PC %d) has another %s than the host (%08x, host %08x): the game may "
				               "not stay in step",
				               name.c_str(), pc, kWhat[k], p->hash[k], mine[k]);
			}
		}
	}
	g_will_apply = g_missing.empty();
}

// --- 1. the hello: clients ------------------------------------------------------------------
void __fastcall ClientUpdateHook(void *logic, void *edx)
{
	{
		Lock lock;
		g_client_seen = GetTickCount();
		g_host_seen = 0;
		static const void *last_logic = nullptr;
		if (g_in_game || logic != last_logic) {  // another lobby: what the last host sent is over
			last_logic = logic;
			g_in_game = false;
			g_lobby_known = false;
			g_assembler = rules::LobbyAssembler();
		}
	}
	static int step = 0;
	static DWORD next_at = 0;
	const DWORD now = GetTickCount();
	if (g_enabled && static_cast<LONG>(now - next_at) >= 0) {
		LoadFiles();
		// A pending request on the first row (as rmg_factions.cpp's hello): +0xc4 type, +0xc8 a byte.
		const DWORD rows = Read(logic, 0x38);
		BYTE *full = rows && Read(logic, 0x3c) > rows ? reinterpret_cast<BYTE *>(Read(reinterpret_cast<void *>(rows), 0)) - 8 : nullptr;
		if (full && Readable(full + 0xc4, 8) && *reinterpret_cast<DWORD *>(full + 0xc4) == 0) {
			uint32_t type = rules::kHelloType;
			uint8_t byte = rules::kProtocol;
			bool known;
			uint32_t rest, whole, stats;
			{
				Lock lock;
				known = g_file_known;
				rest = g_hash_rest, whole = g_hash_whole, stats = g_hash_stats;
			}
			if (step == 1 && known)
				rules::HashRequest(rules::kMarkSettingsRest, rest, &type, &byte);
			else if (step == 2 && known)
				rules::HashRequest(rules::kMarkSettingsWhole, whole, &type, &byte);
			else if (step == 3)
				rules::HashRequest(rules::kMarkStats, stats, &type, &byte);
			if (step == 0 || known) {
				*reinterpret_cast<DWORD *>(full + 0xc4) = type;
				full[0xc8] = byte;
			}
			static int logged = 0;
			if (step == 0 && logged++ < 1)
				hooklog::Write("rules: hello to the host (this PC applies the host's rules, version %u)", rules::kProtocol);
			step = (step + 1) % 4;
			if (step == 0)
				next_at = now + 4000;
		}
	}
	g_client_update_next(logic, edx);
}

// --- 2. the lobby display: clients -------------------------------------------------------------

int __fastcall ClientMessageHook(void *logic, void *edx, int sender, BYTE *message)
{
	if (message && Read(message, 0) == kLockCommandVftable && rules::IsLobbyRow(Read(message, 0xc))) {
		std::string text;
		if (g_assembler.Add(Read(message, 0xc), Read(message, 0x10), &text)) {
			Values values;
			int flags = 0;
			if (rules::ReadLobbyText(text, &flags, &values)) {
				Lock lock;
				const bool changed = !g_lobby_known || values != g_lobby_values || flags != g_lobby_flags;
				g_lobby_known = true;
				g_lobby_values = values;
				g_lobby_flags = flags;
				if (changed)
					hooklog::Write("rules: the host's rules (lobby): %s; %s", ValueList(values).c_str(),
					               (g_lobby_flags & rules::kWillApply) ? "every player has open-homm5's rules: they will be used"
					                                                   : "a player lacks open-homm5's rules: the files' rules will be used");
			}
			else
				hooklog::Write("rules: a lobby message from the host that this PC cannot read (\"%s\"): another version?",
				               text.c_str());
		}
		return 1;  // ours: the game never sees it (it would ignore the row anyway)
	}
	return g_client_message_next(logic, edx, sender, message);
}

// --- 1 and 2: the host ---------------------------------------------------------------------------
int __fastcall HostRequestHook(void *logic, void *edx, int sender, BYTE *message)
{
	if (message && Read(message, 0) == kRequestVftable) {
		const uint32_t type = Read(message, 0x10);
		const uint8_t byte = message[0x14];
		char mark = 0;
		uint32_t hash = 0;
		Lock lock;
		HostLobby(logic);
		if (type == rules::kHelloType || rules::ReadHashRequest(type, byte, &mark, &hash)) {
			Pc *p = FindPc(sender);
			if (!p) {
				g_pcs.push_back(Pc{sender, 0, {}, {}, {}, std::string(), 0, 0, std::string()});
				p = &g_pcs.back();
			}
			if (type == rules::kHelloType) {
				if (p->version != byte)
					hooklog::Write("rules: hello from PC %d (version %u%s)", sender, byte,
					               byte == rules::kProtocol ? "" : ", not this PC's: the files' rules unless it updates");
				p->version = byte;
				p->hello_at = GetTickCount();
				p->name = PcName(reinterpret_cast<const BYTE *>(Read(logic, 0x74)), sender);
			}
			else {
				const int k = mark == rules::kMarkSettingsRest ? 0 : mark == rules::kMarkSettingsWhole ? 1 : 2;
				if (!p->has[k] || p->hash[k] != hash)
					p->told[k] = false;
				p->has[k] = true;
				p->hash[k] = hash;
			}
		}
	}
	return g_host_request_next(logic, edx, sender, message);  // the game ignores these types
}

// Sends a client our lobby text, in pieces: "lock buttons" commands for rows
// that do not exist (a client without the mod ignores them).
void SendLobbyText(void *logic, int pc, const std::string &text, BYTE seq)
{
	void *net = reinterpret_cast<void *>(Read(logic, 0x10));
	if (!net || !Readable(net, 4))
		return;
	const SendToFn send = reinterpret_cast<SendToFn>(Read(reinterpret_cast<void *>(Read(net, 0)), 0x20));
	for (const auto &piece : rules::LobbyPieces(text, seq)) {
		const DWORD mask = piece.second;
		BYTE *command = reinterpret_cast<NewLockCommandFn>(kNewLockCommand)(static_cast<int>(piece.first), &mask);
		if (!command)
			return;
		LONG *count = reinterpret_cast<LONG *>(command + 4);
		++*count;
		send(net, nullptr, pc, command);
		if ((--*count & 0xfffff) == 0)
			reinterpret_cast<ReleaseFn>(kReleaseObject)(command, nullptr, 0xfffff);
	}
}

// The lobby's map (the host logic's map id, +0x80, its path at +0x14): does it
// name a map script? In a multiplayer game MMH5.5's scripts run only on maps
// that do (a random map straight from the generator does not). Read once per map.
void CheckMap(void *logic)
{
	const GameString *p = reinterpret_cast<const GameString *>(static_cast<BYTE *>(logic) + 0x80 + 0x14);
	std::string path;
	if (Readable(p, sizeof(*p)) && p->begin && p->end > p->begin && p->end - p->begin < 400 && Readable(p->begin, p->end - p->begin))
		path.assign(p->begin, p->end);
	{
		Lock lock;
		if (path == g_map_path)
			return;
	}
	std::string file = path.substr(0, path.find('#'));
	while (!file.empty() && (file[0] == '/' || file[0] == '\\'))
		file.erase(0, 1);
	std::string xdb;
	const int script = file.empty() || !ReadInstalled(file.c_str(), &xdb) ? -1 : rules::HasMapScript(xdb) ? 1 : 0;
	Lock lock;
	g_map_path = path;
	g_map_script = script;
	if (!path.empty())
		hooklog::Write(script == 1   ? "rules: the lobby's map %s has a map script: MMH5.5's scripts run on it"
		               : script == 0 ? "rules: the lobby's map %s has NO map script: in a multiplayer game MMH5.5's scripts do not "
		                               "run on it, so these rules change nothing there"
		                             : "rules: the lobby's map %s could not be read",
		               path.c_str());
}

void __fastcall HostUpdateHook(void *logic, void *edx)
{
	const DWORD now = GetTickCount();
	static DWORD last = 0;
	if (g_enabled && now - last >= 500) {
		last = now;
		LoadFiles();
		CheckMap(logic);
		const BYTE *info = reinterpret_cast<const BYTE *>(Read(logic, 0x74));
		struct Send {
			int pc;
			std::string text;
			BYTE seq;
		};
		std::vector<Send> sends;
		{
			Lock lock;
			HostLobby(logic);
			g_host_seen = now;
			g_client_seen = 0;
			if (g_in_game)
				g_in_game = false;
			CheckPlayers(logic, info, true);
			const Values values = Effective();
			for (Pc &p : g_pcs) {
				if (p.version != rules::kProtocol)
					continue;
				int flags = g_will_apply ? rules::kWillApply : 0;
				if (g_map_script == 0)
					flags |= rules::kNoScript;
				const uint32_t mine[3] = {g_hash_rest, g_hash_whole, g_hash_stats};
				if (g_file_known && ((p.has[0] && p.hash[0] != mine[0]) || (p.has[2] && p.hash[2] != mine[2])))
					flags |= rules::kDiffers;
				const std::string text = rules::LobbyText(flags, values);
				if (text != p.sent) {
					p.sent = text;
					sends.push_back({p.sender, text, ++p.seq});
				}
			}
		}
		for (const Send &s : sends) {
			SendLobbyText(logic, s.pc, s.text, s.seq);
			hooklog::Write("rules: the host's rules sent to PC %d for its lobby: %s", s.pc, s.text.c_str());
		}
	}
	else if (g_enabled) {
		Lock lock;
		g_host_seen = now;
	}
	g_host_update_next(logic, edx);
}

// --- 3. the start ------------------------------------------------------------------------------
void __fastcall HostStartHook(void *logic, void *edx)
{
	if (g_enabled) {
		LoadFiles();
		const BYTE *info = reinterpret_cast<const BYTE *>(Read(logic, 0x74));
		Lock lock;
		HostLobby(logic);
		CheckPlayers(logic, info, true);
		const Values values = Effective();
		if (!g_will_apply) {
			std::string names;
			for (const std::string &n : g_missing)
				names += (names.empty() ? "" : ", ") + n;
			g_start_text.clear();
			g_armed = false;
			hooklog::Write("rules: START: %s without open-homm5's rules: every PC uses its own H55-Settings.lua (the "
			               "host's choices are not used in this game)",
			               names.c_str());
		}
		else {
			g_start_text = rules::Encode(values);
			g_armed = true;
			g_armed_values = values;
			std::string differ;
			for (const std::string &d : g_differ)
				differ += "; files differ: " + d;
			hooklog::Write("rules: START: every player has open-homm5's rules: this game uses %s (the host's choices: %s)%s",
			               ValueList(values).c_str(), ValueList(g_chosen).c_str(), differ.c_str());
		}
	}
	g_host_start_next(logic, edx);
}

// The start message's own fields, then ours (a chunk the game's reader skips).
int __fastcall StartSerializeHook(BYTE *message, void *edx, void *saver)
{
	const int r = g_serialize_next(message, edx, saver);
	if (!g_enabled || !saver || !Readable(saver, 4))
		return r;
	void **vt = *reinterpret_cast<void ***>(saver);
	const bool reading = reinterpret_cast<IsReadingFn>(vt[15])(saver, nullptr) != 0;
	const DataChunkFn chunk = reinterpret_cast<DataChunkFn>(vt[10]);
	char buf[kChunkSize];
	memset(buf, 0, sizeof(buf));
	if (!reading) {
		std::string text;
		{
			Lock lock;
			text = g_start_text;
		}
		if (text.empty() || text.size() >= sizeof(buf))
			return r;
		memcpy(buf, text.c_str(), text.size());
		chunk(saver, nullptr, kChunk, buf, kChunkSize, 1);
		static LONG logged = 0;
		if (InterlockedIncrement(&logged) <= 10)
			hooklog::Write("rules: the start message carries the host's rules (%s)", text.c_str());
		return r;
	}
	chunk(saver, nullptr, kChunk, buf, kChunkSize, 1);  // all zeros when the host sent none
	buf[kChunkSize - 1] = 0;
	Values values;
	Lock lock;
	if (buf[0] && rules::Decode(buf, &values)) {
		g_armed = true;
		g_armed_values = values;
		hooklog::Write("rules: the host's start message: this game uses %s", ValueList(values).c_str());
	}
	else {
		g_armed = false;
		hooklog::Write(buf[0] ? "rules: the host's start message has rules this PC cannot read (\"%.60s\"): the file's rules"
		                      : "rules: the host's start message has no rules%.0s: every PC uses its own H55-Settings.lua",
		               buf);
	}
	return r;
}

// --- 4. a game starts -------------------------------------------------------------------------
void __fastcall RunMpHook(void *self, void *edx)
{
	{
		Lock lock;
		g_serving = g_armed;
		g_serving_values = g_armed ? g_armed_values : rules::NoValues();
		g_armed = false;
		g_in_game = true;
		g_served = 0;
		hooklog::Write("rules: a multiplayer game starts: %s",
		               g_serving ? ("the host's rules, " + ValueList(g_serving_values)).c_str() : "the file's rules");
	}
	g_run_mp_next(self, edx);
}

void __fastcall RunSingleHook(void *self, void *edx)
{
	{
		Lock lock;
		if (g_serving || g_armed)
			hooklog::Write("rules: a single-player game starts: the file's rules");
		g_serving = false;
		g_armed = false;
		g_start_text.clear();
		g_in_game = true;
	}
	g_run_single_next(self, edx);
}

void ReadChoices()
{
	const std::wstring ini = IniPath();
	Values chosen = rules::NoValues();
	for (int r = 0; r < kRuleCount; ++r) {
		wchar_t text[32] = L"";
		const std::wstring key(kRules[r].key, kRules[r].key + strlen(kRules[r].key));
		GetPrivateProfileStringW(L"rules", key.c_str(), L"", text, 32, ini.c_str());
		const std::string value = Narrow(text);
		if (value.empty())
			continue;
		if (rules::ValidValue(kRules[r], value))
			chosen[r] = value;
		else
			hooklog::Write("rules: [rules] %s=%s is not a value it can take: the file's", kRules[r].key, value.c_str());
	}
	Lock lock;
	g_chosen = chosen;
}

}  // namespace

// --- the panel's side -----------------------------------------------------------------------------
namespace rules {

View::Role LobbyRole()
{
	const DWORD now = GetTickCount();
	Lock lock;
	if (g_host_seen && now - g_host_seen < 1500)
		return View::kHost;
	if (g_client_seen && now - g_client_seen < 1500)
		return View::kClient;
	return g_in_game ? View::kInGame : View::kNoGame;
}

View GetView(View::Role role)
{
	if (role != View::kInGame)
		LoadFiles();
	View v;
	Lock lock;
	v.role = role;
	v.file_known = g_file_known;
	v.file = g_file_values;
	v.chosen = g_chosen;
	if (role == View::kHost || role == View::kSetup || role == View::kNoGame) {
		v.shown_known = true;
		v.shown = role == View::kNoGame ? g_file_values : Effective();
		if (role == View::kHost) {
			v.will_apply = g_will_apply;
			v.map_script = g_map_script;
			v.missing = g_missing;
			v.differ = g_differ;
		}
	}
	else if (role == View::kClient) {
		v.shown_known = g_lobby_known;
		v.shown = g_lobby_values;
		v.will_apply = g_lobby_known && (g_lobby_flags & rules::kWillApply);
		v.this_differs = g_lobby_known && (g_lobby_flags & rules::kDiffers);
		v.map_script = !g_lobby_known ? -1 : (g_lobby_flags & rules::kNoScript) ? 0 : 1;
	}
	else if (role == View::kInGame) {
		v.in_game_rules = g_serving;
		v.shown_known = true;
		v.shown = g_serving ? g_serving_values : g_file_values;
	}
	return v;
}

void SetChoices(const Values &chosen)
{
	Values clean = NoValues();
	std::string list;
	{
		Lock lock;
		for (int r = 0; r < kRuleCount; ++r)
			if (static_cast<size_t>(r) < chosen.size() && ValidValue(kRules[r], chosen[r]) &&
			    !(g_file_known && chosen[r] == g_file_values[r]))
				clean[r] = chosen[r];
		g_chosen = clean;
		list = ValueList(clean);
	}
	const std::wstring ini = IniPath();
	for (int r = 0; r < kRuleCount; ++r) {
		const std::wstring key(kRules[r].key, kRules[r].key + strlen(kRules[r].key));
		const std::wstring value(clean[r].begin(), clean[r].end());
		WritePrivateProfileStringW(L"rules", key.c_str(), clean[r].empty() ? nullptr : value.c_str(), ini.c_str());
	}
	hooklog::Write("rules: the host's choices: %s (saved in open-homm5.ini [rules])", list.c_str());
}

int PanelKey()
{
	return g_key;
}

bool PanelButton()
{
	return g_button;
}

}  // namespace rules

void InstallHostRulesEarly()
{
	// The panel is an overlay client: the Direct3D import must go in before the
	// game makes its device (as the other panels do).
	if (GetPrivateProfileIntW(L"rules", L"enabled", 1, IniPath().c_str()) != 0)
		overlay::InstallEarly();
}

void ApplyHostRulesFromSettings()
{
	InitializeCriticalSection(&g_lock);
	const std::wstring ini = IniPath();
	g_enabled = GetPrivateProfileIntW(L"rules", L"enabled", 1, ini.c_str()) != 0;
	g_trace = GetPrivateProfileIntW(L"rules", L"trace", 0, ini.c_str()) != 0;
	g_button = GetPrivateProfileIntW(L"rules", L"button", 1, ini.c_str()) != 0;
	wchar_t key[16];
	GetPrivateProfileStringW(L"rules", L"key", L"F2", key, 16, ini.c_str());
	g_key = 0;
	if ((key[0] == L'F' || key[0] == L'f') && _wtoi(key + 1) >= 1 && _wtoi(key + 1) <= 12)
		g_key = VK_F1 + _wtoi(key + 1) - 1;
	if (!g_enabled) {
		hooklog::Write("rules: off (enabled=0): no hello, every game uses this PC's H55-Settings.lua");
		return;
	}
	ReadChoices();
	g_open_next = reinterpret_cast<OpenFn>(
		hookpatch::PatchPointer(kWinVfsOpenSlot, kWinVfsOpen, reinterpret_cast<const void *>(&OpenFileHook)));
	g_host_update_next = reinterpret_cast<LogicFn>(hookpatch::PatchPointer(kHostUpdateSlot, kHostUpdate, &HostUpdateHook));
	g_host_request_next = reinterpret_cast<MessageFn>(hookpatch::PatchPointer(kHostRequestSlot, kHostRequest, &HostRequestHook));
	g_client_update_next = reinterpret_cast<LogicFn>(hookpatch::PatchPointer(kClientUpdateSlot, kClientUpdate, &ClientUpdateHook));
	g_client_message_next =
		reinterpret_cast<MessageFn>(hookpatch::PatchPointer(kClientMessageSlot, kClientMessage, &ClientMessageHook));
	g_host_start_next = reinterpret_cast<LogicFn>(hookpatch::RedirectCall(kHostStartCall, kHostStart, &HostStartHook));
	g_serialize_next = reinterpret_cast<SerializeFn>(hookpatch::PatchPointer(kStartSerializeSlot, kStartSerialize, &StartSerializeHook));
	g_run_mp_next = reinterpret_cast<RunFn>(hookpatch::PatchPointer(kRunMpSlot, kRunMp, &RunMpHook));
	g_run_single_next = reinterpret_cast<RunFn>(hookpatch::PatchPointer(kRunSingleSlot, kRunSingle, &RunSingleHook));
	const bool all = g_open_next && g_host_update_next && g_host_request_next && g_client_update_next && g_client_message_next &&
	                 g_host_start_next && g_serialize_next && g_run_mp_next && g_run_single_next;
	if (!all) {
		// Whatever went in stays harmless: with g_enabled off the hooks only pass through.
		g_enabled = false;
		hooklog::Write("rules: the game's code is not as expected (%d%d%d%d%d%d%d%d%d): off, every game uses this PC's "
		               "H55-Settings.lua",
		               !!g_open_next, !!g_host_update_next, !!g_host_request_next, !!g_client_update_next,
		               !!g_client_message_next, !!g_host_start_next, !!g_serialize_next, !!g_run_mp_next, !!g_run_single_next);
		return;
	}
	const bool panel = rules::InstallPanel();
	std::string chosen;
	{
		Lock lock;
		chosen = ValueList(g_chosen);
	}
	hooklog::Write("rules: on: the host's rules for every player (version %u); this PC's choices as host: %s; panel %s",
	               rules::kProtocol, chosen.c_str(), panel ? "on" : "off (no overlay)");
}

}  // namespace mods

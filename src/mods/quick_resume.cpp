// Mod: quick resume after a crash ([coop] resume_save, the host's setting, on by
// default; docs/plans/turn-features.md, part 2).
//
// In plain words: when a friend's game crashes (or loses its connection), the host's
// game drops that player after ~20 s and plays on without them: they are defeated, and
// a save made after that cannot bring them back. With this mod the host asks every PC
// to save the game at that moment, just before the defeat, under the name "Reprise"
// ("Resume"). A banner on the adventure map says so. To play on together: everyone
// (the friend too, once their game is back) goes to Multiplayer, LAN, Create, Load,
// "Reprise", and the host invites them as usual.
//
// What the game does (robot runs 20261104-080739-lan and -091110-lan, a client copy
// killed in the middle of a day):
//   - the host's lockstep waits for the silent PC, then its message pump gets "this PC
//     left" (message type 1) and calls the transceiver's drop (0x007c8d10, from
//     0x007ca916): the PC's bit leaves the active mask (+0x58), its reported segment
//     becomes -1, "Drop player N" goes to the game's console;
//   - right after, the host sends NWorld::CDefeatPlayerCmd (vtable 0x00e0e75c; +0x10
//     the player) through the transceiver (slot 0, 0x007c9810): every PC runs it in a
//     segment, and the player is out ("N" in GiveTurnToNextPlayers from then on).
// What the mod does, on the host only: the first defeat it sends within 60 s of a
// drop, it sends a save first: NMP::CMPSaveWorldCmd (made by 0x008236d0 with a new
// GUID and our name, as the autosave request does), through the same transceiver. The
// commands of a segment run in the order they were sent, on every PC, so every PC
// saves the world with the player still in it, then runs the defeat as before. A save
// never changes the game world (save.md), so the PCs stay in step.
// Every PC whose save command has our name shows the banner (the save command's
// Execute, NMP::CMPSaveWorldCmd slot 7, stacked on the autosave mod's hook).
//
// Robot step: none needed (kill a copy: tools/robot/README.md, "Checking quick resume").
#include "quick_resume.h"

#include <windows.h>
#include <math.h>
#include <string.h>

#include <string>

#include "rmg_builder_widgets.h"

#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/patch.h"
#include "../hook/texts.h"
#include "../hook/texts_logic.h"
#include "../hook/third_party/imgui/imgui.h"

namespace mods {
namespace {

namespace ui = builder_ui;

const char kName[] = "quick_resume";
const char kAdventureScreen[] = ".?AVCAdventureScreen@NUI@@";

// --- the game (vanilla 3.1 addresses, the same in MMH55_Utility) -----------------------------
const DWORD kDropCall = 0x007ca916;        // in the message pump 0x007ca7c0: call 0x007c8d10 (a PC left)
const DWORD kDrop = 0x007c8d10;            // __thiscall(transceiver, int pc), ret 4
const DWORD kSendSlot = 0x00e43f1c;        // NWorld::CMultiPlayerTransceiver slot 0: send a command
const DWORD kSend = 0x007c9810;            // __thiscall(transceiver, command), ret 4
const DWORD kMakeSave = 0x008236d0;        // __fastcall(const wstring *name, bool): a new NMP::CMPSaveWorldCmd
const DWORD kExecuteSlot = 0x00e487bc;     // NMP::CMPSaveWorldCmd (vtable 0x00e487a0), slot 7
const DWORD kExecute = 0x008256a0;         // __thiscall(), returns al = 1
const DWORD kDefeatVftable = 0x00e0e75c;   // NWorld::CDefeatPlayerCmd: +0x10 the player (IPlayer *)
const DWORD kMe = 0x54;                    // the transceiver: this PC's index (the host is 0)
const DWORD kPlayerNumber = 0x2c;          // IPlayer slot: its number (0x00720e90)
const DWORD kPlayerInGame = 0x6c;          // IPlayer slot: 1 while the player is in the game (players.md; turn_undo.cpp)
const DWORD kSaveName = 0x1c;              // CMPSaveWorldCmd: its name (wide string: begin, end)
const DWORD kDropWindowMs = 60000;         // a defeat this long after a drop is the drop's
const DWORD kBannerMs = 30000;

struct Code {
	DWORD address;
	BYTE bytes[10];
	int size;
};
const Code kCode[] = {
	{kDrop, {0x51, 0x8b, 0xd1, 0x8b, 0x4c, 0x24, 0x08, 0x85, 0xc9, 0x0f}, 10},
	{kSend, {0x55, 0x8b, 0x6c, 0x24, 0x08, 0x85, 0xed, 0x56, 0x8b, 0xf1}, 10},
	{kMakeSave, {0x83, 0xec, 0x10, 0x56, 0x57, 0x8d, 0x44, 0x24, 0x08, 0x50}, 10},
	{0x00720e90, {0x8b, 0x41, 0x78, 0xc3}, 4},  // IPlayer slot 0x2c: the number at +0x78
};

struct WString {  // the game's wide string: begin, end, end of the allocation
	const wchar_t *begin, *end, *cap;
};

typedef void(__fastcall *DropFn)(void *transceiver, void *edx, int pc);
typedef void(__fastcall *SendFn)(void *transceiver, void *edx, void *command);
typedef void *(__fastcall *MakeSaveFn)(const WString *name, int flag);
typedef bool(__fastcall *ExecuteFn)(void *command, void *edx);
typedef int(__fastcall *GetFn)(void *self, void *edx);

DropFn g_drop_next = nullptr;
SendFn g_send_next = nullptr;
ExecuteFn g_execute_next = nullptr;

bool g_on = false;           // the host's setting (read at start-up)
DWORD g_drop_at = 0;         // GetTickCount() of the last drop this PC saw (0: none pending)
int g_drop_pc = -1;
std::wstring g_save_name;    // "Reprise", in the add-on's language
const wchar_t kEnglishName[] = L"Resume";  // also ours on a PC whose host plays in English
DWORD g_banner_until = 0;
std::string g_banner;
// Files: this PC's saves folder (learnt from the game's own searches of it, FindFirstFileA
// "...\Saves\*"), and the file of the last resume save this PC wrote (its CreateFileA
// while our save command runs).
char g_saves_folder[MAX_PATH] = "";
// The finished resume save's file (offered to the others: resume_transfer.cpp, on the virtual
// LAN's thread), and the one the game is writing now. The owner's game, 2026-11-05: the file
// was offered while the game still wrote it (311073 of 6806510 bytes, 160 ms before the save
// was done); the friend fetched that cut-off file and could not take his seat in the loaded
// game. So a file is offered only once the save command has returned, never while one runs.
char g_resume_file[MAX_PATH] = "";
char g_writing_file[MAX_PATH] = "";
SRWLOCK g_file_lock = SRWLOCK_INIT;
volatile LONG g_writing = 0;
volatile bool g_in_resume_save = false;
typedef HANDLE(WINAPI *FindFirstFileAFn)(LPCSTR, LPWIN32_FIND_DATAA);
typedef HANDLE(WINAPI *CreateFileAFn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
FindFirstFileAFn g_find_first_next = nullptr;
CreateFileAFn g_create_file_next = nullptr;

// The first "\Saves\" in a path, in any case, or nullptr.
const char *FindSaves(const char *path)
{
	static const char kPart[] = "\\saves\\";
	for (const char *p = path; *p; ++p)
		if (_strnicmp(p, kPart, sizeof(kPart) - 1) == 0)
			return p;
	return nullptr;
}

HANDLE WINAPI FindFirstFileAHook(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
	if (pattern) {
		const char *saves = FindSaves(pattern);
		if (saves) {
			char folder[MAX_PATH];
			const size_t n = static_cast<size_t>(saves - pattern) + 7;
			if (n < sizeof(folder)) {
				memcpy(folder, pattern, n);
				folder[n] = 0;
				if (_stricmp(folder, g_saves_folder) != 0) {
					lstrcpynA(g_saves_folder, folder, sizeof(g_saves_folder));
					hooklog::Write("quick resume: this PC's saves folder is known now (the game searched it)");
				}
			}
		}
	}
	return g_find_first_next(pattern, data);
}

HANDLE WINAPI CreateFileAHook(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD how, DWORD flags,
                              HANDLE templ)
{
	// The profile's settings, read at start-up and when the player changes profiles
	// ("...\Profiles\<name>\user_a2.cfg", robot run 20261104-095411): its folder + "Saves\".
	static const char kProfileFile[] = "\\user_a2.cfg";
	const size_t len = path ? strlen(path) : 0;
	if (len > sizeof(kProfileFile) && len < MAX_PATH - 8 && _stricmp(path + len - (sizeof(kProfileFile) - 1), kProfileFile) == 0) {
		char folder[MAX_PATH];
		const size_t dir = len - (sizeof(kProfileFile) - 1) + 1;  // with its last backslash
		memcpy(folder, path, dir);
		lstrcpynA(folder + dir, "Saves\\", static_cast<int>(sizeof(folder) - dir));
		if (_stricmp(folder, g_saves_folder) != 0) {
			lstrcpynA(g_saves_folder, folder, sizeof(g_saves_folder));
			hooklog::Write("quick resume: this PC's saves folder is known (its profile's settings were read)");
		}
	}
	if (g_in_resume_save && path && (access & GENERIC_WRITE)) {
		const size_t n = strlen(path);
		if (n > 4 && _stricmp(path + n - 4, ".sav") == 0) {
			lstrcpynA(g_writing_file, path, sizeof(g_writing_file));
			const char *name = strrchr(path, '\\');
			hooklog::Write("quick resume: the resume save's file: %s (offered once the save is done)", name ? name + 1 : path);
		}
	}
	return g_create_file_next(path, access, share, sa, how, flags, templ);
}

DWORD Read(const void *p, DWORD off)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off);
}

std::wstring Wide(const char *utf8)
{
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	std::wstring out(n > 0 ? n - 1 : 0, L'\0');
	if (n > 1)
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
	return out;
}

void __fastcall DropHook(void *transceiver, void *edx, int pc)
{
	g_drop_next(transceiver, edx, pc);
	g_drop_at = GetTickCount();
	if (!g_drop_at)
		g_drop_at = 1;
	g_drop_pc = pc;
	const int me = static_cast<int>(Read(transceiver, kMe));
	hooklog::Write("quick resume: PC %d left the game (this PC is %d)%s", pc, me,
	               me != 0 ? "" : g_on ? ": the host saves before the game takes its player out" : ": no save ([coop] resume_save=0)");
}

int PlayerSlot(void *player, DWORD slot, int fallback)
{
	__try {
		return reinterpret_cast<GetFn>(Read(reinterpret_cast<void *>(Read(player, 0)), slot))(player, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return fallback;
	}
}

int PlayerNumber(void *player)
{
	return PlayerSlot(player, kPlayerNumber, 0);
}

void __fastcall SendHook(void *transceiver, void *edx, void *command)
{
	if (g_on && g_drop_at && command && Read(command, 0) == kDefeatVftable && Read(transceiver, kMe) == 0) {
		const DWORD since = GetTickCount() - g_drop_at;
		g_drop_at = 0;  // (one save per drop)
		void *player = reinterpret_cast<void *>(Read(command, 0x10));
		const int number = player ? PlayerNumber(player) : 0;
		// A player who had already lost the game (the owner's game, 2026-11-05: a friend's last
		// hero fell, the day's end took him out, then his PC left): nothing to resume, and the
		// save would replace the last useful "Reprise".
		const bool lost = player && PlayerSlot(player, kPlayerInGame, 1) != 1;
		if (lost)
			hooklog::Write("quick resume: PC %d's player %d had already lost the game: no resume save", g_drop_pc, number);
		if (since < kDropWindowMs && !lost) {
			const WString name = {g_save_name.c_str(), g_save_name.c_str() + g_save_name.size(),
			                      g_save_name.c_str() + g_save_name.size()};
			void *save = reinterpret_cast<MakeSaveFn>(kMakeSave)(&name, 1);
			if (save) {
				g_send_next(transceiver, edx, save);
				hooklog::Write("quick resume: the resume save sent to every PC %lu ms after PC %d left, just before the defeat of "
				               "player %d",
				               since, g_drop_pc, number);
			}
			else
				hooklog::Write("quick resume: the game made no save command: no save");
		}
	}
	g_send_next(transceiver, edx, command);
}

bool Same(const wchar_t *begin, const wchar_t *end, const wchar_t *name, size_t size)
{
	return begin && end - begin == static_cast<ptrdiff_t>(size) && wmemcmp(begin, name, size) == 0;
}

// Is this save command ours ("Reprise" in this PC's language, or "Resume")?
bool Ours(const void *command)
{
	__try {
		const wchar_t *begin = reinterpret_cast<const wchar_t *>(Read(command, kSaveName));
		const wchar_t *end = reinterpret_cast<const wchar_t *>(Read(command, kSaveName + 4));
		return Same(begin, end, g_save_name.c_str(), g_save_name.size()) ||
		       Same(begin, end, kEnglishName, wcslen(kEnglishName));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool __fastcall ExecuteHook(void *command, void *edx)
{
	const bool ours = !g_save_name.empty() && Ours(command);
	if (ours) {
		g_writing_file[0] = 0;
		InterlockedExchange(&g_writing, 1);  // (no offer while the game writes: it may rewrite the same file)
	}
	g_in_resume_save = ours;
	const bool ok = g_execute_next(command, edx);
	g_in_resume_save = false;
	if (ours && g_writing_file[0]) {  // the save is done: the file the others may need, kept for later (and a restart)
		AcquireSRWLockExclusive(&g_file_lock);
		lstrcpynA(g_resume_file, g_writing_file, sizeof(g_resume_file));
		ReleaseSRWLockExclusive(&g_file_lock);
		wchar_t state[MAX_PATH];
		hooklog::PathNextToExe(L"h5-together-resume", L".txt", state, MAX_PATH);
		HANDLE f = CreateFileW(state, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (f != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			WriteFile(f, g_writing_file, static_cast<DWORD>(strlen(g_writing_file)), &written, nullptr);
			CloseHandle(f);
		}
	}
	if (ours)
		InterlockedExchange(&g_writing, 0);
	if (ours) {
		g_banner = texts::Get("resume.banner");
		g_banner_until = GetTickCount() + kBannerMs;
		hooklog::Write("quick resume: this PC made the resume save%s; the banner for %lu s", ok ? "" : " (the game said it failed)",
		               kBannerMs / 1000);
	}
	return ok;
}

// --- the banner: at the top of the adventure map, in the look of the online UI's -------------
const float kRefH = 768.0f, kScreenW = 1024.0f, kBannerY = 64.0f, kBannerW = 940.0f, kBannerFont = 15.0f;

bool Wanted()
{
	if (!g_banner_until || GetTickCount() >= g_banner_until)
		return false;
	char top[96] = "";
	overlay::TopScreen(top, sizeof(top));
	return strcmp(top, kAdventureScreen) == 0;
}

void Prepare(const overlay::Frame &frame)
{
	overlay::WantFont(floorf(kBannerFont * frame.height / kRefH + 0.5f));
}

void Draw(const overlay::Frame &frame)
{
	const float k = frame.height / kRefH;
	const float px = floorf(kBannerFont * k + 0.5f);
	ImFont *font = overlay::Font(px);
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	const std::string text = ui::Fit(font, px, g_banner.c_str(), (kBannerW - 24.0f) * k);
	const float w = ui::TextWidth(font, px, text.c_str());
	const float pad = 12.0f * k;
	const float x0 = floorf(frame.width / 2 - w / 2 - pad), y0 = floorf(kBannerY * k);
	const ImVec2 b0(x0, y0), b1(floorf(frame.width / 2 + w / 2 + pad), y0 + px + 14.0f * k);
	draw->AddRectFilled(b0, b1, IM_COL32(20, 14, 8, 215), 4.0f * k);
	draw->AddRect(b0, b1, ui::kEdge, 4.0f * k, 0, 1.0f * k);
	ui::ShadowText(draw, font, px, ImVec2(b0.x + pad, floorf(y0 + 7.0f * k)), ui::kGold, text.c_str());
}

bool CodeAsExpected()
{
	for (const Code &c : kCode)
		if (memcmp(reinterpret_cast<const void *>(c.address), c.bytes, c.size) != 0) {
			hooklog::Write("quick resume: the game's code at %08lx is not as expected: off", c.address);
			return false;
		}
	return true;
}

}  // namespace

bool ResumeOfferFile(char *out, size_t size)
{
	if (g_writing)
		return false;  // a resume save is being written: nothing to offer until it is done
	bool known = false;
	AcquireSRWLockShared(&g_file_lock);
	if (g_resume_file[0]) {
		lstrcpynA(out, g_resume_file, static_cast<int>(size));
		known = true;
	}
	ReleaseSRWLockShared(&g_file_lock);
	if (known)
		return true;
	wchar_t state[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together-resume", L".txt", state, MAX_PATH);
	HANDLE f = CreateFileW(state, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE)
		return false;
	char path[MAX_PATH] = "";
	DWORD got = 0;
	const bool ok = ReadFile(f, path, sizeof(path) - 1, &got, nullptr) && got > 0;
	CloseHandle(f);
	if (!ok)
		return false;
	path[got] = 0;
	AcquireSRWLockExclusive(&g_file_lock);
	if (!g_resume_file[0])
		lstrcpynA(g_resume_file, path, sizeof(g_resume_file));
	ReleaseSRWLockExclusive(&g_file_lock);
	lstrcpynA(out, path, static_cast<int>(size));
	return true;
}

bool ResumeSavesFolder(char *out, size_t size)
{
	if (!g_saves_folder[0])
		return false;
	lstrcpynA(out, g_saves_folder, static_cast<int>(size));
	return true;
}

void ApplyQuickResumeFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	g_on = GetPrivateProfileIntW(L"coop", L"resume_save", 1, path) != 0;
	if (!CodeAsExpected())
		return;
	g_save_name = Wide(texts::Get("resume.save_name"));
	g_drop_next = reinterpret_cast<DropFn>(hookpatch::RedirectCall(kDropCall, kDrop, &DropHook));
	g_send_next = reinterpret_cast<SendFn>(hookpatch::PatchPointer(kSendSlot, kSend, &SendHook));
	g_execute_next = reinterpret_cast<ExecuteFn>(hookpatch::PatchPointer(kExecuteSlot, kExecute, &ExecuteHook));
	if (!g_drop_next || !g_send_next || !g_execute_next) {
		hooklog::Write("quick resume: part of the game's code is not as expected (drop %d, send %d, save %d): off",
		               g_drop_next != nullptr, g_send_next != nullptr, g_execute_next != nullptr);
		g_on = false;
		return;
	}
	g_find_first_next = reinterpret_cast<FindFirstFileAFn>(
		hookpatch::PatchImport("KERNEL32.dll", "FindFirstFileA", reinterpret_cast<void *>(&FindFirstFileAHook)));
	g_create_file_next = reinterpret_cast<CreateFileAFn>(
		hookpatch::PatchImport("KERNEL32.dll", "CreateFileA", reinterpret_cast<void *>(&CreateFileAHook)));
	if (!g_find_first_next || !g_create_file_next)
		hooklog::Write("quick resume: the game's file imports are not as expected: the save cannot be sent to a player");
	const overlay::Client client = {kName, &Wanted, &Prepare, &Draw};
	overlay::AddClient(client);
	hooklog::Write("quick resume: %s", g_on ? "on (as the host, this PC asks every PC for a resume save when a player's game drops out)"
	                                           : "this PC never asks for a save ([coop] resume_save=0); it still shows the banner");
}

}  // namespace mods

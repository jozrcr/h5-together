// Mod: the main menu's scene and its music, chosen by each player
// (docs/plans/menu-scenes.md).
//
// The main menu is a 3D scene (an "arena") with the menu's buttons over it, and a music
// theme. Which scene? The game builds the one that the file
// Arenas/Interface/MainMenu/MainMenu.(ArenaDesc).xdb describes; its path is a text inside the
// exe, so the content of that one file decides. Tribes of the East (ToE) has its own in
// data.pak (orcs and goblins storming a Haven town wall); MMH5.5 replaces the file with a
// description of the Heroes V (2006) scene (a cleric, an arch devil, a champion on a horse),
// whose models and textures are in MMH55-Data.pak. The ToE scene's files stay in data.pak
// under MMH5.5, only hidden behind that one file. The music comes from the game's root table,
// UI/UIGameRoot.(UIGameRoot).xdb: the item MAIN_MENU_MUSIC_ID points at a music definition,
// Sounds/_(Music)/MainMenu/a2-MainMenu.xdb (ToE's theme; MMH5.5 re-points it at its own
// track). The Heroes V theme has a definition too, MainMenu.xdb, that nothing uses.
//
// With [menu] scene=toe|homm5|random (default: nothing is installed) we wrap the game's
// "open file" function (the file system's table, NVFS::CWinVFS vftable 0x00e66500, slot 7 =
// 0x009e0020, stacked with the other mods that wrap it) and, for four names:
//   - MainMenu.(ArenaDesc).xdb: the arena description of the chosen scene (read from the
//     game's paks at run time: the ToE one from data.pak, the Heroes V one from MMH5.5's);
//   - UIGameRoot.(UIGameRoot).xdb: the game's own file, with one change: the item
//     MAIN_MENU_MUSIC_ID names Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb instead;
//   - OpenHomm5-Menu.xdb (a name that is in no pak): the chosen scene's music definition,
//     data.pak's a2-MainMenu.xdb (ToE) or MainMenu.xdb (Heroes V);
//   - a2-MainMenu.xdb: left alone. Only if the root table was read before our hook went in
//     (the Steam-wrapped exe is hooked late), it is the fallback: this file gets the chosen
//     theme (and the one ToE campaign dialog that plays this definition changes with it).
// Why our own name and not a2-MainMenu.xdb: the campaign dialog A2C3/M4/S2 plays
// a2-MainMenu.xdb, and a1-MainMenu.xdb (Hammers of Fate) is used by a campaign dialog too. With
// our own name only the main menu's item changes: nothing else that plays music definitions
// is touched, and [audio] original_music=1 (which sets a2-MainMenu.xdb back to ToE's theme)
// cannot get in the way: the menu no longer asks for that file. The multiplayer sub-menu's
// theme (MP_MAIN_MENU_MUSIC_ID) and the logo are not touched.
//
// Which scenes can this PC show? At the first file the game opens among those four we read
// the directories of data\*.pak (about 100 ms), find the arena descriptions by the scene
// file each was exported from (see SceneOfArena), check that every file such a description
// names is in some pak, that the music definition is in data.pak and that its .ogg is in
// music\. A scene that fails is "not available" (logged, with the reason). Then the setting is
// applied once per launch: an explicit scene that is not available gives the game's own menu;
// random draws among the available ones. Nothing is written to disk, no game file is in the
// repo, and it is only a picture and a tune: no game state, nothing the multiplayer reads, so
// each player can choose.
//
// Settings, h5-together.ini [menu]:
//   scene=default   nothing installed: the menu as it is
//   scene=toe       the Tribes of the East scene and its theme
//   scene=homm5     the Heroes V scene and its theme (needs MMH5.5, which brings that scene)
//   scene=random    one of the scenes available on this PC, drawn once per launch
#include "menu_scene.h"

#include <windows.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "languages.h"
#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

using menuscene::Choice;
using menuscene::Scene;

// ---- the game (vanilla 3.1 layout, also MMH55_Utility) ----

const DWORD kWinVfsOpenSlot = 0x00e6651c;       // NVFS::CWinVFS vftable 0x00e66500, slot 7: OpenFile
const DWORD kWinVfsOpen = 0x009e0020;           // __thiscall(CWinVFS *, const nstl::string *path), ret 4
const DWORD kMemoryStreamCtor = 0x004034d0;     // __thiscall(this): empty stream, returns this
const DWORD kStreamSetSize = 0x008c7610;        // __thiscall(this, int size), ret 4: new buffer of size bytes
const DWORD kGameNew = 0x00878fd0;              // cdecl void *(size): the game's operator new

struct GameString {  // nstl::string: first char, one past the last, end of the allocation
	const char *begin;
	const char *end;
	const char *cap;
};

struct GameStream {  // the whole file is in memory from begin to end (see original_music.cpp)
	void **vftable;  // slot 0: scalar deleting destructor (this, flags)
	char *begin;
	char *cap;    // end of the buffer
	char *end;    // end of the data
	char *pos;    // read position
	DWORD flags;  // bit 0: broken
};
static_assert(sizeof(GameStream) == 0x18, "CMemoryStream is 0x18 bytes");

typedef GameStream *(__fastcall *OpenFn)(void *vfs, void *edx, const GameString *path);
typedef GameStream *(__fastcall *CtorFn)(GameStream *self, void *edx);
typedef void(__fastcall *SetSizeFn)(GameStream *self, void *edx, int size);
typedef void(__fastcall *DeleteFn)(GameStream *self, void *edx, int flags);
typedef void *(__cdecl *NewFn)(size_t size);

// ---- the names ----

const char kArenaKey[] = "arenas/interface/mainmenu/mainmenu.(arenadesc).xdb";
const char kRootKey[] = "ui/uigameroot.(uigameroot).xdb";
const char kThemeKey[] = "sounds/_(music)/mainmenu/openhomm5-menu.xdb";  // ours: in no pak
const char kThemePath[] = "/Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb";
const char kA2Key[] = "sounds/_(music)/mainmenu/a2-mainmenu.xdb";
const char kToeThemeKey[] = "sounds/_(music)/mainmenu/a2-mainmenu.xdb";   // ToE's theme (data.pak's copy)
const char kHomm5ThemeKey[] = "sounds/_(music)/mainmenu/mainmenu.xdb";    // Heroes V's theme (data.pak's copy)

// The paks of ToE 3.1 itself (every other pak in data\ is a mod's): where the original themes are.
const wchar_t *const kOriginalPaks[] = {L"data.pak", L"a2p1-data.pak"};

OpenFn g_open_next = nullptr;
Choice g_choice = Choice::Default;
bool g_installed = false;  // our function is in the game's file system table
bool g_early = false;      // ... since the DLL was loaded

// ---- paths, files ----

std::string Narrow(const std::wstring &w)
{
	char buffer[MAX_PATH * 3];
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, buffer, sizeof(buffer), nullptr, nullptr);
	return n > 0 ? std::string(buffer) : std::string("?");
}

std::wstring Widen(const std::string &s)
{
	wchar_t buffer[MAX_PATH * 2];
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, buffer, MAX_PATH * 2);
	return n > 0 ? std::wstring(buffer) : std::wstring();
}

// <game>\data\, from <game>\bin\<exe> (the robot's copies too)
std::wstring DataFolder()
{
	wchar_t path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	for (int cut = 0; cut < 2; ++cut) {  // the exe's name, then bin
		while (n > 0 && path[n - 1] != L'\\')
			--n;
		if (n > 0)
			--n;
	}
	return std::wstring(path, n) + L"\\data\\";
}

unsigned long long Hash(const std::string &s)
{
	unsigned long long h = 14695981039346656037ull;  // FNV-1a
	for (unsigned char c : s)
		h = (h ^ c) * 1099511628211ull;
	return h;
}

bool IsOriginalPak(const wchar_t *name)
{
	for (const wchar_t *original : kOriginalPaks)
		if (_wcsicmp(name, original) == 0)
			return true;
	return false;
}

// ---- reading the paks, once ----

struct Found {  // a file of a pak that we keep
	bool found = false;
	DWORD time = 0;  // its date inside the zip: the newest copy wins, as in the game
	std::string bytes;
	std::string pak;
};

struct Scan {
	std::vector<unsigned long long> names;  // every file of every pak (hashes), sorted
	Found arena[2];                          // [0] ToE's, [1] Heroes V's arena description
	Found theme[2];                          // [0] ToE's, [1] Heroes V's music definition (original paks only)
	unsigned files = 0;
	int paks = 0;
};

void Keep(Found *slot, HANDLE pak, const lang::ZipEntry &e, const wchar_t *pak_name)
{
	if (slot->found && e.dos_time <= slot->time)
		return;  // (an older copy, or the same date: the first found stays)
	std::string bytes;
	if (e.size > (1u << 20) || !lang::ReadPakFile(pak, e, &bytes))
		return;
	slot->found = true;
	slot->time = e.dos_time;
	slot->bytes.swap(bytes);
	slot->pak = Narrow(pak_name);
}

void ScanPak(Scan *scan, const std::wstring &folder, const wchar_t *name)
{
	HANDLE file = CreateFileW((folder + name).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;
	std::vector<lang::ZipEntry> entries;
	if (!lang::ReadPakDirectory(file, &entries)) {
		CloseHandle(file);
		hooklog::Write("menu scene: %s is not a zip archive we can read: skipped", Narrow(name).c_str());
		return;
	}
	const bool original = IsOriginalPak(name);
	++scan->paks;
	scan->files += static_cast<unsigned>(entries.size());
	for (const lang::ZipEntry &e : entries) {
		scan->names.push_back(Hash(e.key));
		if (e.key == kArenaKey) {
			// which scene it is, from its content: read it, and keep the newest of each
			std::string bytes;
			if (e.size <= (1u << 20) && lang::ReadPakFile(file, e, &bytes)) {
				const Scene scene = menuscene::SceneOfArena(bytes);
				if (scene != Scene::None) {
					Found *slot = &scan->arena[scene == Scene::Toe ? 0 : 1];
					if (!slot->found || e.dos_time > slot->time) {
						slot->found = true;
						slot->time = e.dos_time;
						slot->bytes.swap(bytes);
						slot->pak = Narrow(name);
					}
				}
			}
		} else if (original && e.key == kToeThemeKey) {
			Keep(&scan->theme[0], file, e, name);
		} else if (original && e.key == kHomm5ThemeKey) {
			Keep(&scan->theme[1], file, e, name);
		}
	}
	CloseHandle(file);
}

bool FileExists(const std::wstring &path)
{
	const DWORD a = GetFileAttributesW(path.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---- what this PC can show, and the pick ----

SRWLOCK g_lock = SRWLOCK_INIT;
volatile LONG g_state = 0;        // 0 not decided yet, 1 decided
Scene g_scene = Scene::None;      // the scene to show (None: the game's own menu)
std::string g_arena;              // its arena description
std::string g_music;              // its music definition, as served under our name
volatile LONG g_root_patched = 0;  // the root table was served with our theme name

unsigned Seed()
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	unsigned x = static_cast<unsigned>(t.LowPart) ^ (GetTickCount() * 2654435761u) ^ (GetCurrentProcessId() * 40503u);
	x ^= x >> 16;  // murmur3's finalizer: every input bit reaches the low bit
	x *= 0x85ebca6bu;
	x ^= x >> 13;
	x *= 0xc2b2ae35u;
	x ^= x >> 16;
	return x;
}

void Decide()
{
	const DWORD start = GetTickCount();
	const std::wstring folder = DataFolder();
	Scan scan;
	WIN32_FIND_DATAW found;
	HANDLE find = FindFirstFileW((folder + L"*.pak").c_str(), &found);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			ScanPak(&scan, folder, found.cFileName);
		} while (FindNextFileW(find, &found));
		FindClose(find);
	}
	std::sort(scan.names.begin(), scan.names.end());
	const auto exists = [&scan](const std::string &key) {
		return std::binary_search(scan.names.begin(), scan.names.end(), Hash(key));
	};

	menuscene::Availability available;
	std::string tracks[2];
	for (int i = 0; i < 2; ++i) {
		const bool toe = i == 0;
		bool &ok = toe ? available.toe : available.homm5;
		std::string &why = toe ? available.toe_why : available.homm5_why;
		const char *what = toe ? "Tribes of the East" : "Heroes V";
		if (!scan.arena[i].found) {
			why = std::string("no ") + what + " menu scene in the game's data";
		} else {
			const std::string missing = menuscene::FirstMissing(scan.arena[i].bytes, exists);
			if (!missing.empty())
				why = std::string(what) + " scene needs " + missing + ", which no pak has";
			else if (!scan.theme[i].found)
				why = std::string("no ") + (toe ? kToeThemeKey : kHomm5ThemeKey) + " in the original paks";
			else {
				tracks[i] = menuscene::TrackOf(scan.theme[i].bytes);
				std::wstring path = Widen(tracks[i]);
				std::replace(path.begin(), path.end(), L'/', L'\\');
				if (tracks[i].empty())
					why = "its music definition names no track";
				else if (!FileExists(folder + path))
					why = std::string("the music file ") + tracks[i] + " is not there";
				else
					ok = true;
			}
		}
	}
	hooklog::Write("menu scene: read %d paks (%u files) in %lu ms; scenes available: %s%s%s%s%s",
	               scan.paks, scan.files, GetTickCount() - start, menuscene::AvailableList(available).c_str(),
	               available.toe ? "" : "; toe: ", available.toe ? "" : available.toe_why.c_str(),
	               available.homm5 ? "" : "; homm5: ", available.homm5 ? "" : available.homm5_why.c_str());

	std::string why;
	const Scene scene = menuscene::Pick(g_choice, available, Seed(), &why);
	if (scene == Scene::None) {
		hooklog::Write("menu scene: %s", why.c_str());
		return;
	}
	const int i = scene == Scene::Toe ? 0 : 1;
	g_arena = scan.arena[i].bytes;
	g_music = menuscene::ServedDefinition(scan.theme[i].bytes);
	g_scene = scene;
	hooklog::Write("menu scene: %s: showing %s: the scene's description from %s (%u bytes), the music %s from %s, "
	               "served as %s",
	               why.c_str(), menuscene::SceneName(scene), scan.arena[i].pak.c_str(),
	               static_cast<unsigned>(g_arena.size()), tracks[i].c_str(), scan.theme[i].pak.c_str(), kThemePath);
}

// The pick, made once (by whichever thread first needs it). True when a scene is to be served.
bool Ready()
{
	if (g_state == 0) {
		AcquireSRWLockExclusive(&g_lock);
		if (g_state == 0) {
			try {
				Decide();
			} catch (...) {  // out of memory: the game's own menu
				g_scene = Scene::None;
				hooklog::Write("menu scene: out of memory while reading the paks: the menu as installed");
			}
			InterlockedExchange(&g_state, 1);
		}
		ReleaseSRWLockExclusive(&g_lock);
	}
	return g_scene != Scene::None;
}

// ---- the hook ----

void Delete(GameStream *s)
{
	if (s)
		reinterpret_cast<DeleteFn>(s->vftable[0])(s, nullptr, 1);
}

// A new stream (the game's CMemoryStream) holding this text; null if it could not be made.
GameStream *NewStream(const std::string &data)
{
	GameStream *m = static_cast<GameStream *>(reinterpret_cast<NewFn>(kGameNew)(sizeof(GameStream)));
	if (!m)
		return nullptr;
	reinterpret_cast<CtorFn>(kMemoryStreamCtor)(m, nullptr);
	reinterpret_cast<SetSizeFn>(kStreamSetSize)(m, nullptr, static_cast<int>(data.size()));
	if ((m->flags & 1) || !m->begin || static_cast<size_t>(m->end - m->begin) != data.size()) {
		Delete(m);
		return nullptr;
	}
	memcpy(m->begin, data.data(), data.size());
	return m;
}

// This text instead of what the game opened (which may be nothing: a name in no pak).
GameStream *Replace(GameStream *opened, const std::string &data)
{
	GameStream *ours = NewStream(data);
	if (!ours)
		return opened;
	Delete(opened);
	return ours;
}

enum class File { Other, Arena, Root, Theme, A2 };

bool Same(const char *name, size_t length, const char *what)
{
	return length == strlen(what) && _strnicmp(name, what, length) == 0;
}

File Classify(const char *name, size_t length)
{
	if (Same(name, length, "MainMenu.(ArenaDesc).xdb"))
		return File::Arena;
	if (Same(name, length, "UIGameRoot.(UIGameRoot).xdb"))
		return File::Root;
	if (Same(name, length, "OpenHomm5-Menu.xdb"))
		return File::Theme;
	if (Same(name, length, "a2-MainMenu.xdb"))
		return File::A2;
	return File::Other;
}

volatile LONG g_told_a2 = 0;

GameStream *Serve(File file, const std::string &key, GameStream *s)
{
	switch (file) {
	case File::Arena: {
		if (key != kArenaKey || !Ready())
			return s;
		static LONG served = 0;
		if (InterlockedIncrement(&served) <= 3)
			hooklog::Write("menu scene: %s served (%s's scene)", kArenaKey, menuscene::SceneName(g_scene));
		return Replace(s, g_arena);
	}
	case File::Root: {
		if (key != kRootKey || !Ready())
			return s;
		if (!s || (s->flags & 1) || !s->begin || s->end < s->begin || s->pos != s->begin)
			return s;  // not freshly opened: left alone
		std::string patched, old_path;
		if (!menuscene::WithMenuMusic(std::string(s->begin, s->end), kThemePath, &patched, &old_path)) {
			hooklog::Write("menu scene: the game's root table (%s) has no MAIN_MENU_MUSIC_ID item in the form we know: its "
			               "music definition is served instead",
			               kRootKey);
			return s;
		}
		GameStream *ours = Replace(s, patched);
		if (ours != s && InterlockedExchange(&g_root_patched, 1) == 0)
			hooklog::Write("menu scene: the root table's main menu music %s -> %s", old_path.c_str(), kThemePath);
		return ours;
	}
	case File::Theme: {
		if (key != kThemeKey || !Ready())
			return s;
		static LONG served = 0;
		if (InterlockedIncrement(&served) <= 3)
			hooklog::Write("menu scene: %s served (%s's theme)", kThemeKey, menuscene::SceneName(g_scene));
		return Replace(s, g_music);
	}
	case File::A2:
		if (key != kA2Key || !Ready())
			return s;
		if (g_root_patched) {
			// the menu does not ask for it any more: someone else does (the campaign dialog)
			if (InterlockedIncrement(&g_told_a2) <= 3)
				hooklog::Write("menu scene: %s opened (not by the main menu): left as the game has it", kA2Key);
			return s;
		}
		// the fallback: the root table was read before our hook went in
		if (InterlockedIncrement(&g_told_a2) <= 3)
			hooklog::Write("menu scene: %s served with %s's theme (the root table was read before this hook: this also "
			               "changes the music of the one campaign dialog that plays this file)",
			               kA2Key, menuscene::SceneName(g_scene));
		return Replace(s, g_music);
	case File::Other:
		break;
	}
	return s;
}

GameStream *__fastcall OpenFileHook(void *vfs, void *edx, const GameString *path)
{
	GameStream *s = g_open_next(vfs, edx, path);
	if (!path || !path->begin || path->end <= path->begin)
		return s;
	const size_t length = static_cast<size_t>(path->end - path->begin);
	if (length < 12 || length > 300 || _strnicmp(path->end - 4, ".xdb", 4) != 0)
		return s;  // (cheap test first: the game opens thousands of files)
	const char *name = path->end;  // the last name of the path
	while (name > path->begin && name[-1] != '/' && name[-1] != '\\')
		--name;
	const File file = Classify(name, static_cast<size_t>(path->end - name));
	if (file == File::Other)
		return s;
	try {
		return Serve(file, menuscene::NormalizePath(path->begin, path->end), s);
	} catch (...) {  // out of memory: the game's own file
		return s;
	}
}

// ---- the setting ----

Choice ReadChoice(bool *known, std::string *raw)
{
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);  // h5-together-<instance>.ini for the robot
	wchar_t value[32] = L"";
	GetPrivateProfileStringW(L"menu", L"scene", L"default", value, 32, ini);
	*raw = Narrow(value);
	return menuscene::ParseChoice(*raw, known);
}

void Install(bool early)
{
	if (g_installed)
		return;
	bool known = true;
	std::string raw;
	g_choice = ReadChoice(&known, &raw);
	if (g_choice == Choice::Default)
		return;  // nothing to do: nothing installed
	g_open_next = reinterpret_cast<OpenFn>(
		hookpatch::PatchPointer(kWinVfsOpenSlot, kWinVfsOpen, reinterpret_cast<const void *>(&OpenFileHook)));
	if (!g_open_next) {
		if (!early)
			hooklog::Write("menu scene: the game's file system is not as expected (%08lx): the menu as installed",
			               kWinVfsOpenSlot);
		return;
	}
	g_installed = true;
	g_early = early;
}

}  // namespace

void InstallMenuSceneEarly()
{
	Install(true);
}

void ApplyMenuSceneFromSettings()
{
	bool known = true;
	std::string raw;
	const Choice choice = ReadChoice(&known, &raw);
	if (!known)
		hooklog::Write("menu scene: scene=%s is not default, toe, homm5 or random: the menu as installed", raw.c_str());
	if (choice == Choice::Default) {
		if (known)
			hooklog::Write("menu scene: scene=default (the menu as installed: nothing changes)");
		return;
	}
	Install(false);  // not at DLL load (e.g. the Steam-wrapped exe is still encrypted then)
	if (!g_installed)
		return;
	Ready();  // the pick and its log lines now, not when the game first asks
	if (!g_early)
		hooklog::Write("menu scene: installed late (the game was already running): files it read before now keep their "
		               "content; if that was the root table of music, the theme is served as a2-MainMenu.xdb instead");
}

}  // namespace mods

// Mod: the 2008 game started in a folder where MMH5.5 is installed.
//
// In plain words: MMH5.5 installs into the game's own folder. Its data are archives in data\
// (MMH55-Data.pak, MMH55-Index.pak, ...) and UserMODs\ (MMH55-SPMaps.h5u, ...), and its own
// programs (MMH55_Utility_64.exe ...) load its 55u.dll, which changes the game's code to
// match those data. The 2008 game's own exe (Steam's H5_GameOLD.exe in the owner's group,
// renamed as the GameRanger guide asks) does not load 55u.dll, but it reads every archive
// of those folders, whatever its name: MMH5.5's data without MMH5.5's code changes, a broken
// mix. So, when the 2008 exe starts in such a folder, this mod hides MMH5.5's archives from
// it: the game is then the original 2008 game, from the same folder, and MMH5.5 starts as
// before from its own exes. Nothing in the folder is changed.
//
// How: the game finds its archives with FindFirstFileA / FindNextFileA ("data\*.pak", then
// "*.h5m", "*.h5p", "*.h5c", and UserMODs; NVFS::CWinVFS, 0x009e08d0, docs/subsystems/texts.md).
// Our import hooks skip the names of MMH5.5's archives (mmh55_files_logic.h). They go in at
// the DLL's load, before the game builds its file table (the loader has filled the import
// table by then, also in the Steam-wrapped exe, whose import table is the plain one).
//
// Its own profile: the two games share the player's Documents folder (profiles, options,
// saves), and a save of one crashes the other when loaded (robot, 2026-11-06: an MMH5.5 save
// in the 2008 game, an access violation at 0x00a49280). So while it hides MMH5.5's files, the
// 2008 game gets "Documents\H5 Together\Original game" as its Documents folder (the game's
// SHGetFolderPathA, CSIDL_PERSONAL). At its first start the profiles' settings are copied
// there from the shared folder (the player's name, options and keys), never the saves.
// Not in robot mode: the robot gives every copy its own home already.
//
// [video] hd_textures=1 keeps MMH5.5's HD textures (MMH55-HDTex.pak: terrain, town
// exteriors, terrain objects, sky domes; textures only, every file it points to is in it or
// in the 2008 game's own data), the owner's "lighter MMH5.5". Each PC may choose: the
// pictures are not part of the game world.
#include "mmh55_files.h"

#include <windows.h>
#include <shlobj.h>

#include <string>

#include "mmh55_files_logic.h"

#include "../hook/log.h"
#include "../hook/patch.h"
#include "../hook/robot.h"

namespace mods {
namespace {

const DWORD kTimeDateStamp2008 = 0x48469f9f;  // the 2008 build (game_build.cpp)

typedef HANDLE(WINAPI *FindFirstFileAFn)(LPCSTR, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI *FindNextFileAFn)(HANDLE, LPWIN32_FIND_DATAA);
FindFirstFileAFn g_first_next = nullptr;
FindNextFileAFn g_next_next = nullptr;
bool g_hidden = false;
bool g_keep_hd = false;
LONG g_logged = 0;

typedef HRESULT(WINAPI *SHGetFolderPathAFn)(HWND, int, HANDLE, DWORD, LPSTR);
SHGetFolderPathAFn g_folder_next = nullptr;
char g_documents[MAX_PATH] = "";  // the original game's own Documents folder
const char kGameFolder[] = "\\My Games\\Heroes of Might and Magic V - Tribes of the East";

// The exe's import table names `dll`.
bool Imports(const char *dll)
{
	const BYTE *base = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
	const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	const IMAGE_NT_HEADERS32 *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
	const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!dir.VirtualAddress)
		return false;
	for (const IMAGE_IMPORT_DESCRIPTOR *imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>(base + dir.VirtualAddress);
	     imp->Name; ++imp)
		if (_stricmp(reinterpret_cast<const char *>(base + imp->Name), dll) == 0)
			return true;
	return false;
}

// The 2008 game's own exe: its build date, and it loads the original um.dll, not MMH5.5's
// 55u.dll (MMH5.5's exes have the same date). Only the header and the import table are read:
// both are plain also in the Steam-wrapped exe at the DLL's load.
bool Is2008Exe()
{
	const BYTE *base = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
	const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	const IMAGE_NT_HEADERS32 *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
	return nt->FileHeader.TimeDateStamp == kTimeDateStamp2008 && Imports("um.dll") && !Imports("55u.dll");
}

std::wstring GameFolder()
{
	wchar_t path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	while (n > 0 && path[n - 1] != L'\\')
		--n;
	path[n] = 0;
	return std::wstring(path) + L"..\\";
}

// MMH5.5's archives this folder holds (the names, for the log), and whether its HD textures are there.
std::string Mmh55Archives(bool *hd_textures)
{
	*hd_textures = false;
	std::string names;
	const std::wstring root = GameFolder();
	const wchar_t *const patterns[] = {L"data\\MMH55-*.pak", L"UserMODs\\MMH55-*.h5u"};
	for (const wchar_t *pattern : patterns) {
		WIN32_FIND_DATAW fd;
		HANDLE h = FindFirstFileW((root + pattern).c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			continue;
		do {
			char name[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof(name), nullptr, nullptr);
			if (_stricmp(name, "MMH55-HDTex.pak") == 0)
				*hd_textures = true;
			names += (names.empty() ? "" : ", ") + std::string(name);
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
	return names;
}

// Skips MMH5.5's archives; `ok` says whether `data` holds a file now.
bool Skip(HANDLE h, LPWIN32_FIND_DATAA data, bool ok)
{
	while (ok && mmh55files::Hidden(data->cFileName, g_keep_hd)) {
		if (InterlockedIncrement(&g_logged) <= 60)
			hooklog::Write("mmh55 files: hidden from the game: %s", data->cFileName);
		ok = g_next_next(h, data) != FALSE;
	}
	return ok;
}

HANDLE WINAPI FindFirstFileAHook(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
	HANDLE h = g_first_next(pattern, data);
	if (h == INVALID_HANDLE_VALUE || Skip(h, data, true))
		return h;
	// only MMH5.5's archives matched: as if there were none
	FindClose(h);
	SetLastError(ERROR_FILE_NOT_FOUND);
	return INVALID_HANDLE_VALUE;
}

BOOL WINAPI FindNextFileAHook(HANDLE h, LPWIN32_FIND_DATAA data)
{
	if (!g_next_next(h, data))
		return FALSE;
	if (Skip(h, data, true))
		return TRUE;
	SetLastError(ERROR_NO_MORE_FILES);
	return FALSE;
}

// Copies the files of `from` (not its folders) into `to`, those not there yet.
int CopyFiles(const std::string &from, const std::string &to)
{
	int copied = 0;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((from + "\\*").c_str(), &fd);  // (kernel32's own: not the game's import)
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		if (CopyFileA((from + "\\" + fd.cFileName).c_str(), (to + "\\" + fd.cFileName).c_str(), TRUE))
			++copied;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return copied;
}

// The first start of the original game: the shared profiles' settings, without the saves
// (Profiles\*.cfg, and each Profiles\<player>\ file; not its Saves folder).
void FirstStart(const std::string &shared_documents)
{
	const std::string from = shared_documents + kGameFolder + "\\Profiles";
	const std::string to = std::string(g_documents) + kGameFolder + "\\Profiles";
	if (GetFileAttributesA(to.c_str()) != INVALID_FILE_ATTRIBUTES)
		return;  // not the first start
	SHCreateDirectoryExA(nullptr, to.c_str(), nullptr);
	int files = CopyFiles(from, to), players = 0;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((from + "\\*").c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.')
				continue;
			const std::string player = to + "\\" + fd.cFileName;
			CreateDirectoryA(player.c_str(), nullptr);
			files += CopyFiles(from + "\\" + fd.cFileName, player);
			++players;
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
	hooklog::Write("mmh55 files: the original game's first start: %d profile(s), %d settings file(s) copied from %s (no saves)",
	               players, files, from.c_str());
}

// The first time the game asks for "My Documents" (not at the DLL's load: the shell's folder
// functions must not run under the loader's lock): the original game's own folder, made and,
// at its first start, given the profiles' settings. False: the shared folder stays.
bool PrepareOwnFolder(HWND hwnd, HANDLE token)
{
	char shared[MAX_PATH] = "";
	if (FAILED(g_folder_next(hwnd, CSIDL_PERSONAL, token, SHGFP_TYPE_CURRENT, shared)) || !shared[0]) {
		hooklog::Write("mmh55 files: the Documents folder is not known: the original game shares MMH5.5's profiles and saves "
		               "(never load a save of the other game)");
		return false;
	}
	const std::string own = std::string(shared) + "\\H5 Together\\Original game";
	if (own.size() >= MAX_PATH - 80) {
		hooklog::Write("mmh55 files: the Documents folder's path is too long: the original game shares MMH5.5's profiles and "
		               "saves (never load a save of the other game)");
		return false;
	}
	const int made = SHCreateDirectoryExA(nullptr, own.c_str(), nullptr);
	if (made != ERROR_SUCCESS && made != ERROR_ALREADY_EXISTS && made != ERROR_FILE_EXISTS) {
		hooklog::Write("mmh55 files: %s could not be made (error %d): the original game shares MMH5.5's profiles and saves "
		               "(never load a save of the other game)",
		               own.c_str(), made);
		return false;
	}
	lstrcpynA(g_documents, own.c_str(), MAX_PATH);
	FirstStart(shared);
	hooklog::Write("mmh55 files: the original game's own profiles and saves: %s%s (MMH5.5's saves stay with MMH5.5)", own.c_str(),
	               kGameFolder);
	return true;
}

// The game asks where "My Documents" is: the original game's own folder.
HRESULT WINAPI FolderHook(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPSTR path)
{
	static LONG prepared = 0;  // 0 not yet, 1 being prepared, 2 done (the game asks from its main thread)
	if ((csidl & 0xff) == CSIDL_PERSONAL && path) {
		if (InterlockedCompareExchange(&prepared, 1, 0) == 0) {
			PrepareOwnFolder(hwnd, token);
			InterlockedExchange(&prepared, 2);
		}
		while (InterlockedCompareExchange(&prepared, 2, 2) != 2)  // another thread prepares it: wait
			Sleep(1);
		if (g_documents[0]) {
			lstrcpynA(path, g_documents, MAX_PATH);
			return S_OK;
		}
	}
	return g_folder_next(hwnd, csidl, token, flags, path);
}

// While MMH5.5's files are hidden (not in robot mode): the original game's own Documents
// folder, from the game's first question.
void OwnProfile(const wchar_t *ini)
{
	// (testing: [debug] own_profile_test=1 does it in robot mode too, inside the robot's home)
	if (robot::Active() && GetPrivateProfileIntW(L"debug", L"own_profile_test", 0, ini) == 0)
		return;
	g_folder_next = reinterpret_cast<SHGetFolderPathAFn>(
		hookpatch::PatchImport("SHELL32.dll", "SHGetFolderPathA", reinterpret_cast<void *>(&FolderHook)));
	if (!g_folder_next)
		hooklog::Write("mmh55 files: the game's Documents question could not be wrapped: the original game shares MMH5.5's "
		               "profiles and saves (never load a save of the other game)");
}

}  // namespace

void InstallMmh55FilesEarly()
{
	if (!Is2008Exe())
		return;
	bool hd = false;
	const std::string names = Mmh55Archives(&hd);
	if (names.empty()) {
		hooklog::Write("mmh55 files: the 2008 game, no MMH5.5 files in this folder: nothing to hide");
		return;
	}
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);
	g_keep_hd = hd && GetPrivateProfileIntW(L"video", L"hd_textures", 0, ini) != 0;
	g_next_next = reinterpret_cast<FindNextFileAFn>(
		hookpatch::PatchImport("KERNEL32.dll", "FindNextFileA", reinterpret_cast<void *>(&FindNextFileAHook)));
	g_first_next = reinterpret_cast<FindFirstFileAFn>(
		g_next_next ? hookpatch::PatchImport("KERNEL32.dll", "FindFirstFileA", reinterpret_cast<void *>(&FindFirstFileAHook))
		            : nullptr);
	if (!g_first_next || !g_next_next) {
		hooklog::Write("mmh55 files: the 2008 game in a folder with MMH5.5 (%s), but the file search could not be wrapped: "
		               "the game will read MMH5.5's files and may not work",
		               names.c_str());
		return;
	}
	g_hidden = true;
	hooklog::Write("mmh55 files: the 2008 game in a folder with MMH5.5: its files are hidden from the game, which is the "
	               "original game (%s); MMH5.5's HD textures %s",
	               names.c_str(),
	               g_keep_hd ? "kept ([video] hd_textures=1)" : hd ? "hidden too ([video] hd_textures=0)" : "not installed");
	OwnProfile(ini);
}

bool Mmh55FilesHidden()
{
	return g_hidden;
}

bool Mmh55HdTexturesKept()
{
	return g_hidden && g_keep_hd;
}

}  // namespace mods

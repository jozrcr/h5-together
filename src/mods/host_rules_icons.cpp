// Host rules: the files of ToE's bonus pictures for the waiting room
// (docs/plans/host-rules.md, "The lobby's bonus icons"). MMH5.5 repainted the
// four pictures of the bonus arrows for its game modes (MMH55-Data.pak:
// Textures/Interface/MPWait/PlayersList/Item/Bonuses/{Random,RandomArtifact,
// Gold,Resource}.(Texture).dds, a fist and a burning town for gold = Savage...).
// With ToE's start bonuses the rows should show ToE's own pictures again, the
// ones in the game's data.pak.
//
// The game loads a picture once per session, and the rules change in the lobby
// (a client learns them after it joined), so serving other pictures under the
// same names cannot follow them. Instead the bonus icon gets four more pictures
// to choose from, and host_rules_bonus.cpp points each row at ToE's or MMH5.5's.
// Through our wrap of the game's "open file" (host_rules.cpp; any thread):
//   - the icon's list of related textures, UI/MPWait/PlayersList/Item/
//     Bonuses.(WindowRelatedTextures).xdb (the game's own, read below us), gets
//     four more items after "bonus0".."bonus3": "oh_toe_bonus0".."3", each naming
//     a texture record of ours (rules::BonusTexturesWithToe);
//   - our records, .../Bonuses/OpenHomm5-ToE-<name>.(Texture).xdb: data.pak's
//     record of that picture with its DestName renamed to ours;
//   - our pictures, .../Bonuses/OpenHomm5-ToE-<name>.(Texture).dds: data.pak's
//     bytes (ToE's 55x55 pictures).
// data.pak and a2p1-data.pak (the later one wins, as in the game) are read here
// once, when the game first opens the list (their directories: about 85000
// files, a few ms; the zip reading is languages.cpp's). If a file is missing
// or not as expected, the list is served as it is and the icons stay MMH5.5's
// (logged). Nothing is written to disk, and no game file is in the repo. The
// game's own names and pictures do not change, so with the rule off nothing
// shown differs. Only pictures: no game state, nothing the multiplayer
// checksums read.
#include <windows.h>
#include <string.h>

#include <string>
#include <vector>

#include "host_rules_internal.h"
#include "languages.h"
#include "rmg_templates.h"

#include "../hook/log.h"

namespace mods {
namespace {

struct GameStream {  // CMemoryStream: the whole file in memory (see original_music.cpp)
	void **vftable;  // slot 0: scalar deleting destructor (this, flags)
	char *begin;
	char *cap;
	char *end;
	char *pos;
	DWORD flags;  // bit 0: broken (e.g. no such file)
};
typedef void(__fastcall *DeleteFn)(GameStream *self, void *edx, int flags);

const char kListName[] = "bonuses.(windowrelatedtextures).xdb";  // the file names we look at first (cheap)
const char kOurName[] = "openhomm5-toe-";

// "/UI\MPWait/X.xdb" -> "ui/mpwait/x.xdb"
std::string NormalizePath(const char *b, const char *e)
{
	while (b < e && (*b == '/' || *b == '\\'))
		++b;
	std::string s(b, e);
	for (char &c : s)
		c = c == '\\' ? '/' : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	return s;
}

std::string Narrow(const std::wstring &w)
{
	char buffer[MAX_PATH * 3];
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, buffer, sizeof(buffer), nullptr, nullptr);
	return n > 0 ? std::string(buffer) : std::string("?");
}

void Delete(GameStream *s)
{
	if (s)
		reinterpret_cast<DeleteFn>(s->vftable[0])(s, nullptr, 1);
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

// --- ToE's records and pictures, read once ---------------------------------------------------
SRWLOCK g_load_lock = SRWLOCK_INIT;
volatile LONG g_state = 0;  // 0 not read yet, 1 ready, 2 not available
std::string g_records[4], g_pictures[4];  // ours: the records renamed, the pictures as in data.pak
volatile LONG g_list_served = 0;

bool ReadOriginals(std::string *why)
{
	const DWORD start = GetTickCount();
	std::string records[4], pictures[4];
	std::string paths[8];  // the game's record and picture of each bonus
	for (int i = 0; i < 8; ++i)
		paths[i] = rules::BonusPicturePath(i / 2, false, i % 2 == 1);
	std::string from;  // the paks that gave them (for the log)
	const wchar_t *const kPaks[] = {L"data.pak", L"a2p1-data.pak"};  // ToE 3.1's own data; later ones win
	const std::wstring folder = DataFolder();
	for (const wchar_t *pak : kPaks) {
		HANDLE file = CreateFileW((folder + pak).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			continue;  // (a2p1-data.pak: optional)
		std::vector<lang::ZipEntry> entries;
		if (!lang::ReadPakDirectory(file, &entries)) {
			CloseHandle(file);
			*why = Narrow(folder + pak) + " is not a zip archive we can read";
			return false;
		}
		bool gave = false;
		for (const lang::ZipEntry &e : entries) {
			for (int i = 0; i < 8; ++i) {
				if (e.key != paths[i])
					continue;
				std::string bytes;
				if (e.size > (1u << 20) || !lang::ReadPakFile(file, e, &bytes)) {
					CloseHandle(file);
					*why = e.key + " could not be read from " + Narrow(pak);
					return false;
				}
				(i % 2 ? pictures : records)[i / 2] = bytes;
				gave = true;
			}
		}
		CloseHandle(file);
		if (gave)
			from += (from.empty() ? "" : ", ") + Narrow(pak);
	}
	for (int b = 0; b < 4; ++b) {
		if (records[b].empty() || pictures[b].empty()) {
			*why = paths[b * 2 + (records[b].empty() ? 0 : 1)] + " is in neither data.pak nor a2p1-data.pak";
			return false;
		}
		if (!rules::ToeTextureRecord(records[b], b, &g_records[b])) {
			*why = rules::BonusPicturePath(b, false, false) + " is not the record we know (no single DestName for its picture)";
			return false;
		}
		if (!rules::IsDds(pictures[b])) {
			*why = rules::BonusPicturePath(b, false, true) + " is not a .dds picture";
			return false;
		}
		g_pictures[b] = pictures[b];
	}
	hooklog::Write("rules: ToE's bonus pictures read from %s (%s %u, %s %u, %s %u, %s %u bytes) in %lu ms", from.c_str(),
	               rules::kBonusPictures[0],
	               static_cast<unsigned>(g_pictures[0].size()), rules::kBonusPictures[1], static_cast<unsigned>(g_pictures[1].size()),
	               rules::kBonusPictures[2], static_cast<unsigned>(g_pictures[2].size()), rules::kBonusPictures[3],
	               static_cast<unsigned>(g_pictures[3].size()), GetTickCount() - start);
	return true;
}

bool Ready()
{
	if (g_state == 0) {
		AcquireSRWLockExclusive(&g_load_lock);
		if (g_state == 0) {
			std::string why;
			bool ok = false;
			try {
				ok = ReadOriginals(&why);
			} catch (...) {  // out of memory
				why = "out of memory";
			}
			if (!ok)
				hooklog::Write("rules: ToE's bonus pictures are not available (%s): the lobby's bonus icons stay MMH5.5's", why.c_str());
			InterlockedExchange(&g_state, ok ? 1 : 2);
		}
		ReleaseSRWLockExclusive(&g_load_lock);
	}
	return g_state == 1;
}

// The game opens the icon's list: ours, with ToE's four pictures, when everything is as expected.
GameStream *ServeList(GameStream *opened)
{
	if (!opened || (opened->flags & 1) || !opened->begin || opened->end < opened->begin || opened->pos != opened->begin)
		return opened;  // not there, or not freshly opened: left alone
	if (!Ready())
		return opened;
	std::string served;
	if (!rules::BonusTexturesWithToe(std::string(opened->begin, opened->end), &served)) {
		static LONG told = 0;
		if (InterlockedExchange(&told, 1) == 0)
			hooklog::Write("rules: the bonus icon's texture list (%s) is not the one we know: the lobby's bonus icons stay MMH5.5's",
			               rules::kBonusTexturesPath);
		return opened;
	}
	GameStream *ours = static_cast<GameStream *>(NewGameStream(served));
	if (!ours)
		return opened;
	Delete(opened);
	if (InterlockedExchange(&g_list_served, 1) == 0)
		hooklog::Write("rules: the bonus icon's texture list served with ToE's four pictures (\"%s\"..\"%s\", %u bytes)",
		               rules::BonusTextureName(0, true).c_str(), rules::BonusTextureName(3, true).c_str(),
		               static_cast<unsigned>(served.size()));
	return ours;
}

}  // namespace

namespace rules {

void *ServeBonusPicture(const char *begin, const char *end, void *opened)
{
	GameStream *s = static_cast<GameStream *>(opened);
	const char *name = end;
	while (name > begin && name[-1] != '/' && name[-1] != '\\')
		--name;
	const size_t n = static_cast<size_t>(end - name);
	const bool list = n == strlen(kListName) && _strnicmp(name, kListName, n) == 0;
	const bool ours = n > strlen(kOurName) && _strnicmp(name, kOurName, strlen(kOurName)) == 0;
	if (!list && !ours)
		return s;  // (cheap test first: the game opens thousands of files)
	try {
		const std::string key = NormalizePath(begin, end);
		if (list)
			return key == kBonusTexturesPath ? ServeList(s) : s;
		bool dds = false;
		const int bonus = OurBonusPicture(key, &dds);
		if (bonus < 0 || !g_list_served || !Ready())
			return s;
		GameStream *served = static_cast<GameStream *>(NewGameStream(dds ? g_pictures[bonus] : g_records[bonus]));
		if (!served)
			return s;
		Delete(s);
		static LONG told = 0;
		if (InterlockedIncrement(&told) <= 16)
			hooklog::Write("rules: %s served (ToE's %s %s)", key.c_str(), kBonusPictures[bonus], dds ? "picture" : "record");
		return served;
	} catch (...) {  // out of memory: the game's own files
		return s;
	}
}

bool ToePicturesServed()
{
	return g_list_served != 0;
}

}  // namespace rules
}  // namespace mods

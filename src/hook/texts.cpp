// Our texts in the player's language (see texts.h): the built-in English, the
// player's file, and the choice of the language.
#include "texts.h"

#include <windows.h>
#include <string.h>

#include <set>
#include <vector>

#include "log.h"
#include "old_names.h"
#include "overlay.h"
#include "../mods/languages.h"

namespace texts {
namespace {

// en.txt as it was when the DLL was built: build.ps1 writes texts_en.inc (its
// bytes) from src/mods/texts/en.txt.
const unsigned char kEnglishFile[] = {
#include "texts_en.inc"
};

// Steam's id of Heroes V: Tribes of the East: its file steamapps\appmanifest_15370.acf.
const wchar_t kManifest[] = L"appmanifest_15370.acf";
// The game's own text we recognise its language by ("Joueur rouge" in French).
const char kRedPlayer[] = "Text/Game/Player/Colors/red_player.txt";
const size_t kMaxFile = 1 << 20;
const int kMaxProblemsLogged = 20;

struct State {
	Book english;  // built in
	Book chosen;   // the player's language, checked against English (empty: English)
	std::string code = "en";
};
State *g_state = nullptr;
INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
volatile LONG g_generation = 0;
CRITICAL_SECTION g_missing_lock;
std::set<std::string> *g_missing = nullptr;  // keys asked for that English lacks (logged once)

bool ReadFileBytes(const std::wstring &path, std::string *out)
{
	HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE)
		return false;
	const DWORD size = GetFileSize(f, nullptr);
	bool ok = size != INVALID_FILE_SIZE && size <= kMaxFile;
	if (ok) {
		out->resize(size);
		DWORD got = 0;
		ok = size == 0 || (ReadFile(f, &(*out)[0], size, &got, nullptr) && got == size);
	}
	CloseHandle(f);
	return ok;
}

std::string Utf8(const std::wstring &w)
{
	return Narrow(w);
}

// bin\h5-together\texts (or the folder's old name while only that one is there).
std::wstring TextsFolder()
{
	wchar_t dir[MAX_PATH];
	oldnames::PathInOurFolder(L"texts", dir, MAX_PATH);
	return dir;
}

std::wstring FileOf(const std::string &code)
{
	return TextsFolder() + L"\\" + std::wstring(code.begin(), code.end()) + L".txt";
}

void LogProblems(const char *file, const Book &book)
{
	if (book.problems.empty())
		return;
	hooklog::Write("texts: %s: %d line(s) not used (they show in English):", file, static_cast<int>(book.problems.size()));
	for (size_t i = 0; i < book.problems.size() && i < kMaxProblemsLogged; ++i)
		hooklog::Write("texts:   %s", book.problems[i].c_str());
	if (book.problems.size() > kMaxProblemsLogged)
		hooklog::Write("texts:   ... and %d more", static_cast<int>(book.problems.size() - kMaxProblemsLogged));
}

// The folder of the exe, and the folder names above it.
std::wstring ExeFolder()
{
	wchar_t path[MAX_PATH];
	const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	std::wstring s(path, n < MAX_PATH ? n : 0);
	return s.substr(0, s.rfind(L'\\'));
}

std::wstring Parent(const std::wstring &dir)
{
	const size_t at = dir.rfind(L'\\');
	return at == std::wstring::npos ? std::wstring() : dir.substr(0, at);
}

// --- the clues to the player's language ---------------------------------------------------------
struct Choice {
	std::string code;
	std::string why;
};

// 2. Steam's language for the game: steamapps\appmanifest_15370.acf, two folders
// above the game folder (steamapps\common\<game>\bin\<exe>). Only when its
// installdir is this game folder (not a copy elsewhere, such as a test copy).
bool FromSteam(Choice *c)
{
	const std::wstring game = Parent(ExeFolder());
	const std::wstring steamapps = Parent(Parent(game));
	if (game.empty() || steamapps.empty())
		return false;
	std::string acf, name, installdir;
	if (!ReadFileBytes(steamapps + L"\\" + kManifest, &acf) || !ManifestLanguage(acf, &name, &installdir))
		return false;
	const std::string folder = Utf8(game.substr(game.rfind(L'\\') + 1));
	if (_stricmp(folder.c_str(), installdir.c_str()) != 0) {
		hooklog::Write("texts: Steam's file for the game is about \"%s\", not this folder (%s): not used", installdir.c_str(),
		               folder.c_str());
		return false;
	}
	c->code = SteamCode(name);
	if (c->code.empty()) {
		hooklog::Write("texts: Steam's language for the game is \"%s\", which we do not know: the next clue", name.c_str());
		return false;
	}
	c->why = "Steam's language for the game (" + name + ", appmanifest_15370.acf)";
	return true;
}

// The [language] red_player line of each texts file there is (and of the built-in
// English): {code, the red player's name in that language}.
std::vector<std::pair<std::string, std::string>> RedPlayerNames(const Book &english)
{
	std::vector<std::pair<std::string, std::string>> out;
	const auto en = english.texts.find("language.red_player");
	if (en != english.texts.end())
		out.push_back({"en", en->second});
	WIN32_FIND_DATAW found;
	HANDLE h = FindFirstFileW((TextsFolder() + L"\\*.txt").c_str(), &found);
	if (h == INVALID_HANDLE_VALUE)
		return out;
	do {
		std::wstring name = found.cFileName;
		const std::string code = Utf8(name.substr(0, name.size() - 4));
		std::string bytes;
		Book book;
		if (code == "en" || !ValidCode(code) || !ReadFileBytes(TextsFolder() + L"\\" + name, &bytes) || !Parse(bytes, &book))
			continue;
		const auto it = book.texts.find("language.red_player");
		if (it != book.texts.end())
			out.push_back({code, it->second});
	} while (FindNextFileW(h, &found));
	FindClose(h);
	return out;
}

// 3. The game's own texts: the red player's name, compared with each texts
// file's own line for it. A name none of them has: the game is in a language
// we have no file for, so English.
bool FromGameText(const Book &english, Choice *c)
{
	std::string data;
	if (!overlay::ReadGameFile(kRedPlayer, &data) || data.size() < 4)
		return false;
	size_t start = static_cast<unsigned char>(data[0]) == 0xff && static_cast<unsigned char>(data[1]) == 0xfe ? 2 : 0;
	std::wstring w;
	for (size_t i = start; i + 1 < data.size(); i += 2) {
		const wchar_t ch = static_cast<wchar_t>(static_cast<unsigned char>(data[i]) | (static_cast<unsigned char>(data[i + 1]) << 8));
		if (ch == L'\r' || ch == L'\n')
			break;
		w += ch;
	}
	std::string name = Utf8(w);
	while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
		name.pop_back();
	for (const auto &known : RedPlayerNames(english))
		if (_stricmp(known.second.c_str(), name.c_str()) == 0) {
			c->code = known.first;
			c->why = "the game's texts (the red player is \"" + name + "\")";
			return true;
		}
	c->code = "en";
	c->why = "the game's texts (the red player is \"" + name + "\": no texts file has that name)";
	return true;
}

Choice Choose(const Book &english)
{
	Choice c;
	wchar_t path[MAX_PATH], setting[32] = L"";
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	GetPrivateProfileStringW(L"ui", L"language", L"auto", setting, 32, path);
	std::string s;
	for (const wchar_t *p = setting; *p; ++p)
		s += static_cast<char>(*p >= L'A' && *p <= L'Z' ? *p - L'A' + L'a' : (*p < 0x80 ? *p : '?'));
	while (!s.empty() && s.back() == ' ')
		s.pop_back();
	if (!s.empty() && s != "auto") {
		if (ValidCode(s))
			return {s, "[ui] language=" + s};
		hooklog::Write("texts: [ui] language=%s is not a language code (en, fr, de...): auto", s.c_str());
	}
	// 1. the game's texts in a language chosen by the player ([lang] texts=)
	switch (mods::TextsChoice()) {
	case mods::Texts::French:
		return {"fr", "the game's texts ([lang] texts=fr)"};
	case mods::Texts::English:
		return {"en", "the game's texts ([lang] texts=en)"};
	default:
		break;
	}
	if (FromSteam(&c) || FromGameText(english, &c))
		return c;
	// 4. Windows
	c.code = WindowsCode(PRIMARYLANGID(GetUserDefaultUILanguage()));
	if (!c.code.empty()) {
		c.why = "Windows' language (the game's language is not known yet)";
		return c;
	}
	return {"en", "nothing else known"};
}

BOOL CALLBACK Load(PINIT_ONCE, void *, void **)
{
	InitializeCriticalSection(&g_missing_lock);
	g_missing = new std::set<std::string>;
	State *s = new State;
	const std::string built_in(reinterpret_cast<const char *>(kEnglishFile), sizeof(kEnglishFile));
	if (!Parse(built_in, &s->english) || !s->english.problems.empty())
		LogProblems("the built-in English", s->english);
	const Choice c = Choose(s->english);
	const std::string file = c.code + ".txt";
	if (c.code == "en")
		hooklog::Write("texts: English, from %s (built in: %d texts)", c.why.c_str(), static_cast<int>(s->english.texts.size()));
	else {
		std::string bytes;
		if (!ReadFileBytes(FileOf(c.code), &bytes))
			hooklog::Write("texts: %s, from %s; but there is no %s in h5-together\\texts: English", c.code.c_str(), c.why.c_str(),
			               file.c_str());
		else {
			Book book;
			const bool read = Parse(bytes, &book);
			if (read)
				CheckAgainst(s->english, &book);
			LogProblems(file.c_str(), book);
			if (!read || book.texts.empty())
				hooklog::Write("texts: %s, from %s; but %s holds no usable text: English", c.code.c_str(), c.why.c_str(), file.c_str());
			else {
				s->chosen = book;
				s->code = c.code;
				int missing = 0;
				for (const auto &t : s->english.texts)
					missing += s->chosen.texts.count(t.first) == 0;
				hooklog::Write("texts: %s, from %s: %s, %d of %d texts (the others in English)", c.code.c_str(), c.why.c_str(),
				               file.c_str(), static_cast<int>(s->english.texts.size()) - missing,
				               static_cast<int>(s->english.texts.size()));
			}
		}
	}
	g_state = s;
	InterlockedIncrement(&g_generation);
	return TRUE;
}

State &Loaded()
{
	InitOnceExecuteOnce(&g_once, &Load, nullptr, nullptr);
	return *g_state;
}

}  // namespace

const char *Get(const char *key)
{
	const State &s = Loaded();
	auto it = s.chosen.texts.find(key);
	if (it != s.chosen.texts.end())
		return it->second.c_str();
	it = s.english.texts.find(key);
	if (it != s.english.texts.end())
		return it->second.c_str();
	EnterCriticalSection(&g_missing_lock);
	const bool first = g_missing->insert(key).second;
	LeaveCriticalSection(&g_missing_lock);
	if (first)
		hooklog::Write("texts: no English text \"%s\" (en.txt lacks it): the key is shown", key);
	return key;
}

std::string Format(const char *key, std::initializer_list<Arg> args)
{
	return Fill(Get(key), args);
}

std::wstring Wide(const std::string &utf8)
{
	if (utf8.empty())
		return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0)
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &w[0], n);
	return w;
}

const char *Language()
{
	return Loaded().code.c_str();
}

int Generation()
{
	Loaded();
	return g_generation;
}

}  // namespace texts

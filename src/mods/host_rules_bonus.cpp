// Host rules: the waiting room's bonus tooltips and icons (docs/plans/host-rules.md,
// "Game mode and start bonuses"). MMH5.5 gave the bonus arrows its game modes, so its
// four tooltips (UI/MPWait/PlayersList/Item/bonus_tooltip_*.txt) describe
// Default, Savage and Nightmare. With the host's start rules, the arrows choose
// ToE's start bonuses again (or nothing, when MMH5.5 still takes the bonus back
// but the mode is the host's), and the tooltips should say so.
//
// Why not serve other text files: the game reads a text file once per session
// and keeps it (languages.cpp), and each row copies the four tooltips when it is
// made (0x00714f00), when a client joins and before it knows the host's rules.
// So this file changes each row's own copies instead, as often as the rules
// change:
//   - a row (NUI::CMPWaitItem, vftable 0x00e39ce8; the lobby logic's list at
//     +0x38 points 8 bytes into it) keeps at +0x108 a vector of five
//     hash_map<int, wstring> (0x18 bytes each); the fourth (+0x48) holds the
//     bonus tooltips by bonus number (0 random, 1 minor artifact, 2 gold, 3
//     resources), filled by 0x00714f00 through 0x00717a10 (operator[]: buckets at
//     +4..+8, a node is {next, key, wstring}) and 0x0040e960 (wstring::assign
//     (first, last), thiscall, ret 8);
//   - the row's refresh (0x00713fe0) gives the bonus icon (+0x9c) the tooltip
//     of its bonus (+0x84) through the icon's vtable slot 0x68 (thiscall(text,
//     a 5-dword format {0x80000000, 0x80000000, 0, 0, 0})); we do the same after
//     a change, so the new text shows at once.
// The texts are ours (the [bonus] texts, in the panels' language: src/hook/texts.h):
// ToE's bonuses, and the game mode. MMH5.5's own texts come back when the rules go back to
// MMH5.5's way (we keep them from the first row we saw).
//
// The icons: MMH5.5 repainted the four pictures for its modes (a fist and a
// burning town for Savage...). With ToE's start bonuses (tips.toe) each row shows
// ToE's own pictures, which host_rules_icons.cpp adds to the icon's list of
// related textures as "oh_toe_bonus0".."3"; otherwise the game's "bonus0".."3"
// (MMH5.5's). The same way as the texts, in each row's own data:
//   - a row keeps at +0x6c its pictures by bonus number, a hash_map<int,
//     CDBPtr<NDb::STexture>> (a node is {next, key, object, resolved}), set when
//     the row is made (0x00715cd0) from the icon's slot 0x110 (0x00a50a10: the
//     related texture of a name), one more reference (+8) to each picture;
//   - its refresh (0x007148e0, on every change: bonus arrows, players...) shows
//     the one of its bonus through the icon's slot 0xa4 (0x00a4e290: the
//     background's texture), after CDBPtr::Get (0x006710a0).
// We set the four entries as the row's making does (the old picture's
// reference back: the icon's list keeps it alive, so nothing is ever freed
// here) and show the row's one at once; the game's own refresh then shows ours
// by itself. The icon is a CWindowTextView held by its IWindow part (vftable
// 0x00e81aa4, checked, with the two slots); nothing else changes.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "host_rules_internal.h"

#include "../hook/log.h"
#include "../hook/texts.h"

namespace mods {
namespace {

const DWORD kWaitItemVftable = 0x00e39ce8;  // NUI::CMPWaitItem
const DWORD kAssign = 0x0040e960;           // nstl::wstring::assign(first, last): thiscall, ret 8
const DWORD kRowTables = 0x108;             // vector of 5 hash_map<int, wstring> (0x18 bytes each)
const DWORD kBonusTable = 0x48;             // the fourth: the bonus tooltips
const DWORD kRowBonus = 0x84;               // the row's bonus (0..3)
const DWORD kRowBonusIcon = 0x9c;           // the bonus icon (a window)
const DWORD kSetTooltipSlot = 0x68;         // the window's vtable: thiscall(const wstring *, const DWORD format[5])
// the icon's pictures (below, "the icons")
const DWORD kRowPictures = 0x6c;            // hash_map<int, CDBPtr<NDb::STexture>>: the bonus pictures by bonus
const DWORD kIconVftable = 0x00e81aa4;      // CWindowTextView, its IWindow part (how a row holds the icon)
const DWORD kRelatedTextureSlot = 0x110;    // thiscall(const nstl::string *name) -> NDb::STexture *, ret 4
const DWORD kRelatedTexture = 0x0088f810;   //   (a vtordisp thunk to 0x00a50a10)
const DWORD kSetTextureSlot = 0xa4;         // thiscall(NDb::STexture *), ret 4: the background's picture
const DWORD kSetTexture = 0x0088f6a0;       //   (a vtordisp thunk to 0x00a4e290)
const DWORD kDbPtrGet = 0x006710a0;         // CDBPtr<NDb::STexture>::Get: thiscall, returns the object
const BYTE kDbPtrGetCode[] = {0x56, 0x8b, 0xf1, 0x80, 0x7e, 0x04, 0x00, 0x8b, 0x0d, 0x84, 0x12, 0xf9, 0x00};
const DWORD kReleaseObject = 0x008c6150;    // CObjectBase: destroyed when no reference is left (ecx = the object)
const BYTE kReleaseObjectCode[] = {0x51, 0x83, 0x79, 0x08, 0x00};  // push ecx; cmp dword [ecx+8], 0

struct WString {  // nstl::wstring
	wchar_t *begin;
	wchar_t *end;
	wchar_t *cap;
};
struct GameString {  // nstl::string
	const char *begin;
	const char *end;
	const char *cap;
};
struct DbPtr {  // CDBPtr<NDb::STexture>: the object (its references at +8), and whether Get resolved it
	BYTE *object;
	BYTE resolved;
};

typedef void *(__fastcall *AssignFn)(WString *self, void *edx, const wchar_t *first, const wchar_t *last);
typedef void(__fastcall *SetTooltipFn)(void *window, void *edx, const WString *text, const DWORD *format);
typedef BYTE *(__fastcall *RelatedTextureFn)(void *window, void *edx, const GameString *name);
typedef void(__fastcall *SetTextureFn)(void *window, void *edx, BYTE *texture);
typedef BYTE *(__fastcall *DbPtrGetFn)(DbPtr *self, void *edx);
typedef void(__fastcall *ReleaseFn)(BYTE *object);

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

// --- the words: the [bonus] texts (src/hook/texts.h), with the game's colour tags --------------
struct Words {
	const char *bonus[4];  // ToE's bonuses
	const char *none;      // MMH5.5 takes the bonus back: nothing to choose
	const char *mode;      // {mode} = the mode
	const char *modes[4];  // the game mode rule's values 1..4
};

const Words kKeys = {
	{"bonus.random", "bonus.artifact", "bonus.gold", "bonus.resources"},
	"bonus.none",
	"bonus.mode",
	{"bonus.mode.default", "bonus.mode.savage", "bonus.mode.nightmare", "bonus.mode.random"},
};

// The bonuses, for the log.
const char *const kBonusNames[4] = {"random", "minor artifact", "gold", "resources"};

// The four tooltips for these tips.
void Texts(const rules::BonusTips &tips, std::wstring out[4])
{
	const Words &w = texts::Table(kKeys);
	std::string mode;
	if (tips.mode >= 1 && tips.mode <= 4)
		mode = texts::Fill(w.mode, {{"mode", w.modes[tips.mode - 1]}});
	for (int i = 0; i < 4; ++i)
		out[i] = texts::Wide(std::string(tips.toe ? w.bonus[i] : w.none) + mode);
}

// --- the game's side (each call alone, so a bad guess cannot take the game down) ------------------
// A row's four bonus tooltips (their wstrings in the row's table), its bonus and its icon.
bool FindRow(BYTE *full, WString *texts[4], int *bonus, void **icon)
{
	__try {
		if (Read(full, 0) != kWaitItemVftable)
			return false;
		const BYTE *tables = reinterpret_cast<const BYTE *>(Read(full, kRowTables));
		const BYTE *map = tables ? tables + kBonusTable : nullptr;
		if (!map || !Readable(map, 0xc))
			return false;
		const DWORD b = Read(map, 4), e = Read(map, 8);
		const DWORD buckets = e > b ? (e - b) / 4 : 0;
		if (buckets == 0 || buckets > 4096 || !Readable(reinterpret_cast<const void *>(b), buckets * 4))
			return false;
		for (DWORD key = 0; key < 4; ++key) {
			texts[key] = nullptr;
			const BYTE *node = reinterpret_cast<const BYTE *>(Read(reinterpret_cast<const void *>(b), (key % buckets) * 4));
			for (int steps = 0; node && steps < 64; ++steps) {
				if (!Readable(node, 0x14))
					return false;
				if (Read(node, 4) == key) {
					texts[key] = reinterpret_cast<WString *>(const_cast<BYTE *>(node) + 8);
					break;
				}
				node = reinterpret_cast<const BYTE *>(Read(node, 0));
			}
			WString *t = texts[key];
			if (!t || t->end < t->begin || t->end - t->begin > 8192 ||
			    (t->end > t->begin && !Readable(t->begin, (t->end - t->begin) * sizeof(wchar_t))))
				return false;
		}
		*bonus = static_cast<int>(Read(full, kRowBonus));
		*icon = reinterpret_cast<void *>(Read(full, kRowBonusIcon));
		if (*bonus < 0 || *bonus > 3 || !*icon || !Readable(*icon, 4) || !Readable(reinterpret_cast<void *>(Read(*icon, 0)), 0x6c))
			return false;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool Assign(WString *text, const wchar_t *first, const wchar_t *last)
{
	__try {
		reinterpret_cast<AssignFn>(kAssign)(text, nullptr, first, last);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool SetTooltip(void *icon, const WString *text)
{
	__try {
		const DWORD format[5] = {0x80000000u, 0x80000000u, 0, 0, 0};
		reinterpret_cast<SetTooltipFn>(Read(reinterpret_cast<void *>(Read(icon, 0)), kSetTooltipSlot))(icon, nullptr, text, format);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

std::wstring Copy(const WString *t)
{
	return std::wstring(t->begin, t->end);
}

std::string Narrow(const std::wstring &w)
{
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0)
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
	return s;
}

// --- the icons: the game's side --------------------------------------------------------------------
// Is the icon the window we know (its vtable, and the two slots we call)?
bool IconAsExpected(void *icon, DWORD *vtable)
{
	*vtable = Read(icon, 0);
	return *vtable == kIconVftable && Read(reinterpret_cast<void *>(*vtable), kRelatedTextureSlot) == kRelatedTexture &&
	       Read(reinterpret_cast<void *>(*vtable), kSetTextureSlot) == kSetTexture &&
	       Readable(reinterpret_cast<const void *>(kDbPtrGet), sizeof(kDbPtrGetCode)) &&
	       memcmp(reinterpret_cast<const void *>(kDbPtrGet), kDbPtrGetCode, sizeof(kDbPtrGetCode)) == 0 &&
	       Readable(reinterpret_cast<const void *>(kReleaseObject), sizeof(kReleaseObjectCode)) &&
	       memcmp(reinterpret_cast<const void *>(kReleaseObject), kReleaseObjectCode, sizeof(kReleaseObjectCode)) == 0;
}

// The row's four picture entries (bonus 0..3), found as 0x00717f80 finds them (a
// node is {next, key, object, resolved}); each names a picture.
bool RowPictures(BYTE *full, DbPtr *entries[4])
{
	__try {
		const BYTE *map = full + kRowPictures;
		const DWORD b = Read(map, 4), e = Read(map, 8);
		const DWORD buckets = e > b ? (e - b) / 4 : 0;
		if (buckets == 0 || buckets > 4096 || !Readable(reinterpret_cast<const void *>(b), buckets * 4))
			return false;
		for (DWORD key = 0; key < 4; ++key) {
			entries[key] = nullptr;
			BYTE *node = reinterpret_cast<BYTE *>(Read(reinterpret_cast<const void *>(b), (key % buckets) * 4));
			for (int steps = 0; node && steps < 64; ++steps) {
				if (!Readable(node, 0x10))
					return false;
				if (Read(node, 4) == key) {
					entries[key] = reinterpret_cast<DbPtr *>(node + 8);
					break;
				}
				node = reinterpret_cast<BYTE *>(Read(node, 0));
			}
			if (!entries[key] || !entries[key]->object || !Readable(entries[key]->object, 0x30))
				return false;
		}
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The icon's related texture of this name (as the row's making asks for "bonus0".."bonus3").
BYTE *RelatedTexture(void *icon, const char *name)
{
	__try {
		const size_t n = strlen(name);
		const GameString s = {name, name + n, name + n};
		BYTE *t = reinterpret_cast<RelatedTextureFn>(Read(reinterpret_cast<void *>(Read(icon, 0)), kRelatedTextureSlot))(icon, nullptr, &s);
		return t && Readable(t, 0x30) ? t : nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

// An entry gets this picture, as the row's making (0x00715cd0) sets it: one more
// reference to the new picture (+8), one less to the old one (released at none,
// as the game does; it does not happen here: the icon's list keeps its
// pictures), and "not resolved" for Get.
bool SetPicture(DbPtr *entry, BYTE *texture)
{
	__try {
		BYTE *old = entry->object;
		entry->object = texture;
		++*reinterpret_cast<int *>(texture + 8);
		if (old && --*reinterpret_cast<int *>(old + 8) == 0)
			reinterpret_cast<ReleaseFn>(kReleaseObject)(old);
		entry->resolved = 0;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The icon shows the entry's picture, as the row's refresh (0x007148e0) does.
bool ShowPicture(void *icon, DbPtr *entry)
{
	__try {
		BYTE *t = reinterpret_cast<DbPtrGetFn>(kDbPtrGet)(entry, nullptr);
		reinterpret_cast<SetTextureFn>(Read(reinterpret_cast<void *>(Read(icon, 0)), kSetTextureSlot))(icon, nullptr, t);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// --- the state (main thread only) ---------------------------------------------------------------
std::wstring g_original[4];  // MMH5.5's texts (the first row's, before we changed any)
bool g_have_original = false;
std::vector<std::wstring> g_written;  // every text we gave a row (to know ours)
const void *g_logic = nullptr;
int g_last_bonus[16];
bool g_failed = false;
std::wstring g_last_wanted;  // for the log

bool Ours(const std::wstring &text)
{
	for (const std::wstring &w : g_written)
		if (w == text)
			return true;
	return false;
}

bool g_icons_failed = false;
int g_icons_logged = -1;  // the pictures last logged: 0 the game's, 1 ToE's

// A row's four bonus pictures: ToE's (when the icon's list has them) or the
// game's own (MMH5.5's); the shown one at once when it changed.
void SyncIcon(int row, BYTE *full, void *icon, int bonus, bool toe)
{
	if (g_icons_failed)
		return;
	DWORD vtable = 0;
	if (!IconAsExpected(icon, &vtable)) {
		g_icons_failed = true;
		hooklog::Write("rules: the lobby's bonus icon is not the window we know (vtable 0x%08x, or the code it uses): its "
		               "pictures are left alone from now on",
		               vtable);
		return;
	}
	if (toe && !rules::ToePicturesServed()) {
		static bool told = false;
		if (!told)
			hooklog::Write("rules: ToE's bonus pictures were not served (see above): the lobby's bonus icons stay MMH5.5's");
		told = true;
		toe = false;
	}
	if ((toe ? 1 : 0) != g_icons_logged) {
		g_icons_logged = toe ? 1 : 0;
		hooklog::Write(toe ? "rules: the lobby's bonus icons: ToE's pictures (\"%s\"...)"
		                   : "rules: the lobby's bonus icons: the game's own (MMH5.5's pictures, \"%s\"...)",
		               rules::BonusTextureName(0, toe).c_str());
	}
	DbPtr *entries[4];
	BYTE *want[4];
	bool found = RowPictures(full, entries);
	for (int k = 0; found && k < 4; ++k) {
		want[k] = RelatedTexture(icon, rules::BonusTextureName(k, toe).c_str());
		found = want[k] != nullptr;
	}
	if (!found) {
		static int told = 0;
		if (told++ < 3)
			hooklog::Write("rules: the lobby's row %d: its bonus pictures (or the icon's \"%s\"...) are not as expected: left alone",
			               row, rules::BonusTextureName(0, toe).c_str());
		return;
	}
	int changed = 0;
	bool shown = false;
	for (int k = 0; k < 4; ++k) {
		if (entries[k]->object == want[k])
			continue;
		if (!SetPicture(entries[k], want[k])) {
			g_icons_failed = true;
			hooklog::Write("rules: the lobby's bonus pictures could not be changed (row %d): left as they are from now on", row);
			return;
		}
		++changed;
		shown = shown || k == bonus;
	}
	if (shown && !ShowPicture(icon, entries[bonus])) {
		g_icons_failed = true;
		hooklog::Write("rules: the lobby's bonus icon could not show its new picture (row %d): left alone from now on", row);
		return;
	}
	static int logged = 0;
	if (changed && logged++ < 40)
		hooklog::Write("rules: the lobby's row %d: %s bonus pictures (%d changed, its bonus %d shown)", row,
		               toe ? "ToE's" : "the game's", changed, bonus);
}

}  // namespace

namespace rules {

BonusTips TipsFor(const Values &values)
{
	BonusTips tips;
	const bool toe = values[kToeBonusRule] == "1";
	const int mode = ModeUsed(atoi(values[kGameModeRule].c_str()), toe);
	if (toe || mode != 0) {
		tips.ours = true;
		tips.toe = toe;
		tips.mode = mode;
	}
	return tips;
}

void SyncBonusTips(void *logic, const BonusTips &tips)
{
	if (g_failed || !logic)
		return;
	if (logic != g_logic) {
		g_logic = logic;
		for (int &b : g_last_bonus)
			b = -1;
	}
	const DWORD first = Read(logic, 0x38), last = Read(logic, 0x3c);
	const int rows = last > first ? static_cast<int>((last - first) / 4) : 0;
	if (rows <= 0 || rows > 16 || !Readable(reinterpret_cast<void *>(first), rows * 4))
		return;
	std::wstring wanted[4];
	if (tips.ours) {
		Texts(tips, wanted);
		for (const std::wstring &w : wanted)
			if (!Ours(w) && g_written.size() < 64)
				g_written.push_back(w);
	}
	if (wanted[0] != g_last_wanted) {
		g_last_wanted = wanted[0];
		if (tips.ours)
			hooklog::Write("rules: the lobby's bonus tooltips: ours (%s, game mode %d)",
			               tips.toe ? "ToE's start bonuses" : "no start bonus to choose", tips.mode);
		else
			hooklog::Write("rules: the lobby's bonus tooltips: the game's own (MMH5.5's modes)");
	}
	for (int i = 0; i < rows; ++i) {
		BYTE *full = reinterpret_cast<BYTE *>(Read(reinterpret_cast<void *>(first), i * 4));
		if (!full)
			continue;
		full -= 8;
		WString *texts[4];
		int bonus = 0;
		void *icon = nullptr;
		if (!FindRow(full, texts, &bonus, &icon)) {
			static int told = 0;
			if (told++ < 3)
				hooklog::Write("rules: the lobby's row %d is not as expected (its bonus tooltips are left alone)", i);
			continue;
		}
		if (bonus != g_last_bonus[i]) {
			g_last_bonus[i] = bonus;
			hooklog::Write("rules: the lobby's row %d: start bonus %d (%s)", i, bonus, kBonusNames[bonus]);
		}
		SyncIcon(i, full, icon, bonus, tips.toe);
		if (!g_have_original) {
			bool mmh = true;
			for (int k = 0; k < 4; ++k)
				mmh = mmh && !Ours(Copy(texts[k]));
			if (mmh) {
				for (int k = 0; k < 4; ++k)
					g_original[k] = Copy(texts[k]);
				g_have_original = true;
				hooklog::Write("rules: the lobby's bonus tooltips as the game has them: \"%.40s\", \"%.40s\", \"%.40s\", \"%.40s\"",
				               Narrow(g_original[0]).c_str(), Narrow(g_original[1]).c_str(), Narrow(g_original[2]).c_str(),
				               Narrow(g_original[3]).c_str());
			}
		}
		bool shown_changed = false;
		for (int k = 0; k < 4; ++k) {
			const std::wstring now = Copy(texts[k]);
			const std::wstring *want = nullptr;
			if (tips.ours)
				want = &wanted[k];
			else if (g_have_original && Ours(now))
				want = &g_original[k];
			if (!want || now == *want)
				continue;
			if (!Assign(texts[k], want->c_str(), want->c_str() + want->size()) || Copy(texts[k]) != *want) {
				g_failed = true;
				hooklog::Write("rules: the lobby's bonus tooltips could not be changed (row %d): left as they are from now on", i);
				return;
			}
			shown_changed = shown_changed || k == bonus;
		}
		if (shown_changed && !SetTooltip(icon, texts[bonus])) {
			g_failed = true;
			hooklog::Write("rules: the lobby's bonus icon's tooltip could not be refreshed (row %d): left alone from now on", i);
			return;
		}
	}
}

}  // namespace rules
}  // namespace mods

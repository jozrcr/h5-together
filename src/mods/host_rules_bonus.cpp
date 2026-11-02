// Host rules: the waiting room's bonus tooltips (docs/plans/host-rules.md, "Game
// mode and start bonuses"). MMH5.5 gave the bonus arrows its game modes, so its
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
// The texts are ours (English and French, the panel's language): ToE's bonuses,
// and the game mode. MMH5.5's own texts come back when the rules go back to
// MMH5.5's way (we keep them from the first row we saw).
// Not changed: the icons (MMH5.5 repainted them for its modes: a fist and a
// burning town for Savage...). The game loads them once too; see the plan.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "host_rules_internal.h"

#include "../hook/log.h"
#include "../hook/overlay.h"

namespace mods {
namespace {

const DWORD kWaitItemVftable = 0x00e39ce8;  // NUI::CMPWaitItem
const DWORD kAssign = 0x0040e960;           // nstl::wstring::assign(first, last): thiscall, ret 8
const DWORD kRowTables = 0x108;             // vector of 5 hash_map<int, wstring> (0x18 bytes each)
const DWORD kBonusTable = 0x48;             // the fourth: the bonus tooltips
const DWORD kRowBonus = 0x84;               // the row's bonus (0..3)
const DWORD kRowBonusIcon = 0x9c;           // the bonus icon (a window)
const DWORD kSetTooltipSlot = 0x68;         // the window's vtable: thiscall(const wstring *, const DWORD format[5])

struct WString {  // nstl::wstring
	wchar_t *begin;
	wchar_t *end;
	wchar_t *cap;
};

typedef void *(__fastcall *AssignFn)(WString *self, void *edx, const wchar_t *first, const wchar_t *last);
typedef void(__fastcall *SetTooltipFn)(void *window, void *edx, const WString *text, const DWORD *format);

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

// --- the words -------------------------------------------------------------------------------
struct Words {
	const wchar_t *bonus[4];  // ToE's bonuses
	const wchar_t *none;      // MMH5.5 takes the bonus back: nothing to choose
	const wchar_t *mode;      // %ls = the mode
	const wchar_t *modes[5];
	const char *names[4];     // for the log
};

const Words kEnglish = {
	{L"<color=yellow>Start bonus: random<color_default><br>Gold, resources or a minor artifact, as in Tribes of the East.",
	 L"<color=yellow>Start bonus: minor artifact<color_default><br>Your first hero starts with a random minor artifact.",
	 L"<color=yellow>Start bonus: gold<color_default><br>500 to 1000 more gold at the start.",
	 L"<color=yellow>Start bonus: resources<color_default><br>3 to 6 more of one random resource at the start."},
	L"<color=yellow>No start bonus to choose<color_default><br>MMH5.5 gives every human player's first hero a random minor "
	L"artifact.",
	L"<br><br>Game mode: %ls (the host's game rules).",
	{L"", L"Default", L"Savage", L"Nightmare", L"Random (Default, Savage or Nightmare, drawn at the start)"},
	{"random", "minor artifact", "gold", "resources"},
};

const Words kFrench = {
	{L"<color=yellow>Bonus de d\u00e9part : al\u00e9atoire<color_default><br>De l'or, des ressources ou un artefact mineur, comme "
	 L"dans Tribes of the East.",
	 L"<color=yellow>Bonus de d\u00e9part : artefact mineur<color_default><br>Votre premier h\u00e9ros commence avec un artefact "
	 L"mineur al\u00e9atoire.",
	 L"<color=yellow>Bonus de d\u00e9part : or<color_default><br>500 \u00e0 1000 pi\u00e8ces d'or de plus au d\u00e9part.",
	 L"<color=yellow>Bonus de d\u00e9part : ressources<color_default><br>3 \u00e0 6 unit\u00e9s d'une ressource "
	 L"al\u00e9atoire de plus au d\u00e9part."},
	L"<color=yellow>Pas de bonus de d\u00e9part \u00e0 choisir<color_default><br>MMH5.5 donne au premier h\u00e9ros de chaque "
	L"joueur humain un artefact mineur al\u00e9atoire.",
	L"<br><br>Mode de jeu : %ls (r\u00e8gles de la partie choisies par l'h\u00f4te).",
	{L"", L"Par d\u00e9faut", L"Sauvage", L"Cauchemar", L"Al\u00e9atoire (Par d\u00e9faut, Sauvage ou Cauchemar, tir\u00e9 au d\u00e9but)"},
	{"random", "minor artifact", "gold", "resources"},
};

// The four tooltips for these tips.
void Texts(const rules::BonusTips &tips, std::wstring out[4])
{
	const Words &w = overlay::PanelLanguage() == overlay::kFrench ? kFrench : kEnglish;
	wchar_t mode[256] = L"";
	if (tips.mode >= 1 && tips.mode <= 4)
		_snwprintf_s(mode, _countof(mode), _TRUNCATE, w.mode, w.modes[tips.mode]);
	for (int i = 0; i < 4; ++i)
		out[i] = std::wstring(tips.toe ? w.bonus[i] : w.none) + mode;
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
			hooklog::Write("rules: the lobby's row %d: start bonus %d (%s)", i, bonus, kEnglish.names[bonus]);
		}
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

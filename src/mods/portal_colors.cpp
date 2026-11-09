// Mod: each pair of two-way portals glows in a colour of its own ([ui] portal_colors, each
// player's choice, off). The owner's request, 2026-11-09: "color coded portals on rmg maps
// (would make traversal much more comfortable)".
//
// The game's portals. A two-way monolith is one object type for every pair
// (/MapObjects/Monolith_Two_Way.(AdvMapBuildingShared).xdb: one model, one effect), so all
// look alike; a map pairs them with the building's GroupID. Every map object can carry
// point lights of its own ("pointLights": a position over the object, a colour, a radius),
// which the game draws live; its scenario maps use hundreds of them.
//
// What this does: when the game reads a map's buildings (NDb::SAdvMapBuilding, vftable
// 0x00e6cf54; the game reads maps in their binary form, slot 3 0x00a08450, chunk 1 the base
// with pointLights at +0x6c, then Shared +0x7c, PlayerID +0x84, CaptureTrigger +0x88,
// GroupID +0x9c; slot 7 0x00a09640 is the same by names, from XML, not used for a map), for a two-way
// monolith it adds one point light, coloured by its GroupID: both portals of a pair get the
// same colour, another pair another one (the order in which the groups first come, through
// a list of 8 colours). The light is added with the game's own vector code (0x004b8730,
// insert n default items, as its reader 0x00a24280 does): items of 0x20 bytes, the position
// at +4, the colour at +0x10, the radius at +0x1c.
//
// Display only: the map's data in memory, not the world (no world checksum moves, the map
// file is not touched), so each PC chooses for itself. The game keeps a map's data once
// read, so the setting acts from the next start of the game.
#include "portal_colors.h"

#include <windows.h>
#include <string.h>

#include <map>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kBuildingSerializeSlot = 0x00e6cf60;  // NDb::SAdvMapBuilding (vftable 0x00e6cf54) slot 3: the binary reader
const DWORD kBuildingSerialize = 0x00a08450;      // __thiscall(object, saver): chunks 1 base, 2 Shared, 3 PlayerID...
const DWORD kInsertLights = 0x004b8730;           // vector<point light>::insert(pos, n), __thiscall
const int kLights = 0x6c;                         // SAdvMapObject: pointLights (begin, end, capacity)
const int kShared = 0x7c;                         // SAdvMapBuilding: Shared (CDBPtr: the entry first)
const int kGroup = 0x9c;                          // SAdvMapBuilding: GroupID
const int kEntryPath = 0x20;                      // a database entry: its path (begin, end)
const int kLightSize = 0x20, kLightPos = 4, kLightColor = 0x10, kLightRadius = 0x1c;

typedef int(__fastcall *SerializeFn)(BYTE *object, void *edx, void *serializer);
typedef void(__fastcall *InsertFn)(void *vector, void *edx, void *pos, int count);
SerializeFn g_next = nullptr;

struct Colour {
	float r, g, b;
	const char *name;
};
// Bright enough to show on snow and in a dark forest (the first try, 2026-11-09: 1, 0.1, 0.1
// read well; 0.2, 0.5, 1 was faint)
const Colour kColours[] = {
	{2.0f, 0.2f, 0.15f, "red"},   {0.3f, 0.6f, 2.0f, "blue"},    {0.3f, 1.8f, 0.3f, "green"},
	{1.8f, 1.5f, 0.1f, "yellow"}, {1.4f, 0.35f, 2.0f, "violet"}, {0.2f, 1.7f, 1.8f, "cyan"},
	{2.0f, 0.8f, 0.1f, "orange"}, {1.8f, 1.8f, 1.8f, "white"},
};
const int kColourCount = sizeof(kColours) / sizeof(kColours[0]);
const float kRadius = 9.0f;
const float kX = 1.0f, kY = 1.0f, kZ = 3.0f;  // over the middle of the 2x2 portal

CRITICAL_SECTION g_lock;
std::map<int, int> g_colour_of;  // GroupID -> colour (the order the groups came in)
LONG g_added = 0;

// The building's Shared path contains `what` (false when it cannot be read).
bool SharedIs(const BYTE *object, const char *what)
{
	__try {
		const BYTE *entry = *reinterpret_cast<BYTE *const *>(object + kShared);
		if (!entry)
			return false;
		const char *b = *reinterpret_cast<const char *const *>(entry + kEntryPath);
		const char *e = *reinterpret_cast<const char *const *>(entry + kEntryPath + 4);
		if (!b || e <= b || e - b > 400)
			return false;
		const size_t n = strlen(what);
		for (const char *p = b; p + n <= e; ++p)
			if (memcmp(p, what, n) == 0)
				return true;
		return false;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The object's last light is one of ours (same place and radius): a second read of the same
// data (the game reads a map's data once, but a save may write it and read it back).
bool HasOurLight(const BYTE *object)
{
	__try {
		const BYTE *begin = *reinterpret_cast<BYTE *const *>(object + kLights);
		const BYTE *end = *reinterpret_cast<BYTE *const *>(object + kLights + 4);
		if (!begin || end - begin < kLightSize)
			return false;
		const BYTE *light = end - kLightSize;
		const float *pos = reinterpret_cast<const float *>(light + kLightPos);
		return pos[0] == kX && pos[1] == kY && pos[2] == kZ && *reinterpret_cast<const float *>(light + kLightRadius) == kRadius;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return true;  // unreadable: add nothing
	}
}

bool AddLight(BYTE *object, const Colour &c)
{
	__try {
		void *vector = object + kLights;
		BYTE *end = *reinterpret_cast<BYTE **>(object + kLights + 4);
		reinterpret_cast<InsertFn>(kInsertLights)(vector, nullptr, end, 1);
		BYTE *begin = *reinterpret_cast<BYTE **>(object + kLights);
		BYTE *now_end = *reinterpret_cast<BYTE **>(object + kLights + 4);
		if (!begin || now_end - begin < kLightSize)
			return false;
		BYTE *light = now_end - kLightSize;
		float *pos = reinterpret_cast<float *>(light + kLightPos);
		pos[0] = kX;
		pos[1] = kY;
		pos[2] = kZ;
		float *colour = reinterpret_cast<float *>(light + kLightColor);
		colour[0] = c.r;
		colour[1] = c.g;
		colour[2] = c.b;
		*reinterpret_cast<float *>(light + kLightRadius) = kRadius;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ReadGroup(const BYTE *object, int *group)
{
	__try {
		*group = *reinterpret_cast<const int *>(object + kGroup);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The first buildings read, for the log: what their Shared entry holds.
void LogShared(const BYTE *object)
{
	char path[96] = "";
	DWORD words[4] = {};
	__try {
		const BYTE *entry = *reinterpret_cast<BYTE *const *>(object + kShared);
		if (entry) {
			for (int i = 0; i < 4; ++i)
				words[i] = *reinterpret_cast<const DWORD *>(entry + kEntryPath - 8 + 4 * i);
			const char *b = *reinterpret_cast<const char *const *>(entry + kEntryPath);
			const char *e = *reinterpret_cast<const char *const *>(entry + kEntryPath + 4);
			if (b && e > b) {
				const size_t n = static_cast<size_t>(e - b) < sizeof(path) - 1 ? static_cast<size_t>(e - b) : sizeof(path) - 1;
				memcpy(path, b, n);
				path[n] = 0;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		strcpy_s(path, "(unreadable)");
	}
	hooklog::Write("portal colours: a map building read: Shared \"%s\" (entry +0x18..: %08lx %08lx %08lx %08lx)", path, words[0],
	               words[1], words[2], words[3]);
}

int __fastcall SerializeHook(BYTE *object, void *edx, void *serializer)
{
	const int r = g_next(object, edx, serializer);
	if (!object)
		return r;
	static LONG s_seen = 0;
	if (InterlockedIncrement(&s_seen) <= 3)
		LogShared(object);
	int group = 0;
	if (!SharedIs(object, "Monolith_Two_Way") || !ReadGroup(object, &group) || HasOurLight(object))
		return r;
	int index;
	EnterCriticalSection(&g_lock);
	auto it = g_colour_of.find(group);
	if (it == g_colour_of.end())
		it = g_colour_of.emplace(group, static_cast<int>(g_colour_of.size()) % kColourCount).first;
	index = it->second;
	LeaveCriticalSection(&g_lock);
	const bool added = AddLight(object, kColours[index]);
	const LONG n = InterlockedIncrement(&g_added);
	if (n <= 40 || !added)
		hooklog::Write("portal colours: a two-way portal of pair %d: %s light %s", group, kColours[index].name,
		               added ? "added" : "NOT added (its lights could not be grown)");
	return r;
}

}  // namespace

void ApplyPortalColorsFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	if (GetPrivateProfileIntW(L"ui", L"portal_colors", 0, path) == 0) {
		hooklog::Write("portal colours: off ([ui] portal_colors=0: the portals look as in the game)");
		return;
	}
	InitializeCriticalSection(&g_lock);
	g_next = reinterpret_cast<SerializeFn>(
		hookpatch::PatchPointer(kBuildingSerializeSlot, kBuildingSerialize, reinterpret_cast<const void *>(&SerializeHook)));
	if (!g_next) {
		hooklog::Write("portal colours: the map buildings' reader is not as expected (another build?): off");
		return;
	}
	hooklog::Write("portal colours: on (each pair of two-way portals gets a light of its own colour, %d colours)", kColourCount);
}

}  // namespace mods

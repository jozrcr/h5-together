// Mod: each pair of two-way portals shines in a colour of its own ([ui] portal_colors, each
// player's choice, off). The owner's request, 2026-11-09: "color coded portals on rmg maps
// (would make traversal much more comfortable)", then "don't color the stone but the light in
// the portal".
//
// The game's portals. A two-way monolith is one object type for every pair
// (/MapObjects/Monolith_Two_Way.(AdvMapBuildingShared).xdb), so all look alike; a map pairs
// them with the building's GroupID. Its light is its Effect
// (/Effects/_(Effect)/Buildings/Monolith_Two_Way_Entrance.xdb): three models (01: the cloud,
// texture _(Model)/Effects/Spells/Phantom-Phantom_Clot01; 02: the swirl, material ShinyMat01,
// texture Textures/Effects/_Various/Angle_violet_blue; 03: the lightning, material
// LightingMat01, texture .../LightningBolt_Lightning01/Lightning04.tga, DXT3) and a particle
// set (Glow05, Glow04), all violet.
//
// What this does. For each colour (1..7; colour 0 is the game's own violet) it serves, through
// the game's file system (NVFS::CWinVFS slot 7, stacked with the other mods), copies of those
// files named OpenHomm5-Portal-<colour>... next to the originals: the building type, the
// effect, the three models, the two materials and the five texture records, each naming the
// next copy instead of the original, and the five pictures with their colours turned (hue,
// from the original's violet to the colour's; DXT pictures by their blocks' two end colours).
// Nothing is written to disk. Then, the first time the game's database gives a map's data
// (its "resource of this id", vtable slot 7 of the database at 0x0103ffe4: before a new game's
// world is made, 0x00c9fa80, a loaded game's, or the waiting room's map), every two-way monolith of the map (NDb::SAdvMapDesc +0x5c) whose
// pair is not the first one gets a copy of its building type (SAdvMapBuilding +0x7c, a database
// pointer) made by the type's own copy (vtable slot 1, 0x00a1f550: 0xf0 bytes) whose Effect
// (+0x8c, and EffectWhenOwned +0x94) is our coloured one, loaded by its path (0x00bf6c30: path
// -> database id; 0x009b0f60: id -> resource, as the game's dialog_scene command does). The
// copy keeps the type's name and data: the world, which names the type in its checksums
// (a served type of another name put two PCs out of step at once, robot 20261109-153143-lan),
// sees the same. The same pair, the same colour; pairs through 8 colours in the order the
// map lists them.
//
// The map data's checksums. A map object caches a checksum of its data, with its building
// type's (0x00a05040: +0x78); it is computed before the type changes, so a PC with coloured
// portals keeps the same checksums as one without. Display only, each PC's own choice.
#include "portal_colors.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "rmg_templates.h"
#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kDatabase = 0x0103ffe4;         // the game's database (0x00ba06b0 returns it); made at start-up
const int kDatabaseGetSlot = 7;             // its "resource of this id" (0x009b0f60 calls it): __thiscall(db, id)
const DWORD kGetResource = 0x009b0f60;      // __thiscall(database id) -> NDb::CResource *
const DWORD kMakeId = 0x00bf6c30;           // __thiscall(id *out, const nstl::string *path) -> id *, ret 4
const DWORD kMapDescVftable = 0x00e6e1c8;   // NDb::SAdvMapDesc
const DWORD kBuildingVftable = 0x00e6cf54;  // NDb::SAdvMapBuilding
const DWORD kSharedVftable = 0x00e6cf28;    // NDb::SAdvMapBuildingShared
const int kSharedSize = 0xf0;               // SAdvMapBuildingShared (its copy, vtable slot 1 0x00a1f550, makes 0xf0)
const DWORD kGameNew = 0x00878fd0;          // cdecl void *(size)
const DWORD kEffectVftable = 0x00e906ac;    // NDb::SEffect, 0x70 bytes (its copy, slot 1 0x00c83b50)
const int kEffectSize = 0x70;
const int kEffectLists = 0x40, kEffectListsSize = 0x24;  // SEffect: Instances, Lights, Models (3 vectors)
const int kChecksumSlot = 4;                // every NDb type's data checksum, cached once computed
const int kEffect = 0x8c, kEffectWhenOwned = 0x94;  // SAdvMapObjectBaseShared: database pointers
const int kBuildingChecksumSlot = 4;        // 0x00a05040: the building's data checksum (cached at +0x78)
const int kMapObjects = 0x5c;               // SAdvMapDesc: objects (begin, end), 8 bytes each
const int kShared = 0x7c;                   // SAdvMapBuilding: Shared (database pointer: resource, "got" byte)
const int kGroup = 0x9c;                    // SAdvMapBuilding: GroupID
const int kResourcePath = 0x20;             // an NDb resource: its file (narrow string)
const DWORD kWinVfsOpenSlot = 0x00e6651c;   // NVFS::CWinVFS vftable 0x00e66500, slot 7: OpenFile
const DWORD kWinVfsOpen = 0x009e0020;

struct GameString {  // nstl::string
	const char *begin;
	const char *end;  // the terminating zero
	const char *cap;
};
struct GameStream {  // CMemoryStream: the whole file in memory (see original_music.cpp)
	void **vftable;  // slot 0: scalar deleting destructor (this, flags)
	char *begin;
	char *cap;
	char *end;
	char *pos;
	DWORD flags;  // bit 0: broken (e.g. no such file)
};
typedef BYTE *(__fastcall *GetFn)(void *id, void *edx);
typedef BYTE *(__fastcall *DbGetFn)(void *db, void *edx, void *id);
typedef void *(__fastcall *MakeIdFn)(void *id, void *edx, const GameString *path);
typedef int(__fastcall *ChecksumFn)(void *object, void *edx);
typedef void *(__cdecl *NewFn)(size_t size);
typedef GameStream *(__fastcall *OpenFn)(void *vfs, void *edx, const GameString *path);
typedef void(__fastcall *DeleteFn)(GameStream *self, void *edx, int flags);
DbGetFn g_db_get_next = nullptr;
volatile LONG g_db_state = 0;  // 0 not yet, 1 hooked, 2 could not
bool g_on = false;
OpenFn g_open_next = nullptr;

// --- the colours -----------------------------------------------------------------------------
struct Colour {
	const char *name;
	float hue;  // the target hue (0..1); < 0: white (little colour)
};
// 0 is the game's own (no copy); the pictures' violet is at hue 0.70
const Colour kColours[] = {
	{"violet (the game's)", 0.70f}, {"red", 0.0f}, {"green", 0.33f}, {"gold", 0.13f},
	{"cyan", 0.50f}, {"orange", 0.06f}, {"pink", 0.90f}, {"white", -1.0f},
};
const int kColourCount = sizeof(kColours) / sizeof(kColours[0]);
const float kSourceHue = 0.70f;

float Max3(float a, float b, float c)
{
	return a > b ? (a > c ? a : c) : (b > c ? b : c);
}
float Min3(float a, float b, float c)
{
	return a < b ? (a < c ? a : c) : (b < c ? b : c);
}
BYTE ToByte(float v)
{
	v = v * 255.0f + 0.5f;
	return static_cast<BYTE>(v < 0 ? 0 : v > 255 ? 255 : v);
}

void Turn(BYTE &r, BYTE &g, BYTE &b, int colour)
{
	const float R = r / 255.0f, G = g / 255.0f, B = b / 255.0f;
	const float mx = Max3(R, G, B), mn = Min3(R, G, B), d = mx - mn;
	float h = 0, s = mx > 0 ? d / mx : 0;
	const float v = mx;
	if (d > 0) {
		if (mx == R)
			h = fmodf((G - B) / d, 6.0f);
		else if (mx == G)
			h = (B - R) / d + 2.0f;
		else
			h = (R - G) / d + 4.0f;
		h /= 6.0f;
		if (h < 0)
			h += 1.0f;
	}
	if (kColours[colour].hue < 0)
		s *= 0.12f;
	else
		h = fmodf(h + (kColours[colour].hue - kSourceHue) + 2.0f, 1.0f);
	const float c = v * s, hh = h * 6.0f, x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f)), m = v - c;
	float rr = 0, gg = 0, bb = 0;
	if (hh < 1)
		rr = c, gg = x;
	else if (hh < 2)
		rr = x, gg = c;
	else if (hh < 3)
		gg = c, bb = x;
	else if (hh < 4)
		gg = x, bb = c;
	else if (hh < 5)
		rr = x, bb = c;
	else
		rr = c, bb = x;
	r = ToByte(rr + m);
	g = ToByte(gg + m);
	b = ToByte(bb + m);
}

void Turn565(BYTE *p, int colour)
{
	const unsigned c = p[0] | (p[1] << 8);
	BYTE r = static_cast<BYTE>(((c >> 11) & 31) * 255 / 31), g = static_cast<BYTE>(((c >> 5) & 63) * 255 / 63),
	     b = static_cast<BYTE>((c & 31) * 255 / 31);
	Turn(r, g, b, colour);
	const unsigned n = ((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255);
	p[0] = static_cast<BYTE>(n & 0xff);
	p[1] = static_cast<BYTE>(n >> 8);
}

// A DDS picture with its colours turned: 32-bit BGRA pixel by pixel; DXT3/DXT5 by each block's
// two end colours (their colour blocks always use 4 colours, so the order may change). Other
// formats: unchanged (false).
bool TurnPicture(std::string *dds, int colour)
{
	if (dds->size() < 128 || dds->compare(0, 4, "DDS ") != 0)
		return false;
	BYTE *b = reinterpret_cast<BYTE *>(&(*dds)[0]);
	const DWORD pf_flags = *reinterpret_cast<DWORD *>(b + 80), fourcc = *reinterpret_cast<DWORD *>(b + 84),
	            bits = *reinterpret_cast<DWORD *>(b + 88);
	const size_t n = dds->size();
	if (!(pf_flags & 4) && bits == 32) {  // uncompressed BGRA
		for (size_t i = 128; i + 4 <= n; i += 4)
			Turn(b[i + 2], b[i + 1], b[i], colour);
		return true;
	}
	if (fourcc == 0x33545844 || fourcc == 0x35545844) {  // "DXT3", "DXT5"
		for (size_t i = 128; i + 16 <= n; i += 16) {
			Turn565(b + i + 8, colour);
			Turn565(b + i + 10, colour);
		}
		return true;
	}
	return false;
}

// --- the files -------------------------------------------------------------------------------
const char kPrefix[] = "OpenHomm5-Portal-";
// Our file name after "OpenHomm5-Portal-<colour>" -> the game's file name (same folder)
struct Copy {
	const char *ours;
	const char *game;
};
const Copy kCopies[] = {
	{".xdb", "Monolith_Two_Way_Entrance.xdb"},
	{"-01.xdb", "Monolith_Two_Way_Entrance01.xdb"},
	{"-02.xdb", "Monolith_Two_Way_Entrance02.xdb"},
	{"-03.xdb", "Monolith_Two_Way_Entrance03.xdb"},
	{"-Shiny.(Material).xdb", "Monolith_Two_Way_Entrance02-ShinyMat01.1.(Material).xdb"},
	{"-Lightning.(Material).xdb", "Monolith_Two_Way_Entrance03-LightingMat01.1.(Material).xdb"},
	{"-Phantom.(Texture).xdb", "Phantom-Phantom_Clot01.(Texture).xdb"},
	{"-Phantom.(Texture).dds", "Phantom-Phantom_Clot01.(Texture).dds"},
	{"-Angle.xdb", "Angle_violet_blue.xdb"},
	{"-Angle.dds", "Angle_violet_blue.dds"},
	{"-Lightning.tga.xdb", "Lightning04.tga.xdb"},
	{"-Lightning.tga.dds", "Lightning04.tga.dds"},
	{"-Glow05.xdb", "Glow05.xdb"},
	{"-Glow05.dds", "Glow05.dds"},
	{"-Glow04.xdb", "Glow04.xdb"},
	{"-Glow04.dds", "Glow04.dds"},
};
const int kCopyCount = sizeof(kCopies) / sizeof(kCopies[0]);

// In our copies of the records, every name of the chain becomes ours: "Glow05.xdb" (in an
// href) -> "OpenHomm5-Portal-3-Glow05.xdb". A name is replaced only whole: after "/" or "\""
// and before "#" or "\"" (hrefs and DestName).
std::string WithOurNames(std::string text, int colour)
{
	for (int k = 0; k < kCopyCount; ++k) {
		const std::string game = kCopies[k].game;
		const std::string ours = std::string(kPrefix) + static_cast<char>('0' + colour) + kCopies[k].ours;
		size_t at = 0;
		while ((at = text.find(game, at)) != std::string::npos) {
			const char before = at ? text[at - 1] : '"';
			const size_t after_at = at + game.size();
			const char after = after_at < text.size() ? text[after_at] : '"';
			if ((before == '/' || before == '"') && (after == '#' || after == '"')) {
				text.replace(at, game.size(), ours);
				at += ours.size();
			}
			else
				at += game.size();
		}
	}
	return text;
}

LONG g_served = 0;

GameStream *Serve(void *vfs, const GameString *path, GameStream *opened)
{
	const char *b = path->begin, *e = path->end;
	const char *name = e;
	while (name > b && name[-1] != '/' && name[-1] != '\\')
		--name;
	const size_t plen = sizeof(kPrefix) - 1;
	if (static_cast<size_t>(e - name) < plen + 2 || _strnicmp(name, kPrefix, plen) != 0)
		return opened;  // (cheap test first: the game opens thousands of files)
	const int colour = name[plen] - '0';
	if (colour < 1 || colour >= kColourCount)
		return opened;
	const char *rest = name + plen + 1;
	int copy = -1;
	for (int k = 0; k < kCopyCount; ++k)
		if (static_cast<size_t>(e - rest) == strlen(kCopies[k].ours) && _strnicmp(rest, kCopies[k].ours, e - rest) == 0)
			copy = k;
	if (copy < 0)
		return opened;
	try {
		std::string game_path(b, name);
		game_path += kCopies[copy].game;
		const GameString game_name = {game_path.c_str(), game_path.c_str() + game_path.size(),
		                              game_path.c_str() + game_path.size()};
		GameStream *s = g_open_next(vfs, nullptr, &game_name);
		const bool ok = s && !(s->flags & 1) && s->begin && s->end > s->begin;
		std::string data;
		if (ok)
			data.assign(s->begin, s->end);
		if (s)
			reinterpret_cast<DeleteFn>(s->vftable[0])(s, nullptr, 1);
		if (!ok) {
			hooklog::Write("portal colours: %s: the game's %s was not found", std::string(b, e).c_str(), game_path.c_str());
			return opened;
		}
		if (_strnicmp(e - 4, ".dds", 4) == 0) {
			if (!TurnPicture(&data, colour))
				hooklog::Write("portal colours: %s: a picture format we do not turn: served as it is", game_path.c_str());
		}
		else
			data = WithOurNames(data, colour);
		GameStream *ours = static_cast<GameStream *>(NewGameStream(data));
		if (!ours)
			return opened;
		if (opened)
			reinterpret_cast<DeleteFn>(opened->vftable[0])(opened, nullptr, 1);
		if (InterlockedIncrement(&g_served) <= 40)
			hooklog::Write("portal colours: served %s (%s, %s, %u bytes)", std::string(b, e).c_str(), kCopies[copy].game,
			               kColours[colour].name, static_cast<unsigned>(data.size()));
		return ours;
	} catch (...) {  // out of memory: as the game opened it
		return opened;
	}
}

void HookDatabase();

GameStream *__fastcall OpenFileHook(void *vfs, void *edx, const GameString *path)
{
	if (g_on && g_db_state == 0)
		HookDatabase();
	GameStream *s = g_open_next(vfs, edx, path);
	if (!path || !path->begin || path->end <= path->begin || path->end - path->begin > 400)
		return s;
	return Serve(vfs, path, s);
}

// --- the map ---------------------------------------------------------------------------------
DWORD Read(const void *p, int offset)
{
	__try {
		return *reinterpret_cast<const DWORD *>(reinterpret_cast<const BYTE *>(p) + offset);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

bool PathOf(const BYTE *resource, char *out, size_t size)
{
	out[0] = 0;
	__try {
		const char *b = *reinterpret_cast<const char *const *>(resource + kResourcePath);
		const char *e = *reinterpret_cast<const char *const *>(resource + kResourcePath + 4);
		if (!b || e <= b || static_cast<size_t>(e - b) >= size)
			return false;
		memcpy(out, b, e - b);
		out[e - b] = 0;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		out[0] = 0;
		return false;
	}
}

// A resource by its path, loaded (as the game's dialog_scene command: path -> id -> resource).
BYTE *LoadId(void *id, const GameString *s)
{
	__try {
		void *made = reinterpret_cast<MakeIdFn>(kMakeId)(id, nullptr, s);
		return made ? reinterpret_cast<GetFn>(kGetResource)(made, nullptr) : nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

BYTE *ByPath(const char *path)
{
	void *id = calloc(1, 0x40);  // a database id (0x20 bytes used; kept: tiny)
	if (!id)
		return nullptr;
	const size_t n = strlen(path);
	const GameString s = {path, path + n, path + n};
	return LoadId(id, &s);
}

bool AddRef(BYTE *resource)
{
	__try {
		*reinterpret_cast<LONG *>(resource + 8) += 1;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The object a database pointer leads to, loaded: a resource not read yet is a stand-in (+0x2c
// zero) whose database id (+0xc) the database reads, as CDBPtr::Get (0x004a5020) does.
BYTE *Resolve(const BYTE *dbptr)
{
	BYTE *object = reinterpret_cast<BYTE *>(Read(dbptr, 0));
	if (!object)
		return nullptr;
	__try {
		if (object[0x2c] != 0)
			return object;
		return reinterpret_cast<GetFn>(kGetResource)(object + 0xc, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

// A byte for byte copy of a database object (a building type, an effect): the same name and
// database id, and the same data. The world names the type and its effect in its checksums:
// a type of another name (robot 20261109-153143-lan), the type's own copy (slot 1, which
// leaves the name empty: 20261109-153627-lan), or a copy pointing at our coloured effect by
// its own name (20261109-154028-lan) each put two PCs out of step at once; a copy with the
// game's effect did not (20261109-155031-lan). Its checksum is computed (and cached) before,
// so the copy keeps the original's. It shares the original's lists and strings, so it is never
// freed: its count stays above zero for the session.
BYTE *CopyOf(BYTE *object, int size)
{
	__try {
		void **vt = *reinterpret_cast<void ***>(object);
		reinterpret_cast<ChecksumFn>(vt[kChecksumSlot])(object, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
	BYTE *copy = static_cast<BYTE *>(reinterpret_cast<NewFn>(kGameNew)(size));
	if (!copy)
		return nullptr;
	__try {
		memcpy(copy, object, size);
		*reinterpret_cast<LONG *>(copy + 8) = 1000;  // never 0
		return copy;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

// The game's effect (copied: its name) with the models and particles of our coloured one.
BYTE *ColouredEffect(BYTE *game_effect, const BYTE *ours)
{
	BYTE *copy = CopyOf(game_effect, kEffectSize);
	if (!copy)
		return nullptr;
	__try {
		memcpy(copy + kEffectLists, ours + kEffectLists, kEffectListsSize);  // (shared with ours: both kept)
		return copy;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool SetEffect(BYTE *type, BYTE *effect)
{
	__try {
		for (int at : {kEffect, kEffectWhenOwned}) {
			*reinterpret_cast<BYTE **>(type + at) = effect;  // (the original keeps its own reference)
			type[at + 4] = 1;                                // "got"
			*reinterpret_cast<LONG *>(effect + 8) += 1;
		}
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

BYTE *g_types[kColourCount] = {};  // our copies of the building type, once made

BYTE *OurType(BYTE *game_type, int colour)
{
	if (!g_types[colour]) {
		char path[160], name[256];
		_snprintf_s(path, sizeof(path), _TRUNCATE, "/Effects/_(Effect)/Buildings/%s%d.xdb#xpointer(/Effect)", kPrefix, colour);
		BYTE *ours = ByPath(path);
		BYTE *game_effect = Resolve(game_type + kEffect);
		BYTE *effect = ours && Read(ours, 0) == kEffectVftable && game_effect && Read(game_effect, 0) == kEffectVftable
		                   ? ColouredEffect(game_effect, ours)
		                   : nullptr;
		BYTE *copy = effect ? CopyOf(game_type, kSharedSize) : nullptr;
		if (copy && Read(copy, 0) == kSharedVftable && SetEffect(copy, effect))
			g_types[colour] = copy;
		PathOf(copy ? copy : game_type, name, sizeof(name));
		hooklog::Write("portal colours: our %s building type %s (effect %p, vtable %08lx; copy %p named \"%s\")", kColours[colour].name,
		               g_types[colour] ? "made" : "NOT made", effect, effect ? Read(effect, 0) : 0, copy, name);
	}
	return g_types[colour];
}

bool CacheChecksum(BYTE *object)
{
	__try {
		void **vt = *reinterpret_cast<void ***>(object);
		reinterpret_cast<ChecksumFn>(vt[kBuildingChecksumSlot])(object, nullptr);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool SetType(BYTE *building, BYTE *type)
{
	__try {
		*reinterpret_cast<BYTE **>(building + kShared) = type;  // (the stand-in keeps its count: kept)
		building[kShared + 4] = 1;                              // "got"
		*reinterpret_cast<LONG *>(type + 8) += 1;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void ReadPos(const BYTE *object, float *pos)
{
	__try {
		memcpy(pos, object + 0x44, 3 * sizeof(float));  // SAdvMapObject: Pos
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

// False when the map's object list is not there yet (a map's data asked for before it is read
// whole, as the waiting room does: then the next time).
bool ColourMap(BYTE *desc)
{
	const BYTE *begin = reinterpret_cast<const BYTE *>(Read(desc, kMapObjects));
	const BYTE *end = reinterpret_cast<const BYTE *>(Read(desc, kMapObjects + 4));
	if (!begin && !end)
		return false;
	if (!begin || end < begin || (end - begin) % 8 != 0 || end - begin > 8 * 200000) {
		hooklog::Write("portal colours: the map's object list is not as expected (%p %p): nothing coloured", begin, end);
		return true;
	}
	const int count = static_cast<int>((end - begin) / 8);
	std::map<int, int> colour_of;  // GroupID -> colour, in the order the pairs come
	int portals = 0, coloured = 0, already = 0, failed = 0;
	char path[256];
	for (int i = 0; i < count; ++i) {
		BYTE *building = reinterpret_cast<BYTE *>(Read(begin + 8 * i, 0));
		if (!building || Read(building, 0) != kBuildingVftable)
			continue;
		const BYTE *type = reinterpret_cast<const BYTE *>(Read(building, kShared));
		if (!type || !PathOf(type, path, sizeof(path)))
			continue;
		bool ours_already = false;
		for (int c = 1; c < kColourCount; ++c)
			ours_already = ours_already || (g_types[c] && type == g_types[c]);
		if (ours_already) {
			++already;  // this map's data was coloured before (the database keeps it)
			continue;
		}
		if (!strstr(path, "/Monolith_Two_Way.("))
			continue;
		++portals;
		const int group = static_cast<int>(Read(building, kGroup));
		auto it = colour_of.find(group);
		if (it == colour_of.end())
			it = colour_of.emplace(group, static_cast<int>(colour_of.size()) % kColourCount).first;
		const int colour = it->second;
		if (colour == 0)
			continue;  // the game's own violet
		BYTE *game_type = Resolve(building + kShared);
		BYTE *ours = game_type && Read(game_type, 0) == kSharedVftable ? OurType(game_type, colour) : nullptr;
		if (!ours || !CacheChecksum(building) || !SetType(building, ours)) {
			++failed;
			continue;
		}
		++coloured;
		if (coloured <= 40) {
			float pos[3] = {};
			ReadPos(building, pos);
			hooklog::Write("portal colours: portal of pair %d at %.0f,%.0f (floor %ld): %s", group, pos[0], pos[1],
			               static_cast<long>(Read(building, 0x54)), kColours[colour].name);
		}
	}
	hooklog::Write("portal colours: the map: %d objects, %d two-way portals in %d pairs: %d coloured (the first pair keeps the "
	               "game's violet), %d done before, %d failed",
	               count, portals, static_cast<int>(colour_of.size()), coloured, already, failed);
	return true;
}

CRITICAL_SECTION g_lock;
BYTE *g_done[64] = {};  // the maps' data coloured so far (the database keeps a map's data)
int g_done_count = 0;

// Every resource the database gives: a map's data (NDb::SAdvMapDesc) is coloured once, the
// first time it is asked for, before the game makes a world from it: a new game (the world's
// creation 0x00c9fa80), a loaded one, the waiting room's map.
BYTE *__fastcall DbGetHook(void *db, void *edx, void *id)
{
	BYTE *resource = g_db_get_next(db, edx, id);
	if (!resource || Read(resource, 0) != kMapDescVftable)
		return resource;
	EnterCriticalSection(&g_lock);
	bool done = false;
	for (int i = 0; i < g_done_count && !done; ++i)
		done = g_done[i] == resource;
	if (!done && ColourMap(resource)) {
		if (g_done_count == 64)
			g_done_count = 0;
		g_done[g_done_count++] = resource;
	}
	LeaveCriticalSection(&g_lock);
	return resource;
}

// The database exists once the game has started up: its "resource of this id" gets our wrap.
void HookDatabase()
{
	const BYTE *db = reinterpret_cast<const BYTE *>(Read(reinterpret_cast<const void *>(kDatabase), 0));
	if (!db || InterlockedCompareExchange(&g_db_state, 1, 0) != 0)
		return;
	const DWORD vtable = Read(db, 0);
	const DWORD slot = vtable + 4 * kDatabaseGetSlot;
	const DWORD original = Read(reinterpret_cast<const void *>(slot), 0);
	g_db_get_next = reinterpret_cast<DbGetFn>(hookpatch::PatchPointer(slot, original, reinterpret_cast<const void *>(&DbGetHook)));
	if (!g_db_get_next) {
		InterlockedExchange(&g_db_state, 2);
		hooklog::Write("portal colours: the game's database is not as expected (vtable %08lx): off", vtable);
		return;
	}
	hooklog::Write("portal colours: on (the database's vtable %08lx slot %d, %08lx): each pair of two-way portals shines in a "
	               "colour of its own, %d colours",
	               vtable, kDatabaseGetSlot, original, kColourCount);
}

}  // namespace

void ApplyPortalColorsFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	// Our files are served whatever the setting: a save from a PC with coloured portals may
	// name them.
	g_open_next = reinterpret_cast<OpenFn>(
		hookpatch::PatchPointer(kWinVfsOpenSlot, kWinVfsOpen, reinterpret_cast<const void *>(&OpenFileHook)));
	if (!g_open_next) {
		hooklog::Write("portal colours: the game's file system is not as expected (another build?): off");
		return;
	}
	if (GetPrivateProfileIntW(L"ui", L"portal_colors", 0, path) == 0) {
		hooklog::Write("portal colours: off ([ui] portal_colors=0: the portals look as in the game)");
		return;
	}
	InitializeCriticalSection(&g_lock);
	g_on = true;
	hooklog::Write("portal colours: on once the game's database is made");
}

}  // namespace mods

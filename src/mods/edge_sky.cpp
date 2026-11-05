// Mod: black past the adventure map's edge instead of a violet sky
// (docs/plans/lobby-audio-classes.md, item 5).
//
// Zoomed out (camera.cpp), players see past the map's edge. On random maps
// that past is often a flat dark violet with a lavender corner: a sky dome, a
// big textured shape drawn around the scene. It is the Sylvan siege
// battlefield's sky (/_(Model)/SkyDomes/SkyDome_Ramp_SphereCS.xdb, textures
// SkyDomes/ScyDome_Rampart_siege/sky1..6: the flat caps are (31, 29, 51), the
// sides end in (86, 80, 115), the two colours of the owner's screenshot).
//
// Who draws it: each map names its ambient light (NDb::SAmbientLight), and a
// light may name a sky dome. The random map generator picks one of four lights
// (RMG/Params/Default.xdb, GroundTerrainLights). MMH5.5 put
// Lights/_(AmbientLight)/Arena/Dusk_light_BoatArena01(VicTest).xdb in that list
// and gave its copy of that light (MMH55-Data.pak) this sky dome; the original
// game's copy has none. In the game data only the generator's list names this
// light: no battlefield uses it, so battle skies do not change.
//
// The underground (the owner's game, 2026-11-05: "map borders aren't black when in
// underground"): the generator gives every random map's underground one light,
// Lights/_(AmbientLight)/Tests/underground.xdb (RMG/Params/Default.xdb,
// UndergroundTerrainLight), which has a sky dome of its own
// (/_(Model)/SkyDomes/Virtual/SkyDome_Dung_Virt.xdb) and a black BackColor. Only
// adventure maps' undergrounds name it (random maps, six multiplayer and single
// maps), and Maps/CombatArenas/AbandonedMine/AbandonedMine.xdb, an old map file
// whose only use of it is an "underground lights" list (a battlefield has no
// underground): served without its dome too.
//
// With [camera] edge_sky=none we wrap the game's "open file" function (the
// file system's table, NVFS::CWinVFS vftable 0x00e66500, slot 7 = 0x009e0020,
// stacked with the other mods that wrap it) and serve that light with an empty
// <SkyDome/>, as the lights without a sky dome have. Past the edge the screen
// then stays as cleared: black (every light's BackColor is black), like
// unexplored land. Drawn, not simulated: each player may choose.
//
// Settings, h5-together.ini [camera]:
//   edge_sky=game          the game's sky (nothing installed)
//   edge_sky=none          no sky dome on the random maps' dusk light: black
//   edge_sky_test_light=   testing: one more light file served without its
//                          sky dome (a path in the game data, e.g. the robot
//                          map's Lights/_(AmbientLight)/AdvMap/Addon/AC1M4.xdb)
#include "edge_sky.h"

#include <windows.h>
#include <string.h>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

// ---- the game (vanilla 3.1 layout, also MMH55_Utility) ----

const DWORD kWinVfsOpenSlot = 0x00e6651c;       // NVFS::CWinVFS vftable 0x00e66500, slot 7: OpenFile
const DWORD kWinVfsOpen = 0x009e0020;           // __thiscall(CWinVFS *, const nstl::string *path), ret 4
const DWORD kMemoryStreamVftable = 0x00e0a980;  // CMemoryStream
const DWORD kMemoryStreamCtor = 0x004034d0;     // __thiscall(this): empty stream, returns this
const DWORD kStreamSetSize = 0x008c7610;        // __thiscall(this, int size), ret 4: new buffer of size bytes
const DWORD kGameNew = 0x00878fd0;              // cdecl void *(size): the game's operator new

const char kDuskLight[] = "lights/_(ambientlight)/arena/dusk_light_boatarena01(victest).xdb";
const char kUndergroundLight[] = "lights/_(ambientlight)/tests/underground.xdb";

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

OpenFn g_open_next = nullptr;
std::string g_test_light;  // edge_sky_test_light, normalized; empty = none
volatile LONG g_served = 0;

// A stream with this content instead of the one the game opened (the same as
// original_music.cpp's WithContent).
GameStream *WithContent(GameStream *s, const std::string &data)
{
	const SetSizeFn set_size = reinterpret_cast<SetSizeFn>(kStreamSetSize);
	const auto ok = [&data](const GameStream *m) {
		return !(m->flags & 1) && m->begin && static_cast<size_t>(m->end - m->begin) == data.size();
	};
	if (s->vftable == reinterpret_cast<void **>(kMemoryStreamVftable)) {
		// the usual case (a deflated zip entry): the same stream, new content
		set_size(s, nullptr, static_cast<int>(data.size()));
		if (ok(s))
			memcpy(s->begin, data.data(), data.size());
		return s;
	}
	// a mapped view of the pak (a stored entry): a new memory stream instead
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

GameStream *__fastcall OpenFileHook(void *vfs, void *edx, const GameString *path)
{
	GameStream *s = g_open_next(vfs, edx, path);
	if (!s || (s->flags & 1) || !path || !path->begin || path->end <= path->begin)
		return s;
	const size_t length = static_cast<size_t>(path->end - path->begin);
	if (length < 8 || length > 300 || _strnicmp(path->end - 4, ".xdb", 4) != 0)
		return s;  // (cheap test first: the game opens thousands of files)
	const std::string key = edgesky::NormalizePath(std::string(path->begin, path->end));
	if (key != kDuskLight && key != kUndergroundLight && (g_test_light.empty() || key != g_test_light))
		return s;
	if (!s->begin || s->end < s->begin || s->pos != s->begin)
		return s;  // not freshly opened: left alone
	std::string light;
	if (!edgesky::WithoutSkyDome(std::string(s->begin, s->end), &light))
		return s;  // no sky dome there (another MMH5.5 version?): nothing to do
	s = WithContent(s, light);
	if (InterlockedIncrement(&g_served) <= 20)
		hooklog::Write("edge sky: %s served without its sky dome (edge_sky=none)", key.c_str());
	return s;
}

}  // namespace

void ApplyEdgeSkyFromSettings()
{
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);  // h5-together-<instance>.ini for the robot
	wchar_t choice[16] = L"";
	GetPrivateProfileStringW(L"camera", L"edge_sky", L"game", choice, 16, ini);
	if (lstrcmpiW(choice, L"none") != 0) {
		if (choice[0] && lstrcmpiW(choice, L"game") != 0)
			hooklog::Write("edge sky: edge_sky=%ls is not game or none: the game's sky", choice);
		else
			hooklog::Write("edge sky: the game's sky past the map's edge (edge_sky=game)");
		return;
	}
	wchar_t test[MAX_PATH] = L"";
	GetPrivateProfileStringW(L"camera", L"edge_sky_test_light", L"", test, MAX_PATH, ini);
	char narrow[MAX_PATH * 2] = "";
	WideCharToMultiByte(CP_ACP, 0, test, -1, narrow, sizeof(narrow), nullptr, nullptr);
	g_test_light = edgesky::NormalizePath(narrow);
	g_open_next = reinterpret_cast<OpenFn>(
		hookpatch::PatchPointer(kWinVfsOpenSlot, kWinVfsOpen, reinterpret_cast<const void *>(&OpenFileHook)));
	if (!g_open_next) {
		hooklog::Write("edge sky: the game's file system is not as expected (%08lx): the game's sky", kWinVfsOpenSlot);
		return;
	}
	hooklog::Write("edge sky: edge_sky=none: random maps' dusk light and the undergrounds' light without their sky "
	               "dome, black past the map's edge%s%s",
	               g_test_light.empty() ? "" : "; also (test) ", g_test_light.c_str());
}

}  // namespace mods

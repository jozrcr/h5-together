// Mod: a bigger vertex pool for big maps zoomed out ([camera] vertex_pool=4, on: an exact
// speed-up, the same picture; docs/plans/map-drawing.md).
//
// In plain words: the game builds the 3D models' vertices into one shared "dynamic" vertex
// pool that the graphics card reads, and keeps them there while they do not change. It
// made that pool for the game's own zoom: zoomed out on a big map (robot, 2026-11-04, the
// owner's 6-player map at 2560x1440, 250%), the models on screen do not fit, and every
// other frame the pool is full: the game then waits and throws everything in it away
// ("discard"), and every model is built again (14,700 builds a second, 8.5 ms of a 28 ms
// frame, against 7,100 and about 3 ms with room enough). With the pool 4 times as large
// (about 80 MB more video memory): no discards, 36 -> 49 frames a second there (70 with
// the lighter animations).
//
// How: the pools are made with the size in "mov ecx, 0C0000h" at 0x00c324a6, just before
// the call to 0x00c3a180 (the pools of map 0x00f79ab4); this DLL loads before the game
// makes them, and multiplies that number. When a pool is full (in its allocation
// 0x00c3a660: the block found is still in use by the last gfx_lag frames, 0x009ae800),
// the game moves its frame counter 0x00f79a50 on by 5 (0x00c315d0), locks the buffer with
// discard and calls 0x00c357a0: everything in it is marked dying, and the scene walk
// (0x009ba480) builds each model again at 0x009ba654 (its buffer, +0x14, missing or dying).
//
// [camera] vb_trace=1 (hidden, research): every 10 s, how often the models' builders
// (NGScene::CVBCombiner, vtable 0x00e54e58) are asked and why they build, where the builds
// come from, their cost and size, the pool's discards and the free video memory. What
// makes a model build again: its CAnimationWatch (vtable 0x00e8f48c, slot 7 0x00c69bf0)
// watches the parts' NGScene::CBind nodes (vtable 0x00e66464), which follow the
// skeletons; when one moved on, that part's cached vertices are freed and the builder's
// +0x5d is set (by its bounds, slot 9 0x008f1da0 -> 0x00c696a0).
#include "vertex_pool.h"

#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include <d3d9.h>

#include "../hook/d3d9_hook.h"
#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kNeedsCell = 0x00e54e58 + 7 * 4;
const DWORD kNeeds = 0x00c68db0;
// slot 9 (0x008f1da0): the bounds, asked by the scene walk; it refreshes B and C first
// (0x008f1dc0) and, if one changed, computes the bounds again (0x00c696a0), which sets
// +0x5d: "rebuild the vertices"
const DWORD kBoundsCell = 0x00e54e58 + 9 * 4;
const DWORD kBounds = 0x008f1da0;
// NGScene::CAnimationWatch (vtable 0x00e8f48c): slot 7 (0x00c69bf0) does the work (slot 8
// is empty): its watched edges (node, version) at +0x1c..+0x20, 8 bytes each; when one
// moved on, the matching part's cached vertices are freed and it says "changed"
const DWORD kWatchCell = 0x00e8f48c + 7 * 4;
const DWORD kWatch = 0x00c69bf0;

typedef bool(__fastcall *NeedsFn)(void *self, void *edx);
NeedsFn g_next = nullptr;
typedef void *(__fastcall *BoundsFn)(void *self, void *edx);
BoundsFn g_bounds_next = nullptr;
LONG g_bounds_calls = 0, g_bounds_changed[3] = {};
std::map<DWORD, LONG> g_bounds_classes[3];
NeedsFn g_watch_next = nullptr;
LONG g_watch_calls = 0, g_watch_yes = 0, g_watched = 0, g_watched_changed = 0;
// slot 8 (0x00c6a360): the rebuild itself, timed, with its size (the parts' vertices, 20
// bytes each: part +0x1c -> node +0x14 -> +0x18..+0x1c, as 0x00c6a130 counts them)
const DWORD kRebuildCell = 0x00e54e58 + 8 * 4;
const DWORD kRebuild = 0x00c6a360;
typedef void(__fastcall *RebuildFn)(void *self, void *edx);
RebuildFn g_rebuild_next = nullptr;
LONGLONG g_rebuild_ticks = 0;
LONG g_rebuilds = 0;
double g_rebuild_vertices = 0;
struct Heavy {
	const void *combiner;
	double ms;
	LONG count;
	DWORD vertices;
};
std::map<const void *, Heavy> g_heavy;
LARGE_INTEGER g_freq = {};
std::map<DWORD, LONG> g_rebuild_callers;
// the dynamic vertex pool's "full: wait 5 frames and discard everything" (0x00c3a6c4..:
// in the pool's allocation 0x00c3a660, the call 0x00c3a6cd to 0x00c357a0)
const DWORD kDiscardCall = 0x00c3a6cd, kDiscardTarget = 0x00c357a0;
typedef int(__fastcall *DiscardFn)(void *self, void *edx);
DiscardFn g_discard_next = nullptr;
LONG g_discards = 0;
int __fastcall DiscardHook(void *self, void *edx)
{
	++g_discards;
	return g_discard_next(self, edx);
}
LONG g_watch_held = 0;

std::map<DWORD, LONG> g_watched_classes, g_watched_changed_classes;

struct Edge {
	DWORD node_off, version_off;
	const char *name;
};
const Edge kEdges[] = {{0x54, 0x58, "A"}, {0x40, 0x44, "B"}, {0x4c, 0x50, "C"}};

LONG g_calls = 0, g_yes = 0, g_changed[3] = {}, g_flag5c = 0, g_flag5d = 0, g_none = 0;
std::map<DWORD, LONG> g_classes[3];  // changed node's vtable -> count
DWORD g_window = 0;

DWORD Read(const void *p, DWORD off)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off);
}

void ClassName(DWORD vtable, char *out, size_t size)
{
	__try {
		const DWORD col = *reinterpret_cast<const DWORD *>(vtable - 4);
		const DWORD td = *reinterpret_cast<const DWORD *>(col + 12);
		_snprintf_s(out, size, _TRUNCATE, "%s", reinterpret_cast<const char *>(td + 8));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		_snprintf_s(out, size, _TRUNCATE, "%08lx", vtable);
	}
}

std::string ClassOf(DWORD vtable)
{
	char out[96];
	ClassName(vtable, out, sizeof(out));
	return out;
}

void Report()
{
	const DWORD now = GetTickCount();
	if (!g_window) {
		g_window = now;
		return;
	}
	if (now - g_window < 10000)
		return;
	const double s = (now - g_window) / 1000.0;
	std::string classes;
	for (int e = 0; e < 3; ++e) {
		int shown = 0;
		for (int round = 0; round < 3 && !g_classes[e].empty(); ++round) {
			auto best = g_classes[e].begin();
			for (auto it = g_classes[e].begin(); it != g_classes[e].end(); ++it)
				if (it->second > best->second)
					best = it;
			char item[160];
			_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%s %s %.0f/s", shown++ ? ", " : (classes.empty() ? "" : "; "),
			            kEdges[e].name, ClassOf(best->first).c_str(), best->second / s);
			classes += item;
			g_classes[e].erase(best);
		}
		g_classes[e].clear();
	}
	std::string bclasses;
	for (int e = 1; e < 3; ++e) {
		for (int round = 0; round < 4 && !g_bounds_classes[e].empty(); ++round) {
			auto best = g_bounds_classes[e].begin();
			for (auto it = g_bounds_classes[e].begin(); it != g_bounds_classes[e].end(); ++it)
				if (it->second > best->second)
					best = it;
			char item[160];
			_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%s %s %.0f/s", bclasses.empty() ? "" : ", ", kEdges[e].name,
			            ClassOf(best->first).c_str(), best->second / s);
			bclasses += item;
			g_bounds_classes[e].erase(best);
		}
		g_bounds_classes[e].clear();
	}
	hooklog::Write("vb trace: combiners asked %.0f/s, rebuilt %.0f/s; changed: A %.0f/s, B %.0f/s, C %.0f/s, flag 5c %.0f/s, "
	               "flag 5d %.0f/s, none of these %.0f/s; %s",
	               g_calls / s, g_yes / s, g_changed[0] / s, g_changed[1] / s, g_changed[2] / s, g_flag5c / s, g_flag5d / s,
	               g_none / s, classes.c_str());
	hooklog::Write("vb trace: bounds asked %.0f/s; changed there: B %.0f/s, C %.0f/s; %s", g_bounds_calls / s,
	               g_bounds_changed[1] / s, g_bounds_changed[2] / s, bclasses.c_str());
	g_bounds_calls = 0;
	memset(g_bounds_changed, 0, sizeof(g_bounds_changed));
	std::string w, wc;
	for (auto *pair : {&g_watched_classes, &g_watched_changed_classes}) {
		std::string &out = pair == &g_watched_classes ? w : wc;
		for (int round = 0; round < 4 && !pair->empty(); ++round) {
			auto best = pair->begin();
			for (auto it = pair->begin(); it != pair->end(); ++it)
				if (it->second > best->second)
					best = it;
			char item[160];
			_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%s %.0f/s", out.empty() ? "" : ", ", ClassOf(best->first).c_str(),
			            best->second / s);
			out += item;
			pair->erase(best);
		}
		pair->clear();
	}
	hooklog::Write("vb trace: animation watches asked %.0f/s, changed %.0f/s, held %.0f/s; watched nodes %.0f/s (%s); moved on %.0f/s (%s)",
	               g_watch_calls / s, g_watch_yes / s, g_watch_held / s, g_watched / s, w.c_str(), g_watched_changed / s, wc.c_str());
	g_watch_calls = g_watch_yes = g_watched = g_watched_changed = g_watch_held = 0;
	std::string heavy;
	for (int round = 0; round < 6 && !g_heavy.empty(); ++round) {
		auto best = g_heavy.begin();
		for (auto it = g_heavy.begin(); it != g_heavy.end(); ++it)
			if (it->second.ms > best->second.ms)
				best = it;
		char item[160];
		_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%p: %.1f ms/s, %.0f rebuilds/s, %lu vertices", heavy.empty() ? "" : "; ",
		            best->first, best->second.ms / s, best->second.count / s, best->second.vertices);
		heavy += item;
		g_heavy.erase(best);
	}
	hooklog::Write("vb trace: rebuilds %.0f/s, %.1f ms/s, %.0f vertices/s, %zu combiners; the heaviest: %s", g_rebuilds / s,
	               g_rebuild_ticks * 1000.0 / g_freq.QuadPart / s, g_rebuild_vertices / s, g_heavy.size() + 6, heavy.c_str());
	g_heavy.clear();
	std::string callers;
	for (int round = 0; round < 6 && !g_rebuild_callers.empty(); ++round) {
		auto best = g_rebuild_callers.begin();
		for (auto it = g_rebuild_callers.begin(); it != g_rebuild_callers.end(); ++it)
			if (it->second > best->second)
				best = it;
		char item[48];
		_snprintf_s(item, sizeof(item), _TRUNCATE, "%s%08lx %.0f/s", callers.empty() ? "" : ", ", best->first, best->second / s);
		callers += item;
		g_rebuild_callers.erase(best);
	}
	g_rebuild_callers.clear();
	IDirect3DDevice9 *device = d3dhook::PresentedDevice();
	hooklog::Write("vb trace: rebuilds called from %s; the vertex pool discarded everything %.1f times/s; video memory free %u MB",
	               callers.c_str(), g_discards / s, device ? device->GetAvailableTextureMem() >> 20 : 0);
	g_discards = 0;
	g_rebuilds = 0;
	g_rebuild_ticks = 0;
	g_rebuild_vertices = 0;
	g_calls = g_yes = g_flag5c = g_flag5d = g_none = 0;
	memset(g_changed, 0, sizeof(g_changed));
	g_window = now;
}

bool __fastcall NeedsHook(void *self, void *edx)
{
	DWORD before[3];
	for (int e = 0; e < 3; ++e)
		before[e] = Read(self, kEdges[e].version_off);
	const BYTE *b = static_cast<const BYTE *>(self);
	const bool flag5c = b[0x5c] != 0, flag5d = b[0x5d] != 0;
	const bool yes = g_next(self, edx);
	++g_calls;
	if (yes) {
		++g_yes;
		bool any = false;
		for (int e = 0; e < 3; ++e) {
			const DWORD node = Read(self, kEdges[e].node_off);
			if (node && Read(self, kEdges[e].version_off) != before[e]) {
				++g_changed[e];
				++g_classes[e][Read(reinterpret_cast<void *>(node), 0)];
				any = true;
			}
		}
		if (flag5c) {
			++g_flag5c;
			any = true;
		}
		if (flag5d) {
			++g_flag5d;
			any = true;
		}
		if (!any)
			++g_none;
	}
	Report();
	return yes;
}

DWORD Vertices(const void *combiner)
{
	DWORD n = 0;
	const BYTE *list = reinterpret_cast<const BYTE *>(Read(combiner, 0x40));
	if (!list)
		return 0;
	const DWORD *p = reinterpret_cast<const DWORD *>(Read(list, 0x14)), *end = reinterpret_cast<const DWORD *>(Read(list, 0x18));
	for (; p && p < end; ++p) {
		const BYTE *part = reinterpret_cast<const BYTE *>(*p);
		const BYTE *node = part ? reinterpret_cast<const BYTE *>(Read(part, 0x1c)) : nullptr;
		const BYTE *mesh = node ? reinterpret_cast<const BYTE *>(Read(node, 0x14)) : nullptr;
		if (mesh)
			n += (Read(mesh, 0x1c) - Read(mesh, 0x18)) / 20;
	}
	return n;
}

DWORD SafeVertices(const void *combiner)
{
	__try {
		return Vertices(combiner);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

void __fastcall RebuildHook(void *self, void *edx)
{
	++g_rebuild_callers[reinterpret_cast<DWORD>(_ReturnAddress())];
	LARGE_INTEGER a, b;
	QueryPerformanceCounter(&a);
	g_rebuild_next(self, edx);
	QueryPerformanceCounter(&b);
	const DWORD v = SafeVertices(self);
	++g_rebuilds;
	g_rebuild_ticks += b.QuadPart - a.QuadPart;
	g_rebuild_vertices += v;
	Heavy &h = g_heavy[self];
	h.ms += (b.QuadPart - a.QuadPart) * 1000.0 / g_freq.QuadPart;
	++h.count;
	h.vertices = v;
}

void *__fastcall BoundsHook(void *self, void *edx)
{
	DWORD before[3];
	for (int e = 1; e < 3; ++e)
		before[e] = Read(self, kEdges[e].version_off);
	void *r = g_bounds_next(self, edx);
	++g_bounds_calls;
	for (int e = 1; e < 3; ++e) {
		const DWORD node = Read(self, kEdges[e].node_off);
		if (node && Read(self, kEdges[e].version_off) != before[e]) {
			++g_bounds_changed[e];
			++g_bounds_classes[e][Read(reinterpret_cast<void *>(node), 0)];
		}
	}
	return r;
}

bool __fastcall WatchHook(void *self, void *edx)
{
	const DWORD begin = Read(self, 0x1c), end = Read(self, 0x20);
	const DWORD n = end > begin ? (end - begin) / 8 : 0;
	DWORD nodes[64], versions[64];
	const DWORD count = n < 64 ? n : 64;
	for (DWORD i = 0; i < count; ++i) {
		nodes[i] = *reinterpret_cast<const DWORD *>(begin + 8 * i);
		versions[i] = *reinterpret_cast<const DWORD *>(begin + 8 * i + 4);
	}
	const bool yes = g_watch_next(self, edx);
	++g_watch_calls;
	if (yes)
		++g_watch_yes;
	if (Read(self, 0x1c) == begin && Read(self, 0x20) == end) {  // the same list (not rebuilt)
		for (DWORD i = 0; i < count; ++i) {
			if (!nodes[i])
				continue;
			const DWORD vt = Read(reinterpret_cast<void *>(nodes[i]), 0);
			++g_watched;
			++g_watched_classes[vt];
			if (*reinterpret_cast<const DWORD *>(begin + 8 * i + 4) != versions[i]) {
				++g_watched_changed;
				++g_watched_changed_classes[vt];
			}
		}
	}
	return yes;
}

// The dynamic vertex pool's size, set when the game makes its pools: 0x00c324a6
// "mov ecx, 0C0000h" before the call to 0x00c3a180 (the pools of map 0x00f79ab4).
// [camera] vertex_pool=N: N times as large (4 by default; 1 = the game's own).
void PoolSize(DWORD times)
{
	BYTE *at = reinterpret_cast<BYTE *>(0x00c324a6);
	if (at[0] != 0xb9 || *reinterpret_cast<const DWORD *>(at + 1) != 0x000c0000) {
		hooklog::Write("vertex pool: the game's code is not as expected: the game's own size");
		return;
	}
	DWORD old;
	if (!VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old))
		return;
	*reinterpret_cast<DWORD *>(at + 1) = 0x000c0000 * times;
	VirtualProtect(at + 1, 4, old, &old);
	FlushInstructionCache(GetCurrentProcess(), at + 1, 4);
	hooklog::Write("vertex pool: %lu times the game's size (0x%lx instead of 0xc0000): no full pool, and no models built "
	               "again for it, zoomed out on big maps",
	               times, 0x000c0000 * times);
}

}  // namespace

void ApplyVertexPoolFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	const UINT pool = GetPrivateProfileIntW(L"camera", L"vertex_pool", 4, path);
	if (pool > 1 && pool <= 16)
		PoolSize(pool);
	else
		hooklog::Write("vertex pool: the game's own size (vertex_pool=%u)", pool);
	if (GetPrivateProfileIntW(L"camera", L"vb_trace", 0, path) == 0)
		return;
	static const BYTE kStart[] = {0x51, 0x55, 0x8b, 0xe9, 0x56, 0x8b, 0x75, 0x54};
	if (memcmp(reinterpret_cast<const void *>(kNeeds), kStart, sizeof(kStart)) != 0) {
		hooklog::Write("vb trace: the game's code is not as expected: off");
		return;
	}
	g_next = reinterpret_cast<NeedsFn>(hookpatch::PatchPointer(kNeedsCell, kNeeds, &NeedsHook));
	g_bounds_next = reinterpret_cast<BoundsFn>(hookpatch::PatchPointer(kBoundsCell, kBounds, &BoundsHook));
	g_watch_next = reinterpret_cast<NeedsFn>(hookpatch::PatchPointer(kWatchCell, kWatch, &WatchHook));
	QueryPerformanceFrequency(&g_freq);
	g_rebuild_next = reinterpret_cast<RebuildFn>(hookpatch::PatchPointer(kRebuildCell, kRebuild, &RebuildHook));
	g_discard_next = reinterpret_cast<DiscardFn>(hookpatch::RedirectCall(kDiscardCall, kDiscardTarget, &DiscardHook));
	hooklog::Write(g_next ? "vb trace: on (research): which inputs make the models' vertices be rebuilt"
	                      : "vb trace: the combiner's vtable is not as expected: off");
}

}  // namespace mods

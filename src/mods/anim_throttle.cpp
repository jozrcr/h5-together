// Mod: lighter animations when the map is slow ([camera] anim_saver, on by default: the
// owner's choice, 2026-11-04, as it acts only when the map is already slow;
// docs/plans/map-drawing.md).
//
// In plain words: zoomed out on a big map, a large part of each frame goes into animating
// the 3D models (heroes, creatures, buildings) on the CPU: their skeletons, then sending
// the moved models to the graphics card (robot, 2026-11-04, the owner's 6-player epic map
// at 2560x1440, zoomed out to 250%: 28 ms a frame, about 1,270 skeletons a frame). With
// this on, while the map runs below about 55 frames a second (two half seconds in a row:
// a short dip, as a computer player's move, changes nothing), each skeleton is animated
// on one frame in 2, 3 or 4 only (the frame rate decides, every half second), spread over
// the frames; at 80 frames a second or more (again two half seconds in a row) it steps
// back towards every frame (the game's own way). Only in a frame that animates many models (300 or more: the adventure map has
// over 1,000, a battle a few dozen, so battles and towns keep every frame). Robot, same map and camera: every frame 35.7 fps, 1 in 2 43.2, 1 in 3 47.8, 1 in
// 4 50.0.
//
// How: the game's scene is a graph of nodes; each frame, a node is asked whether it must
// be computed again (vtable slot 7), then computed (slot 8), and its version goes up so
// that what depends on it is computed again too. A skeleton
// (NAnimation::CSkeletonAnimator, vtable 0x00e4e7dc; slot 7 0x00893600) says yes every
// frame, as its time moves on. Here it says yes on its frames only (frames counted by the
// scene's update stamp 0x00f482ac, which moves on twice per frame). Never before its
// second computation: until its first, a skeleton has no pose (Granny would read a null
// one: robot 20261104-150631, a crash at the menu's scene).
//
// Display only: the world (and so the PCs' lockstep) never reads the scene's skeletons;
// each player chooses.
//
// [camera] anim_every=N (hidden, testing): a fixed 1 in N; 0 = 1, 2, 3, 4 in turn, 20 s
// each, with the frames and updates per second in the log.
#include "anim_throttle.h"

#include "frame_limit.h"

#include <windows.h>
#include <string.h>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

const DWORD kNeedsRecalcCell = 0x00e4e7dc + 7 * 4;  // CSkeletonAnimator slot 7
const DWORD kNeedsRecalc = 0x00893600;              // __thiscall, returns bool (al)
const DWORD kStamp = 0x00f482ac;                    // the scene's update stamp (twice per frame)

// The automatic mode: every half second, from the frames of that half second.
const DWORD kCheckMs = 500;
const double kSlowerBelowFps = 55.0, kFasterAboveFps = 80.0;
const DWORD kMaxEvery = 4;
const LONG kBusySkeletons = 300;  // a frame that animates fewer keeps every frame

enum Mode { kOff, kAuto, kFixed, kCycle };

typedef bool(__fastcall *NeedsFn)(void *self, void *edx);
NeedsFn g_next = nullptr;
Mode g_mode = kOff;
DWORD g_every = 1;  // the skeletons' 1 in N now

// frames, from the stamp
DWORD g_last_frame = 0;
DWORD g_window_start = 0, g_window_frames = 0;
DWORD g_cycle_start = 0, g_cycle_stamp = 0;
LONG g_cycle_yes = 0, g_cycle_held = 0;
DWORD g_logged_changes = 0;
DWORD g_slow_windows = 0;  // half seconds in a row below kSlowerBelowFps...
DWORD g_fast_windows = 0;  // ...and above kFasterAboveFps
LONG g_frame_yes = 0;  // skeletons that asked to be animated in this frame...
bool g_busy = false;   // ...and whether the last frame had many

DWORD Frame()
{
	return *reinterpret_cast<const DWORD *>(kStamp) >> 1;
}

// Once per frame (the first skeleton asked in it).
void NewFrame()
{
	const DWORD now = GetTickCount();
	if (g_mode == kCycle) {
		const DWORD stamp = *reinterpret_cast<const DWORD *>(kStamp);
		if (!g_cycle_start) {
			g_cycle_start = now;
			g_cycle_stamp = stamp;
		}
		else if (now - g_cycle_start >= 20000) {
			const double ms = now - g_cycle_start;
			hooklog::Write("anim saver: phase 1 in %lu: %.1f frames/s, %.0f skeleton updates/s, %.0f held back/s", g_every,
			               (stamp - g_cycle_stamp) * 500.0 / ms, g_cycle_yes * 1000.0 / ms, g_cycle_held * 1000.0 / ms);
			g_every = g_every % 4 + 1;
			g_cycle_start = now;
			g_cycle_stamp = stamp;
			g_cycle_yes = g_cycle_held = 0;
		}
		return;
	}
	if (g_mode != kAuto)
		return;
	if (!g_window_start) {
		g_window_start = now;
		g_window_frames = 0;
		return;
	}
	++g_window_frames;
	const DWORD ms = now - g_window_start;
	if (ms < kCheckMs)
		return;
	const double fps = g_window_frames * 1000.0 / ms;
	const DWORD before = g_every;
	// with a frame rate limit (frame_limit.cpp), "fast" is near the limit: at 60, slower
	// below 51 and faster from 57 (else the limit would keep the animations light)
	const int cap = FrameLimit();
	const double slower = cap > 0 && cap * 0.85 < kSlowerBelowFps ? cap * 0.85 : kSlowerBelowFps;
	const double faster = cap > 0 && cap * 0.95 < kFasterAboveFps ? cap * 0.95 : kFasterAboveFps;
	g_slow_windows = fps < slower ? g_slow_windows + 1 : 0;
	g_fast_windows = fps > faster ? g_fast_windows + 1 : 0;
	if (ms > 3 * kCheckMs) {
		g_every = 1;  // a long pause (a loading screen, a save): start again from the game's own way
		g_slow_windows = g_fast_windows = 0;
	}
	else if (g_slow_windows >= 2 && g_every < kMaxEvery) {
		++g_every;
		g_slow_windows = 0;
	}
	else if (g_fast_windows >= 2 && g_every > 1) {
		--g_every;
		g_fast_windows = 0;
	}
	if (g_every != before && g_logged_changes < 40) {
		++g_logged_changes;
		hooklog::Write("anim saver: %.0f frames/s: skeletons animated on 1 frame in %lu%s", fps, g_every,
		               g_logged_changes == 40 ? " (no more of these lines in this run)" : "");
	}
	g_window_start = now;
	g_window_frames = 0;
}

bool __fastcall NeedsRecalcHook(void *self, void *edx)
{
	const bool yes = g_next(self, edx);
	const DWORD frame = Frame();
	if (frame != g_last_frame) {
		g_last_frame = frame;
		g_busy = g_frame_yes >= kBusySkeletons;
		g_frame_yes = 0;
		NewFrame();
	}
	if (!yes)
		return yes;
	++g_cycle_yes;
	++g_frame_yes;
	if (g_every <= 1 || (g_mode == kAuto && !g_busy))
		return yes;
	// never before its second computation (see the top of this file)
	if (*reinterpret_cast<const LONG *>(static_cast<const BYTE *>(self) + 0x10) < 2)
		return yes;
	// the skeletons spread over the N frames by a hash of their address (their low bits
	// are alike: allocations of the same size)
	const DWORD slot = ((reinterpret_cast<DWORD>(self) >> 4) * 2654435761u >> 16) % g_every;
	if (frame % g_every != slot) {
		++g_cycle_held;
		return false;
	}
	return true;
}

}  // namespace

void ApplyAnimThrottleFromSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	const int every = GetPrivateProfileIntW(L"camera", L"anim_every", 1, path);
	if (every == 0)
		g_mode = kCycle;
	else if (every > 1) {
		g_mode = kFixed;
		g_every = every > 8 ? 8 : static_cast<DWORD>(every);
	}
	else if (GetPrivateProfileIntW(L"camera", L"anim_saver", 1, path) != 0)
		g_mode = kAuto;
	if (g_mode == kOff)
		return;
	static const BYTE kStart[] = {0x51, 0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8};
	if (memcmp(reinterpret_cast<const void *>(kNeedsRecalc), kStart, sizeof(kStart)) != 0) {
		hooklog::Write("anim saver: the game's code is not as expected: off");
		return;
	}
	g_next = reinterpret_cast<NeedsFn>(hookpatch::PatchPointer(kNeedsRecalcCell, kNeedsRecalc, &NeedsRecalcHook));
	if (!g_next) {
		hooklog::Write("anim saver: the skeleton's vtable is not as expected: off");
		return;
	}
	if (g_mode == kAuto)
		hooklog::Write("anim saver: on: below %.0f frames a second on a busy map, each 3D model is animated on 1 frame in 2 to %lu",
		               kSlowerBelowFps, kMaxEvery);
	else if (g_mode == kFixed)
		hooklog::Write("anim saver: testing: each 3D model is animated on 1 frame in %lu (anim_every)", g_every);
	else
		hooklog::Write("anim saver: testing: 1, 2, 3, 4 in turn, 20 s each (anim_every=0)");
}

}  // namespace mods

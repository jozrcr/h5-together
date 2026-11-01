// Mod: the mouse wheel's zoom under Wine / Proton ([camera] wheel_fix), and a
// trace of the wheel's input ([debug] trace_wheel).
//
// How the game reads the wheel (vanilla 3.1, the same in MMH55_Utility; static
// reading, 2026-11-01; docs/subsystems/camera.md, "The mouse wheel"):
//   - 0x00c43be0 makes the DirectInput mouse with c_dfDIMouse2 (the wheel is Z,
//     offset 8) and sets it to ABSOLUTE axis mode (DIPROP_AXISMODE =
//     DIPROPAXISMODE_ABS) with a buffer of 0x400 events.
//   - Every frame 0x00c42530 reads only buffered events (GetDeviceData, 20-byte
//     events). For an axis it works the motion out itself: this value minus the
//     last value it kept for that axis. Windows reports positions for an
//     absolute axis, so the difference is the wheel's motion, 120 a notch.
//   - On each Acquire and after a buffer overflow, 0x00c420b0 reads
//     GetDeviceState and takes its lZ as the axis' value.
//   - It drops a "press" (bit 7 of the value's low byte set) while its window
//     is not active or the cursor is outside its client area (0x00c3b190).
//     Harmless with positions: the next value it takes carries the motion.
//   - The zoom: "bind -camera_zoom_mouse 'MOUSE_AXIS_Z'" (wheel up = closer);
//     Page Down / Page Up are "+camera_zoom" / "-camera_zoom" (input_a2.cfg).
// Wine's DirectInput reports the wheel's buffered events as steps (+-120), even
// in absolute mode (its X and Y are positions there). The game's difference
// then makes every repeated step 0, and a step down (-120, low byte 0x88) looks
// like a press, dropped while the game thinks the cursor is outside: the
// camera zooms in once and never out (the Linux player's report, 2026-11-01:
// Page Up / Page Down work, wheel down never did, with or without our DLL).
//
// The fix: the game gets positions, the running sum of Wine's steps, in its
// events and in GetDeviceState's lZ, as on Windows (wheel_logic.cpp).
// wheel_fix=auto (the default) does it under Wine only (ntdll exports
// wine_get_version), and only after a check: after a batch of wheel events the
// device's own state is read (the real GetDeviceState). With positions its lZ
// is the last event's value; with steps it is their sum. wheel_fix=1 converts
// from the first event (for a Wine the check misreads; never on Windows),
// wheel_fix=0 leaves the wheel alone.
//
// Nothing is installed on Windows (unless trace_wheel=1). The camera is not
// part of the game world: only this PC's zoom changes.
#define DIRECTINPUT_VERSION 0x0800
#include "wheel.h"

#include <windows.h>
#include <dinput.h>
#include <string.h>

#include "wheel_logic.h"

#include "../hook/dinput_hook.h"
#include "../hook/log.h"

namespace mods {
namespace {

// What the game's event loop (0x00c42530) checks before it takes a "press"
// (vanilla 3.1 layout, also MMH55_Utility). Only read for the trace.
const DWORD kInputChecksOff = 0x010d8669;  // set: no check
const DWORD kActiveA = 0x00f7a208;         // 0x00c3b0b0
const DWORD kActiveB = 0x00f7a207;         // 0x00c3b0a0
const DWORD kGameWindow = 0x010d85b8;      // HWND: the cursor must be in its client area (0x00c3b190)

enum Setting { kAuto, kOn, kOff };

wheel::Converter g_converter(wheel::kAsIs);
Setting g_setting = kAuto;
bool g_trace = false;
const char *g_wine = nullptr;  // wine_get_version(), null on Windows
DWORD g_state_size = 0;        // the size the mouse's GetDeviceState takes (20 or 16), 0 = not known yet
bool g_first_logged = false;
bool g_state_failed_logged = false;
int g_event_lines = 0, g_state_lines = 0, g_acquire_lines = 0, g_message_lines = 0;
const int kEventLines = 60, kStateLines = 20, kAcquireLines = 10, kMessageLines = 40;
HHOOK g_message_hook = nullptr;

const char *ModeName(wheel::Mode m)
{
	switch (m) {
	case wheel::kWatching:
		return "checking";
	case wheel::kConverting:
		return "steps turned into positions";
	default:
		return "as the device gives them";
	}
}

// --- the game's own checks (trace only) ------------------------------------------------------
struct GameView {
	bool known;     // its variables could be read
	bool checks_off;
	bool active_a, active_b;
	bool inside;    // the cursor is in its window's client area
};

bool ReadGameVariables(GameView *v, HWND *window)
{
	__try {
		v->checks_off = *reinterpret_cast<const BYTE *>(kInputChecksOff) != 0;
		v->active_a = *reinterpret_cast<const BYTE *>(kActiveA) != 0;
		v->active_b = *reinterpret_cast<const BYTE *>(kActiveB) != 0;
		*window = *reinterpret_cast<HWND const *>(kGameWindow);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
	return true;
}

GameView ReadGameView()
{
	GameView v = {};
	HWND window = nullptr;
	if (!ReadGameVariables(&v, &window))
		return v;
	v.known = true;
	RECT r;
	POINT a, b, cursor;
	if (window && GetClientRect(window, &r) && GetCursorPos(&cursor)) {
		a.x = r.left;
		a.y = r.top;
		b.x = r.right;
		b.y = r.bottom;
		ClientToScreen(window, &a);
		ClientToScreen(window, &b);
		v.inside = cursor.x >= a.x && cursor.x <= b.x && cursor.y >= a.y && cursor.y <= b.y;
	}
	return v;
}

bool GameDropsPress(const GameView &v, LONG value)
{
	return v.known && (value & 0x80) && !v.checks_off && (!v.active_a || !v.active_b || !v.inside);
}

// --- DirectInput -----------------------------------------------------------------------------
void Check(void *device, LONG last_raw)
{
	BYTE state[20] = {};
	HRESULT hr = E_FAIL;
	const DWORD sizes[] = {20, 16};
	for (DWORD size : sizes) {
		if (g_state_size && g_state_size != size)
			continue;
		hr = dihook::RealGetDeviceState(device, size, state);
		if (SUCCEEDED(hr)) {
			g_state_size = size;
			break;
		}
	}
	if (FAILED(hr)) {
		if (!g_state_failed_logged) {
			g_state_failed_logged = true;
			hooklog::Write("wheel: check: the mouse's state cannot be read (%08lx)", static_cast<unsigned long>(hr));
		}
		return;
	}
	LONG lz;
	memcpy(&lz, state + 8, 4);
	const wheel::Mode before = g_converter.mode();
	const wheel::Vote vote = g_converter.Check(last_raw, lz);
	if (g_trace)
		hooklog::Write("wheel: check %d: last wheel event %+ld, the mouse's lZ %+ld: %s", g_converter.checks(), last_raw, lz,
		               vote == wheel::kStepsVote       ? "steps (lZ is their sum)"
		               : vote == wheel::kPositionsVote ? "positions (lZ is the last one)"
		                                               : "not clear");
	if (g_converter.mode() == before)
		return;
	if (g_converter.mode() == wheel::kConverting)
		hooklog::Write("wheel: Wine reports the mouse wheel as steps (%d of %d checks): from now on the game gets "
		               "positions, as on Windows, so the wheel zooms both ways (wheel_fix=auto)",
		               g_converter.steps_votes(), g_converter.checks());
	else
		hooklog::Write("wheel: the mouse wheel's events were left as they are (%d checks: %d steps, %d positions). If "
		               "the wheel still does not zoom out, try [camera] wheel_fix=1 and send us open-homm5.log",
		               g_converter.checks(), g_converter.steps_votes(), g_converter.positions_votes());
}

HRESULT WheelData(dihook::DeviceKind kind, void *device, DWORD size, DIDEVICEOBJECTDATA *data, DWORD *count,
                  DWORD capacity, DWORD flags, HRESULT hr)
{
	if (kind != dihook::kMouse || FAILED(hr) || !data || !count || !*count || size < 8)
		return hr;
	const bool peek = (flags & DIGDD_PEEK) != 0;
	BYTE *events = reinterpret_cast<BYTE *>(data);
	int wheel_events = 0;
	LONG last_raw = 0;
	g_converter.Begin(peek);
	for (DWORD i = 0; i < *count; ++i) {
		BYTE *e = events + i * size;
		DWORD ofs;
		memcpy(&ofs, e, 4);
		if (ofs != DIMOFS_Z)
			continue;
		LONG raw;
		memcpy(&raw, e + 4, 4);
		const LONG out = g_converter.Next(raw);
		if (out != raw)
			memcpy(e + 4, &out, 4);
		last_raw = raw;
		++wheel_events;
		if (!g_first_logged) {
			g_first_logged = true;
			hooklog::Write("wheel: first mouse wheel event from DirectInput: %+ld (%lu-byte events; %s)", raw, size,
			               ModeName(g_converter.mode()));
			if (g_trace) {
				const GameView v = ReadGameView();
				hooklog::Write("wheel: the game's checks for presses now: window active %d %d, cursor in its window %d, "
				               "checks off %d%s",
				               v.active_a, v.active_b, v.inside, v.checks_off, v.known ? "" : " (not readable)");
			}
		}
		if (g_trace && g_event_lines < kEventLines) {
			++g_event_lines;
			const bool dropped = GameDropsPress(ReadGameView(), out);
			hooklog::Write("wheel: event %+ld, the game gets %+ld%s%s%s", raw, out, peek ? " (peek)" : "",
			               dropped ? "; it drops it as a press (bit 7 set; its window is inactive or the cursor is "
			                         "outside it)"
			                       : "",
			               g_event_lines == kEventLines ? " (no more of these lines)" : "");
		}
	}
	g_converter.End();
	if (wheel_events && !peek && g_converter.WantsCheck() && *count < capacity)
		Check(device, last_raw);
	return hr;
}

HRESULT WheelState(dihook::DeviceKind kind, void *device, DWORD size, void *data, HRESULT hr)
{
	(void)device;
	if (kind != dihook::kMouse || FAILED(hr) || size < 12)
		return hr;
	BYTE *state = static_cast<BYTE *>(data);
	LONG lz;
	memcpy(&lz, state + 8, 4);
	const LONG out = g_converter.State(lz);
	if (out != lz)
		memcpy(state + 8, &out, 4);
	if (g_trace && g_state_lines < kStateLines) {
		++g_state_lines;
		hooklog::Write("wheel: the game reads the mouse's state (%lu bytes): lZ %+ld, the game gets %+ld", size, lz, out);
	}
	return hr;
}

HRESULT WheelAcquire(dihook::DeviceKind kind, void *device, HRESULT hr)
{
	(void)device;
	if (kind != dihook::kMouse)
		return hr;
	if (hr == DI_OK)
		g_converter.Acquired();
	if (g_trace && g_acquire_lines < kAcquireLines) {
		++g_acquire_lines;
		hooklog::Write("wheel: the game acquires the mouse: %08lx", static_cast<unsigned long>(hr));
	}
	return hr;
}

// --- window messages (trace only) ----------------------------------------------------------------
LRESULT CALLBACK MessageHook(int code, WPARAM wp, LPARAM lp)
{
	if (code == HC_ACTION && wp == PM_REMOVE && lp) {
		const MSG *m = reinterpret_cast<const MSG *>(lp);
		if ((m->message == WM_MOUSEWHEEL || m->message == WM_MOUSEHWHEEL) && g_message_lines < kMessageLines) {
			++g_message_lines;
			hooklog::Write("wheel: window message %s, delta %+d%s", m->message == WM_MOUSEWHEEL ? "WM_MOUSEWHEEL" : "WM_MOUSEHWHEEL",
			               static_cast<int>(GET_WHEEL_DELTA_WPARAM(m->wParam)),
			               g_message_lines == kMessageLines ? " (no more of these lines)" : "");
		}
	}
	return CallNextHookEx(g_message_hook, code, wp, lp);
}

Setting ReadSetting(const wchar_t *ini)
{
	wchar_t text[32] = L"";
	GetPrivateProfileStringW(L"camera", L"wheel_fix", L"auto", text, ARRAYSIZE(text), ini);
	const wchar_t *p = text;
	while (*p == L' ' || *p == L'\t')
		++p;
	if (*p == L'0')
		return kOff;
	if (*p == L'1')
		return kOn;
	if (*p && _wcsnicmp(p, L"auto", 4) != 0)
		hooklog::Write("wheel: wheel_fix=%ls is not auto, 0 or 1: auto is used", p);
	return kAuto;
}

}  // namespace

void ApplyWheelFromSettings()
{
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"open-homm5", L".ini", ini, MAX_PATH);
	g_setting = ReadSetting(ini);
	g_trace = GetPrivateProfileIntW(L"debug", L"trace_wheel", 0, ini) != 0;
	typedef const char *(__cdecl * WineVersionFn)();
	const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	const WineVersionFn wine = ntdll ? reinterpret_cast<WineVersionFn>(GetProcAddress(ntdll, "wine_get_version")) : nullptr;
	g_wine = wine ? wine() : nullptr;

	wheel::Mode start = wheel::kAsIs;
	if (g_setting == kOn)
		start = wheel::kConverting;
	else if (g_setting == kAuto && g_wine)
		start = wheel::kWatching;
	g_converter = wheel::Converter(start);

	const char *trace = g_trace ? "; trace_wheel on: the wheel's input goes to the log" : "";
	if (g_setting == kOff)
		hooklog::Write("wheel: the game's own mouse wheel (wheel_fix=0)%s", trace);
	else if (g_setting == kOn)
		hooklog::Write("wheel: wheel_fix=1: the wheel's steps are given to the game as positions from the first event%s%s",
		               g_wine ? "" : " (this is not Wine: on Windows this breaks the wheel; use wheel_fix=auto)", trace);
	else if (g_wine)
		hooklog::Write("wheel: Wine %s: the first mouse wheel events are checked; if Wine reports them as steps, the game "
		               "gets positions, as on Windows (wheel_fix=auto)%s",
		               g_wine, trace);
	else
		hooklog::Write("wheel: the game's own mouse wheel (not Wine; wheel_fix=auto)%s", trace);

	if (start == wheel::kAsIs && !g_trace)
		return;  // nothing to change or to log: DirectInput stays untouched
	const dihook::Filters filters = {&WheelState, &WheelData, &WheelAcquire, nullptr};
	dihook::AddFilters(filters, -10);  // first: the robot and the modal panels see what the game gets
	if (g_trace) {
		// We are on the game's main thread (it makes the DirectInput object): its messages.
		g_message_hook = SetWindowsHookExW(WH_GETMESSAGE, &MessageHook, nullptr, GetCurrentThreadId());
		if (!g_message_hook)
			hooklog::Write("wheel: trace: the window messages cannot be watched (error %lu)", GetLastError());
	}
}

}  // namespace mods

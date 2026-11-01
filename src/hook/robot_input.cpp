// Robot keyboard: lets robot mode press keys in the game through DirectInput.
//
// The game reads the keyboard with DirectInput 8 (window messages are ignored),
// and DirectInput8Create goes through our proxy. In robot mode we filter what
// the game reads from the keyboard (the shared DirectInput hooks,
// dinput_hook.cpp, order 0):
//   Acquire          always succeeds (the robot's window is in the background);
//   GetDeviceState   our key press is added to the keys that are down;
//   GetDeviceData    our key press and release are added to the events.
// The mouse passes untouched, except the wheel notches of a "wheel" step, added
// to its events (order -20, before the wheel fix of mods/wheel.cpp).
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include "dinput_hook.h"
#include "log.h"
#include "robot.h"

namespace robot {
namespace {

void *g_keyboard = nullptr;

// One pending key press, held for g_hold_ms (kHoldMs unless the step said) then released.
// phase: 0 idle, 1 pressed (down not yet delivered), 2 down delivered, 3 release pending
const DWORD kHoldMs = 150;
DWORD g_hold_ms = kHoldMs;
volatile LONG g_key = 0;  // DIK_ code, 0 = none
volatile LONG g_phase = 0;
DWORD g_down_at = 0;
bool g_uses_buffer = false;  // the game reads buffered events (GetDeviceData)
DWORD g_sequence = 0x10000;

// The robot's mouse wheel: notches left to give (+ up, - down), one per read.
volatile LONG g_wheel_pending = 0;
volatile LONG g_wheel_steps = 0;  // 1 = as steps (Wine), 0 = as positions (Windows)
LONG g_wheel_position = 0;

void Advance()
{
	if (g_phase == 2 && GetTickCount() - g_down_at >= g_hold_ms)
		InterlockedExchange(&g_phase, 3);
}

void Clear()
{
	hooklog::Write("robot: key %02lx released", g_key);
	InterlockedExchange(&g_key, 0);
	InterlockedExchange(&g_phase, 0);
}

HRESULT FilterState(dihook::DeviceKind kind, void *self, DWORD size, void *data, HRESULT hr)
{
	if (kind != dihook::kKeyboard)
		return hr;
	static bool logged = false;
	if (!logged) {
		logged = true;
		hooklog::Write("robot: game polls the keyboard (GetDeviceState, hr=%08lx)", static_cast<unsigned long>(hr));
	}
	if (self != g_keyboard || size < 256)
		return hr;
	if (FAILED(hr)) {  // not acquired (window in background): report "no keys", plus ours
		ZeroMemory(data, size);
		hr = DI_OK;
	}
	if (!g_key)
		return hr;
	if (!g_uses_buffer && g_phase == 1)
		InterlockedExchange(&g_phase, 2);
	Advance();
	if (g_phase == 1 || g_phase == 2)
		static_cast<BYTE *>(data)[g_key] = 0x80;
	else if (g_phase == 3 && !g_uses_buffer)
		Clear();
	return hr;
}

HRESULT FilterData(dihook::DeviceKind kind, void *self, DWORD size, DIDEVICEOBJECTDATA *data, DWORD *count,
                   DWORD capacity, DWORD flags, HRESULT hr)
{
	if (kind != dihook::kKeyboard || self != g_keyboard)
		return hr;
	if (!g_uses_buffer)
		hooklog::Write("robot: game reads keyboard events (GetDeviceData, hr=%08lx)", static_cast<unsigned long>(hr));
	g_uses_buffer = true;
	if (FAILED(hr) && count) {  // not acquired (window in background): "no events", plus ours
		*count = 0;
		hr = DI_OK;
	}
	if (!g_key || !count || !data || size < sizeof(DIDEVICEOBJECTDATA_DX3) || (flags & DIGDD_PEEK))
		return hr;
	Advance();
	const LONG phase = g_phase;
	if ((phase == 1 || phase == 3) && *count < capacity) {
		DIDEVICEOBJECTDATA *e = reinterpret_cast<DIDEVICEOBJECTDATA *>(reinterpret_cast<BYTE *>(data) + *count * size);
		ZeroMemory(e, size);
		e->dwOfs = static_cast<DWORD>(g_key);
		e->dwData = phase == 1 ? 0x80 : 0;
		e->dwTimeStamp = GetTickCount();
		e->dwSequence = ++g_sequence;
		++*count;
		if (phase == 1) {
			g_down_at = GetTickCount();
			InterlockedExchange(&g_phase, 2);
		}
		else {
			Clear();
		}
	}
	return hr;
}

// The robot's wheel notches, added to the mouse's events (filter order -20: before
// the wheel fix, mods/wheel.cpp, which must take them for the device's own).
HRESULT FilterMouseData(dihook::DeviceKind kind, void *self, DWORD size, DIDEVICEOBJECTDATA *data, DWORD *count,
                        DWORD capacity, DWORD flags, HRESULT hr)
{
	(void)self;
	if (kind != dihook::kMouse || !g_wheel_pending || !count || !data || size < sizeof(DIDEVICEOBJECTDATA_DX3) ||
	    (flags & DIGDD_PEEK))
		return hr;
	if (FAILED(hr)) {  // not acquired (window in background): "no events", plus ours
		*count = 0;
		hr = DI_OK;
	}
	if (*count >= capacity)
		return hr;
	const LONG notch = g_wheel_pending > 0 ? 120 : -120;
	InterlockedExchangeAdd(&g_wheel_pending, notch > 0 ? -1 : 1);
	g_wheel_position += notch;
	DIDEVICEOBJECTDATA *e = reinterpret_cast<DIDEVICEOBJECTDATA *>(reinterpret_cast<BYTE *>(data) + *count * size);
	ZeroMemory(e, size);
	e->dwOfs = DIMOFS_Z;
	e->dwData = static_cast<DWORD>(g_wheel_steps ? notch : g_wheel_position);
	e->dwTimeStamp = GetTickCount();
	e->dwSequence = ++g_sequence;
	++*count;
	hooklog::Write("robot: wheel notch %+ld given to the game's mouse as %+ld", notch, static_cast<LONG>(e->dwData));
	return hr;
}

// The keyboard can only be "acquired" while the game window is in front. The robot
// runs in the background, so for the keyboard we always report success.
HRESULT FilterAcquire(dihook::DeviceKind kind, void *self, HRESULT hr)
{
	if (kind == dihook::kKeyboard && self == g_keyboard && FAILED(hr)) {
		static bool logged = false;
		if (!logged) {
			logged = true;
			hooklog::Write("robot: keyboard Acquire failed (%08lx, window in background): reporting success",
			               static_cast<unsigned long>(hr));
		}
		return DI_OK;
	}
	return hr;
}

void OnDevice(dihook::DeviceKind kind, void *device)
{
	if (kind != dihook::kKeyboard)
		return;
	g_keyboard = device;
	hooklog::Write("robot: keyboard device %p hooked", g_keyboard);
}

}  // namespace

void OnDirectInputCreated(void *direct_input)
{
	if (!Active() || !direct_input)
		return;
	static bool registered = false;
	if (!registered) {
		registered = true;
		InstallMenuHooks();  // DirectInput is created before the main menu is built
		dihook::Filters filters = {&FilterState, &FilterData, &FilterAcquire, &OnDevice};
		dihook::AddFilters(filters, 0);
		const dihook::Filters mouse = {nullptr, &FilterMouseData, nullptr, nullptr};
		dihook::AddFilters(mouse, -20);
	}
	hooklog::Write("robot: DirectInput %p hooked for the robot keyboard", direct_input);
}

bool PressKey(int dik_code, DWORD hold_ms)
{
	if (!g_keyboard || g_key)
		return false;  // no keyboard yet, or a key is still being pressed
	g_hold_ms = hold_ms ? hold_ms : kHoldMs;
	g_down_at = GetTickCount();
	InterlockedExchange(&g_phase, 1);
	InterlockedExchange(&g_key, dik_code);
	return true;
}

void TurnWheel(int notches, bool steps)
{
	InterlockedExchange(&g_wheel_steps, steps ? 1 : 0);
	InterlockedExchangeAdd(&g_wheel_pending, notches);
}

}  // namespace robot

// H5 Together hook DLL, loaded by the game as a proxy for dinput8.dll.
//
// The game imports one function from DINPUT8.dll: DirectInput8Create. Windows
// looks for the DLL in the game folder first, so our dinput8.dll is loaded at
// start-up. We forward every export to the real system dinput8.dll, and on the
// first DirectInput8Create call (the game is running and its code is decrypted
// by then, even for the Steam-wrapped exe) we check which exe we are in and
// install our hooks only for a build we know. The few hooks that must see the
// game's first data reads (hooks::InstallEarly) go in when the DLL is loaded,
// if the exe can already be recognized then (not the Steam-wrapped one).
//
// Another dinput8.dll mod, renamed dinput8_next.dll (chain.h): we forward to it
// instead of to the system DLL. When it calls "dinput8.dll" by name it gets us
// again: those calls (made from inside one of ours, on the same thread) go to
// the system DLL.
#include <windows.h>
#include <unknwn.h>

#include "chain.h"
#include "crash.h"
#include "dinput_hook.h"
#include "game_build.h"
#include "hooks.h"
#include "log.h"
#include "old_names.h"
#include "robot.h"
#include "version.h"

namespace {

HMODULE g_real = nullptr;
bool g_hooks_done = false;
// How deep this thread is in our forwarded calls: above 0, the chained mod is
// calling us back (it asked for "dinput8.dll" and got ours).
__declspec(thread) int t_depth = 0;

struct Forwarding {
	Forwarding() { ++t_depth; }
	~Forwarding() { --t_depth; }
	Forwarding(const Forwarding &) = delete;
	Forwarding &operator=(const Forwarding &) = delete;
};

FARPROC System(const char *name)
{
	if (!g_real) {
		wchar_t path[MAX_PATH];
		UINT n = GetSystemDirectoryW(path, MAX_PATH);  // SysWOW64 for a 32-bit process
		lstrcpynW(path + n, L"\\dinput8.dll", MAX_PATH - n);
		g_real = LoadLibraryW(path);
		if (!g_real) {
			hooklog::Write("cannot load the system dinput8.dll (error %lu)", GetLastError());
			return nullptr;
		}
	}
	return GetProcAddress(g_real, name);
}

// Where a call goes: the chained mod's function, else the system DLL's (also
// when the chained mod is the caller).
FARPROC Real(const char *name)
{
	if (HMODULE chained = t_depth == 0 ? chain::Module() : nullptr)
		if (FARPROC f = GetProcAddress(chained, name))
			return f;
	return System(name);
}

void InstallHooksOnce()
{
	if (g_hooks_done)
		return;
	g_hooks_done = true;
	const GameBuild build = DetectGameBuild();
	hooklog::Write("game build: %s", GameBuildName(build));
	crash::SetGameBuild(GameBuildName(build), IsHookable(build));  // (and this is the game's main thread)
	if (IsHookable(build))
		hooks::InstallAll();
	else {
		hooklog::Write("unknown build: no hooks installed, forwarding only");
		hooks::InstallUnknownBuild();  // only the Steam test, when it is switched on
	}
}

}  // namespace

extern "C" {

HRESULT WINAPI Proxy_DirectInput8Create(HINSTANCE inst, DWORD version, REFIID riid, LPVOID *out, LPUNKNOWN outer)
{
	const bool from_game = t_depth == 0;  // (not the chained mod calling us back)
	if (from_game)
		InstallHooksOnce();
	typedef HRESULT(WINAPI * Fn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
	Fn fn = reinterpret_cast<Fn>(Real("DirectInput8Create"));
	Forwarding forwarding;
	const HRESULT hr = fn ? fn(inst, version, riid, out, outer) : E_FAIL;
	if (from_game && SUCCEEDED(hr) && out) {
		robot::OnDirectInputCreated(*out);   // no-op unless robot mode is on
		dihook::OnDirectInputCreated(*out);  // no-op unless the robot or a modal panel needs it
	}
	return hr;
}

HRESULT WINAPI Proxy_DllCanUnloadNow()
{
	typedef HRESULT(WINAPI * Fn)();
	Fn fn = reinterpret_cast<Fn>(Real("DllCanUnloadNow"));
	Forwarding forwarding;
	return fn ? fn() : S_FALSE;
}

HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *out)
{
	typedef HRESULT(WINAPI * Fn)(REFCLSID, REFIID, LPVOID *);
	Fn fn = reinterpret_cast<Fn>(Real("DllGetClassObject"));
	Forwarding forwarding;
	return fn ? fn(clsid, riid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI Proxy_DllRegisterServer()
{
	typedef HRESULT(WINAPI * Fn)();
	Fn fn = reinterpret_cast<Fn>(Real("DllRegisterServer"));
	Forwarding forwarding;
	return fn ? fn() : E_FAIL;
}

HRESULT WINAPI Proxy_DllUnregisterServer()
{
	typedef HRESULT(WINAPI * Fn)();
	Fn fn = reinterpret_cast<Fn>(Real("DllUnregisterServer"));
	Forwarding forwarding;
	return fn ? fn() : E_FAIL;
}

LPCVOID WINAPI Proxy_GetdfDIJoystick()
{
	typedef LPCVOID(WINAPI * Fn)();
	Fn fn = reinterpret_cast<Fn>(Real("GetdfDIJoystick"));
	Forwarding forwarding;
	return fn ? fn() : nullptr;
}

}  // extern "C"

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
		// Files of the add-on's old name (open-homm5, until 0.2.7): before the log is
		// opened (the old log may become our older log) and before any setting is read.
		const std::vector<std::string> moved = oldnames::MoveOldFiles();
		hooklog::Open(inst);
		hooklog::Write("H5 Together hook DLL " OPEN_HOMM5_VERSION " loaded (built " __DATE__ " " __TIME__ ")");
		for (const std::string &line : moved)
			hooklog::Write("%s", line.c_str());
		hooks::InstallNetEarly();  // [online] mode (off by default): under the robot's network log
		robot::Init();  // before the game reads its profile
		hooks::InstallEarly();  // before the game reads its data
		chain::Load(inst);  // another dinput8.dll mod (dinput8_next.dll): loaded now, as the game would have loaded it
	}
	else if (reason == DLL_PROCESS_DETACH) {
		hooklog::Write("process exiting");
		hooklog::Close();
	}
	return TRUE;
}

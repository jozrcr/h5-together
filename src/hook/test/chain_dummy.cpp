// A stand-in for another mod's dinput8.dll proxy (an "ASI loader", a widescreen fix), for
// the chain-loading tests (test/proxy_test.cpp and robot runs, docs/install.md): renamed
// dinput8_next.dll, H5 Together loads it and DirectInput goes through it.
//
// It notes what happens in chain_dummy.txt next to the exe ("loaded", "called"), and passes
// DirectInput8Create on: by default to the system's dinput8.dll (as such mods do); with the
// environment variable CHAIN_DUMMY_MODE=self it asks for "dinput8.dll" by name instead,
// which is H5 Together's (already loaded under that name): the add-on must then pass the
// call to the system's DLL, not back into this one (no endless loop).
#include <windows.h>
#include <unknwn.h>
#include <string.h>

namespace {

void Note(const char *what)
{
	wchar_t path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	while (n > 0 && path[n - 1] != L'\\')
		--n;
	lstrcpynW(path + n, L"chain_dummy.txt", MAX_PATH - n);
	HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE)
		return;
	DWORD written = 0;
	WriteFile(f, what, static_cast<DWORD>(strlen(what)), &written, nullptr);
	WriteFile(f, "\r\n", 2, &written, nullptr);
	CloseHandle(f);
}

HMODULE Next()
{
	static HMODULE next = nullptr;
	if (next)
		return next;
	char mode[16] = "";
	GetEnvironmentVariableA("CHAIN_DUMMY_MODE", mode, sizeof(mode));
	if (strcmp(mode, "self") == 0) {
		next = LoadLibraryW(L"dinput8.dll");  // by name: the one already loaded, H5 Together's
	}
	else {
		wchar_t path[MAX_PATH];
		const UINT n = GetSystemDirectoryW(path, MAX_PATH);
		lstrcpynW(path + n, L"\\dinput8.dll", MAX_PATH - n);
		next = LoadLibraryW(path);
	}
	char name[MAX_PATH + 8] = "next: ";
	if (next)
		GetModuleFileNameA(next, name + 6, MAX_PATH);
	Note(name);
	return next;
}

}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD version, REFIID riid, LPVOID *out, LPUNKNOWN outer)
{
	Note("called");
	typedef HRESULT(WINAPI * Fn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
	HMODULE next = Next();
	Fn fn = next ? reinterpret_cast<Fn>(GetProcAddress(next, "DirectInput8Create")) : nullptr;
	return fn ? fn(inst, version, riid, out, outer) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
		Note("loaded");
	}
	return TRUE;
}

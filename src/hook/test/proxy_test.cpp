// Smoke test for the hook DLL outside the game: load build/hook/dinput8.dll like
// the game would, call DirectInput8Create, and check we get a real DirectInput
// object from the system DLL. In this test exe the build check must fail, so the
// DLL must stay in forwarding-only mode (see the log next to this exe).
//
// Then chain-loading (chain.cpp): with another mod's DLL renamed dinput8_next.dll
// next to the exe (test/chain_dummy.cpp, given as the second argument), DirectInput
// must go through it and still give a real object; also when it calls us back by
// name, and never when that file is a copy of our own DLL, is broken, or chaining is
// off. Each case runs in a child process (a DLL is loaded once per process), under
// the robot's instance name "proxytest" (h5-together-proxytest.ini / .log).
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "../chain.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

// Loads the DLL, calls DirectInput8Create, releases the object. 0 when it worked.
int Smoke(const char *dll)
{
	HMODULE h = LoadLibraryA(dll);
	if (!h) {
		printf("FAIL: cannot load %s (error %lu)\n", dll, GetLastError());
		return 1;
	}
	typedef HRESULT(WINAPI * Fn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
	Fn create = reinterpret_cast<Fn>(GetProcAddress(h, "DirectInput8Create"));
	if (!create) {
		printf("FAIL: DirectInput8Create not exported\n");
		return 1;
	}
	IDirectInput8A *di = nullptr;
	HRESULT hr = create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
	                    reinterpret_cast<void **>(&di), nullptr);
	if (FAILED(hr) || !di) {
		printf("FAIL: DirectInput8Create returned %08lx\n", static_cast<unsigned long>(hr));
		return 1;
	}
	di->Release();
	return 0;
}

std::wstring Folder()
{
	wchar_t path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	while (n > 0 && path[n - 1] != L'\\')
		--n;
	return std::wstring(path, n);
}

std::string ReadAll(const std::wstring &path)
{
	std::string out;
	FILE *f = _wfsopen(path.c_str(), L"rb", _SH_DENYNO);
	if (!f)
		return out;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0)
		out.append(buf, n);
	fclose(f);
	return out;
}

void WriteAll(const std::wstring &path, const std::string &data)
{
	FILE *f = _wfsopen(path.c_str(), L"wb", _SH_DENYNO);
	if (f) {
		fwrite(data.data(), 1, data.size(), f);
		fclose(f);
	}
}

bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

DWORD RunChild(const char *dll, const char *mode)
{
	SetEnvironmentVariableA("CHAIN_DUMMY_MODE", mode);
	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring line = L"\"" + std::wstring(exe) + L"\" --child \"";
	for (const char *p = dll; *p; ++p)
		line += static_cast<wchar_t>(*p);
	line += L"\"";
	STARTUPINFOW si = {sizeof(si)};
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessW(nullptr, &line[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
		return 0xffffffff;
	if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0)
		TerminateProcess(pi.hProcess, 0xfffffffe);
	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	SetEnvironmentVariableA("CHAIN_DUMMY_MODE", nullptr);
	return code;
}

const std::wstring &Next()
{
	static const std::wstring path = Folder() + L"dinput8_next.dll";
	return path;
}

void Clean()
{
	DeleteFileW(Next().c_str());
	DeleteFileW((Folder() + L"chain_sub\\other.dll").c_str());
	RemoveDirectoryW((Folder() + L"chain_sub").c_str());
	DeleteFileW((Folder() + L"h5-together-proxytest.ini").c_str());
	DeleteFileW((Folder() + L"h5-together-proxytest.log").c_str());
	DeleteFileW((Folder() + L"chain_dummy.txt").c_str());
}

void ChainTests(const char *dll, const char *dummy)
{
	const std::wstring wdll(dll, dll + strlen(dll)), wdummy(dummy, dummy + strlen(dummy));
	const std::wstring log_path = Folder() + L"h5-together-proxytest.log", notes = Folder() + L"chain_dummy.txt";

	// the pure parts
	Check(chain::PathFor(L"", L"C:\\Game\\bin\\") == L"C:\\Game\\bin\\dinput8_next.dll", "chain: by default dinput8_next.dll next to the exe");
	Check(chain::PathFor(L" \"mods/widescreen.dll\" ", L"C:\\Game\\bin") == L"C:\\Game\\bin\\mods\\widescreen.dll",
	      "chain: a name from the ini, next to the exe (quotes, spaces and / taken care of)");
	Check(chain::PathFor(L"D:\\Mods\\dinput8.dll", L"C:\\Game\\bin\\") == L"D:\\Mods\\dinput8.dll", "chain: a full path stays");
	Check(chain::PathFor(L"none", L"C:\\x\\").empty() && chain::PathFor(L"0", L"C:\\x\\").empty(), "chain: none or 0 = off");
	Check(chain::IsOurs("xx H5 Together hook DLL 0.3.0 loaded") && chain::IsOurs("open-homm5 hook DLL 0.2.3") &&
	          !chain::IsOurs("Ultimate ASI Loader"),
	      "chain: our own DLL is recognised by its text");

	// no other mod: nothing chained, nothing said
	Clean();
	DWORD code = RunChild(dll, "");
	std::string log = ReadAll(log_path);
	Check(code == 0 && !Has(log, "chain"), "no dinput8_next.dll: DirectInput works, the log says nothing about chaining");

	// another mod renamed dinput8_next.dll: loaded, and DirectInput goes through it
	Clean();
	CopyFileW(wdummy.c_str(), Next().c_str(), FALSE);
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	std::string seen = ReadAll(notes);
	Check(code == 0 && Has(log, "chained: dinput8_next.dll (another dinput8.dll mod) is loaded"),
	      "dinput8_next.dll: loaded (the log says so), and DirectInput gives a real object");
	Check(Has(seen, "loaded") && Has(seen, "called"), "dinput8_next.dll: its start-up ran and DirectInput8Create went through it");

	// it asks for "dinput8.dll" by name and gets ours: no endless loop, the system's DLL answers
	Clean();
	CopyFileW(wdummy.c_str(), Next().c_str(), FALSE);
	code = RunChild(dll, "self");
	seen = ReadAll(notes);
	const std::string ours = "next: " + std::string(dll);
	const size_t at = seen.find("next: ");
	const bool got_ours = at != std::string::npos && _strnicmp(seen.c_str() + at, ours.c_str(), ours.size()) == 0;
	Check(code == 0 && Has(seen, "called") && got_ours,
	      "a chained mod that calls dinput8.dll by name gets ours: its call goes to the system DLL, not back to it");

	// a copy of our own DLL: not loaded
	Clean();
	CopyFileW(wdll.c_str(), Next().c_str(), FALSE);
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	Check(code == 0 && Has(log, "chain: dinput8_next.dll is H5 Together itself") && !Has(log, "chained:"),
	      "dinput8_next.dll that is a copy of H5 Together: refused, the system DLL is used");

	// a broken file: not loaded
	Clean();
	WriteAll(Next(), "MZ this is not a DLL");
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	Check(code == 0 && Has(log, "chain: dinput8_next.dll could not be loaded (error"), "a broken dinput8_next.dll: the log says so, the system DLL is used");

	// a name from the ini, in a folder of its own
	Clean();
	CreateDirectoryW((Folder() + L"chain_sub").c_str(), nullptr);
	CopyFileW(wdummy.c_str(), (Folder() + L"chain_sub\\other.dll").c_str(), FALSE);
	WriteAll(Folder() + L"h5-together-proxytest.ini", "[chain]\r\ndll=chain_sub\\other.dll\r\n");
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	seen = ReadAll(notes);
	Check(code == 0 && Has(log, "chained: chain_sub\\other.dll") && Has(seen, "called"), "[chain] dll=<file>: that file is chained");

	// a name from the ini that is not there
	Clean();
	WriteAll(Folder() + L"h5-together-proxytest.ini", "[chain]\r\ndll=nothere.dll\r\n");
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	Check(code == 0 && Has(log, "chain: [chain] dll=nothere.dll: there is no such file"), "[chain] dll=<missing file>: the log says so");

	// off
	Clean();
	CopyFileW(wdummy.c_str(), Next().c_str(), FALSE);
	WriteAll(Folder() + L"h5-together-proxytest.ini", "[chain]\r\ndll=none\r\n");
	code = RunChild(dll, "");
	log = ReadAll(log_path);
	Check(code == 0 && Has(log, "chain: off") && ReadAll(notes).empty(), "[chain] dll=none: dinput8_next.dll is not loaded");
	Clean();
}

}  // namespace

int main(int argc, char **argv)
{
	if (argc >= 3 && strcmp(argv[1], "--child") == 0) {
		SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", L"proxytest");
		return Smoke(argv[2]);
	}
	const char *dll = argc > 1 ? argv[1] : "dinput8.dll";
	if (Smoke(dll) != 0)
		return 1;
	printf("OK: DirectInput8Create forwarded to the system DLL\n");
	if (argc > 2) {
		ChainTests(dll, argv[2]);
		printf(g_failures ? "%d FAILED\n" : "chain-loading: all passed\n", g_failures);
	}
	return g_failures ? 1 : 0;
}

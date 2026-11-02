// Chain-loading another dinput8.dll mod (see chain.h).
#include "chain.h"

#include <windows.h>
#include <string.h>

#include "log.h"

namespace chain {
namespace {

HMODULE g_chain = nullptr;

std::string Utf8(const std::wstring &w)
{
	if (w.empty())
		return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string out(n > 0 ? n : 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
	out.resize(strlen(out.c_str()));
	return out;
}

// The file's bytes (at most 64 MB), or false.
bool ReadFileBytes(const std::wstring &path, std::string *out)
{
	HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE)
		return false;
	const DWORD size = GetFileSize(f, nullptr);
	bool ok = size != INVALID_FILE_SIZE && size <= 64u * 1024 * 1024;
	if (ok) {
		out->resize(size);
		DWORD got = 0;
		ok = size == 0 || (ReadFile(f, &(*out)[0], size, &got, nullptr) && got == size);
	}
	CloseHandle(f);
	return ok;
}

std::wstring ModulePath(HMODULE m)
{
	wchar_t path[MAX_PATH];
	const DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
	return std::wstring(path, n < MAX_PATH ? n : 0);
}

}  // namespace

std::wstring PathFor(const std::wstring &value, const std::wstring &exe_folder)
{
	std::wstring v = value;
	while (!v.empty() && (v.back() == L' ' || v.back() == L'\t' || v.back() == L'"'))
		v.pop_back();
	while (!v.empty() && (v[0] == L' ' || v[0] == L'\t' || v[0] == L'"'))
		v.erase(0, 1);
	if (_wcsicmp(v.c_str(), L"none") == 0 || v == L"0")
		return std::wstring();
	if (v.empty())
		v = L"dinput8_next.dll";
	for (wchar_t &c : v)
		if (c == L'/')
			c = L'\\';
	const bool full = (v.size() > 2 && v[1] == L':' && v[2] == L'\\') || (v.size() > 1 && v[0] == L'\\' && v[1] == L'\\');
	if (full)
		return v;
	std::wstring folder = exe_folder;
	if (!folder.empty() && folder.back() != L'\\')
		folder += L'\\';
	return folder + v;
}

bool IsOurs(const std::string &bytes)
{
	return bytes.find("H5 Together hook DLL") != std::string::npos || bytes.find("open-homm5 hook DLL") != std::string::npos;
}

void Load(HMODULE self)
{
	wchar_t ini[MAX_PATH], value[MAX_PATH] = L"";
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);
	GetPrivateProfileStringW(L"chain", L"dll", L"", value, MAX_PATH, ini);
	const bool named = value[0] != 0;
	std::wstring exe = ModulePath(nullptr);
	const std::wstring folder = exe.substr(0, exe.rfind(L'\\') + 1);
	const std::wstring path = PathFor(value, folder);
	if (path.empty()) {
		hooklog::Write("chain: off ([chain] dll=%s): DirectInput goes to Windows' own dinput8.dll", Utf8(value).c_str());
		return;
	}
	const std::string shown = Utf8(path.compare(0, folder.size(), folder) == 0 ? path.substr(folder.size()) : path);
	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
		if (named)
			hooklog::Write("chain: [chain] dll=%s: there is no such file (%s): DirectInput goes to Windows' own dinput8.dll",
			               Utf8(value).c_str(), Utf8(path).c_str());
		return;  // (no dinput8_next.dll: the usual case, nothing to say)
	}
	std::string bytes;
	if (_wcsicmp(path.c_str(), ModulePath(self).c_str()) == 0 || (ReadFileBytes(path, &bytes) && IsOurs(bytes))) {
		hooklog::Write("chain: %s is H5 Together itself (another copy of this add-on), not another mod: not loaded; "
		               "DirectInput goes to Windows' own dinput8.dll. Delete that file.",
		               shown.c_str());
		return;
	}
	// Its own DLLs are looked for in its folder first (a mod kept in a folder of its own).
	HMODULE m = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!m) {
		const DWORD error = GetLastError();
		hooklog::Write("chain: %s could not be loaded (error %lu%s): DirectInput goes to Windows' own dinput8.dll",
		               shown.c_str(), error, error == ERROR_BAD_EXE_FORMAT ? ": not a 32-bit DLL" : "");
		return;
	}
	if (!GetProcAddress(m, "DirectInput8Create")) {
		// (left loaded: its start-up code has run, and it may have changed things it expects to stay)
		hooklog::Write("chain: %s is loaded but has no DirectInput8Create: it is not a dinput8.dll; DirectInput goes to "
		               "Windows' own dinput8.dll",
		               shown.c_str());
		return;
	}
	g_chain = m;
	hooklog::Write("chained: %s (another dinput8.dll mod) is loaded, and DirectInput goes through it; to undo, delete or "
	               "rename that file (it was the other mod's dinput8.dll)",
	               shown.c_str());
}

HMODULE Module()
{
	return g_chain;
}

}  // namespace chain

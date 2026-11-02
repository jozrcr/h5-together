#include "log.h"

#include <share.h>
#include <stdarg.h>
#include <stdio.h>

namespace hooklog {
namespace {

CRITICAL_SECTION g_lock;
FILE *g_file = nullptr;
DWORD g_start = 0;

}  // namespace

void PathNextToExe(const wchar_t *stem, const wchar_t *ext, wchar_t *out, DWORD size)
{
	DWORD n = GetModuleFileNameW(nullptr, out, size);
	while (n > 0 && out[n - 1] != L'\\')
		--n;
	wchar_t instance[32] = {};
	const DWORD len = GetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", instance, 32);
	if (len == 0 || len >= 32)
		instance[0] = 0;
	_snwprintf_s(out + n, size - n, _TRUNCATE, L"%s%s%s%s", stem, instance[0] ? L"-" : L"", instance, ext);
}

namespace {

// Size of a file in bytes, or 0 if it is missing.
unsigned long long FileSize(const wchar_t *path)
{
	WIN32_FILE_ATTRIBUTE_DATA info;
	if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info))
		return 0;
	return (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
}

}  // namespace

bool RotateIfLarge(const wchar_t *path, const wchar_t *old_path, unsigned long long limit)
{
	if (FileSize(path) <= limit)
		return false;
	return MoveFileExW(path, old_path, MOVEFILE_REPLACE_EXISTING) != 0;
}

void Open(HMODULE)
{
	InitializeCriticalSection(&g_lock);
	g_start = GetTickCount();
	// The log goes next to the game exe (the current directory may differ).
	wchar_t path[MAX_PATH], old_path[MAX_PATH];
	PathNextToExe(L"open-homm5", L".log", path, MAX_PATH);
	PathNextToExe(L"open-homm5", L".old.log", old_path, MAX_PATH);
	RotateIfLarge(path, old_path, kMaxLogBytes);
	const bool appending = FileSize(path) > 0;
	// shared for reading, so the log can be watched while the game runs
	g_file = _wfsopen(path, L"a", _SH_DENYWR);
	// A log that goes on from earlier runs gets a line to tell them apart. (A new log
	// starts with the "loaded" line, which tools and guides read.)
	if (g_file && appending)
		Write("---------------- new run ----------------");
}

void Close()
{
	EnterCriticalSection(&g_lock);
	if (g_file) {
		fclose(g_file);
		g_file = nullptr;
	}
	LeaveCriticalSection(&g_lock);
}

void Write(const char *fmt, ...)
{
	char line[1024];
	va_list args;
	va_start(args, fmt);
	_vsnprintf_s(line, sizeof(line), _TRUNCATE, fmt, args);
	va_end(args);

	EnterCriticalSection(&g_lock);
	if (g_file) {
		fprintf(g_file, "%10lu [%5lu] %s\n", GetTickCount() - g_start, GetCurrentThreadId(), line);
		fflush(g_file);
	}
	LeaveCriticalSection(&g_lock);
}

}  // namespace hooklog

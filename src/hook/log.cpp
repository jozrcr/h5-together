#include "log.h"

#include <share.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace hooklog {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
FILE *g_file = nullptr;
DWORD g_start = 0;
FILETIME g_previous_end = {};

// The last lines, for a crash report: a ring of fixed buffers (nothing is allocated).
const int kKeptChars = 240;
char g_kept[kKeptLines][kKeptChars];
int g_kept_next = 0;   // the slot the next line goes to
int g_kept_count = 0;  // lines in the ring (up to kKeptLines)

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

// One finished line ("  123456 [ 4321] text") into the file and the ring. Under the lock.
void Put(const char *line)
{
	if (g_file) {
		fputs(line, g_file);
		fputc('\n', g_file);
		fflush(g_file);
	}
	lstrcpynA(g_kept[g_kept_next], line, kKeptChars);
	g_kept_next = (g_kept_next + 1) % kKeptLines;
	if (g_kept_count < kKeptLines)
		++g_kept_count;
}

void Format(char *out, size_t size, const char *fmt, va_list args)
{
	char text[1024];
	_vsnprintf_s(text, sizeof(text), _TRUNCATE, fmt, args);
	_snprintf_s(out, size, _TRUNCATE, "%10lu [%5lu] %s", GetTickCount() - g_start, GetCurrentThreadId(), text);
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
	g_lock_ready = true;
	g_start = GetTickCount();
	// The log goes next to the game exe (the current directory may differ).
	wchar_t path[MAX_PATH], old_path[MAX_PATH];
	PathNextToExe(L"h5-together", L".log", path, MAX_PATH);
	PathNextToExe(L"h5-together", L".old.log", old_path, MAX_PATH);
	WIN32_FILE_ATTRIBUTE_DATA info;
	if (GetFileAttributesExW(path, GetFileExInfoStandard, &info))
		g_previous_end = info.ftLastWriteTime;
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

unsigned long Milliseconds()
{
	return GetTickCount() - g_start;
}

void Write(const char *fmt, ...)
{
	char line[1100];
	va_list args;
	va_start(args, fmt);
	Format(line, sizeof(line), fmt, args);
	va_end(args);

	EnterCriticalSection(&g_lock);
	Put(line);
	LeaveCriticalSection(&g_lock);
}

namespace {

// The lock within about a second, else false (another thread keeps it: maybe a crashed one).
bool TryLock()
{
	if (!g_lock_ready)
		return false;
	for (int i = 0; i < 100; ++i) {
		if (TryEnterCriticalSection(&g_lock))
			return true;
		Sleep(10);
	}
	return false;
}

}  // namespace

void WriteNoWait(const char *fmt, ...)
{
	char line[1100];
	va_list args;
	va_start(args, fmt);
	Format(line, sizeof(line), fmt, args);
	va_end(args);
	if (!TryLock())
		return;
	Put(line);
	LeaveCriticalSection(&g_lock);
}

void LastLines(char *out, size_t size)
{
	if (!size)
		return;
	out[0] = 0;
	// Without the lock (a crashed thread may hold it), a line being written may show half done.
	const bool locked = TryLock();
	size_t used = 0;
	for (int i = 0; i < g_kept_count; ++i) {
		const char *line = g_kept[(g_kept_next - g_kept_count + i + kKeptLines) % kKeptLines];
		const int n = _snprintf_s(out + used, size - used, _TRUNCATE, "%.*s\n", kKeptChars, line);
		if (n < 0)
			break;
		used += static_cast<size_t>(n);
	}
	if (locked)
		LeaveCriticalSection(&g_lock);
}

FILETIME PreviousRunEnd()
{
	return g_previous_end;
}

}  // namespace hooklog

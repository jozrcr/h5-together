// Crash information (see crash.h).
//
// The game's own filters, found by reading the exe (2026-11-02, vanilla 3.1 layout, the same
// in MMH55_Utility):
//   - 0x00c67b10 (called once, from the renderer's start 0x00a415d0, after Direct3DCreate9)
//     sets 0x00c67a50: an access violation on an address inside one of its memory blocks
//     (0x00c44170's list) goes on after the faulting instruction (EIP moved on, "continue
//     execution"); anything else goes to the filter that was there before (0x010d9e24), or
//     to Windows' UnhandledExceptionFilter if there was none. We are there before it, so it
//     hands us the real crashes and keeps the ones it recovers from.
//   - 0x00c2e330 would set an "exception report" dialog filter (0x00c2dff0, the dialog
//     IDD_EXCEPTION_REPORT, FatalExit(0xdead)), but nothing calls it.
//   - MMH5.5's 55u.dll, 55.dll, um.dll and UbiStats.dll only have the C runtime's /GS
//     failure report (it removes every filter and ends the game: no report of ours then).
//   - msvcr71.dll (the game's C runtime) has its filter for C++ exceptions nobody caught
//     (it ends the game), which passes the others on.
// The exe imports the game folder's dbghelp.dll (2002) for its own stack walks; ours is
// loaded by its full path from the system folder, a separate copy.
#include "crash.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "log.h"
#include "version.h"
#include "watchdog.h"

namespace crash {

LONG WINAPI Filter(EXCEPTION_POINTERS *info);

namespace {

typedef BOOL(WINAPI *MiniDumpFn)(HANDLE process, DWORD pid, HANDLE file, MINIDUMP_TYPE type,
                                 PMINIDUMP_EXCEPTION_INFORMATION exception, PMINIDUMP_USER_STREAM_INFORMATION user,
                                 PMINIDUMP_CALLBACK_INFORMATION callback);

const int kKeep = 3;           // reports kept (the older ones go at the next start)
const DWORD kWaitMs = 30000;   // the crashed thread waits at most this long for the report

// The game's own filter (vanilla 3.1 layout): its "installed" flag and the filter it passes on to.
const DWORD kGameFilterSet = 0x010d9e20;
const DWORD kGameFilterNext = 0x010d9e24;
const DWORD kGameFilter = 0x00c67a50;

volatile LONG g_installed = 0;
volatile LONG g_state = 0;  // 1 = a crash is being reported (or was)
volatile LONG g_ready = 0;  // the report thread waits
DWORD g_writer_id = 0;
HANDLE g_go = nullptr, g_done = nullptr;
LPTOP_LEVEL_EXCEPTION_FILTER g_next = nullptr;
MiniDumpFn g_minidump = nullptr;
char g_dumper[32] = "";      // "dbghelp.dll" or "dbgcore.dll" (the system's)
wchar_t g_base[MAX_PATH];    // "<exe folder>\h5-together" (or h5-together-<instance>)
const char *g_build = "not checked yet";
bool g_vanilla = false;
DWORD g_main_thread = 0;
volatile DWORD g_zero = 0;   // CrashOnPurpose's address (read at run time)

EXCEPTION_POINTERS *volatile g_info = nullptr;
DWORD g_crash_thread = 0;

// The report (static: nothing is allocated while the game is crashing).
char g_text[48 * 1024];
size_t g_used = 0;
wchar_t g_txt[MAX_PATH], g_dmp[MAX_PATH];
char g_txt_name[MAX_PATH], g_dmp_name[MAX_PATH];

void Add(const char *fmt, ...)
{
	if (g_used >= sizeof(g_text) - 1)
		return;
	va_list args;
	va_start(args, fmt);
	const int n = _vsnprintf_s(g_text + g_used, sizeof(g_text) - g_used, _TRUNCATE, fmt, args);
	va_end(args);
	g_used = n < 0 ? sizeof(g_text) - 1 : g_used + static_cast<size_t>(n);
}

// The file name part of a path, as UTF-8.
void FileName(const wchar_t *path, char *out, int size)
{
	const wchar_t *name = wcsrchr(path, L'\\');
	name = name ? name + 1 : path;
	if (!WideCharToMultiByte(CP_UTF8, 0, name, -1, out, size, nullptr, nullptr))
		lstrcpynA(out, "?", size);
}

// "MMH55_Utility_64.exe+0x46e9d0 (0x0086e9d0, AI turn driver+0x90)", as the watchdog says it;
// while the watchdog is looking (its tables are in use): the module's base and the offset.
void Where(DWORD a, char *out, int size)
{
	if (watchdog::DescribeAddress(a, out, size))
		return;
	MEMORY_BASIC_INFORMATION m;
	if (VirtualQuery(reinterpret_cast<const void *>(a), &m, sizeof(m)) == sizeof(m) && m.Type == MEM_IMAGE)
		_snprintf_s(out, size, _TRUNCATE, "module@%08lx+0x%lx", reinterpret_cast<DWORD>(m.AllocationBase),
		            a - reinterpret_cast<DWORD>(m.AllocationBase));
	else
		_snprintf_s(out, size, _TRUNCATE, "0x%08lx (not in a module)", a);
}

// "exception c0000005 (access violation: writing address 00000000)"
void What(const EXCEPTION_RECORD *r, char *out, int size)
{
	if (!r) {
		lstrcpynA(out, "no exception record", size);
		return;
	}
	const char *name = ExceptionName(r->ExceptionCode);
	if ((r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || r->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
	    r->NumberParameters >= 2) {
		const ULONG_PTR kind = r->ExceptionInformation[0];
		_snprintf_s(out, size, _TRUNCATE, "exception %08lx (%s: %s address %08lx)", r->ExceptionCode, name,
		            kind == 0 ? "reading" : kind == 1 ? "writing" : kind == 8 ? "running code at" : "using",
		            static_cast<DWORD>(r->ExceptionInformation[1]));
	}
	else
		_snprintf_s(out, size, _TRUNCATE, "exception %08lx (%s)", r->ExceptionCode, name[0] ? name : "no name known");
}

bool WriteText()
{
	HANDLE f = CreateFileW(g_txt, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(f, g_text, static_cast<DWORD>(g_used), &written, nullptr);
	CloseHandle(f);
	return ok && written == g_used;
}

bool WriteDump(DWORD *bytes, DWORD *error)
{
	*bytes = 0;
	*error = 0;
	if (!g_minidump)
		return false;
	HANDLE f = CreateFileW(g_dmp, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE) {
		*error = GetLastError();
		return false;
	}
	MINIDUMP_EXCEPTION_INFORMATION mei;
	mei.ThreadId = g_crash_thread;
	mei.ExceptionPointers = g_info;
	mei.ClientPointers = FALSE;
	const MINIDUMP_TYPE type =
		static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
	BOOL ok = g_minidump(GetCurrentProcess(), GetCurrentProcessId(), f, type, g_info ? &mei : nullptr, nullptr, nullptr);
	if (!ok) {  // an older dbghelp (Wine's?) may not take those two extras
		*error = GetLastError();
		SetFilePointer(f, 0, nullptr, FILE_BEGIN);
		SetEndOfFile(f);
		ok = g_minidump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, g_info ? &mei : nullptr, nullptr, nullptr);
		if (!ok)
			*error = GetLastError();
	}
	*bytes = GetFileSize(f, nullptr);
	CloseHandle(f);
	if (!ok)
		DeleteFileW(g_dmp);
	return ok != 0;
}

// Does the game's own filter sit in front of ours (and so passed this crash on)?
const char *ChainText()
{
	if (!g_vanilla)
		return "this exe's own filters are not known";
	BYTE set = 0;
	DWORD next = 0;
	SIZE_T got = 0;
	if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void *>(kGameFilterSet), &set, 1, &got) ||
	    !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void *>(kGameFilterNext), &next, 4, &got))
		return "the game's filter could not be read";
	if (!set)
		return "the game's own filter (0x00c67a50) was not set yet: ours was the first";
	return next == reinterpret_cast<DWORD>(&Filter)
	           ? "the game's own filter (0x00c67a50) is in front of ours and passed this crash on (it was not one it recovers from)"
	           : "the game's own filter (0x00c67a50) is set, and passes crashes on to another filter than ours";
}

void Report()
{
	const EXCEPTION_RECORD *r = g_info ? g_info->ExceptionRecord : nullptr;
	const CONTEXT *c = g_info ? g_info->ContextRecord : nullptr;
	char what[200], where[240];
	What(r, what, sizeof(what));
	Where(r ? reinterpret_cast<DWORD>(r->ExceptionAddress) : 0, where, sizeof(where));
	const char *thread = g_crash_thread == g_main_thread ? "the game's main thread" : "not the game's main thread";
	hooklog::WriteNoWait("crash: the game crashed: %s at %s, in thread %lu (%s); writing the crash report", what, where,
	                     g_crash_thread, thread);

	SYSTEMTIME t;
	GetLocalTime(&t);
	wchar_t stamp[32];
	_snwprintf_s(stamp, 32, _TRUNCATE, L"%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	_snwprintf_s(g_txt, MAX_PATH, _TRUNCATE, L"%s-crash-%s.txt", g_base, stamp);
	_snwprintf_s(g_dmp, MAX_PATH, _TRUNCATE, L"%s-crash-%s.dmp", g_base, stamp);
	FileName(g_txt, g_txt_name, sizeof(g_txt_name));
	FileName(g_dmp, g_dmp_name, sizeof(g_dmp_name));

	// The text first (the dump may take a while, or fail), then again with the dump's result.
	for (int pass = 0; pass < 2; ++pass) {
		static DWORD bytes = 0, error = 0;
		static bool dumped = false;
		if (pass == 1)
			dumped = WriteDump(&bytes, &error);
		g_used = 0;
		g_text[0] = 0;
		Add("H5 Together crash report\r\n");
		Add("time: %04u-%02u-%02u %02u:%02u:%02u (this PC's clock)\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		Add("add-on: H5 Together %s (built %s %s)\r\n", OPEN_HOMM5_VERSION, __DATE__, __TIME__);
		char exe[MAX_PATH];
		wchar_t exe_path[MAX_PATH];
		GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
		FileName(exe_path, exe, sizeof(exe));
		Add("game: %s, %s\r\n", exe, g_build);
		Add("what: %s\r\n", what);
		Add("where: %s\r\n", where);
		Add("thread: %lu (%s)\r\n", g_crash_thread, thread);
		if (c)
			Add("registers: eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx eip=%08lx\r\n",
			    c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);
		Add("crash filters: %s\r\n", ChainText());
		DWORD callers[12];
		const int n = c ? watchdog::CallersOnStack(c->Esp, callers, 12) : 0;
		Add("probable callers (return addresses on the stack, innermost first; the minidump has the exact stack):\r\n");
		for (int i = 0; i < n; ++i) {
			char at[240];
			Where(callers[i], at, sizeof(at));
			Add("  %s\r\n", at);
		}
		if (n <= 0)
			Add("  %s\r\n", n < 0 ? "(not looked at: the hang watchdog was looking at the same moment)" : "(none found)");
		if (pass == 0)
			Add("minidump: %s is being written (if this line stays, it could not be finished)\r\n", g_dmp_name);
		else if (dumped)
			Add("minidump: %s (%lu KB, by the system's %s)\r\n", g_dmp_name, (bytes + 1023) / 1024, g_dumper);
		else if (!g_minidump)
			Add("minidump: none (the system's dbghelp.dll could not be loaded)\r\n");
		else
			Add("minidump: none (it could not be written: error %08lx)\r\n", error);
		Add("\r\nthe last lines of the log before the crash:\r\n");
		{
			static char last[hooklog::kKeptLines * 256];
			hooklog::LastLines(last, sizeof(last));
			for (const char *p = last; *p;) {
				const char *end = strchr(p, '\n');
				const size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
				Add("  %.*s\r\n", static_cast<int>(len), p);
				p += len + (end ? 1 : 0);
			}
		}
		Add("\r\nThis file and the .dmp stay on this PC: nothing is sent anywhere. bug-report.bat (or bug-report.sh)\r\n"
		    "adds the newest ones to a bug report. The .dmp holds part of the game's memory at the crash (player\r\n"
		    "names, folder names): send it only to people you trust with that.\r\n");
		const bool written = WriteText();
		if (pass == 1)
			hooklog::WriteNoWait("crash: %s %s%s%s next to this log (the 3 newest are kept); then the game's own crash "
			                     "handling, as before",
			                     written ? "wrote" : "could not write", g_txt_name, dumped ? " and " : "",
			                     dumped ? g_dmp_name : " (no minidump)");
	}
}

void ReportSafe()
{
	__try {
		Report();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		hooklog::WriteNoWait("crash: the crash report itself failed (exception %08lx)", GetExceptionCode());
	}
}

// --- at start-up: the dump function, the old reports, the last run ---------------------------
void LoadDumper()
{
	wchar_t path[MAX_PATH];
	const UINT n = GetSystemDirectoryW(path, MAX_PATH);  // SysWOW64 for a 32-bit process
	const wchar_t *const names[] = {L"\\dbghelp.dll", L"\\dbgcore.dll"};
	for (const wchar_t *name : names) {
		if (!n || n >= MAX_PATH - 16)
			break;
		lstrcpynW(path + n, name, MAX_PATH - n);
		HMODULE m = LoadLibraryW(path);
		if (!m)
			continue;
		g_minidump = reinterpret_cast<MiniDumpFn>(GetProcAddress(m, "MiniDumpWriteDump"));
		if (g_minidump) {
			FileName(path, g_dumper, sizeof(g_dumper));
			return;
		}
	}
}

// The folder's file names that start with "<prefix>-crash-".
std::vector<std::wstring> ReportNames(const std::wstring &folder, const std::wstring &prefix)
{
	std::vector<std::wstring> names;
	WIN32_FIND_DATAW d;
	HANDLE h = FindFirstFileW((folder + prefix + L"-crash-*").c_str(), &d);
	if (h == INVALID_HANDLE_VALUE)
		return names;
	do {
		if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			names.push_back(d.cFileName);
	} while (FindNextFileW(h, &d));
	FindClose(h);
	return names;
}

std::string Utf8(const std::wstring &w)
{
	char out[MAX_PATH * 3];
	if (!WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out, sizeof(out), nullptr, nullptr))
		return "?";
	return out;
}

void LookAtOldReports()
{
	const std::wstring base = g_base;
	const size_t slash = base.rfind(L'\\');
	const std::wstring folder = base.substr(0, slash + 1), prefix = base.substr(slash + 1);
	int removed = 0;
	for (const std::wstring &name : OldReports(ReportNames(folder, prefix), prefix, kKeep))
		removed += DeleteFileW((folder + name).c_str()) ? 1 : 0;
	// The newest report: was it the last run's? (Written at about the time the log last was.)
	std::vector<std::wstring> texts;
	for (const std::wstring &name : ReportNames(folder, prefix)) {
		bool text = false;
		if (ReportName(name, prefix, nullptr, &text) && text)
			texts.push_back(name);
	}
	if (texts.empty())
		return;
	std::sort(texts.begin(), texts.end());
	const std::wstring newest = texts.back();
	WIN32_FILE_ATTRIBUTE_DATA info;
	ULARGE_INTEGER report = {}, log = {};
	if (GetFileAttributesExW((folder + newest).c_str(), GetFileExInfoStandard, &info)) {
		report.LowPart = info.ftLastWriteTime.dwLowDateTime;
		report.HighPart = info.ftLastWriteTime.dwHighDateTime;
	}
	const FILETIME end = hooklog::PreviousRunEnd();
	log.LowPart = end.dwLowDateTime;
	log.HighPart = end.dwHighDateTime;
	const ULONGLONG minute = 60ull * 10000000ull;  // FILETIME counts 100 ns
	const bool last_run = report.QuadPart && log.QuadPart && report.QuadPart + minute >= log.QuadPart &&
	                      report.QuadPart <= log.QuadPart + minute;
	hooklog::Write("crash: %s %s (and its .dmp if there is one) is next to this log; %d crash report(s) kept%s%s",
	               last_run ? "THE LAST RUN OF THE GAME CRASHED: its report" : "an older crash report:",
	               Utf8(newest).c_str(), static_cast<int>(texts.size()), removed ? ", older files removed: " : "",
	               removed ? std::to_string(removed).c_str() : "");
}

DWORD WINAPI WriterThread(void *)
{
	LoadDumper();
	LookAtOldReports();
	InterlockedExchange(&g_ready, 1);
	WaitForSingleObject(g_go, INFINITE);
	ReportSafe();
	SetEvent(g_done);
	return 0;
}

}  // namespace

// (outside the unnamed namespace: ChainText compares the game's saved pointer with it)
LONG WINAPI Filter(EXCEPTION_POINTERS *info)
{
	const DWORD me = GetCurrentThreadId();
	if (me != g_writer_id) {
		if (InterlockedCompareExchange(&g_state, 1, 0) == 0) {
			g_info = info;
			g_crash_thread = me;
			watchdog::Quiet();  // the main thread may wait here: that is not a freeze
			if (g_ready) {
				SetEvent(g_go);
				WaitForSingleObject(g_done, kWaitMs);
			}
			else
				ReportSafe();  // our thread is not running yet: here
		}
		else if (g_ready)
			WaitForSingleObject(g_done, kWaitMs);  // another thread's crash is being reported: let it finish
	}
	LPTOP_LEVEL_EXCEPTION_FILTER next = g_next;
	if (next && next != &Filter)
		return next(info);
	return EXCEPTION_CONTINUE_SEARCH;  // Windows' own handling, as without us
}

void Install(const char *when)
{
	if (InterlockedCompareExchange(&g_installed, 1, 0) != 0)
		return;
	wchar_t ini[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", ini, MAX_PATH);
	if (!GetPrivateProfileIntW(L"debug", L"crash_report", 1, ini)) {
		hooklog::Write("crash: off ([debug] crash_report=0): no crash report");
		return;
	}
	hooklog::PathNextToExe(L"h5-together", L"", g_base, MAX_PATH);
	g_go = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	g_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	HANDLE t = g_go && g_done ? CreateThread(nullptr, 0, &WriterThread, nullptr, 0, &g_writer_id) : nullptr;
	if (t)
		CloseHandle(t);
	g_next = SetUnhandledExceptionFilter(&Filter);
	watchdog::NameFunction(reinterpret_cast<const void *>(&Filter), "crash filter");
	watchdog::NameFunction(reinterpret_cast<const void *>(&CrashOnPurpose), "crash test (on purpose)");
	hooklog::Write("crash: on (%s): if the game crashes, h5-together-crash-<date>.txt (what crashed) and .dmp (a small "
	               "minidump) go next to this log, then the game's own crash handling as before%s",
	               when, t ? "" : "; the report thread could not start: the crashed thread writes it");
	const UINT test_at = GetPrivateProfileIntW(L"debug", L"crash_test_at", 0, ini);
	if (test_at) {
		watchdog::RunOnMainLoopAt(test_at * 1000, &CrashOnPurpose);
		hooklog::Write("crash: test on ([debug] crash_test_at=%u): the game crashes on purpose %u s after its main loop "
		               "starts (needs the hang watchdog on)",
		               test_at, test_at);
	}
}

void SetGameBuild(const char *name, bool vanilla_layout)
{
	g_build = name;
	g_vanilla = vanilla_layout;
	g_main_thread = GetCurrentThreadId();
}

__declspec(noinline) void CrashOnPurpose()
{
	hooklog::Write("crash: crash test ([debug] crash_test_at): this thread now writes to address 0, on purpose");
	*reinterpret_cast<volatile DWORD *>(static_cast<DWORD>(g_zero)) = 1;
}

std::wstring Stamp(const SYSTEMTIME &t)
{
	wchar_t s[32];
	_snwprintf_s(s, 32, _TRUNCATE, L"%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	return s;
}

bool ReportName(const std::wstring &name, const std::wstring &prefix, std::wstring *stamp, bool *text)
{
	// "<prefix>-crash-YYYYMMDD-HHMMSS.txt|.dmp": the stamp sorts as the time does
	const std::wstring head = prefix + L"-crash-";
	const size_t stamp_len = 15;
	if (name.size() != head.size() + stamp_len + 4 || _wcsnicmp(name.c_str(), head.c_str(), head.size()) != 0)
		return false;
	const std::wstring s = name.substr(head.size(), stamp_len);
	const std::wstring ext = name.substr(head.size() + stamp_len);
	const bool txt = _wcsicmp(ext.c_str(), L".txt") == 0;
	bool ok = s[8] == L'-' && (txt || _wcsicmp(ext.c_str(), L".dmp") == 0);
	for (size_t i = 0; i < stamp_len && ok; ++i)
		ok = i == 8 || (s[i] >= L'0' && s[i] <= L'9');
	if (ok && stamp)
		*stamp = s;
	if (ok && text)
		*text = txt;
	return ok;
}

std::vector<std::wstring> OldReports(const std::vector<std::wstring> &names, const std::wstring &prefix, int keep)
{
	std::vector<std::wstring> stamps;
	std::vector<std::pair<std::wstring, std::wstring>> reports;  // (stamp, name)
	for (const std::wstring &name : names) {
		std::wstring stamp;
		if (!ReportName(name, prefix, &stamp, nullptr))
			continue;
		reports.push_back({stamp, name});
		if (std::find(stamps.begin(), stamps.end(), stamp) == stamps.end())
			stamps.push_back(stamp);
	}
	std::sort(stamps.begin(), stamps.end());
	std::vector<std::wstring> old;
	const size_t drop = stamps.size() > static_cast<size_t>(keep < 0 ? 0 : keep) ? stamps.size() - keep : 0;
	for (const auto &report : reports)
		if (std::find(stamps.begin(), stamps.begin() + drop, report.first) != stamps.begin() + drop)
			old.push_back(report.second);
	std::sort(old.begin(), old.end());
	return old;
}

const char *ExceptionName(DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION: return "access violation";
	case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer division by zero";
	case EXCEPTION_INT_OVERFLOW: return "integer overflow";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
	case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
	case EXCEPTION_FLT_DENORMAL_OPERAND: return "floating point: denormal operand";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "floating point: division by zero";
	case EXCEPTION_FLT_INEXACT_RESULT: return "floating point: inexact result";
	case EXCEPTION_FLT_INVALID_OPERATION: return "floating point: invalid operation";
	case EXCEPTION_FLT_OVERFLOW: return "floating point: overflow";
	case EXCEPTION_FLT_STACK_CHECK: return "floating point: stack check";
	case EXCEPTION_FLT_UNDERFLOW: return "floating point: underflow";
	case EXCEPTION_IN_PAGE_ERROR: return "in-page error (a file being read went away)";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "noncontinuable exception";
	case EXCEPTION_INVALID_DISPOSITION: return "invalid disposition";
	case EXCEPTION_GUARD_PAGE: return "guard page";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
	case EXCEPTION_BREAKPOINT: return "breakpoint";
	case EXCEPTION_SINGLE_STEP: return "single step";
	case 0xe06d7363: return "C++ exception nobody caught";
	case 0xc0000409: return "stack buffer overrun";
	case 0xc0000374: return "heap corruption";
	default: return "";
	}
}

#ifdef CRASH_TEST
bool ReadyForTest()
{
	return g_ready != 0;
}
#endif

}  // namespace crash

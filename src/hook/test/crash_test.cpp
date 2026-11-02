// Test of the crash report (src/hook/crash.cpp) without the game.
//
// The pure parts first (file names, which old reports go), then real crashes: this exe
// starts itself again as a child ("--child <case>") that installs the crash filter the
// way the DLL does and crashes on purpose. Before it, the child sets a filter of its own,
// as the C runtime does before the DLL loads: ours must call it after writing the report
// (it ends the child at once, so Windows' "stopped working" never shows). Cases:
//   av      a write to address 0 on the main thread
//   stack   a stack overflow (the report is written by our own thread: none is left here)
//   thread  a crash on another thread
//   above   a filter set after ours (as the game's own) recovers from an exception: no report
//   off     [debug] crash_report=0: no filter, no report
//   ready   no crash: the start-up work only (old reports removed, "the last run crashed")
// The files go next to this exe under the robot's instance name "crashtest"
// (h5-together-crashtest.log, h5-together-crashtest-crash-<date>.txt/.dmp).
#include <windows.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../crash.h"
#include "../log.h"
#include "../version.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
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

std::vector<std::wstring> Matching(const std::wstring &pattern)
{
	std::vector<std::wstring> names;
	WIN32_FIND_DATAW d;
	HANDLE h = FindFirstFileW((Folder() + pattern).c_str(), &d);
	if (h == INVALID_HANDLE_VALUE)
		return names;
	do
		names.push_back(d.cFileName);
	while (FindNextFileW(h, &d));
	FindClose(h);
	return names;
}

const wchar_t kPrefix[] = L"h5-together-crashtest";

void Clean()
{
	for (const std::wstring &name : Matching(std::wstring(kPrefix) + L"-crash-*"))
		DeleteFileW((Folder() + name).c_str());
	DeleteFileW((Folder() + kPrefix + L".log").c_str());
	DeleteFileW((Folder() + kPrefix + L".old.log").c_str());
	DeleteFileW((Folder() + kPrefix + L".ini").c_str());
	DeleteFileW((Folder() + kPrefix + L"-bottom.txt").c_str());
}

// --- the child --------------------------------------------------------------------------------
volatile DWORD g_zero = 0;

LONG WINAPI Bottom(EXCEPTION_POINTERS *info)
{
	HANDLE f = CreateFileW((Folder() + kPrefix + L"-bottom.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (f != INVALID_HANDLE_VALUE)
		CloseHandle(f);
	(void)info;
	return EXCEPTION_EXECUTE_HANDLER;  // the process ends with the exception's code
}

LPTOP_LEVEL_EXCEPTION_FILTER g_below = nullptr;

// As the game's own filter: one exception it recovers from, the others to the filter below.
LONG WINAPI Recovering(EXCEPTION_POINTERS *info)
{
	if (info->ExceptionRecord->ExceptionCode == 0xe0001234)
		return EXCEPTION_CONTINUE_EXECUTION;
	return g_below ? g_below(info) : EXCEPTION_CONTINUE_SEARCH;
}

__declspec(noinline) void Crash()
{
	*reinterpret_cast<volatile DWORD *>(static_cast<DWORD>(g_zero)) = 1;
}

__declspec(noinline) int Recurse(int n)
{
	volatile char pad[512];
	pad[0] = static_cast<char>(n);
	if (n < 0x7fffffff)
		return Recurse(n + 1) + pad[0];
	return 0;
}

DWORD WINAPI CrashThread(void *)
{
	Crash();
	return 0;
}

int Child(const char *what)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	hooklog::Open(nullptr);
	SetUnhandledExceptionFilter(&Bottom);
	crash::Install("test");
	crash::SetGameBuild("crash test", false);
	for (int i = 0; i < 500 && strcmp(what, "off") != 0 && !crash::ReadyForTest(); ++i)
		Sleep(10);
	hooklog::Write("crash test: about to crash (%s)", what);
	if (strcmp(what, "av") == 0 || strcmp(what, "off") == 0)
		Crash();
	else if (strcmp(what, "stack") == 0)
		Recurse(1);
	else if (strcmp(what, "thread") == 0) {
		HANDLE t = CreateThread(nullptr, 0, &CrashThread, nullptr, 0, nullptr);
		WaitForSingleObject(t, INFINITE);
	}
	else if (strcmp(what, "above") == 0) {
		g_below = SetUnhandledExceptionFilter(&Recovering);
		RaiseException(0xe0001234, 0, 0, nullptr);
		hooklog::Write("crash test: went on after the exception");
		return 7;
	}
	hooklog::Write("crash test: no crash (%s)", what);
	return 0;
}

// --- the parent -------------------------------------------------------------------------------
DWORD RunChild(const char *what)
{
	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring line = L"\"" + std::wstring(exe) + L"\" --child ";
	for (const char *p = what; *p; ++p)
		line += static_cast<wchar_t>(*p);
	STARTUPINFOW si = {sizeof(si)};
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessW(nullptr, &line[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
		return 0xffffffff;
	if (WaitForSingleObject(pi.hProcess, 60000) != WAIT_OBJECT_0)
		TerminateProcess(pi.hProcess, 0xfffffffe);
	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return code;
}

std::string TheReport(std::wstring *dump)
{
	const std::vector<std::wstring> texts = Matching(std::wstring(kPrefix) + L"-crash-*.txt");
	const std::vector<std::wstring> dumps = Matching(std::wstring(kPrefix) + L"-crash-*.dmp");
	if (dump)
		*dump = dumps.size() == 1 ? Folder() + dumps[0] : std::wstring();
	return texts.size() == 1 ? ReadAll(Folder() + texts[0]) : std::string();
}

bool Exists(const std::wstring &path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void PureTests()
{
	SYSTEMTIME t = {};
	t.wYear = 2026, t.wMonth = 11, t.wDay = 2, t.wHour = 9, t.wMinute = 5, t.wSecond = 7;
	Check(crash::Stamp(t) == L"20261102-090507", "a report's stamp: date-time, sorts as the time");
	const std::vector<std::wstring> names = {
		L"h5-together-crash-20261101-120000.txt", L"h5-together-crash-20261101-120000.dmp",
		L"h5-together-crash-20261102-080000.txt",  // its .dmp is missing
		L"h5-together-crash-20261030-235959.dmp",  // its .txt is missing
		L"h5-together-crash-20261102-090000.txt", L"h5-together-crash-20261102-090000.dmp",
		L"h5-together-crash-notes.txt", L"h5-together-crash-20261102-0900.txt",  // not ours
		L"h5-together-host-crash-20200101-000000.txt",  // another copy's (a robot run)
		L"h5-together.log",
	};
	const std::vector<std::wstring> old = crash::OldReports(names, L"h5-together", 3);
	Check(old == std::vector<std::wstring>({L"h5-together-crash-20261030-235959.dmp"}),
	      "old reports: of 4, the oldest goes (by its stamp; a lone .dmp counts); other files stay");
	const std::vector<std::wstring> old1 = crash::OldReports(names, L"h5-together", 1);
	Check(old1.size() == 4 && old1[0] == L"h5-together-crash-20261030-235959.dmp" &&
	          old1[3] == L"h5-together-crash-20261102-080000.txt",
	      "old reports: keeping 1, both files of the older ones go");
	Check(crash::OldReports(names, L"h5-together-host", 3).empty(), "old reports: a robot copy's own reports are counted apart");
	Check(crash::OldReports(names, L"h5-together", 5).empty(), "old reports: fewer than kept: none goes");
	Check(strcmp(crash::ExceptionName(0xc0000005), "access violation") == 0 &&
	          strcmp(crash::ExceptionName(0xc00000fd), "stack overflow") == 0 && crash::ExceptionName(0x12345678)[0] == 0,
	      "exception names");
}

}  // namespace

int main(int argc, char **argv)
{
	SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", L"crashtest");
	if (argc >= 3 && strcmp(argv[1], "--child") == 0)
		return Child(argv[2]);
	PureTests();
	const std::wstring log_path = Folder() + kPrefix + L".log";
	const std::wstring bottom = Folder() + kPrefix + L"-bottom.txt";

	// a crash on the main thread
	Clean();
	DWORD code = RunChild("av");
	std::wstring dump;
	std::string report = TheReport(&dump);
	std::string log = ReadAll(log_path);
	Check(code == 0xc0000005, "av: the child ended with the crash's code (our filter called the one before it)");
	Check(Exists(bottom), "av: the filter that was there before ours was called after it");
	Check(Has(report, "H5 Together crash report") && Has(report, "add-on: H5 Together " OPEN_HOMM5_VERSION),
	      "av: the report names our version");
	Check(Has(report, "what: exception c0000005 (access violation: writing address 00000000)"), "av: what crashed");
	Check(Has(report, "where: crash_test.exe+0x"), "av: where, as module + offset");
	Check(Has(report, "thread: ") && Has(report, "(the game's main thread)"), "av: on the main thread");
	Check(Has(report, "registers: eax="), "av: the registers");
	Check(Has(report, "probable callers") && report.find("  crash_test.exe+0x", report.find("probable callers")) != std::string::npos,
	      "av: the callers on the stack, in this exe");
	Check(Has(report, "crash test: about to crash (av)"), "av: the last lines of the log are in the report");
	Check(Has(report, "minidump: h5-together-crashtest-crash-") && Has(report, "KB, by the system's dbg"),
	      "av: the report says the minidump was written");
	const std::string dmp = dump.empty() ? std::string() : ReadAll(dump);
	Check(dmp.size() > 1024 && dmp.compare(0, 4, "MDMP") == 0, "av: the minidump is there and is one (MDMP)");
	Check(Has(log, "crash: the game crashed: exception c0000005 (access violation: writing address 00000000)") &&
	          Has(log, "crash: wrote h5-together-crashtest-crash-"),
	      "av: the log says what crashed and which files were written");
	if (g_failures)
		printf("--- report:\n%s\n--- log:\n%s\n", report.c_str(), log.c_str());

	// the next start: "the last run crashed"
	code = RunChild("ready");
	log = ReadAll(log_path);
	Check(code == 0 && Has(log, "crash: THE LAST RUN OF THE GAME CRASHED: its report h5-together-crashtest-crash-"),
	      "next start: the log says the last run crashed, and names its report");

	// a stack overflow: no stack left on the crashed thread
	Clean();
	code = RunChild("stack");
	report = TheReport(&dump);
	Check(code == 0xc00000fd, "stack: the child ended with a stack overflow");
	Check(Has(report, "what: exception c00000fd (stack overflow)") && !dump.empty(), "stack: report and minidump written");

	// another thread
	Clean();
	code = RunChild("thread");
	report = TheReport(nullptr);
	Check(code == 0xc0000005 && Has(report, "(not the game's main thread)"), "thread: reported, as another thread than the main one");

	// a filter in front of ours recovers: no report
	Clean();
	code = RunChild("above");
	log = ReadAll(log_path);
	Check(code == 7 && TheReport(nullptr).empty() && !Has(log, "crash: the game crashed") &&
	          Has(log, "crash test: went on after the exception"),
	      "above: an exception the filter in front of ours recovers from gives no report");

	// off
	Clean();
	WriteAll(Folder() + kPrefix + L".ini", "[debug]\r\ncrash_report=0\r\n");
	code = RunChild("off");
	log = ReadAll(log_path);
	Check(code == 0xc0000005 && TheReport(nullptr).empty() && Has(log, "crash: off ([debug] crash_report=0)") && Exists(bottom),
	      "off: no report, and the other filter is still called");

	// old reports: only the 3 newest stay
	Clean();
	for (int i = 1; i <= 5; ++i) {
		wchar_t name[96];
		_snwprintf_s(name, 96, _TRUNCATE, L"%s-crash-20200101-00000%d", kPrefix, i);
		WriteAll(Folder() + name + L".txt", "old");
		WriteAll(Folder() + name + L".dmp", "old");
	}
	WriteAll(Folder() + kPrefix + L"-crash-notes.txt", "not a report");
	code = RunChild("ready");
	log = ReadAll(log_path);
	std::vector<std::wstring> left = Matching(std::wstring(kPrefix) + L"-crash-*");
	Check(code == 0 && left.size() == 7 && Exists(Folder() + kPrefix + L"-crash-20200101-000003.dmp") &&
	          !Exists(Folder() + kPrefix + L"-crash-20200101-000002.txt") && Exists(Folder() + kPrefix + L"-crash-notes.txt"),
	      "old reports: the 3 newest are kept (6 files) and other files stay");
	Check(Has(log, "crash: an older crash report: h5-together-crashtest-crash-20200101-000005.txt") &&
	          Has(log, "3 crash report(s) kept, older files removed: 4"),
	      "old reports: the log names the newest and what was removed");
	Clean();
	printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
	return g_failures ? 1 : 0;
}

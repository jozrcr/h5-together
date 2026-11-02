// Crash information (docs/subsystems/hangs.md, "Crashes"): when the game crashes, a short
// "what crashed" text and a small minidump next to the log, for bug reports.
//
//   h5-together-crash-<date>-<time>.txt   what crashed (the exception, where: module + offset,
//                                         the probable callers), our version, the game build,
//                                         the last lines of the log
//   h5-together-crash-<date>-<time>.dmp   a small minidump (Windows' own dbghelp.dll from the
//                                         system folder: the game's bin\dbghelp.dll is from 2002)
//
// The 3 newest are kept (older ones go at the next start). Nothing is sent anywhere; the
// bug report scripts (tools/package/files/bug-report.*) add the newest pair to a report.
//
// How: one top-level exception filter (SetUnhandledExceptionFilter), installed when the DLL
// loads, before the game's own code runs. The game sets its own filter later (0x00c67b10,
// after it made its Direct3D object: it lets some access violations go on, and passes the
// others to the filter that was there before, ours): so a crash the game recovers from
// never reaches us. Ours writes the report and then calls the filter that was there before
// it (MMH5.5's runtime msvcr71.dll has one for C++ exceptions), so whatever happened after
// a crash before (Windows' "stopped working", Wine's crash dialog) still happens.
// The report is written by a thread of ours that waits for it from the start (a crashed
// thread may have no stack left, or hold a lock), with a time limit.
//
// Settings ([debug] in h5-together.ini): crash_report=1 (0 = off: no filter at all);
// crash_test_at=<s> (testing: the game's main thread crashes on purpose, once, that many
// seconds after it first took its messages; needs the hang watchdog on).
#pragma once
#include <windows.h>

#include <string>
#include <vector>

namespace crash {

// From hooks::InstallEarly (the DLL is loading, in an exe we know) or, for an exe that can
// only be recognised later (the Steam-wrapped one), from hooks::InstallAll. Once.
void Install(const char *when);

// The game build's name for the reports (and whether it has the vanilla 3.1 layout, whose
// own crash filter we know); the thread that calls it is the game's main thread
// (dllmain.cpp: the first DirectInput8Create).
void SetGameBuild(const char *name, bool vanilla_layout);

// [debug] crash_test_at: a write to address 0, on the calling thread.
void CrashOnPurpose();

// --- the pure parts (tested by src/hook/test/crash_test.cpp) ------------------------------
// "20261102-153012"
std::wstring Stamp(const SYSTEMTIME &t);
// Of the file names in a folder, the crash reports of `prefix` ("h5-together" or, for a robot
// copy, "h5-together-host") to remove so that only the `keep` newest remain: each report is
// "<prefix>-crash-<stamp>.txt" and ".dmp" (either may be missing). Other names are left alone.
std::vector<std::wstring> OldReports(const std::vector<std::wstring> &names, const std::wstring &prefix, int keep);
// Is this file name one of `prefix`'s reports? Its stamp, and whether it is the .txt.
bool ReportName(const std::wstring &name, const std::wstring &prefix, std::wstring *stamp, bool *text);
// "access violation", "stack overflow"... ("" for a code we have no name for).
const char *ExceptionName(DWORD code);

#ifdef CRASH_TEST
// The report thread is waiting (it looked at the old reports and the last run).
bool ReadyForTest();
#endif

}  // namespace crash

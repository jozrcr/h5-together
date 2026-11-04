// Tiny thread-safe logger: h5-together.log next to the game exe.
#pragma once
#include <windows.h>

namespace hooklog {

// Path of a file next to the game exe: <exe folder>\<stem><ext>, or
// <stem>-<instance><ext> when the OPEN_HOMM5_INSTANCE environment variable is
// set (the robot sets it when it runs several copies of the game at once, so
// each copy gets its own log and robot config).
void PathNextToExe(const wchar_t *stem, const wchar_t *ext, wchar_t *out, DWORD size);

// The log is kept to a size. At start-up a log over kMaxLogBytes becomes
// h5-together.old.log (the one older log that is kept; an earlier .old.log goes) and a
// new log starts. Without this the file grew for ever (1.4 MB after a few weeks of play).
const unsigned long long kMaxLogBytes = 5ull * 1024 * 1024;

// Moves the log at `path` to `old_path` (replacing what is there) if it holds more than
// `limit` bytes. Returns true if it moved it. Does nothing, and returns false, when the
// log is small or missing, or when another copy of the game has it open (Windows then
// refuses to move it, and that copy goes on writing to the same file).
bool RotateIfLarge(const wchar_t *path, const wchar_t *old_path, unsigned long long limit);

void Open(HMODULE self);
void Close();
// printf-style; adds a timestamp (ms since the DLL loaded) and the thread id.
void Write(const char *fmt, ...);
// The log's clock: milliseconds since the log was opened (the first column of its lines).
unsigned long Milliseconds();

// --- for the crash report (crash.cpp) ---------------------------------------------------
// The same as Write, but it gives up (and writes nothing) when another thread holds the
// log for more than about a second: a crashed thread may hold it for ever.
void WriteNoWait(const char *fmt, ...);
// The last lines written (up to kKeptLines, as in the log, oldest first, each ending
// with "\n"), into `out` (always ended with 0). Safe from a crash: it never waits long.
const int kKeptLines = 20;
void LastLines(char *out, size_t size);
// When the log was last written before this run (its time on disk when Open() found it),
// or a zero time when there was no log. A crash report written at about that time
// belongs to the previous run.
FILETIME PreviousRunEnd();

}  // namespace hooklog

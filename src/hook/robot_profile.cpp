// Robot AI profiler (robot mode, profile_ai=1): where does the AI spend its
// thinking time?
//
// While an AI turn driver call is running (the AI trace marks it, see
// ai_trace.cpp), a sampling thread pauses the main thread every few
// milliseconds, notes its instruction pointer and the game's return addresses
// on its stack, and lets it go on. Each sample is one line of
// h5-together-profile[-<instance>].txt next to the exe:
//   <eip> <return address> <return address> ...
// tools/robot/profile_report.py turns the samples into a list of the game
// functions the AI spends its time in (self and inclusive), using the inventory.
#include <windows.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "robot.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;  // this DLL's load address (linker symbol)

namespace robot {
namespace {

volatile LONG g_thinking = 0;
HANDLE g_ai_thread = nullptr;
bool g_all = false;  // profile_all=1: the main thread all the time, not only while the AI thinks
FILE *g_out = nullptr;
volatile LONG g_samples = 0;
DWORD g_exe_begin = 0, g_exe_end = 0;

bool AfterCall(const BYTE *p)
{
	return p[-5] == 0xE8 || (p[-6] == 0xFF && p[-5] == 0x15) || (p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) ||
	       (p[-3] == 0xFF && (p[-2] & 0xF8) == 0x50) || (p[-6] == 0xFF && (p[-5] & 0xF8) == 0x90);
}

// One sample: eip and up to 24 game return addresses of the paused thread.
// Only copies memory: no C library call while the thread is paused (it might
// hold a lock we would then wait for forever).
int Sample(const CONTEXT &ctx, DWORD *out, int max)
{
	int n = 0;
	out[n++] = ctx.Eip;
	const DWORD *stack = reinterpret_cast<const DWORD *>(ctx.Esp);
	for (int i = 0; i < 8192 && n < max; ++i) {
		__try {
			const DWORD v = stack[i];
			if (v >= g_exe_begin + 5 && v < g_exe_end && AfterCall(reinterpret_cast<const BYTE *>(v)))
				out[n++] = v;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			break;  // end of the stack
		}
	}
	return n;
}

void Write(const DWORD *sample, int n)
{
	char line[512];
	int used = 0;
	for (int i = 0; i < n && used < 480; ++i)
		used += _snprintf_s(line + used, sizeof(line) - used, _TRUNCATE, i ? " %08lx" : "%08lx", sample[i]);
	fprintf(g_out, "%s\n", line);
	if ((InterlockedIncrement(&g_samples) & 63) == 0)
		fflush(g_out);
}

DWORD WINAPI Sampler(LPVOID)
{
	DWORD sample[25];
	DWORD marked = 0;
	for (;;) {
		Sleep(2);
		const DWORD now = GetTickCount();
		if (g_out && now - marked >= 500) {  // a time mark (the log's clock), for profile_report.py --from-ms
			marked = now;
			fprintf(g_out, "# t %lu\n", hooklog::Milliseconds());
		}
		if ((!g_thinking && !g_all) || !g_ai_thread)
			continue;
		if (SuspendThread(g_ai_thread) == static_cast<DWORD>(-1))
			continue;
		CONTEXT ctx = {};
		ctx.ContextFlags = CONTEXT_CONTROL;
		int n = 0;
		if ((g_thinking || g_all) && GetThreadContext(g_ai_thread, &ctx))
			n = Sample(ctx, sample, 25);
		ResumeThread(g_ai_thread);
		if (n)
			Write(sample, n);
	}
}

}  // namespace

// With profile_all: how often MMH5.5's main script loop runs. Each pass of
// H55_ContinuesActivator (H55-Core.lua) calls the script function GetPlayerState 8 times
// (for i = 1, 8), so its calls per second (logged with the frame times) / 8 = passes per
// second, at most. The game's script functions are a table of {name, function}
// (GetPlayerState: name 0x00e24f54, function 0x005c1390 at 0x00f34c2c), read when a map's
// script engine starts: the entry is replaced at start-up.
const DWORD kGetPlayerStateCell = 0x00f34c2c, kGetPlayerState = 0x005c1390;
typedef int(__fastcall *ScriptFn)(void *self, void *edx);
ScriptFn g_get_player_state = nullptr;
volatile LONG g_get_player_state_calls = 0;

int __fastcall GetPlayerStateHook(void *self, void *edx)
{
	InterlockedIncrement(&g_get_player_state_calls);
	return g_get_player_state(self, edx);
}

// And what a script's sleep(n) waits. The script function sleep (0x004bfe80; the base
// table's "sleep" at 0x00f7bcb8 and the game's "Sleep" at 0x00f2df00) stores n in the
// script thread (+0x2c of the thread at +0x10 of its first argument); the scheduler's tick
// (0x004c0720, from the world's update: script_rhythm.cpp) takes 1 from every sleeping
// thread's count and wakes the threads at 0. Logged with the frame times: the ticks per
// second (run and held back by the script rhythm) and the sleeps by value (2: MMH5.5's
// main loop).
const DWORD kSleep = 0x004bfe80, kSleepCells[] = {0x00f7bcbc, 0x00f2df04};
ScriptFn g_sleep = nullptr;
volatile LONG g_sleeps[3] = {};  // sleeps: of 2, of 1, others
volatile LONG g_ticks_ran = 0, g_ticks_held = 0;

int SleepCount(void *self)
{
	__try {
		const BYTE *thread = *reinterpret_cast<BYTE *const *>(static_cast<BYTE *>(self) + 0x10);
		return thread ? *reinterpret_cast<const int *>(thread + 0x2c) : -1;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return -1;
	}
}

int __fastcall SleepHook(void *self, void *edx)
{
	const int r = g_sleep(self, edx);
	const int n = SleepCount(self);
	InterlockedIncrement(&g_sleeps[n == 2 ? 0 : n == 1 ? 1 : 2]);
	return r;
}

bool ReplaceCell(DWORD at, DWORD expected, const void *with)
{
	DWORD *cell = reinterpret_cast<DWORD *>(at);
	DWORD old;
	if (*cell != expected || !VirtualProtect(cell, 4, PAGE_READWRITE, &old))
		return false;
	*cell = reinterpret_cast<DWORD>(with);
	VirtualProtect(cell, 4, old, &old);
	return true;
}

void InstallScriptCounter()
{
	g_get_player_state = reinterpret_cast<ScriptFn>(kGetPlayerState);
	if (!ReplaceCell(kGetPlayerStateCell, kGetPlayerState, &GetPlayerStateHook)) {
		hooklog::Write("robot: script counter: the script function table is not as expected");
		return;
	}
	g_sleep = reinterpret_cast<ScriptFn>(kSleep);
	bool sleep_ok = true;
	for (DWORD at : kSleepCells)
		sleep_ok = ReplaceCell(at, kSleep, &SleepHook) && sleep_ok;
	hooklog::Write("robot: script counter on (the script function GetPlayerState's calls; sleep: %s)",
	               sleep_ok ? "counted" : "not as expected");
}

void CountScriptTick(bool held)
{
	InterlockedIncrement(held ? &g_ticks_held : &g_ticks_ran);
}

void StartAiProfiler(bool all)
{
	g_all = all;
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together-profile", L".txt", path, MAX_PATH);
	g_out = _wfsopen(path, L"w", _SH_DENYWR);
	if (!g_out) {
		hooklog::Write("robot: AI profiler: cannot write %ls", path);
		return;
	}
	// where our DLL is loaded, so samples in it can be named with build/hook/dinput8.map
	fprintf(g_out, "# dll %08lx\n", reinterpret_cast<DWORD>(&__ImageBase));
	const BYTE *exe = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
	g_exe_begin = reinterpret_cast<DWORD>(exe);
	g_exe_end = g_exe_begin + reinterpret_cast<const IMAGE_NT_HEADERS32 *>(
	                              exe + reinterpret_cast<const IMAGE_DOS_HEADER *>(exe)->e_lfanew)
	                              ->OptionalHeader.SizeOfImage;
	HANDLE t = CreateThread(nullptr, 0, &Sampler, nullptr, 0, nullptr);
	if (t) {
		SetThreadPriority(t, THREAD_PRIORITY_ABOVE_NORMAL);
		CloseHandle(t);
	}
	hooklog::Write("robot: %s profiler on, samples go to %ls", all ? "main thread (all the time)" : "AI", path);
	if (all)
		InstallScriptCounter();
}

// Frame times, for profile_all (single player has no lockstep line with them): every 10 s,
// frames per second, the median and 95th percentile frame and the longest.
void NoteFrameForProfiler()
{
	static LARGE_INTEGER freq = {}, last = {};
	static DWORD window_start = 0;
	static float times[4096];
	static int count = 0;
	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	if (last.QuadPart && count < 4096)
		times[count++] = static_cast<float>((now.QuadPart - last.QuadPart) * 1000.0 / freq.QuadPart);
	last = now;
	const DWORD tick = GetTickCount();
	if (!window_start)
		window_start = tick;
	if (tick - window_start < 10000 || count < 2)
		return;
	float sorted[4096];
	memcpy(sorted, times, count * sizeof(float));
	for (int i = 1; i < count; ++i)  // (insertion sort: a few hundred values every 10 s)
		for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; --j) {
			const float t = sorted[j];
			sorted[j] = sorted[j - 1];
			sorted[j - 1] = t;
		}
	const LONG calls = InterlockedExchange(&g_get_player_state_calls, 0);
	const double secs = (tick - window_start) / 1000.0;
	const LONG ran = InterlockedExchange(&g_ticks_ran, 0), held = InterlockedExchange(&g_ticks_held, 0);
	hooklog::Write("robot: frames: %.0f/s, median %.1f ms, p95 %.1f ms, longest %.0f ms; script GetPlayerState %.1f calls/s; "
	               "scheduler %.1f ticks/s (%.1f held back); sleeps/s: %.1f of 2, %.1f of 1, %.1f others",
	               count / secs, sorted[count / 2], sorted[count * 95 / 100], sorted[count - 1], calls / secs,
	               ran / secs, held / secs, InterlockedExchange(&g_sleeps[0], 0) / secs,
	               InterlockedExchange(&g_sleeps[1], 0) / secs, InterlockedExchange(&g_sleeps[2], 0) / secs);
	count = 0;
	window_start = tick;
}

void NoteMainThreadForProfiler()
{
	if (g_out && g_all && !g_ai_thread)
		g_ai_thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, GetCurrentThreadId());
}

void OnAiThinking(bool thinking)
{
	if (!g_out)
		return;
	if (thinking && !g_ai_thread)
		g_ai_thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, GetCurrentThreadId());
	InterlockedExchange(&g_thinking, thinking ? 1 : 0);
}

}  // namespace robot

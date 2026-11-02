// Test of the log's size limit (src/hook/log.cpp): a log over the limit becomes
// open-homm5.old.log at start-up, a small one is left alone, and a log that another
// copy of the game holds open stays where it is. No game needed; the files are made in
// a new folder of the temp folder, and next to this exe (instance "logtest").
#include <windows.h>
#include <share.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "../log.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
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

bool Exists(const std::wstring &path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring NewFolder()
{
	wchar_t temp[MAX_PATH];
	GetTempPathW(MAX_PATH, temp);
	std::wstring dir = std::wstring(temp) + L"open-homm5-log-test-" + std::to_wstring(GetCurrentProcessId());
	CreateDirectoryW(dir.c_str(), nullptr);
	return dir;
}

void TestRotateIfLarge()
{
	const std::wstring dir = NewFolder();
	const std::wstring log = dir + L"\\open-homm5.log";
	const std::wstring old = dir + L"\\open-homm5.old.log";
	const unsigned long long limit = 100;

	Check(!hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit) && !Exists(old), "no log: nothing happens");

	WriteAll(log, std::string(50, 'a'));
	Check(!hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit) && Exists(log) && !Exists(old), "small log: left alone");

	WriteAll(log, std::string(100, 'b'));
	Check(!hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit) && Exists(log) && !Exists(old),
	      "a log of exactly the limit: left alone");

	const std::string big(101, 'c');
	WriteAll(log, big);
	Check(hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit), "log over the limit: moved");
	Check(!Exists(log) && ReadAll(old) == big, "log over the limit: it is now the old log, byte for byte, and the log is gone");

	WriteAll(log, std::string(200, 'd'));
	Check(hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit) && ReadAll(old) == std::string(200, 'd') && !Exists(log),
	      "a second rotation replaces the old log (only one is kept)");

	// Another copy of the game has the log open for writing (as ours does): Windows does not
	// let it move, and nothing breaks.
	WriteAll(log, std::string(300, 'e'));
	FILE *held = _wfsopen(log.c_str(), L"a", _SH_DENYWR);
	const bool moved = hooklog::RotateIfLarge(log.c_str(), old.c_str(), limit);
	if (held)
		fclose(held);
	Check(held != nullptr && !moved && ReadAll(log) == std::string(300, 'e') && ReadAll(old) == std::string(200, 'd'),
	      "log held open by another copy: not moved, nothing lost");

	DeleteFileW(log.c_str());
	DeleteFileW(old.c_str());
	RemoveDirectoryW(dir.c_str());
}

// Open() end to end, on the files next to this exe: instance "logtest" gives open-homm5-logtest.log.
void TestOpen()
{
	SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", L"logtest");
	wchar_t log_c[MAX_PATH], old_c[MAX_PATH];
	hooklog::PathNextToExe(L"open-homm5", L".log", log_c, MAX_PATH);
	hooklog::PathNextToExe(L"open-homm5", L".old.log", old_c, MAX_PATH);
	const std::wstring log = log_c, old = old_c;
	Check(log.find(L"open-homm5-logtest.log") != std::wstring::npos && old.find(L"open-homm5-logtest.old.log") != std::wstring::npos,
	      "paths: open-homm5-<instance>.log and open-homm5-<instance>.old.log");
	DeleteFileW(log.c_str());
	DeleteFileW(old.c_str());

	// a new log starts with the line we write first, nothing before it
	hooklog::Open(nullptr);
	hooklog::Write("first line");
	hooklog::Close();
	std::string text = ReadAll(log);
	Check(text.size() > 0 && text.find("first line") != std::string::npos && text.find("new run") == std::string::npos &&
	          text.find("first line") < 30,
	      "new log: the first line is the first thing written, with no separator before it");

	// a log that goes on from a small earlier one: kept, with a separator before the new run
	hooklog::Open(nullptr);
	hooklog::Write("second run");
	hooklog::Close();
	text = ReadAll(log);
	const size_t first = text.find("first line"), sep = text.find("new run"), second = text.find("second run");
	Check(first != std::string::npos && sep != std::string::npos && second != std::string::npos && first < sep && sep < second &&
	          !Exists(old),
	      "small log: the next run is appended after a separator line; no old log");

	// a log over the limit: becomes the old log, the new run starts a clean file
	const std::string big(static_cast<size_t>(hooklog::kMaxLogBytes) + 1, 'x');
	WriteAll(log, big);
	hooklog::Open(nullptr);
	hooklog::Write("third run");
	hooklog::Close();
	text = ReadAll(log);
	Check(ReadAll(old) == big, "big log: kept whole as the old log");
	Check(text.find("third run") != std::string::npos && text.find("new run") == std::string::npos && text.find('x') == std::string::npos &&
	          text.size() < 200,
	      "big log: the new log starts empty and has only the new run");

	DeleteFileW(log.c_str());
	DeleteFileW(old.c_str());
	SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", nullptr);
}

}  // namespace

int main()
{
	TestRotateIfLarge();
	TestOpen();
	if (g_failures) {
		printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	printf("log: all checks passed\n");
	return 0;
}

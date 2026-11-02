// Test of the move from the add-on's old name (src/hook/old_names.cpp): open-homm5.ini,
// open-homm5-skins.ini and open-homm5.log become h5-together.*, only when the new name is
// not there yet, and our folder is found under its old name while the new one is missing.
// No game needed; the files are made next to this exe (instance "oldnamestest", so they
// do not meet any other test's files) and removed again.
#include <windows.h>
#include <share.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "../log.h"
#include "../old_names.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

std::wstring Path(const wchar_t *stem, const wchar_t *ext)
{
	wchar_t p[MAX_PATH];
	hooklog::PathNextToExe(stem, ext, p, MAX_PATH);
	return p;
}

std::string ReadAll(const std::wstring &path)
{
	std::string out;
	FILE *f = _wfsopen(path.c_str(), L"rb", _SH_DENYNO);
	if (!f)
		return out;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
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

bool Has(const std::vector<std::string> &lines, const char *text)
{
	for (const std::string &l : lines)
		if (l.find(text) != std::string::npos)
			return true;
	return false;
}

void TestRules()
{
	using oldnames::Step;
	Check(oldnames::PlanMove(false, false) == Step::kNone && oldnames::PlanMove(false, true) == Step::kNone,
	      "rules: no old file: nothing to do");
	Check(oldnames::PlanMove(true, false) == Step::kMove, "rules: only the old file: it moves to the new name");
	Check(oldnames::PlanMove(true, true) == Step::kLeave, "rules: both: the old one stays, the new one is used");
	Check(oldnames::PlanLog(true, false, false) == Step::kMove, "rules: the old log moves while the new name has no log");
	Check(oldnames::PlanLog(true, true, false) == Step::kLeave && oldnames::PlanLog(true, false, true) == Step::kLeave,
	      "rules: the old log stays once the new name has a log or an older log");
	Check(oldnames::PlanLog(false, false, false) == Step::kNone, "rules: no old log: nothing to do");
	Check(!oldnames::UseOldFolder(false, false) && !oldnames::UseOldFolder(true, false) && !oldnames::UseOldFolder(true, true) &&
	          oldnames::UseOldFolder(false, true),
	      "rules: the old folder only while the new one is missing");
}

void TestMove()
{
	SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", L"oldnamestest");
	const std::wstring old_ini = Path(L"open-homm5", L".ini"), ini = Path(L"h5-together", L".ini");
	const std::wstring old_skins = Path(L"open-homm5-skins", L".ini"), skins = Path(L"h5-together-skins", L".ini");
	const std::wstring old_log = Path(L"open-homm5", L".log"), old_old_log = Path(L"open-homm5", L".old.log");
	const std::wstring log = Path(L"h5-together", L".log"), older_log = Path(L"h5-together", L".old.log");
	const std::wstring all[] = {old_ini, ini, old_skins, skins, old_log, old_old_log, log, older_log};
	for (const std::wstring &p : all)
		DeleteFileW(p.c_str());
	Check(ini.find(L"h5-together-oldnamestest.ini") != std::wstring::npos &&
	          old_skins.find(L"open-homm5-skins-oldnamestest.ini") != std::wstring::npos,
	      "paths: each copy of the game has its own old and new names");

	// a 0.2.7 install: the settings, the looks, the log and its older log
	WriteAll(old_ini, "[net]\r\ntuning=0\r\n");
	WriteAll(old_skins, "[skins]\r\nGodric=Freyda\r\n");
	WriteAll(old_log, "the last log of 0.2.7\n");
	WriteAll(old_old_log, "an even older log\n");
	std::vector<std::string> lines = oldnames::MoveOldFiles();
	Check(ReadAll(ini) == "[net]\r\ntuning=0\r\n" && !Exists(old_ini), "0.2.7 install: the settings file has the new name, the same bytes");
	Check(ReadAll(skins) == "[skins]\r\nGodric=Freyda\r\n" && !Exists(old_skins), "0.2.7 install: the hero looks' file has the new name");
	Check(ReadAll(older_log) == "the last log of 0.2.7\n" && !Exists(old_log) && !Exists(log),
	      "0.2.7 install: the old log is now the older log of the new name, and no new log is made");
	Check(ReadAll(old_old_log) == "an even older log\n", "0.2.7 install: the old name's older log is left alone");
	Check(Has(lines, "open-homm5-oldnamestest.ini is now h5-together-oldnamestest.ini") &&
	          Has(lines, "open-homm5-skins-oldnamestest.ini is now h5-together-skins-oldnamestest.ini") &&
	          Has(lines, "the log open-homm5-oldnamestest.log is now h5-together-oldnamestest.old.log"),
	      "0.2.7 install: the log lines say what moved");

	// the next start: nothing left to do, nothing said
	lines = oldnames::MoveOldFiles();
	bool quiet = true;
	for (const std::string &l : lines)
		quiet = quiet && l.find("oldnamestest") == std::string::npos;
	Check(quiet && ReadAll(ini) == "[net]\r\ntuning=0\r\n", "next start: nothing moves, no line about the files");

	// both names there: the new ones are kept and used, the old ones stay as they are
	WriteAll(old_ini, "[net]\r\ntuning=1\r\n");
	WriteAll(old_log, "a log of an old copy\n");
	WriteAll(log, "the new log\n");
	lines = oldnames::MoveOldFiles();
	Check(ReadAll(ini) == "[net]\r\ntuning=0\r\n" && ReadAll(old_ini) == "[net]\r\ntuning=1\r\n",
	      "both settings files: neither changes");
	Check(Has(lines, "open-homm5-oldnamestest.ini stays as it is: h5-together-oldnamestest.ini is there"),
	      "both settings files: the log says the new one is used");
	Check(ReadAll(old_log) == "a log of an old copy\n" && ReadAll(older_log) == "the last log of 0.2.7\n" &&
	          ReadAll(log) == "the new log\n" && !Has(lines, "the log open-homm5"),
	      "an old log once the new name has logs: left alone, the logs of the new name too");

	for (const std::wstring &p : all)
		DeleteFileW(p.c_str());
	SetEnvironmentVariableW(L"OPEN_HOMM5_INSTANCE", nullptr);
}

void TestFolder()
{
	wchar_t exe[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	while (n > 0 && exe[n - 1] != L'\\')
		--n;
	const std::wstring dir(exe, n);
	const std::wstring folder = dir + L"h5-together", old_folder = dir + L"open-homm5";
	const std::wstring item = folder + L"\\old-names-test", old_item = old_folder + L"\\old-names-test";
	wchar_t got[MAX_PATH];

	DWORD at = oldnames::PathInOurFolder(L"old-names-test", got, MAX_PATH);
	Check(std::wstring(got) == item && std::wstring(got + at) == L"h5-together\\old-names-test",
	      "folder: neither there: h5-together\\<name> (where a new file goes)");
	const bool made_old = CreateDirectoryW(old_folder.c_str(), nullptr) != 0;
	CreateDirectoryW(old_item.c_str(), nullptr);
	at = oldnames::PathInOurFolder(L"old-names-test", got, MAX_PATH);
	Check(std::wstring(got) == old_item && std::wstring(got + at) == L"open-homm5\\old-names-test",
	      "folder: only under the old name: open-homm5\\<name>");
	if (made_old && GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) {
		const std::vector<std::string> lines = oldnames::MoveOldFiles();
		Check(Has(lines, "our folder is still open-homm5 (there is no h5-together yet)"),
		      "folder: the start-up line says the old folder is used");
	}
	const bool made = CreateDirectoryW(folder.c_str(), nullptr) != 0;
	CreateDirectoryW(item.c_str(), nullptr);
	oldnames::PathInOurFolder(L"old-names-test", got, MAX_PATH);
	Check(std::wstring(got) == item, "folder: under both names: the new one");
	RemoveDirectoryW(old_item.c_str());
	oldnames::PathInOurFolder(L"old-names-test", got, MAX_PATH);
	Check(std::wstring(got) == item, "folder: only under the new name: the new one");
	RemoveDirectoryW(item.c_str());
	if (made)
		RemoveDirectoryW(folder.c_str());
	if (made_old)
		RemoveDirectoryW(old_folder.c_str());
}

}  // namespace

int main()
{
	TestRules();
	TestMove();
	TestFolder();
	if (g_failures) {
		printf("%d old names test(s) FAILED\n", g_failures);
		return 1;
	}
	printf("all old names tests passed\n");
	return 0;
}

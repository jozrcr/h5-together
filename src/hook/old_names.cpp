#include "old_names.h"

#include <stdio.h>
#include <wchar.h>

#include "log.h"

namespace oldnames {

const wchar_t kFolder[] = L"h5-together";
const wchar_t kOldFolder[] = L"open-homm5";

Step PlanMove(bool old_exists, bool new_exists)
{
	if (!old_exists)
		return Step::kNone;
	return new_exists ? Step::kLeave : Step::kMove;
}

Step PlanLog(bool old_log, bool new_log, bool new_old_log)
{
	return PlanMove(old_log, new_log || new_old_log);
}

bool UseOldFolder(bool new_exists, bool old_exists)
{
	return !new_exists && old_exists;
}

namespace {

bool Exists(const wchar_t *path)
{
	return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

bool IsFolder(const wchar_t *path)
{
	const DWORD a = GetFileAttributesW(path);
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// The file name of a path (after its last backslash), for the log.
const wchar_t *Name(const wchar_t *path)
{
	const wchar_t *slash = wcsrchr(path, L'\\');
	return slash ? slash + 1 : path;
}

std::string Line(const char *fmt, const wchar_t *a, const wchar_t *b, DWORD error = 0)
{
	char line[512];
	_snprintf_s(line, sizeof(line), _TRUNCATE, fmt, Name(a), Name(b), error);
	return line;
}

// Does what `step` says with the file `from` (the old name) and `to` (the new name), and
// adds a line for the log.
void Apply(Step step, const wchar_t *from, const wchar_t *to, const char *moved, std::vector<std::string> *out)
{
	if (step == Step::kMove) {
		if (MoveFileW(from, to))
			out->push_back(Line(moved, from, to));
		else
			out->push_back(Line("old names: could not move %ls to %ls (error %lu)", from, to, GetLastError()));
	}
	else if (step == Step::kLeave) {
		out->push_back(Line("old names: %ls stays as it is: %ls is there, and is the one used", from, to));
	}
}

// <exe folder>\<folder>
void FolderNextToExe(const wchar_t *folder, wchar_t *out, DWORD size, DWORD *dir_len)
{
	DWORD n = GetModuleFileNameW(nullptr, out, size);
	while (n > 0 && out[n - 1] != L'\\')
		--n;
	lstrcpynW(out + n, folder, static_cast<int>(size - n));
	if (dir_len)
		*dir_len = n;
}

}  // namespace

std::vector<std::string> MoveOldFiles()
{
	std::vector<std::string> out;
	wchar_t from[MAX_PATH], to[MAX_PATH], other[MAX_PATH];
	const char moved[] = "old names: %ls is now %ls (the add-on was called open-homm5 until 0.2.7)";

	// the settings, then the hero looks' choices
	hooklog::PathNextToExe(L"open-homm5", L".ini", from, MAX_PATH);
	hooklog::PathNextToExe(L"h5-together", L".ini", to, MAX_PATH);
	Apply(PlanMove(Exists(from), Exists(to)), from, to, moved, &out);
	hooklog::PathNextToExe(L"open-homm5-skins", L".ini", from, MAX_PATH);
	hooklog::PathNextToExe(L"h5-together-skins", L".ini", to, MAX_PATH);
	Apply(PlanMove(Exists(from), Exists(to)), from, to, moved, &out);

	// the log: the old one becomes the older log of the new name
	hooklog::PathNextToExe(L"open-homm5", L".log", from, MAX_PATH);
	hooklog::PathNextToExe(L"h5-together", L".log", other, MAX_PATH);
	hooklog::PathNextToExe(L"h5-together", L".old.log", to, MAX_PATH);
	const Step log = PlanLog(Exists(from), Exists(other), Exists(to));
	if (log == Step::kMove)
		Apply(log, from, to, "old names: the log %ls is now %ls (the add-on was called open-homm5 until 0.2.7)", &out);

	// our folder
	FolderNextToExe(kFolder, to, MAX_PATH, nullptr);
	FolderNextToExe(kOldFolder, from, MAX_PATH, nullptr);
	if (UseOldFolder(IsFolder(to), IsFolder(from)))
		out.push_back(Line("old names: our folder is still %ls (there is no %ls yet): its files are used from there", from, to));
	return out;
}

DWORD PathInOurFolder(const wchar_t *rel, wchar_t *out, DWORD size)
{
	wchar_t folder[MAX_PATH], old_folder[MAX_PATH], path[MAX_PATH], old_path[MAX_PATH];
	DWORD n = 0;
	FolderNextToExe(kFolder, folder, MAX_PATH, &n);
	FolderNextToExe(kOldFolder, old_folder, MAX_PATH, nullptr);
	_snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%s\\%s", folder, rel);
	_snwprintf_s(old_path, MAX_PATH, _TRUNCATE, L"%s\\%s", old_folder, rel);
	lstrcpynW(out, UseOldFolder(Exists(path), Exists(old_path)) ? old_path : path, static_cast<int>(size));
	return n;
}

}  // namespace oldnames

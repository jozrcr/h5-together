// Changing h5-together.ini in place (see settings_ini.h). The line rules are
// install.ps1's Get-IniLine: spaces and tabs around a line do not count; "[name]"
// is a section; a line that starts with ";" or "#" is a comment, and ";key=..."
// (no space) is the commented line of a setting; "key = value" is a setting.
#include "settings_ini.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <string.h>

namespace mods {
namespace settings {
namespace {

struct Line {
	std::string text;  // without its line ending
	std::string eol;   // "\r\n", "\n" or "" (the last line, without one)
};

enum Kind { kBlank, kSection, kComment, kSetting, kOther };

struct Parsed {
	Kind kind = kOther;
	std::string name;  // kSection: the section's name; kComment: the key of ";key=..." (or ""); kSetting: the key
	std::string value;  // kSetting
};

std::string Lower(std::string s)
{
	for (char &c : s)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	return s;
}

std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t'))
		++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t'))
		--b;
	return s.substr(a, b - a);
}

bool KeyChar(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

Parsed Parse(const std::string &line)
{
	Parsed p;
	const std::string t = Trim(line);
	if (t.empty()) {
		p.kind = kBlank;
		return p;
	}
	const size_t close = t.find(']');
	if (t[0] == '[' && close != std::string::npos && close > 0) {
		p.kind = kSection;
		p.name = Trim(t.substr(1, close - 1));
		return p;
	}
	if (t[0] == ';' || t[0] == '#') {
		p.kind = kComment;
		size_t i = 1;
		if (t[0] == ';') {
			while (i < t.size() && KeyChar(t[i]))
				++i;
			if (i > 1 && i < t.size() && t[i] == '=')
				p.name = t.substr(1, i - 1);
		}
		return p;
	}
	const size_t eq = t.find('=');
	if (eq != std::string::npos && eq > 0) {
		p.kind = kSetting;
		p.name = Trim(t.substr(0, eq));
		p.value = Trim(t.substr(eq + 1));
	}
	return p;
}

// The file as lines; *bom: a UTF-8 byte order mark at its start (kept as it is).
std::vector<Line> Split(const std::string &file, std::string *bom)
{
	std::vector<Line> lines;
	size_t start = 0;
	bom->clear();
	if (file.compare(0, 3, "\xef\xbb\xbf") == 0) {
		*bom = file.substr(0, 3);
		start = 3;
	}
	while (start < file.size()) {
		const size_t nl = file.find('\n', start);
		Line l;
		if (nl == std::string::npos) {
			l.text = file.substr(start);
			start = file.size();
		}
		else {
			l.text = file.substr(start, nl - start);
			l.eol = "\n";
			if (!l.text.empty() && l.text.back() == '\r') {
				l.text.pop_back();
				l.eol = "\r\n";
			}
			start = nl + 1;
		}
		lines.push_back(l);
	}
	return lines;
}

std::string Eol(const std::vector<Line> &lines)
{
	bool lf = false;
	for (const Line &l : lines) {
		if (l.eol == "\r\n")
			return "\r\n";
		if (l.eol == "\n")
			lf = true;
	}
	return lf ? "\n" : "\r\n";
}

// [first, end) of the first section of that name: the lines after its heading. False when there is none.
bool SectionRange(const std::vector<Line> &lines, const std::string &section, size_t *first, size_t *end)
{
	const std::string want = Lower(section);
	for (size_t i = 0; i < lines.size(); ++i) {
		const Parsed p = Parse(lines[i].text);
		if (p.kind != kSection || Lower(p.name) != want)
			continue;
		size_t j = i + 1;
		while (j < lines.size() && Parse(lines[j].text).kind != kSection)
			++j;
		*first = i + 1;
		*end = j;
		return true;
	}
	return false;
}

void Apply(std::vector<Line> *lines, const Change &c, const std::string &eol)
{
	size_t first = 0, end = 0;
	const std::string key = Lower(c.key);
	if (!SectionRange(*lines, c.section, &first, &end)) {
		if (c.to_default)
			return;
		if (!lines->empty() && lines->back().eol.empty())
			lines->back().eol = eol;
		if (!lines->empty() && Parse(lines->back().text).kind != kBlank)
			lines->push_back({"", eol});
		lines->push_back({"[" + c.section + "]", eol});
		lines->push_back({c.key + "=" + c.value, eol});
		return;
	}
	std::vector<size_t> active;
	size_t slot = std::string::npos, last = first - 1;  // last: the section's last line that is not blank
	for (size_t i = first; i < end; ++i) {
		const Parsed p = Parse((*lines)[i].text);
		if (p.kind != kBlank)
			last = i;
		if (p.kind == kSetting && Lower(p.name) == key)
			active.push_back(i);
		else if (p.kind == kComment && !p.name.empty() && Lower(p.name) == key && slot == std::string::npos)
			slot = i;
	}
	if (!c.to_default) {
		const std::string text = c.key + "=" + c.value;
		if (!active.empty())
			(*lines)[active[0]].text = text;
		else if (slot != std::string::npos)
			(*lines)[slot].text = text;
		else {
			Line l = {text, eol};
			if ((*lines)[last].eol.empty()) {  // the file's last line, without a line ending: ours is the last now
				(*lines)[last].eol = eol;
				l.eol.clear();
			}
			lines->insert(lines->begin() + static_cast<long>(last + 1), l);
		}
		return;
	}
	if (active.empty())
		return;
	// back to the default: the ";" comes back (one line per setting), later copies go
	std::vector<size_t> gone(active.begin() + 1, active.end());
	if (slot != std::string::npos)
		gone.insert(gone.begin(), active[0]);
	else
		(*lines)[active[0]].text = ";" + c.key + "=" + c.def;
	for (size_t k = gone.size(); k-- > 0;) {
		const size_t i = gone[k];
		if (i + 1 == lines->size() && (*lines)[i].eol.empty() && i > 0)
			(*lines)[i - 1].eol.clear();  // the last line goes: the one before becomes the last, as the file was
		lines->erase(lines->begin() + static_cast<long>(i));
	}
}

}  // namespace

bool Edit(const std::string &file, const std::vector<Change> &changes, std::string *out)
{
	if (file.find('\0') != std::string::npos)
		return false;
	std::string bom;
	std::vector<Line> lines = Split(file, &bom);
	const std::string eol = Eol(lines);
	for (const Change &c : changes)
		Apply(&lines, c, eol);
	std::string s = bom;
	for (const Line &l : lines)
		s += l.text + l.eol;
	*out = s;
	return true;
}

bool Read(const std::string &file, const char *section, const char *key, std::string *value)
{
	std::string bom;
	const std::vector<Line> lines = Split(file, &bom);
	size_t first = 0, end = 0;
	if (!SectionRange(lines, section, &first, &end))
		return false;
	const std::string want = Lower(key);
	for (size_t i = first; i < end; ++i) {
		const Parsed p = Parse(lines[i].text);
		if (p.kind == kSetting && Lower(p.name) == want) {
			*value = p.value;
			return true;
		}
	}
	return false;
}

#ifdef _WIN32
bool EditFile(const wchar_t *path, const std::vector<Change> &changes, std::string *error)
{
	std::string file;
	HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (h != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER size = {};
		if (!GetFileSizeEx(h, &size) || size.QuadPart > 4 * 1024 * 1024) {
			CloseHandle(h);
			*error = "the file is too big to be a settings file";
			return false;
		}
		file.resize(static_cast<size_t>(size.QuadPart));
		DWORD got = 0;
		const BOOL ok = file.empty() || ReadFile(h, &file[0], static_cast<DWORD>(file.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok || got != file.size()) {
			*error = "the file could not be read (error " + std::to_string(GetLastError()) + ")";
			return false;
		}
	}
	else if (GetLastError() != ERROR_FILE_NOT_FOUND) {
		*error = "the file could not be opened (error " + std::to_string(GetLastError()) + ")";
		return false;
	}
	std::string edited;
	if (!Edit(file, changes, &edited)) {
		*error = "the file is not plain text (saved as Unicode?)";
		return false;
	}
	if (edited == file)
		return true;
	const DWORD attributes = GetFileAttributesW(path);
	if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
		*error = "the file is read-only";
		return false;
	}
	const std::wstring temp = std::wstring(path) + L".new";
	h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		const DWORD e = GetLastError();
		*error = e == ERROR_ACCESS_DENIED ? "Windows does not let the game write in its folder (a protected folder?)"
		                                  : "the file could not be written (error " + std::to_string(e) + ")";
		return false;
	}
	DWORD wrote = 0;
	const BOOL ok = WriteFile(h, edited.data(), static_cast<DWORD>(edited.size()), &wrote, nullptr) && FlushFileBuffers(h);
	CloseHandle(h);
	if (!ok || wrote != edited.size() || !MoveFileExW(temp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		const DWORD e = GetLastError();
		DeleteFileW(temp.c_str());
		*error = e == ERROR_ACCESS_DENIED ? "Windows does not let the game write in its folder (a protected folder?)"
		                                  : "the file could not be written (error " + std::to_string(e) + ")";
		return false;
	}
	return true;
}
#endif

}  // namespace settings
}  // namespace mods

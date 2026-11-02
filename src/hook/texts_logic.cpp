// The pure part of our texts (see texts_logic.h).
#include "texts_logic.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace texts {
namespace {

bool IsSpace(char c)
{
	return c == ' ' || c == '\t';
}

std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && IsSpace(s[a]))
		++a;
	while (b > a && IsSpace(s[b - 1]))
		--b;
	return s.substr(a, b - a);
}

bool KeyChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
}

bool ValidKey(const std::string &key)
{
	if (key.empty() || key[0] == '.' || key[key.size() - 1] == '.')
		return false;
	for (char c : key)
		if (!KeyChar(c))
			return false;
	return true;
}

// Is this UTF-8 (no overlong forms, no surrogates)?
bool Utf8(const std::string &s)
{
	const unsigned char *p = reinterpret_cast<const unsigned char *>(s.data()), *end = p + s.size();
	while (p < end) {
		const unsigned c = *p;
		int more = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
		if (more < 0 || (c & 0xfe) == 0xc0 || end - p <= more)
			return false;
		unsigned code = more == 0 ? c : more == 1 ? c & 0x1f : more == 2 ? c & 0x0f : c & 0x07;
		for (int i = 1; i <= more; ++i) {
			if ((p[i] & 0xc0) != 0x80)
				return false;
			code = (code << 6) | (p[i] & 0x3f);
		}
		if ((more == 2 && (code < 0x800 || (code >= 0xd800 && code < 0xe000))) || (more == 3 && (code < 0x10000 || code > 0x10ffff)))
			return false;
		p += more + 1;
	}
	return true;
}

void Append(std::string *out, unsigned code)
{
	if (code < 0x80)
		*out += static_cast<char>(code);
	else if (code < 0x800) {
		*out += static_cast<char>(0xc0 | (code >> 6));
		*out += static_cast<char>(0x80 | (code & 0x3f));
	}
	else if (code < 0x10000) {
		*out += static_cast<char>(0xe0 | (code >> 12));
		*out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
		*out += static_cast<char>(0x80 | (code & 0x3f));
	}
	else {
		*out += static_cast<char>(0xf0 | (code >> 18));
		*out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
		*out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
		*out += static_cast<char>(0x80 | (code & 0x3f));
	}
}

// UTF-16 LE bytes (after the mark) -> UTF-8. A lone surrogate becomes U+FFFD.
std::string FromUtf16(const char *p, size_t size)
{
	std::wstring w;
	for (size_t i = 0; i + 1 < size; i += 2)
		w += static_cast<wchar_t>(static_cast<unsigned char>(p[i]) | (static_cast<unsigned char>(p[i + 1]) << 8));
	return Narrow(w);
}

std::string Line(int n, const std::string &what)
{
	char head[32];
	_snprintf_s(head, sizeof(head), _TRUNCATE, "line %d: ", n);
	return head + what;
}

// "\n" -> a new line, "\\" -> one backslash; any other backslash stays.
std::string Unescape(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == 'n' || s[i + 1] == '\\')) {
			out += s[i + 1] == 'n' ? '\n' : '\\';
			++i;
		}
		else
			out += s[i];
	}
	return out;
}

bool PlaceholderName(const std::string &text, size_t open, size_t *close)
{
	size_t i = open + 1;
	while (i < text.size() && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') || text[i] == '_'))
		++i;
	if (i == open + 1 || i >= text.size() || text[i] != '}')
		return false;
	*close = i;
	return true;
}

std::string Join(const std::vector<std::string> &v)
{
	std::string s;
	for (const std::string &x : v)
		s += (s.empty() ? "" : " ") + x;
	return s.empty() ? "none" : s;
}

}  // namespace

bool Parse(const std::string &bytes, Book *book)
{
	book->texts.clear();
	book->problems.clear();
	std::string text;
	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe)
		text = FromUtf16(bytes.data() + 2, bytes.size() - 2);
	else if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xfe && static_cast<unsigned char>(bytes[1]) == 0xff) {
		book->problems.push_back("the file is UTF-16 big-endian: save it as UTF-8");
		return false;
	}
	else if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0)
		text = bytes.substr(3);
	else
		text = bytes;
	if (text.find('\0') != std::string::npos) {
		book->problems.push_back("the file is not text (it holds zero bytes): save it as UTF-8");
		return false;
	}
	std::string section;
	int n = 0, bad = 0, lines = 0;
	size_t at = 0;
	while (at < text.size()) {
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		std::string line = text.substr(at, end - at);
		at = end + 1;
		++n;
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		line = Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;
		++lines;
		if (!Utf8(line)) {
			book->problems.push_back(Line(n, "not UTF-8 (save the file as UTF-8): left out"));
			++bad;
			continue;
		}
		if (line[0] == '[') {
			const std::string name = Trim(line.substr(1, line.size() >= 2 && line[line.size() - 1] == ']' ? line.size() - 2 : std::string::npos));
			if (line[line.size() - 1] != ']' || !ValidKey(name) || name.find('.') != std::string::npos) {
				book->problems.push_back(Line(n, "\"" + line + "\" is not a section name like [players]: its lines are left out"));
				section.clear();
				++bad;
			}
			else
				section = name;
			continue;
		}
		const size_t eq = line.find('=');
		if (eq == std::string::npos) {
			book->problems.push_back(Line(n, "no \"=\" (a line is \"key = text\"): left out"));
			++bad;
			continue;
		}
		const std::string key = Trim(line.substr(0, eq));
		const std::string value = Unescape(Trim(line.substr(eq + 1)));
		if (!ValidKey(key)) {
			book->problems.push_back(Line(n, "\"" + key + "\" is not a key (small letters, digits, _ and .): left out"));
			++bad;
			continue;
		}
		if (section.empty()) {
			book->problems.push_back(Line(n, key + " is not in a [section]: left out"));
			++bad;
			continue;
		}
		if (value.empty()) {
			book->problems.push_back(Line(n, section + "." + key + " has no text: left out"));
			++bad;
			continue;
		}
		const std::string full = section + "." + key;
		if (book->texts.count(full)) {
			book->problems.push_back(Line(n, full + " is there twice: the first one is used"));
			continue;
		}
		book->texts[full] = value;
	}
	if (lines > 0 && bad == lines) {
		book->problems.push_back("no line could be read");
		return false;
	}
	return true;
}

std::vector<std::string> Marks(const std::string &text)
{
	std::vector<std::string> marks;
	for (size_t i = 0; i < text.size(); ++i) {
		size_t close = 0;
		if (text[i] == '{' && PlaceholderName(text, i, &close)) {
			marks.push_back(text.substr(i, close - i + 1));
			i = close;
		}
		else if (text[i] == '<') {
			close = text.find('>', i + 1);
			if (close != std::string::npos && text.find('<', i + 1) > close) {
				marks.push_back(text.substr(i, close - i + 1));
				i = close;
			}
		}
	}
	std::sort(marks.begin(), marks.end());
	return marks;
}

void CheckAgainst(const Book &english, Book *book)
{
	for (auto it = book->texts.begin(); it != book->texts.end();) {
		const auto en = english.texts.find(it->first);
		if (en == english.texts.end()) {
			book->problems.push_back(it->first + ": English has no such text (an old or misspelt key?): left out");
			it = book->texts.erase(it);
			continue;
		}
		const std::vector<std::string> mine = Marks(it->second), want = Marks(en->second);
		if (mine != want) {
			book->problems.push_back(it->first + ": its {names} and <tags> (" + Join(mine) + ") are not the English ones (" + Join(want) +
			                         "): English is used");
			it = book->texts.erase(it);
			continue;
		}
		++it;
	}
}

std::string Fill(const std::string &text, std::initializer_list<Arg> args)
{
	std::string out;
	for (size_t i = 0; i < text.size(); ++i) {
		size_t close = 0;
		if (text[i] == '{' && PlaceholderName(text, i, &close)) {
			const std::string name = text.substr(i + 1, close - i - 1);
			const Arg *found = nullptr;
			for (const Arg &a : args)
				if (name == a.name)
					found = &a;
			if (found) {
				out += found->value;
				i = close;
				continue;
			}
		}
		out += text[i];
	}
	return out;
}

bool ValidCode(const std::string &code)
{
	size_t i = 0;
	while (i < code.size() && code[i] >= 'a' && code[i] <= 'z')
		++i;
	if (i < 2 || i > 3)
		return false;
	if (i == code.size())
		return true;
	if (code[i] != '-' || code.size() - i - 1 < 1 || code.size() - i - 1 > 8)
		return false;
	for (++i; i < code.size(); ++i)
		if (!((code[i] >= 'a' && code[i] <= 'z') || (code[i] >= '0' && code[i] <= '9')))
			return false;
	return true;
}

std::string SteamCode(const std::string &steam_name)
{
	// Steam's names for its languages (its "API language codes").
	static const char *const kNames[][2] = {
		{"english", "en"},   {"french", "fr"},     {"german", "de"},     {"russian", "ru"},    {"polish", "pl"},
		{"spanish", "es"},   {"latam", "es"},      {"italian", "it"},    {"czech", "cs"},      {"hungarian", "hu"},
		{"brazilian", "pt-br"}, {"portuguese", "pt"}, {"schinese", "zh"}, {"tchinese", "zh-tw"}, {"japanese", "ja"},
		{"koreana", "ko"},   {"ukrainian", "uk"},  {"dutch", "nl"},      {"turkish", "tr"},    {"swedish", "sv"},
		{"danish", "da"},    {"finnish", "fi"},    {"norwegian", "no"},  {"greek", "el"},      {"romanian", "ro"},
		{"bulgarian", "bg"}, {"thai", "th"},       {"vietnamese", "vi"},
	};
	std::string name;
	for (char c : steam_name)
		name += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
	for (const auto &n : kNames)
		if (name == n[0])
			return n[1];
	return std::string();
}

bool ManifestLanguage(const std::string &acf, std::string *steam_name, std::string *installdir)
{
	// Valve's text format: "key" "value" pairs and "key" { ... } blocks.
	std::vector<std::string> blocks;
	std::string language, in_user, install;
	std::vector<std::string> tokens;
	size_t i = 0;
	std::string pending;
	bool have_pending = false;
	while (i < acf.size()) {
		const char c = acf[i];
		if (c == '"') {
			std::string s;
			for (++i; i < acf.size() && acf[i] != '"'; ++i) {
				if (acf[i] == '\\' && i + 1 < acf.size())
					++i;
				s += acf[i];
			}
			++i;
			if (!have_pending) {
				pending = s;
				have_pending = true;
			}
			else {  // a key and its value
				if (pending == "language") {
					if (language.empty())
						language = s;
					if (in_user.empty() && !blocks.empty() && blocks.back() == "UserConfig")
						in_user = s;
				}
				if (pending == "installdir" && blocks.size() == 1)
					install = s;
				have_pending = false;
			}
		}
		else if (c == '{') {
			blocks.push_back(have_pending ? pending : std::string());
			have_pending = false;
			++i;
		}
		else if (c == '}') {
			if (!blocks.empty())
				blocks.pop_back();
			have_pending = false;
			++i;
		}
		else
			++i;
	}
	*steam_name = in_user.empty() ? language : in_user;
	*installdir = install;
	return !steam_name->empty();
}

std::string WindowsCode(unsigned primary_language)
{
	// PRIMARYLANGID values (winnt.h: LANG_ENGLISH 0x09, LANG_FRENCH 0x0c...).
	static const struct {
		unsigned id;
		const char *code;
	} kIds[] = {
		{0x09, "en"}, {0x0c, "fr"}, {0x07, "de"}, {0x19, "ru"}, {0x15, "pl"}, {0x0a, "es"}, {0x10, "it"}, {0x05, "cs"},
		{0x0e, "hu"}, {0x16, "pt"}, {0x04, "zh"}, {0x11, "ja"}, {0x12, "ko"}, {0x22, "uk"}, {0x13, "nl"}, {0x1f, "tr"},
		{0x1d, "sv"}, {0x06, "da"}, {0x0b, "fi"}, {0x14, "no"}, {0x08, "el"}, {0x18, "ro"}, {0x02, "bg"}, {0x1b, "sk"},
	};
	for (const auto &k : kIds)
		if (k.id == primary_language)
			return k.code;
	return std::string();
}

std::string Narrow(const std::wstring &text)
{
	std::string out;
	for (size_t i = 0; i < text.size(); ++i) {
		unsigned code = static_cast<unsigned short>(text[i]);
		if (code >= 0xd800 && code < 0xdc00 && i + 1 < text.size() && text[i + 1] >= 0xdc00 && text[i + 1] < 0xe000) {
			code = 0x10000 + ((code - 0xd800) << 10) + (static_cast<unsigned short>(text[i + 1]) - 0xdc00);
			++i;
		}
		else if (code >= 0xd800 && code < 0xe000)
			code = 0xfffd;
		Append(&out, code);
	}
	return out;
}

}  // namespace texts

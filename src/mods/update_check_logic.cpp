#include "update_check_logic.h"

#include <string.h>

namespace mods {
namespace update {

bool ParseVersion(const std::string &text, Version *out)
{
	Version v;
	size_t p = 0;
	if (p < text.size() && (text[p] == 'v' || text[p] == 'V'))
		++p;
	int count = 0;
	for (;;) {
		if (p >= text.size() || text[p] < '0' || text[p] > '9')
			return false;
		long n = 0;
		while (p < text.size() && text[p] >= '0' && text[p] <= '9') {
			n = n * 10 + (text[p] - '0');
			if (n > 100000)
				return false;
			++p;
		}
		if (count == 3)
			return false;  // a fourth number
		v.part[count++] = static_cast<int>(n);
		if (p < text.size() && text[p] == '.') {
			++p;
			continue;
		}
		break;
	}
	if (count < 2)
		return false;
	if (p < text.size()) {
		if (text[p] != '-' && text[p] != '+')
			return false;
		v.suffix = text.substr(p);
		if (v.suffix.size() < 2)
			return false;
	}
	*out = v;
	return true;
}

int Compare(const Version &a, const Version &b)
{
	for (int i = 0; i < 3; ++i)
		if (a.part[i] != b.part[i])
			return a.part[i] < b.part[i] ? -1 : 1;
	if (a.suffix == b.suffix)
		return 0;
	if (a.suffix.empty())
		return 1;  // the release comes after its test builds
	if (b.suffix.empty())
		return -1;
	return a.suffix < b.suffix ? -1 : 1;
}

bool IsNewer(const std::string &latest, const std::string &ours)
{
	Version a, b;
	return ParseVersion(latest, &a) && ParseVersion(ours, &b) && Compare(a, b) > 0;
}

std::string TagName(const std::string &json)
{
	const char key[] = "\"tag_name\"";
	size_t p = json.find(key);
	if (p == std::string::npos)
		return std::string();
	p += strlen(key);
	while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n'))
		++p;
	if (p >= json.size() || json[p] != ':')
		return std::string();
	++p;
	while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n'))
		++p;
	if (p >= json.size() || json[p] != '"')
		return std::string();
	++p;
	std::string tag;
	while (p < json.size() && json[p] != '"') {
		if (json[p] == '\\' || static_cast<unsigned char>(json[p]) < 0x20 || tag.size() >= 64)
			return std::string();  // a tag of ours never needs escapes
		tag += json[p++];
	}
	return p < json.size() ? tag : std::string();
}

bool Due(long long now, long long last, long long period)
{
	return last <= 0 || now < last || now - last >= period;
}

}  // namespace update
}  // namespace mods

// Mod: what is seen past the adventure map's edge (see edge_sky.cpp).
#pragma once
#include <string>

namespace mods {

// With the settings of open-homm5.ini [camera]: edge_sky=game (the default:
// nothing is installed) or none; edge_sky_test_light= (testing: one more light
// file to serve without its sky dome). Only after DetectGameBuild() said the
// build is hookable.
void ApplyEdgeSkyFromSettings();

namespace edgesky {

// A light file (.xdb, NDb::SAmbientLight) without its sky dome: each
// <SkyDome href="..."/> (or <SkyDome ...>...</SkyDome>) becomes <SkyDome/>, as
// in the lights that have none. Returns false (and leaves *out alone) when the
// file names no sky dome. Nothing else in the file changes.
inline bool WithoutSkyDome(const std::string &xdb, std::string *out)
{
	static const char kOpen[] = "<SkyDome";
	static const char kClose[] = "</SkyDome>";
	std::string result;
	size_t from = 0;
	bool changed = false;
	for (;;) {
		const size_t at = xdb.find(kOpen, from);
		if (at == std::string::npos)
			break;
		const size_t after = at + sizeof(kOpen) - 1;
		const char next = after < xdb.size() ? xdb[after] : '\0';
		if (next != ' ' && next != '\t' && next != '\r' && next != '\n') {
			from = after;  // <SkyDome/>, <SkyDome>, or a longer name: left as it is
			continue;
		}
		const size_t gt = xdb.find('>', after);
		if (gt == std::string::npos)
			break;
		size_t end = gt + 1;  // one past the element
		if (xdb[gt - 1] != '/') {
			const size_t close = xdb.find(kClose, gt);
			if (close == std::string::npos)
				break;  // not well formed: leave the rest alone
			end = close + sizeof(kClose) - 1;
		}
		result.append(xdb, from, at - from);
		result += "<SkyDome/>";
		from = end;
		changed = true;
	}
	if (!changed)
		return false;
	result.append(xdb, from, std::string::npos);
	*out = result;
	return true;
}

// "/Lights\_(AmbientLight)/X.xdb#xpointer(/AmbientLight)" ->
// "lights/_(ambientlight)/x.xdb": how the light files are compared.
inline std::string NormalizePath(const std::string &path)
{
	size_t begin = 0;
	while (begin < path.size() && (path[begin] == '/' || path[begin] == '\\'))
		++begin;
	const size_t hash = path.find('#', begin);
	std::string s = path.substr(begin, hash == std::string::npos ? std::string::npos : hash - begin);
	for (char &c : s) {
		if (c == '\\')
			c = '/';
		else if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	}
	return s;
}

}  // namespace edgesky
}  // namespace mods

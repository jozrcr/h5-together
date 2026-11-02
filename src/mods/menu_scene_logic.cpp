// Mod "main menu scene", part 2: the parts that are only text and numbers (the setting, the
// draw, what a scene needs, the edits of two small data files), tested on their own in
// src/hook/test/menu_scene_test.cpp. menu_scene.cpp has the game side.
#include "menu_scene.h"

#include <string.h>

#include <algorithm>
#include <vector>

namespace mods {
namespace menuscene {
namespace {

char Lower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string LowerCopy(const std::string &s)
{
	std::string r = s;
	for (char &c : r)
		c = Lower(c);
	return r;
}

bool EndsWithNoCase(const std::string &s, const char *suffix)
{
	const size_t n = strlen(suffix);
	if (s.size() < n)
		return false;
	for (size_t i = 0; i < n; ++i)
		if (Lower(s[s.size() - n + i]) != suffix[i])
			return false;
	return true;
}

// The value of the attribute that starts at `at` (right after `name="`), up to the closing quote.
bool AttributeAt(const std::string &xdb, size_t at, std::string *value)
{
	const size_t end = xdb.find('"', at);
	if (end == std::string::npos)
		return false;
	*value = xdb.substr(at, end - at);
	return true;
}

// The names of the scene files the two scenes were exported from (the exporter's source,
// not a file of the game data): the base game's naming for Heroes V, the add-on's for ToE.
const char kToeScene[] = "/h5a2/ui/mainmenu/mainmenu.mb";
const char kHomm5Scene[] = "/models/mainmenu/m_menu.mb";

// The arena's own folder: its objects are named relative to it.
const char kArenaFolder[] = "arenas/interface/mainmenu/";

}  // namespace

Choice ParseChoice(const std::string &text, bool *known)
{
	size_t first = 0, last = text.size();  // spaces around the word do not matter
	while (first < last && (text[first] == ' ' || text[first] == '\t'))
		++first;
	while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t' || text[last - 1] == '\r'))
		--last;
	const std::string t = LowerCopy(text.substr(first, last - first));
	if (known)
		*known = true;
	if (t == "default")
		return Choice::Default;
	if (t == "toe")
		return Choice::Toe;
	if (t == "homm5")
		return Choice::Homm5;
	if (t == "random")
		return Choice::Random;
	if (known)
		*known = false;
	return Choice::Default;
}

const char *ChoiceName(Choice choice)
{
	switch (choice) {
	case Choice::Toe:
		return "toe";
	case Choice::Homm5:
		return "homm5";
	case Choice::Random:
		return "random";
	case Choice::Default:
		break;
	}
	return "default";
}

const char *SceneName(Scene scene)
{
	switch (scene) {
	case Scene::Toe:
		return "toe";
	case Scene::Homm5:
		return "homm5";
	case Scene::None:
		break;
	}
	return "none";
}

std::string AvailableList(const Availability &available)
{
	std::string list;
	if (available.homm5)
		list = "homm5";
	if (available.toe)
		list += std::string(list.empty() ? "" : ", ") + "toe";
	return list.empty() ? "none" : list;
}

Scene Pick(Choice choice, const Availability &available, unsigned seed, std::string *why)
{
	std::string unused;
	std::string &w = why ? *why : unused;
	switch (choice) {
	case Choice::Default:
		w = "scene=default: the menu as installed";
		return Scene::None;
	case Choice::Toe:
		if (available.toe) {
			w = "scene=toe";
			return Scene::Toe;
		}
		w = "scene=toe is not available on this PC (" + available.toe_why + "): the menu as installed";
		return Scene::None;
	case Choice::Homm5:
		if (available.homm5) {
			w = "scene=homm5";
			return Scene::Homm5;
		}
		w = "scene=homm5 is not available on this PC (" + available.homm5_why + "): the menu as installed";
		return Scene::None;
	case Choice::Random:
		break;
	}
	std::vector<Scene> candidates;  // alphabetical, like AvailableList
	if (available.homm5)
		candidates.push_back(Scene::Homm5);
	if (available.toe)
		candidates.push_back(Scene::Toe);
	if (candidates.empty()) {
		w = "scene=random: no scene to draw from (toe: " + available.toe_why + "; homm5: " + available.homm5_why +
		    "): the menu as installed";
		return Scene::None;
	}
	const Scene picked = candidates[seed % candidates.size()];
	w = std::string("scene=random picked ") + SceneName(picked) + " (available: " + AvailableList(available) + ")";
	return picked;
}

std::string NormalizePath(const char *begin, const char *end)
{
	while (begin < end && (*begin == '/' || *begin == '\\'))
		++begin;
	const char *hash = begin;
	while (hash < end && *hash != '#')
		++hash;
	std::string s(begin, hash);
	for (char &c : s)
		c = c == '\\' ? '/' : Lower(c);
	return s;
}

Scene SceneOfArena(const std::string &arena_xdb)
{
	static const char kTag[] = "<ExpScene href=\"";
	const size_t at = arena_xdb.find(kTag);
	std::string value;
	if (at == std::string::npos || !AttributeAt(arena_xdb, at + sizeof(kTag) - 1, &value))
		return Scene::None;
	const std::string scene = NormalizePath(value.data(), value.data() + value.size());
	if (scene == kToeScene + 1)  // (NormalizePath drops the leading slash)
		return Scene::Toe;
	if (scene == kHomm5Scene + 1)
		return Scene::Homm5;
	return Scene::None;
}

std::string FirstMissing(const std::string &arena_xdb, const std::function<bool(const std::string &)> &exists)
{
	static const char kTag[] = "href=\"";
	size_t from = 0;
	for (;;) {
		const size_t at = arena_xdb.find(kTag, from);
		if (at == std::string::npos)
			return std::string();
		std::string href;
		if (!AttributeAt(arena_xdb, at + sizeof(kTag) - 1, &href))
			return std::string();
		from = at + sizeof(kTag) - 1 + href.size() + 1;
		std::string key = NormalizePath(href.data(), href.data() + href.size());
		if (key.empty() || EndsWithNoCase(key, ".mb"))
			continue;  // (an empty reference; the exporter's source scene)
		if (href[0] != '/' && href[0] != '\\')
			key = kArenaFolder + key;
		if (!exists(key))
			return key;
	}
}

std::string TrackOf(const std::string &music_definition)
{
	static const char kOpen[] = "<FileName>";
	static const char kClose[] = "</FileName>";
	const size_t at = music_definition.find(kOpen);
	if (at == std::string::npos)
		return std::string();
	const size_t begin = at + sizeof(kOpen) - 1;
	const size_t end = music_definition.find(kClose, begin);
	if (end == std::string::npos)
		return std::string();
	return music_definition.substr(begin, end - begin);
}

std::string ServedDefinition(const std::string &music_definition)
{
	static const char kAttribute[] = " ObjectRecordID=\"";
	const std::string &d = music_definition;
	const size_t tag = d.find("<Music");
	const size_t tag_end = tag == std::string::npos ? std::string::npos : d.find('>', tag);
	if (tag_end == std::string::npos)
		return d;
	const size_t at = d.find(kAttribute, tag);
	if (at == std::string::npos || at > tag_end)
		return d;
	const size_t quote = d.find('"', at + sizeof(kAttribute) - 1);
	if (quote == std::string::npos || quote > tag_end)
		return d;
	return d.substr(0, at) + d.substr(quote + 1);
}

bool WithMenuMusic(const std::string &root_xdb, const std::string &new_path, std::string *out, std::string *old_path)
{
	static const char kId[] = "<ID>MAIN_MENU_MUSIC_ID</ID>";
	static const char kMusic[] = "<Music href=\"";
	const size_t id = root_xdb.find(kId);
	if (id == std::string::npos)
		return false;
	const size_t item_end = root_xdb.find("</Item>", id);
	const size_t music = root_xdb.find(kMusic, id);
	if (item_end == std::string::npos || music == std::string::npos || music > item_end)
		return false;  // (not the item we know: its music is in the same item as its ID)
	const size_t begin = music + sizeof(kMusic) - 1;
	std::string href;
	if (!AttributeAt(root_xdb, begin, &href))
		return false;
	const size_t hash = href.find('#');
	if (hash == std::string::npos || !EndsWithNoCase(href.substr(0, hash), ".xdb"))
		return false;
	if (old_path)
		*old_path = href.substr(0, hash);
	*out = root_xdb.substr(0, begin) + new_path + root_xdb.substr(begin + hash);
	return true;
}

}  // namespace menuscene
}  // namespace mods

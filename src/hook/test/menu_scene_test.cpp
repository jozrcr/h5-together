// Test of the "main menu scene" mod's pure work (src/mods/menu_scene_logic.cpp): the setting,
// the draw, what a scene needs, and the edits of two small data files. Made-up data in the
// game's format; no game needed.
#include <stdio.h>

#include <set>
#include <string>

#include "../../mods/menu_scene.h"

using namespace mods::menuscene;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

Availability Both()
{
	Availability a;
	a.toe = a.homm5 = true;
	return a;
}

Availability Only(bool toe, bool homm5)
{
	Availability a;
	a.toe = toe;
	a.homm5 = homm5;
	a.toe_why = "no toe files";
	a.homm5_why = "no homm5 files";
	return a;
}

std::string Arena(const char *scene, const char *extra)
{
	return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ArenaDesc ObjectRecordID=\"38\">\n\t<ExpScene href=\"") + scene +
	       "\"/>\n\t<ExpSrcClipFolder href=\"\"/>\n\t<objects>\n"
	       "\t\t<Item href=\"Ground.(ArenaEnvObject).xdb#xpointer(/ArenaEnvObject)\"/>\n"
	       "\t\t<Item href=\"Corps.(ArenaEnvObject).xdb#xpointer(/ArenaEnvObject)\"/>\n\t</objects>\n\t<effects>\n"
	       "\t\t<Item>\n\t\t\t<Effect href=\"/Effects/_(Effect)/Interface/MainMenu/Cleric.(Effect).xdb#xpointer(/Effect)\"/>\n"
	       "\t\t\t<Locator/>\n\t\t</Item>\n\t</effects>\n\t<AmbientLights>\n"
	       "\t\t<Item href=\"/Lights/_(AmbientLight)/Arena/MainMenuSky/Sky.xdb#xpointer(/AmbientLight)\"/>\n"
	       "\t</AmbientLights>\n" +
	       extra + "</ArenaDesc>\n";
}

const char kRoot[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<UIGameRoot>\n\t<musics>\n"
	"\t\t<Item>\n\t\t\t<ID>MAIN_MENU_MUSIC_ID</ID>\n"
	"\t\t\t<Music href=\"/Sounds/_(Music)/MainMenu/a2-MainMenu.xdb#xpointer(/Music)\"/>\n\t\t</Item>\n"
	"\t\t<Item>\n\t\t\t<ID>CREDITS_SCREEN_MUSIC_ID</ID>\n"
	"\t\t\t<Music href=\"/Sounds/_(Music)/Credits/Final-Credits.xdb#xpointer(/Music)\"/>\n\t\t</Item>\n"
	"\t\t<Item>\n\t\t\t<ID>MP_MAIN_MENU_MUSIC_ID</ID>\n"
	"\t\t\t<Music href=\"/Sounds/_(Music)/MainMenu/MP-MainMenu.xdb#xpointer(/Music)\"/>\n\t\t</Item>\n"
	"\t</musics>\n</UIGameRoot>\n";

}  // namespace

int main()
{
	// ---- the setting ----
	{
		bool known = false;
		Check(ParseChoice("default", &known) == Choice::Default && known, "scene=default");
		Check(ParseChoice("toe", &known) == Choice::Toe && known, "scene=toe");
		Check(ParseChoice("homm5", &known) == Choice::Homm5 && known, "scene=homm5");
		Check(ParseChoice("random", &known) == Choice::Random && known, "scene=random");
		Check(ParseChoice("ToE", &known) == Choice::Toe && known, "any case: ToE");
		Check(ParseChoice("RANDOM", &known) == Choice::Random && known, "any case: RANDOM");
		Check(ParseChoice("hof", &known) == Choice::Default && !known, "hof is not a scene: default, not known");
		Check(ParseChoice("", &known) == Choice::Default && !known, "empty: default, not known");
		Check(ParseChoice(" toe\t", &known) == Choice::Toe && known, "spaces around the word do not matter");
		Check(ParseChoice("to e", &known) == Choice::Default && !known, "a space inside the word does");
		Check(ParseChoice("heroes5", nullptr) == Choice::Default, "no known flag asked for");
		Check(std::string(ChoiceName(Choice::Homm5)) == "homm5" && std::string(SceneName(Scene::None)) == "none" &&
		          std::string(SceneName(Scene::Toe)) == "toe",
		      "names for the log");
	}

	// ---- the pick ----
	{
		std::string why;
		Check(Pick(Choice::Default, Both(), 7, &why) == Scene::None && why.find("default") != std::string::npos,
		      "default installs nothing, even when both scenes are there");
		Check(Pick(Choice::Toe, Both(), 7, &why) == Scene::Toe, "toe shows toe");
		Check(Pick(Choice::Homm5, Both(), 7, &why) == Scene::Homm5, "homm5 shows homm5");
		Check(Pick(Choice::Homm5, Only(true, false), 7, &why) == Scene::None && why.find("no homm5 files") != std::string::npos &&
		          why.find("not available") != std::string::npos,
		      "homm5 on a PC without it: the game's menu, and the log says why (plain ToE)");
		Check(Pick(Choice::Toe, Only(false, true), 7, &why) == Scene::None && why.find("no toe files") != std::string::npos,
		      "toe without its files: the game's menu");
		Check(Pick(Choice::Random, Only(true, false), 123, &why) == Scene::Toe && why.find("picked toe") != std::string::npos,
		      "random with only toe: toe");
		Check(Pick(Choice::Random, Only(false, true), 124, &why) == Scene::Homm5, "random with only homm5: homm5");
		Check(Pick(Choice::Random, Only(false, false), 5, &why) == Scene::None && why.find("no scene to draw from") != std::string::npos,
		      "random with nothing: the game's menu");
		int toe = 0, homm5 = 0;
		for (unsigned seed = 0; seed < 1000; ++seed) {
			const Scene s = Pick(Choice::Random, Both(), seed, nullptr);
			toe += s == Scene::Toe;
			homm5 += s == Scene::Homm5;
		}
		Check(toe == 500 && homm5 == 500, "random over 1000 seeds: each scene half of the time");
		Check(Pick(Choice::Random, Both(), 3, &why) == Pick(Choice::Random, Both(), 3, nullptr), "the same seed, the same pick");
		Check(why.find("picked") != std::string::npos && why.find("available: homm5, toe") != std::string::npos,
		      "the log line names the pick and what was available");
		Check(AvailableList(Both()) == "homm5, toe" && AvailableList(Only(true, false)) == "toe" &&
		          AvailableList(Only(false, false)) == "none",
		      "the list of scenes");
	}

	// ---- which scene an arena description is ----
	{
		Check(SceneOfArena(Arena("/H5A2/UI/MainMenu/MainMenu.mb", "")) == Scene::Toe, "ExpScene /H5A2/...: the ToE scene");
		Check(SceneOfArena(Arena("/models/MainMenu/M_Menu.mb", "")) == Scene::Homm5, "ExpScene /models/...: the Heroes V scene");
		Check(SceneOfArena(Arena("/h5a2\\ui\\mainmenu/MAINMENU.MB", "")) == Scene::Toe, "slashes and case do not matter");
		Check(SceneOfArena(Arena("/models/Somebody/Else.mb", "")) == Scene::None, "another mod's scene: neither");
		Check(SceneOfArena("<ArenaDesc/>") == Scene::None && SceneOfArena("") == Scene::None, "no ExpScene: neither");
		Check(SceneOfArena("<ExpScene href=\"/models/MainMenu/M_Menu.mb") == Scene::None, "a cut-off file: neither");
	}

	// ---- what a scene needs ----
	{
		const std::string arena = Arena("/models/MainMenu/M_Menu.mb", "");
		std::set<std::string> have = {
			"arenas/interface/mainmenu/ground.(arenaenvobject).xdb",
			"arenas/interface/mainmenu/corps.(arenaenvobject).xdb",
			"effects/_(effect)/interface/mainmenu/cleric.(effect).xdb",
			"lights/_(ambientlight)/arena/mainmenusky/sky.xdb",
		};
		const auto exists = [&have](const std::string &key) { return have.count(key) != 0; };
		Check(FirstMissing(arena, exists).empty(), "all files named are there (relative names in the arena's folder, the .mb ignored)");
		have.erase("arenas/interface/mainmenu/corps.(arenaenvobject).xdb");
		Check(FirstMissing(arena, exists) == "arenas/interface/mainmenu/corps.(arenaenvobject).xdb", "a missing object is named");
		have.insert("arenas/interface/mainmenu/corps.(arenaenvobject).xdb");
		have.erase("lights/_(ambientlight)/arena/mainmenusky/sky.xdb");
		Check(FirstMissing(arena, exists) == "lights/_(ambientlight)/arena/mainmenusky/sky.xdb", "a missing light is named (absolute name)");
		have.insert("lights/_(ambientlight)/arena/mainmenusky/sky.xdb");
		have.erase("effects/_(effect)/interface/mainmenu/cleric.(effect).xdb");
		Check(FirstMissing(arena, exists) == "effects/_(effect)/interface/mainmenu/cleric.(effect).xdb", "a missing effect is named");
		have.insert("effects/_(effect)/interface/mainmenu/cleric.(effect).xdb");
		Check(FirstMissing(Arena("/models/MainMenu/M_Menu.mb", "\t<Extra href=\"Camera.(AnimSet).xdb#xpointer(/AnimSet)\"/>\n"), exists) ==
		          "arenas/interface/mainmenu/camera.(animset).xdb",
		      "one more name is checked too");
		Check(FirstMissing("<ArenaDesc><x href=\"/a/b.xdb#xpointer(/A)", exists).empty(), "a cut-off attribute ends the search");
		Check(FirstMissing("<ArenaDesc><x href=\"\"/><y href=\"#x\"/></ArenaDesc>", exists).empty(), "empty references are not files");
	}

	// ---- the music definition ----
	{
		const std::string def =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Music ObjectRecordID=\"138\">\n\t<FileName>..\\Music\\MainMenu\\MainMenu-Theme.ogg</FileName>\n"
			"\t<FadeIn>2000</FadeIn>\n\t<FadeOut>2000</FadeOut>\n</Music>\n";
		Check(TrackOf(def) == "..\\Music\\MainMenu\\MainMenu-Theme.ogg", "the track of a definition");
		Check(TrackOf("<Music></Music>").empty() && TrackOf("<Music><FileName>x").empty() && TrackOf("").empty(), "no track");
		const std::string served = ServedDefinition(def);
		Check(served.find("ObjectRecordID") == std::string::npos && served.find("<Music>") != std::string::npos &&
		          TrackOf(served) == TrackOf(def) && served.find("<FadeOut>2000</FadeOut>") != std::string::npos,
		      "served without the record id, the rest as it was");
		const std::string plain = "<?xml version=\"1.0\"?>\n<Music>\n\t<FileName>a.ogg</FileName>\n</Music>\n";
		Check(ServedDefinition(plain) == plain, "a definition without it is served as it is");
		const std::string later = "<Music>\n\t<FileName>a.ogg</FileName>\n\t<Other ObjectRecordID=\"3\"/>\n</Music>\n";
		Check(ServedDefinition(later) == later, "an id further down is not touched");
	}

	// ---- the root table ----
	{
		std::string out, old_path;
		Check(WithMenuMusic(kRoot, "/Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb", &out, &old_path), "the main menu's item is found");
		Check(old_path == "/Sounds/_(Music)/MainMenu/a2-MainMenu.xdb", "the old path is reported");
		Check(out.find("<ID>MAIN_MENU_MUSIC_ID</ID>\n\t\t\t<Music href=\"/Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb#xpointer(/Music)\"/>") !=
		          std::string::npos,
		      "the item now names our definition, the xpointer stays");
		Check(out.find("a2-MainMenu.xdb") == std::string::npos, "nothing else names the old definition");
		Check(out.find("MP-MainMenu.xdb#xpointer(/Music)") != std::string::npos &&
		          out.find("Final-Credits.xdb#xpointer(/Music)") != std::string::npos,
		      "the multiplayer menu's music and the credits' are not touched");
		// only that one change: put the old text back and the file is the same
		std::string back = out;
		const std::string ours = "/Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb";
		back.replace(back.find(ours), ours.size(), old_path);
		Check(back == kRoot, "that is the only change");
		Check(!WithMenuMusic("<UIGameRoot/>", "/x.xdb", &out, nullptr), "no such item: not changed");
		const std::string only_mp =
			"<musics><Item><ID>MP_MAIN_MENU_MUSIC_ID</ID><Music href=\"/Sounds/_(Music)/MainMenu/MP-MainMenu.xdb#xpointer(/Music)\"/></Item></musics>";
		Check(!WithMenuMusic(only_mp, "/x.xdb", &out, nullptr), "the multiplayer menu's item is not the main menu's");
		const std::string other_item =
			"<musics><Item><ID>MAIN_MENU_MUSIC_ID</ID></Item><Item><ID>X</ID><Music href=\"/a/b.xdb#xpointer(/Music)\"/></Item></musics>";
		Check(!WithMenuMusic(other_item, "/x.xdb", &out, nullptr), "a Music in the next item is not this item's");
		const std::string odd =
			"<musics><Item><ID>MAIN_MENU_MUSIC_ID</ID><Music href=\"nothing\"/></Item></musics>";
		Check(!WithMenuMusic(odd, "/x.xdb", &out, nullptr), "a reference in another form: not changed");
		Check(!WithMenuMusic("<ID>MAIN_MENU_MUSIC_ID</ID><Music href=\"/a.xdb#x", "/x.xdb", &out, nullptr), "a cut-off file: not changed");
	}

	// ---- the paths ----
	{
		const std::string raw = "\\Arenas/Interface\\MainMenu/MainMenu.(ArenaDesc).xdb";
		Check(NormalizePath(raw.data(), raw.data() + raw.size()) == "arenas/interface/mainmenu/mainmenu.(arenadesc).xdb",
		      "the game's path as a key");
		const std::string href = "/Sounds/_(Music)/MainMenu/OpenHomm5-Menu.xdb#xpointer(/Music)";
		Check(NormalizePath(href.data(), href.data() + href.size()) == "sounds/_(music)/mainmenu/openhomm5-menu.xdb",
		      "the part after # is dropped");
	}

	printf("%s\n", g_failures ? "FAILED" : "all ok");
	return g_failures ? 1 : 0;
}

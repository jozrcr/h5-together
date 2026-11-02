// Test of our texts' pure part (src/hook/texts_logic.cpp): reading a texts file
// (sections, comments, the byte-order mark, CRLF or LF, UTF-16, broken lines),
// the named placeholders, a translation checked against English and the fallback
// to English, and the clues to the player's language (Steam's file for the game,
// Windows' language). Then the shipped files themselves: en.txt reads without a
// problem and is the English built into the DLL (texts_en.inc), fr.txt fits it.
// No game needed. Usage: texts_test.exe <src\mods\texts>
#include <stdio.h>
#include <string.h>

#include <string>

#include "../texts_logic.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

std::string ReadAll(const std::string &path)
{
	std::string out;
	FILE *f = nullptr;
	if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
		return out;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	fclose(f);
	return out;
}

std::string Text(const texts::Book &b, const char *key)
{
	const auto it = b.texts.find(key);
	return it == b.texts.end() ? std::string("<none>") : it->second;
}

// What the DLL does for each text (texts.cpp, Get): the player's language, else English, else the key.
std::string Lookup(const texts::Book &chosen, const texts::Book &english, const char *key)
{
	auto it = chosen.texts.find(key);
	if (it != chosen.texts.end())
		return it->second;
	it = english.texts.find(key);
	return it != english.texts.end() ? it->second : key;
}

bool HasProblem(const texts::Book &b, const char *part)
{
	for (const std::string &p : b.problems)
		if (p.find(part) != std::string::npos)
			return true;
	return false;
}

void TestParse()
{
	const std::string lf =
	    "# a comment\n"
	    "; another comment\n"
	    "\n"
	    "[players]\n"
	    "day = Day {n}\n"
	    "  ready=ready  \n"
	    "[rules]\n"
	    "title = Game rules\n"
	    "two.lines = one\\ntwo, a \\\\ backslash, a \\x kept\n"
	    "tip = <color=yellow>Gold<color_default><br>more\n";
	texts::Book b;
	Check(texts::Parse(lf, &b) && b.problems.empty(), "a file with sections, comments and blank lines reads without a problem");
	Check(b.texts.size() == 5 && Text(b, "players.day") == "Day {n}" && Text(b, "players.ready") == "ready" &&
	          Text(b, "rules.title") == "Game rules",
	      "keys are <section>.<key>; spaces around the key and the text go");
	Check(Text(b, "rules.two.lines") == "one\ntwo, a \\ backslash, a \\x kept", "\\n is a new line, \\\\ a backslash, any other backslash stays");

	std::string crlf;
	for (char c : lf)
		crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
	texts::Book b2;
	Check(texts::Parse(crlf, &b2) && b2.problems.empty() && b2.texts == b.texts, "CRLF line ends read the same as LF");
	texts::Book b3;
	Check(texts::Parse("\xef\xbb\xbf" + crlf, &b3) && b3.problems.empty() && b3.texts == b.texts, "a UTF-8 byte-order mark is skipped");
	texts::Book b4;
	Check(texts::Parse("[a]\nlast = no line end", &b4) && Text(b4, "a.last") == "no line end", "the last line needs no line end");

	// Notepad's "Unicode": UTF-16 LE with its mark
	const std::wstring wide = L"[players]\r\nready = Prêts\r\nname = Красный \U0001F600\r\n";
	std::string utf16("\xff\xfe", 2);
	for (wchar_t c : wide) {
		utf16 += static_cast<char>(c & 0xff);
		utf16 += static_cast<char>(c >> 8);
	}
	texts::Book b5;
	Check(texts::Parse(utf16, &b5) && b5.problems.empty() && Text(b5, "players.ready") == "Pr\xc3\xaats" &&
	          Text(b5, "players.name") == "\xd0\x9a\xd1\x80\xd0\xb0\xd1\x81\xd0\xbd\xd1\x8b\xd0\xb9 \xf0\x9f\x98\x80",
	      "UTF-16 LE with its mark (accents, Cyrillic, a surrogate pair) becomes UTF-8");
	texts::Book b6;
	Check(!texts::Parse(std::string("\xfe\xff\0[", 4), &b6) && !b6.problems.empty(), "UTF-16 big-endian is refused, with a reason");
	texts::Book b7;
	Check(!texts::Parse(std::string("[a]\nx = 1\0\0", 11), &b7) && HasProblem(b7, "not text"), "a file with zero bytes is not text");
	texts::Book empty;
	Check(texts::Parse("", &empty) && empty.texts.empty() && empty.problems.empty(), "an empty file is read: no texts, no problem");
}

void TestBrokenLines()
{
	const std::string bytes =
	    "orphan = before any section\n"   // 1
	    "[players]\n"                     // 2
	    "day = Day {n}\n"                 // 3
	    "this line has no equals sign\n"  // 4
	    "Bad Key = text\n"                // 5
	    "empty =\n"                       // 6
	    "day = again\n"                   // 7
	    "latin = caf\xe9\n"               // 8: Windows-1252, not UTF-8
	    "[bad section\n"                  // 9
	    "lost = in a bad section\n"       // 10
	    "[rules]\n"                       // 11
	    "title = Game rules\n";           // 12
	texts::Book b;
	Check(texts::Parse(bytes, &b), "a file with broken lines is still read");
	Check(b.texts.size() == 2 && Text(b, "players.day") == "Day {n}" && Text(b, "rules.title") == "Game rules",
	      "only the good lines are kept; a key given twice keeps its first text");
	Check(HasProblem(b, "line 1: orphan is not in a [section]") && HasProblem(b, "line 4: no \"=\"") &&
	          HasProblem(b, "line 5: \"Bad Key\" is not a key") && HasProblem(b, "line 6: players.empty has no text") &&
	          HasProblem(b, "line 7: players.day is there twice") && HasProblem(b, "line 8: not UTF-8") &&
	          HasProblem(b, "line 9: \"[bad section\" is not a section name") && HasProblem(b, "line 10: lost is not in a [section]"),
	      "each broken line is listed with its number and why");
	texts::Book junk;
	Check(!texts::Parse("just some words\nand more\n", &junk) && HasProblem(junk, "no line could be read"),
	      "a file where no line is good is not read at all");
}

void TestMarksAndFill()
{
	const std::vector<std::string> m = texts::Marks("<color=yellow>{n} of {hero}<color_default><br>{n}, {not one}, {}, a < b");
	const std::vector<std::string> want = {"<br>", "<color=yellow>", "<color_default>", "{hero}", "{n}", "{n}"};
	Check(m == want, "marks: the {names} and <tags>, sorted; {not one} and {} are plain text");
	Check(texts::Fill("Computer {n}", {{"n", "3"}}) == "Computer 3", "fill: {n} -> 3");
	Check(texts::Fill("{a} and {b}, {a} again", {{"a", "x"}, {"b", "{a}"}}) == "x and {a}, x again",
	      "fill: every {name}; a value is not filled again");
	Check(texts::Fill("{unknown} {}, {n", {{"n", "1"}}) == "{unknown} {}, {n", "fill: a {name} without a value stays as written");
	Check(texts::Fill("100 % {n}", {{"n", "%s%d"}}) == "100 % %s%d", "fill: % means nothing (no printf: a translator cannot crash the game)");
}

void TestAgainstEnglish()
{
	texts::Book en, de;
	texts::Parse("[players]\nday = Day {n}\nready = ready\ncomputer = Computer {n}\nplayer = Player {n}\n"
	             "[bonus]\ngold = <color=yellow>Gold<color_default><br>more\n",
	             &en);
	texts::Parse("[players]\nday = Tag {n}\nready = bereit\ncomputer = Computer\nplayer = Spieler {n} {n}\nold = alt\n"
	             "[bonus]\ngold = <color=red>Gold<color_default><br>mehr\n",
	             &de);
	texts::CheckAgainst(en, &de);
	Check(de.texts.size() == 2 && Text(de, "players.day") == "Tag {n}" && Text(de, "players.ready") == "bereit",
	      "a translation keeps only lines English has, with the same {names} and <tags>");
	Check(HasProblem(de, "players.old: English has no such text") && HasProblem(de, "players.computer: its {names}") &&
	          HasProblem(de, "players.player: its {names}") && HasProblem(de, "bonus.gold: its {names} and <tags>"),
	      "each dropped line is listed: unknown key, a {name} missing, one too many, another tag");
	Check(Lookup(de, en, "players.day") == "Tag {n}" && Lookup(de, en, "players.computer") == "Computer {n}" &&
	          Lookup(de, en, "bonus.gold") == en.texts["bonus.gold"],
	      "fallback: a dropped line shows in English");
	texts::Book partial;
	texts::Parse("[players]\nready = bereit\n", &partial);
	texts::CheckAgainst(en, &partial);
	Check(partial.problems.empty() && Lookup(partial, en, "players.ready") == "bereit" && Lookup(partial, en, "players.day") == "Day {n}",
	      "fallback: a partial file gives its lines, English the others");
	texts::Book none;
	Check(Lookup(none, en, "players.player") == "Player {n}" && Lookup(none, en, "no.such.key") == "no.such.key",
	      "fallback: no file at all gives English; a key English lacks shows as itself");
}

void TestLanguageClues()
{
	Check(texts::ValidCode("fr") && texts::ValidCode("deu") && texts::ValidCode("pt-br") && texts::ValidCode("xx"),
	      "codes: fr, deu, pt-br, xx are language codes");
	Check(!texts::ValidCode("f") && !texts::ValidCode("fran") && !texts::ValidCode("FR") && !texts::ValidCode("fr-") &&
	          !texts::ValidCode("fr_ca") && !texts::ValidCode("../fr") && !texts::ValidCode("fr\\..") && !texts::ValidCode(""),
	      "codes: anything else (paths, capitals, too long) is refused");
	Check(texts::SteamCode("french") == "fr" && texts::SteamCode("English") == "en" && texts::SteamCode("german") == "de" &&
	          texts::SteamCode("russian") == "ru" && texts::SteamCode("polish") == "pl" && texts::SteamCode("schinese") == "zh" &&
	          texts::SteamCode("klingon").empty(),
	      "Steam's language names -> our codes (unknown: none)");
	// the owner's appmanifest_15370.acf (2026-11-02), shortened
	const std::string acf =
	    "\"AppState\"\n{\n\t\"appid\"\t\t\"15370\"\n\t\"name\"\t\t\"Heroes of Might & Magic V: Tribes of the East\"\n"
	    "\t\"installdir\"\t\t\"Heroes of Might and Magic 5 Tribes of the East\"\n"
	    "\t\"InstalledDepots\"\n\t{\n\t\t\"15371\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"4769518222776312223\"\n\t\t}\n\t}\n"
	    "\t\"UserConfig\"\n\t{\n\t\t\"language\"\t\t\"french\"\n\t}\n"
	    "\t\"MountedConfig\"\n\t{\n\t\t\"language\"\t\t\"french\"\n\t}\n}\n";
	std::string name, dir;
	Check(texts::ManifestLanguage(acf, &name, &dir) && name == "french" && dir == "Heroes of Might and Magic 5 Tribes of the East",
	      "Steam's file for the game: its language and its folder's name");
	const std::string mounted_first =
	    "\"AppState\"\n{\n\"installdir\" \"H5\"\n\"MountedConfig\"\n{\n\"language\" \"english\"\n}\n\"UserConfig\"\n{\n\"language\" \"german\"\n}\n}";
	Check(texts::ManifestLanguage(mounted_first, &name, &dir) && name == "german" && dir == "H5",
	      "Steam's file: the player's own choice (UserConfig) wins over the installed one");
	Check(texts::ManifestLanguage("\"AppState\" { \"installdir\" \"H5\" \"MountedConfig\" { \"language\" \"polish\" } }", &name, &dir) &&
	          name == "polish",
	      "Steam's file: without UserConfig, any language line");
	Check(!texts::ManifestLanguage("\"AppState\"\n{\n\"installdir\" \"H5\"\n}\n", &name, &dir), "Steam's file without a language: no answer");
	Check(texts::WindowsCode(0x0c) == "fr" && texts::WindowsCode(0x09) == "en" && texts::WindowsCode(0x07) == "de" &&
	          texts::WindowsCode(0x19) == "ru" && texts::WindowsCode(0x7f).empty(),
	      "Windows' language ids -> our codes (unknown: none)");
	Check(texts::Narrow(L"Joueur rouge") == "Joueur rouge" && texts::Narrow(L"é×") == "\xc3\xa9\xc3\x97" &&
	          texts::Narrow(std::wstring(1, static_cast<wchar_t>(0xd800))) == "\xef\xbf\xbd",
	      "UTF-16 -> UTF-8 (a lone surrogate becomes U+FFFD)");
}

const unsigned char kBuiltIn[] = {
#include "texts_en.inc"
};

void TestShippedFiles(const std::string &dir)
{
	const std::string en_bytes = ReadAll(dir + "\\en.txt"), fr_bytes = ReadAll(dir + "\\fr.txt");
	texts::Book en, fr;
	Check(texts::Parse(en_bytes, &en) && en.problems.empty() && en.texts.size() >= 190, "en.txt reads without a problem (190 texts or more)");
	Check(en_bytes == std::string(reinterpret_cast<const char *>(kBuiltIn), sizeof(kBuiltIn)),
	      "the English built into the DLL (texts_en.inc) is en.txt, byte for byte");
	Check(texts::Parse(fr_bytes, &fr) && fr.problems.empty(), "fr.txt reads without a problem");
	texts::CheckAgainst(en, &fr);
	for (const std::string &p : fr.problems)
		printf("      fr.txt: %s\n", p.c_str());
	Check(fr.problems.empty(), "fr.txt fits en.txt: no key English lacks, the same {names} and <tags> on every line");
	int missing = 0;
	for (const auto &t : en.texts)
		if (!fr.texts.count(t.first)) {
			printf("      fr.txt lacks %s\n", t.first.c_str());
			++missing;
		}
	Check(missing == 0, "fr.txt has every text of en.txt");
	Check(Text(en, "language.red_player") == "Red player" && Text(fr, "language.red_player") == "Joueur rouge",
	      "each file names the game's red player in its language (how a game in that language is recognised)");
	printf("ok  : en.txt %d texts, fr.txt %d texts\n", static_cast<int>(en.texts.size()), static_cast<int>(fr.texts.size()));
}

}  // namespace

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: texts_test.exe <src\\mods\\texts>\n");
		return 2;
	}
	TestParse();
	TestBrokenLines();
	TestMarksAndFill();
	TestAgainstEnglish();
	TestLanguageClues();
	TestShippedFiles(argv[1]);
	printf("%s (%d failures)\n", g_failures ? "FAILED" : "all texts tests passed", g_failures);
	return g_failures ? 1 : 0;
}

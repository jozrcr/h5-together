// The pure part of our texts (texts.h has the rest): reading a texts file,
// checking a translation against English, filling the named placeholders, and
// the clues to the player's language (Steam's file for the game, Windows'
// language). No game and no Windows calls, so test/texts_test.cpp checks it all.
//
// A texts file (bin\h5-together\texts\<code>.txt, src/mods/texts/TRANSLATING.md):
//
//   # a comment
//   [players]
//   day = Day {n}
//   computer = Computer {n}
//
// gives the texts "players.day" and "players.computer". "\n" in a text is a new line.
#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace texts {

// One language's texts.
struct Book {
	std::map<std::string, std::string> texts;  // "players.day" -> "Day {n}" (UTF-8)
	std::vector<std::string> problems;         // what was left out and why, for the log ("line 12: ...")
};

// Reads a texts file's bytes into *book. The bytes: UTF-8, with or without its
// mark, or UTF-16 LE with its mark (Notepad's "Unicode"); CRLF or LF line ends.
// Each line is "[section]", "key = text", a comment ("#" or ";" first) or blank.
// A line that is none of these, a text before any section, an empty text, a
// line that is not UTF-8 and a key given twice (the first one stays) are left
// out, each listed in book->problems. False when nothing could be read (the
// file is not text): book->problems says why.
bool Parse(const std::string &bytes, Book *book);

// What a translation must keep: a text's placeholders ("{n}") and the game's
// tags ("<color=yellow>", "<br>"), sorted. Two texts fit when theirs are equal.
std::vector<std::string> Marks(const std::string &text);

// Drops from `book` each text that English does not have and each text whose
// marks differ from the English one, listing them in book->problems. What is
// left can stand in for the English text of the same key.
void CheckAgainst(const Book &english, Book *book);

// A placeholder's value: {"n", "3"} for {n}.
struct Arg {
	const char *name;
	std::string value;
};
// `text` with each {name} replaced by its value. A {name} without a value stays as written.
std::string Fill(const std::string &text, std::initializer_list<Arg> args);

// A language code we accept from a setting or a file name: 2 or 3 small letters,
// then maybe "-" and up to 8 small letters or digits ("fr", "pt-br"). Anything
// else (a path, "../x") is refused.
bool ValidCode(const std::string &code);

// Steam's name of a language ("french", "schinese") -> our code ("fr", "zh"); "" when unknown.
std::string SteamCode(const std::string &steam_name);

// From a Steam app manifest (steamapps\appmanifest_15370.acf, Steam's own file
// for the game): its "language" (the one in "UserConfig" if there is one) and
// its "installdir" (the game folder's name). False when it has no language.
bool ManifestLanguage(const std::string &acf, std::string *steam_name, std::string *installdir);

// Windows' language (PRIMARYLANGID of the user's interface language) -> our code; "" when unknown.
std::string WindowsCode(unsigned primary_language);

// UTF-16 (the game's text files, without their mark) -> UTF-8.
std::string Narrow(const std::wstring &text);

}  // namespace texts

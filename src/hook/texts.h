// Our panels' and messages' words in the player's language (src/mods/README.md,
// "Our texts in files"; src/mods/texts/TRANSLATING.md for translators).
//
// Each language is a UTF-8 file bin\h5-together\texts\<code>.txt ("fr.txt"),
// lines "key = text" in [sections], with named placeholders ("Computer {n}").
// English is built into the DLL (build.ps1 takes it from src/mods/texts/en.txt),
// so a missing, partial or broken file never leaves a panel without words: each
// line a file lacks, or gets wrong, shows in English, and the log says which.
//
// The language is chosen once, at the first text asked for ([ui] language=auto:
// the game's texts setting, Steam's language for the game, the game's own
// texts, Windows' language, English); the log says which clue decided.
#pragma once

#include <string>

#include "texts_logic.h"

namespace texts {

// The text of `key` ("rules.title") in the player's language, else English,
// else the key itself (a key en.txt lacks: tools/lang/ui_texts.py check finds
// those). Any thread. The pointer stays valid.
const char *Get(const char *key);

// The same with its placeholders filled: Format("players.computer", {{"n", "3"}}) -> "Computer 3".
std::string Format(const char *key, std::initializer_list<Arg> args);

// UTF-8 -> UTF-16, for the game's own texts (the lobby's tooltips, a native button's label).
std::wstring Wide(const std::string &utf8);

// The language in use: its code ("fr"), "en" for the built-in English.
const char *Language();

// Changes when the language does (only once today: it is chosen once).
int Generation();

// A table of words: a struct of `const char *` fields only (arrays of them
// too), written with keys, as the panels keep them:
//   struct Words { const char *title, *ok; };
//   const Words kKeys = {"rules.title", "rules.ok"};
//   const Words &w = texts::Table(kKeys);   // w.title: "Game rules"
// Table() gives the same struct with each key's text, looked up again when the
// language changes. One table per struct type (each panel's Words is its own).
template <class T>
const T &Table(const T &keys)
{
	static_assert(sizeof(T) % sizeof(const char *) == 0, "a table of words holds const char * fields only");
	static T words;
	static int generation = -1;
	if (generation != Generation()) {
		const char *const *from = reinterpret_cast<const char *const *>(&keys);
		const char **to = reinterpret_cast<const char **>(&words);
		for (size_t i = 0; i < sizeof(T) / sizeof(const char *); ++i)
			to[i] = Get(from[i]);
		generation = Generation();
	}
	return words;
}

}  // namespace texts

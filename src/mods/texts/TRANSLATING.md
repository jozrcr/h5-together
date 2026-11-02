# Translating H5 Together

H5 Together's panels and messages (the player list, the game rules, the template builder, the hero looks, the "your turn" pop-up...) take their words from one text file per language. English and French come with the add-on. Any other language can be added by a player, without changing the add-on.

This is about **the add-on's own words only** (about 200 short texts). The game's own texts and MMH5.5's are not here.

## What you need

- The add-on installed (it puts `en.txt` and `fr.txt` in the game's `bin\h5-together\texts\` folder), or `en.txt` from this folder on GitHub.
- A text editor that saves UTF-8: Notepad does (Save as, Encoding: UTF-8).

## Steps

1. **Copy `en.txt`** to a file named after your language's code: `de.txt` (German), `ru.txt` (Russian), `pl.txt` (Polish), `es.txt`, `it.txt`, `cs.txt`, `hu.txt`, `pt-br.txt`... Two or three small letters, maybe a `-` and a region.
2. **Change the lines at the top**: `# language: de`, `# translated by: your name`, and the version.
3. **Translate the text right of each `=`.** Keep the key on the left as it is:
   ```
   [players]
   day = Tag {n}
   ready = bereit
   computer = Computer {n}
   ```
4. **Keep every `{name}`.** The add-on puts a number or a name there: `Computer {n}` shows "Computer 3". You may move it in the sentence (`{n}. Computer`), but don't leave one out or add one. Keep the game's tags too (`<color=yellow>`, `<color_default>`, `<br>`, in the `[bonus]` lines).
5. **Keep the texts short**, about as long as the English ones. The panels have a fixed size, and a text that does not fit is cut with "...".
6. **`[language]`**: `name` is your language's own name (`Deutsch`). `red_player` is the game's own name of the red player in your language, as the game shows it (in a French game: `Joueur rouge`). The add-on reads that text from the game to recognise a game in your language, so copy it exactly.
7. **Try it:** put the file in the game's `bin\h5-together\texts\` folder. If your game is in that language, the add-on uses it by itself; otherwise set `language=de` under `[ui]` in `h5-together.ini` (next to the game's exe). Start the game and open the panels: F2 (game rules, in the waiting room or on the map), F7 (hero looks, on the map), F11 (template builder, on the random map screen), F9 (the player list).
8. **Check the log:** `h5-together.log` (next to the game's exe) says which language was chosen and why, and lists each line that was not used, with its line number:
   ```
   texts: de, from Steam's language for the game (german, appmanifest_15370.acf): de.txt, 198 of 201 texts (the others in English)
   texts: de.txt: 1 line(s) not used (they show in English):
   texts:   players.computer: its {names} and <tags> ({x}) are not the English ones ({n}): English is used
   ```
9. **Send it to us:** a GitHub issue ("Translation" form) with the file attached, at <https://github.com/jozrcr/h5-together/issues/new/choose>, or on the MMH5.5 Discord. It then ships with the next version.

## Rules the add-on follows

- A line you leave out shows in English. So does a line whose `{names}` or `<tags>` differ from the English line, and a line that is broken (no `=`, before any `[section]`, not UTF-8). Nothing you write can break the game: at worst a line shows in English.
- `\n` in a text starts a new line. Lines starting with `#` or `;` are comments.
- A key that English does not have (misspelt, or a text removed in a newer version) is not used, and the log says so.
- The file can be UTF-8 with or without the "BOM" mark, or Notepad's "Unicode" (UTF-16); Windows (CRLF) or Linux (LF) line ends.

## Which language is used

With `[ui] language=auto` (the default), the first of these that gives an answer:

1. the game's texts chosen in `h5-together.ini` (`[lang] texts=fr`);
2. Steam's language for the game (Steam's file `steamapps\appmanifest_15370.acf`, its `language` line);
3. the game's own texts: the red player's name, compared with each file's `red_player` line (a name no file has means a language without a file: English);
4. Windows' language;
5. English.

If the language found has no file, the panels are in English. `[ui] language=<code>` chooses one directly.

## For developers

- The files are `src/mods/texts/<code>.txt`; `en.txt` is built into the DLL (`src/hook/build.ps1`), so the panels work without any file.
- `python tools/lang/ui_texts.py check` (run by `build.ps1 -Test`): every key the code uses is in `en.txt`, every key of `en.txt` is used, and the translations have no other keys and the same `{names}` and `<tags>`. It lists the lines each translation lacks.
- `python tools/lang/ui_texts.py new de` writes a starter `build/texts/de.txt`; `python tools/lang/ui_texts.py long` writes the long test language `xx` (every text 40% longer, with accents) to see which panels cut long words.
- In the code a text is looked up by its key: `texts::Get("rules.title")`, `texts::Format("players.computer", {{"n", "3"}})`, or a panel's table of keys (`texts::Table`), see `src/hook/texts.h`.

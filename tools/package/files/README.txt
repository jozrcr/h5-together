H5 Together @VERSION@ - a small add-on for Heroes of Might and Magic V (Tribes of the East + MMH5.5)

What it is: one file, dinput8.dll, that the game loads at start-up, plus a settings file and 24 random map templates. It makes co-op games against the computer faster and more stable, and adds a few panels and options. It adds files to the game's bin folder and never changes the game's own files.

You need: the Steam game with MMH5.5 RC19d or RC19k already installed (MMH55_Utility_64.exe in the game's bin folder). This add-on does not install MMH5.5.
Everyone who plays the same co-op game should install the same version.


INSTALL

Windows: extract this zip (right-click it, "Extract All"), then double-click install.bat. It looks for the game (Steam, GOG), shows you what it found (the folder, the language, which MMH5.5 you have), and tells you what it did and what to do next. If it cannot find the game, or finds several, it asks: type a number, or B to choose the folder in a window. If the game is running it asks you to close it first.
Linux (Steam / Proton): extract the zip, open a terminal in the folder, and run: bash install.sh. The same screens, without the B window (paste the folder instead). At the end it gives you one line to paste into the game's Steam launch options.
It warns you in plain words if MMH5.5 is missing or is a version we have not tested (we tested RC19d and RC19k).
Updating: install the new version over the old one. Your settings are kept, and the new ones are added to h5-together.ini (commented out, with what they do); the installer says how many. Your old file is saved as h5-together.ini.bak.
H5 Together was called open-homm5 until version 0.2.7. Installing over an old version gives your files the new name: open-homm5.ini becomes h5-together.ini, open-homm5-skins.ini becomes h5-together-skins.ini, and the folder open-homm5 becomes h5-together (with everything in it). Nothing you set is lost.

Check that it works: start the game. A file named h5-together.log appears in the game's bin folder. Its first line says "H5 Together hook DLL", the version and "loaded". The log does not grow for ever: over 5 MB, the game keeps it as h5-together.old.log (one older log) and starts a new one.


UNINSTALL

Double-click uninstall.bat (Windows) or run: bash uninstall.sh (Linux). Only our own files are removed (dinput8.dll, the logs, our templates, our language files, and your settings file and its .bak copies if you say yes). Templates and translations you added yourself stay.


SETTINGS (you can skip this)

h5-together.ini in the game's bin folder. Every line is commented out and shows the default. The defaults keep the original game: the speed-ups, fixes and panels are on, and anything that changes how the game plays, looks or sounds is off until you turn it on. Restart the game after a change.
Your hero look choices are kept in h5-together-skins.ini, next to it.


LANGUAGES

The add-on's panels and messages are in English or French: one file per language in the game's bin\h5-together\texts folder (en.txt, fr.txt). The add-on uses the game's language by itself (Steam's language for the game, else the game's own texts, else Windows'). To choose, set [ui] language=en or fr in h5-together.ini. Any text a file lacks shows in English.
Translations are welcome: copy en.txt to the code of your language (de.txt, ru.txt, pl.txt...), translate the text right of each "=" (the top of en.txt says how), put it in the same folder, set [ui] language=de, and send it to us: https://github.com/jozrcr/h5-together/issues/new/choose


WHAT IT DOES

- Faster co-op: quicker AI turns and clicks, less AI thinking on big maps, no long autosave freeze, LAN games over ZeroTier.
- Stability: a fix for a rare host freeze during computer turns, a freeze detector, and a short report when the game crashes (see below).
- Multiplayer waiting room: a line at the bottom says whether every player has the same H5 Together version, and who does not.
- Player list on the adventure map: who is playing, who has ended the turn. F9 hides it.
- "Your turn" notification when the game is in the background.
- Template builder: the "Template..." button ("Modeles..." in French) on the random map screen, or F11. Friends rebuild the same map from its name: nothing to share.
- Hero looks: the small button on the hero screen, or F7. Another hero's 3D figure, same faction.
- Game rules (host): the "Rules..." button ("Regles..." in French) in the multiplayer waiting room, or F2. Used only when every player has this version.
- Off by default, in h5-together.ini: a "new version available" notice in the main menu ([update] check=1: at most once a day it asks GitHub for the latest version number; nothing is downloaded), farther camera zoom, black map edges, a quieter volume range, the original Heroes V music, French texts, start terrain that matches each faction on random maps.
- Linux: the mouse wheel zooms out again (automatic).


NEW IN 0.2.7

- Game rules (host): Tribes of the East start bonuses (each player keeps the bonus picked with the lobby arrows: gold, resources, artifact, with ToE's own icons) and a separate game mode choice (Default, Savage, Nightmare, Random).
- Game rules (host): MMH5.5's scripts can also run on random maps straight from the game's generator in multiplayer, so the other rules work there too.
- Game rules (host): "Skills of each class": MMH5.5's lock, or Unlocked (any skill can be offered, the ones off the class's list at a small chance, as in the original game).
- The installer updates h5-together.ini: your values are kept, the new settings are added, and the old file is saved as h5-together.ini.bak.


REPORTING A PROBLEM

Windows: double-click bug-report.bat. Linux: run: bash bug-report.sh
It makes h5-together-bug-report-<date>.zip on your Desktop (Linux: in the folder you ran it from). The zip holds h5-together.log (and the older h5-together.old.log), your settings, and a short list of versions (the add-on, MMH5.5, Windows or Linux, the game's exes and DLLs). No saves, no maps. It sends nothing anywhere: the zip is a normal zip, so you can open it and look first. The log lists player names and LAN addresses.
Attach the zip to a new report at https://github.com/jozrcr/h5-together/issues/new/choose. It also works when the game does not start.
If a game freezes: wait 30 seconds before closing it (so the log can say where it was stuck), then make the report.
If the game crashes, the add-on writes h5-together-crash-<date>-<time>.txt (what crashed) and a small .dmp file next to the log (the 3 newest are kept; nothing is sent anywhere). The report adds the newest. The .dmp holds part of the game's memory at the crash (player names, folder names): bug-report.bat -NoDump (Linux: bash bug-report.sh --no-dump) leaves it out.


ANOTHER MOD THAT IS ALSO A DINPUT8.DLL

Some mods (ASI loaders, widescreen fixes) also come as a dinput8.dll in the game's bin folder, and the installer will not replace it. To use both: rename the other mod's dinput8.dll to dinput8_next.dll, then install H5 Together. H5 Together loads dinput8_next.dll when the game starts and passes DirectInput on to it; h5-together.log says "chained". To undo, delete or rename dinput8_next.dll.

LICENCES

H5 Together is free software under the MIT licence: see LICENSE.txt in this folder. Dear ImGui (the panels) and the small parts it carries are under their own licences: see THIRD-PARTY-NOTICES.txt. The add-on is an unofficial fan project, not made or endorsed by Ubisoft.

Project page, guides and bug reports: https://github.com/jozrcr/h5-together

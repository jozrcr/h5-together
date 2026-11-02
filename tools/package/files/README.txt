open-homm5 @VERSION@ - a small add-on for Heroes of Might and Magic V (Tribes of the East + MMH5.5)

What it is: one file, dinput8.dll, that the game loads at start-up, plus a settings file and 24 random map templates. It makes co-op games against the computer faster and more stable, and adds a few panels and options. It adds files to the game's bin folder and never changes the game's own files.

You need: the Steam game with MMH5.5 RC19d or RC19k already installed (MMH55_Utility_64.exe in the game's bin folder). This add-on does not install MMH5.5.
Everyone who plays the same co-op game should install the same version.


INSTALL

Windows: extract this zip (right-click it, "Extract All"), then double-click install.bat. It finds your Steam copy of the game and tells you what it did.
Linux (Steam / Proton): extract the zip, open a terminal in the folder, and run: bash install.sh. At the end it gives you one line to paste into the game's Steam launch options.
Updating: install the new version over the old one. Your settings are kept, and the new ones are added to open-homm5.ini (commented out, with what they do); the installer says how many. Your old file is saved as open-homm5.ini.bak.

Check that it works: start the game. A file named open-homm5.log appears in the game's bin folder. Its first line says "open-homm5 hook DLL", the version and "loaded". The log does not grow for ever: over 5 MB, the game keeps it as open-homm5.old.log (one older log) and starts a new one.


UNINSTALL

Double-click uninstall.bat (Windows) or run: bash uninstall.sh (Linux). Only our own files are removed (dinput8.dll, the logs, our templates, and your settings file and its .bak copies if you say yes). Templates you added yourself stay.


SETTINGS (you can skip this)

open-homm5.ini in the game's bin folder. Every line is commented out and shows the default. The defaults keep the original game: the speed-ups, fixes and panels are on, and anything that changes how the game plays, looks or sounds is off until you turn it on. Restart the game after a change.
Your hero look choices are kept in open-homm5-skins.ini, next to it.


WHAT IT DOES

- Faster co-op: quicker AI turns and clicks, less AI thinking on big maps, no long autosave freeze, LAN games over ZeroTier.
- Stability: a fix for a rare host freeze during computer turns, and a freeze detector (see below).
- Player list on the adventure map: who is playing, who has ended the turn. F9 hides it.
- "Your turn" notification when the game is in the background.
- Template builder: the "Template..." button ("Modeles..." in French) on the random map screen, or F11. Friends rebuild the same map from its name: nothing to share.
- Hero looks: the small button on the hero screen, or F7. Another hero's 3D figure, same faction.
- Game rules (host): the "Rules..." button ("Regles..." in French) in the multiplayer waiting room, or F2. Used only when every player has this version.
- Off by default, in open-homm5.ini: farther camera zoom, black map edges, a quieter volume range, the original Heroes V music, French texts, start terrain that matches each faction on random maps.
- Linux: the mouse wheel zooms out again (automatic).


NEW IN 0.2.7

- Game rules (host): Tribes of the East start bonuses (each player keeps the bonus picked with the lobby arrows: gold, resources, artifact, with ToE's own icons) and a separate game mode choice (Default, Savage, Nightmare, Random).
- Game rules (host): MMH5.5's scripts can also run on random maps straight from the game's generator in multiplayer, so the other rules work there too.
- Game rules (host): "Skills of each class": MMH5.5's lock, or Unlocked (any skill can be offered, the ones off the class's list at a small chance, as in the original game).
- The installer updates open-homm5.ini: your values are kept, the new settings are added, and the old file is saved as open-homm5.ini.bak.


REPORTING A PROBLEM

Windows: double-click bug-report.bat. Linux: run: bash bug-report.sh
It makes open-homm5-bug-report-<date>.zip on your Desktop (Linux: in the folder you ran it from). The zip holds open-homm5.log (and the older open-homm5.old.log), your settings, and a short list of versions (the add-on, MMH5.5, Windows or Linux, the game's exes and DLLs). No saves, no maps. It sends nothing anywhere: the zip is a normal zip, so you can open it and look first. The log lists player names and LAN addresses.
Attach the zip to a new report at https://github.com/jozrcr/open-homm5/issues/new/choose. It also works when the game does not start.
If a game freezes: wait 30 seconds before closing it (so the log can say where it was stuck), then make the report.

LICENCES

open-homm5 is free software under the MIT licence: see LICENSE.txt in this folder. Dear ImGui (the panels) and the small parts it carries are under their own licences: see THIRD-PARTY-NOTICES.txt. The add-on is an unofficial fan project, not made or endorsed by Ubisoft.

Project page, guides and bug reports: https://github.com/jozrcr/open-homm5

H5 Together @VERSION@ - a small add-on for Heroes of Might and Magic V: Tribes of the East (the original game first, MMH5.5 too)

What it is: one file, dinput8.dll, that the game loads at start-up, plus a settings file and 24 random map templates. It makes co-op games against the computer faster and more stable, and adds a few panels and options. It adds files to the game's bin folder and never changes the game's own files.

You need: the Steam game. Since 0.3.5 the original 2008 game comes first: the installer can make bin\H5_Original_4GB.exe for it, with your yes. MMH5.5 RC19d or RC19k works too, when it is already installed (MMH55_Utility_64.exe in the game's bin folder); this add-on does not install MMH5.5.
Everyone who plays the same co-op game should install the same version.


INSTALL

Windows: extract this zip (right-click it, "Extract All"), then double-click install.bat. It looks for the game (Steam, GOG), shows you what it found (the folder, the language, which MMH5.5 you have), and tells you what it did and what to do next. If it cannot find the game, or finds several, it asks: type a number, or B to choose the folder in a window. If the game is running it asks you to close it first.
Linux (Steam / Proton): extract the zip, open a terminal in the folder, and run: bash install.sh. The same screens, without the B window (paste the folder instead). At the end it gives you one line to paste into the game's Steam launch options.
It says which exe to start (the original game's, or MMH5.5's when you have it), and warns you in plain words if your MMH5.5 is a version we have not tested (we tested RC19d and RC19k).
Updating: install the new version over the old one. Your settings are kept, and the new ones are added to h5-together.ini (commented out, with what they do); the installer says how many. Your old file is saved as h5-together.ini.bak.
H5 Together was called open-homm5 until version 0.2.7. Installing over an old version gives your files the new name: open-homm5.ini becomes h5-together.ini, open-homm5-skins.ini becomes h5-together-skins.ini, and the folder open-homm5 becomes h5-together (with everything in it). Nothing you set is lost.

Check that it works: start the game. A file named h5-together.log appears in the game's bin folder. Its first line says "H5 Together hook DLL", the version and "loaded". The log does not grow for ever: over 5 MB, the game keeps it as h5-together.old.log (one older log) and starts a new one.


UNINSTALL

Double-click uninstall.bat (Windows) or run: bash uninstall.sh (Linux). Only our own files are removed (dinput8.dll, the logs, our templates, our language files, Valve's steam_api.dll if this package installed it, and your settings file and its .bak copies if you say yes). Templates and translations you added yourself stay.


SETTINGS (you can skip this)

In the game: main menu, Options, then "H5 Together" (or press F3). The page is part of the game's own Options screen: change a setting, then Apply (Default puts all of them back). A name that ends in * applies after a restart of the game, ** from the next game; the others apply at once. During a multiplayer game the host's and the advanced settings are read only.
The settings are saved in h5-together.ini in the game's bin folder; you can also edit it with Notepad (then restart the game). Every line that starts with ";" is not changed and shows the default. The speed-ups, fixes and panels are on. Since 0.3.5 a few co-op choices are on by default too (endless simultaneous turns, Undo End Turn, start terrain of your faction, online play over Steam: see NEW IN 0.3.5); the rest of what changes how the game plays, looks or sounds is off until you turn it on.
The main menu shows your H5 Together version under the game's own version.
Your hero look choices are kept in h5-together-skins.ini, next to it.


LANGUAGES

The add-on's panels and messages are in English or French: one file per language in the game's bin\h5-together\texts folder (en.txt, fr.txt). The add-on uses the game's language by itself (Steam's language for the game, else the game's own texts, else Windows'). To choose: Options, H5 Together, Language (or [ui] language=en or fr in h5-together.ini). Any text a file lacks shows in English.
Translations are welcome: copy en.txt to the code of your language (de.txt, ru.txt, pl.txt...), translate the text right of each "=" (the top of en.txt says how), put it in the same folder, set [ui] language=de, and send it to us: https://github.com/jozrcr/h5-together/issues/new/choose


WHAT IT DOES

- Faster co-op: quicker AI turns and clicks, less AI thinking on big maps, no long autosave freeze, LAN games over ZeroTier.
- Online play over Steam (on by default since 0.3.5): your Steam friends' games in the game's LAN list, invites from the waiting room.
- Stability: a fix for a rare host freeze during computer turns, a freeze detector, and a short report when the game crashes (see below).
- Multiplayer waiting room: a line at the bottom says whether every player has the same H5 Together version, and who does not.
- Player list on the adventure map: who is playing, who has ended the turn. F9 hides it.
- "Your turn" notification when the game is in the background.
- Template builder: the "Template..." button ("Modeles..." in French) on the random map screen, or F11. Friends rebuild the same map from its name: nothing to share.
- Hero looks: the small button on the hero screen, or F7. Another hero's 3D figure, same faction.
- Game rules (host): the "Rules..." button ("Regles..." in French) in the multiplayer waiting room, or F2. Used only when every player has this version.
- Off by default (Options, H5 Together, or h5-together.ini): a "new version available" notice in the main menu (the first start asks; at most once a day it asks GitHub for the latest version number; nothing is downloaded), farther camera zoom, black map edges, the original Heroes V music, French texts.
- Linux: the mouse wheel zooms out again (automatic).


NEW IN 0.3.6

- Computers playing together: fixed. In 0.3.5 (and since 0.3.2 when it was turned on), only the first computer player of each day moved its heroes; the others stayed home after the first day, so they were much weaker. Now every one moves its heroes every day. It is OFF by default again (an experiment: about 12-19% shorter waits, and three long robot games of nine crashed with it). Every PC needs 0.3.6 for it: with an older PC in the game, the computers play one after another.


NEW IN 0.3.5

- The original 2008 game, without MMH5.5: supported, also from a folder where MMH5.5 is installed (H5 Together hides MMH5.5's files from it; MMH5.5 still starts as before). The installer can make bin\H5_Original_4GB.exe (the original game with 4 GB of memory): with your yes it fetches only the 2008 exe from MMH5.5's official GitHub release and checks it. Its own profile and saves (Documents\H5 Together\Original game). Endless simultaneous turns for it (the host's choice).
- A Linux PC can join a Windows host's big random map over Steam (it timed out: "timeout 0.18.0").
- Watching a friend's battle: you also see their army's placement before the battle, as they move it.
- A sword on an ally's row in the player list while they fight: click it to follow them and open their battle.
- Hero details in the waiting room: the starting hero's tooltip gives its class, starting skills and what its specialty does.
- Skill wheel (the original game): a small wheel button on the hero screen shows what the hero's class can learn.
- Builder maps: the team arrows move you to another start of your own team.
- New defaults, the group's usual co-op setup: endless simultaneous turns, computers playing together, Undo End Turn, start terrain of your faction (the host's choices), the wider volume range, online play over Steam, a 180 frames a second limit.
- The installer's last screen names the exe to start (the original game's when it made it).
- Mixing versions: endless turns, the army placement and the sword need 0.3.5 on every PC; everything else mixes with 0.3.4.


NEW IN 0.3.4

- Following: click an ally's name or eye to start and stop; your camera goes to their hero at once, also when they are in a town or a battle. A short line after the eye says where they are; while they fight, a small "Watch the battle" button sits there. Nothing in the middle of the map any more.
- Watching their battle: a right click shows a creature's card; watching works also when they were already fighting, and ends by itself when their battle was their game loss.
- Stay and watch after a defeat (on): when you lose the game while your friends play on, closing the results keeps you on the map to follow your team and watch their battles.
- "Resume my turn" is a round button over the hourglass.
- The computer's weekly bonus (the host's F2 rules) also applies to a loaded game, from the next week.
- Black map edges in the underground too (edge_sky=none). Quick resume: the "Reprise" save reaches the friend who crashed whole.
- The computer bonus in a loaded game needs 0.3.4 on every PC; following works best with 0.3.4 everywhere. Everything else mixes with 0.3.3.


NEW IN 0.3.3

- Follow a friend's view: while you wait, click an ally's name in the player list and your camera follows theirs on the adventure map (allies only, each through their own fog). A button just over the turn banner stops it; it also stops when your turn begins.
- Watch their battle live: while you follow an ally who fights, "Watch the battle" opens their battle on your screen, even mid-battle. You only watch (the battle gets none of your clicks); "Stop watching" or Esc brings you back.
- Options, H5 Together: "Allies may follow my view" and "Watch the followed ally's battles" (both on).
- Following needs 0.3.3 on both PCs. Everything else mixes with 0.3.2.


NEW IN 0.3.2

- A smoother map: big maps zoomed far out run about twice as fast (the game gets more room for its 3D models; far-away animations get lighter when the map runs slow), and MMH5.5's scripts run at their own pace again with the faster co-op steps.
- Computers play together (the host's choice, off by default): the computer players take their turns at the same time.
- Undo End Turn (the host's choice, off by default): with simultaneous turns, take back an End Turn while another human still plays.
- Quick resume after a crash (on): every PC saves "Reprise" with the player who crashed; they get the save over Steam.
- Each game its own autosaves: named after their map, so a new game no longer overwrites yesterday's.
- A frame rate limit and the vertical sync (off by default), in Options, H5 Together.
- Game rules, "Skills of each class: Unlocked": the skills of the other classes of the hero's own faction only, never another faction's.
- Fixed: the Options page sometimes did not save with many settings on.
- Every player needs 0.3.2 for the co-op features and the host's game rules.


NEW IN 0.3.1

- Co-op games no longer lag: a PC that falls behind now catches up (before, every other PC waited for it at each step), and the game's connection sends five times more often, so the computer players' big moves no longer hold up everything else. In our tests over Steam, a click on the map went from about 0.5 s to 0.12 s, and the computer players' moves from 0.57 s apart to 0.15 s. Also for LAN and GameRanger games.
- Hero looks: the Look button on the hero screen now changes the hero the screen shows (before, the hero selected on the map). In multiplayer, the hero screen shows the new look at once, also for a hero in a town.
- Online play: "Invite friends..." also in the waiting room of a loaded game.
- No more false "REBUILD FAILED" lines in the log at autosaves.


NEW IN 0.3.0 (a test release)

- Online play with your Steam friends, without GameRanger: Options, H5 Together, Online play: Steam, then restart the game (every player). Your friends' games show in Multiplayer, LAN. The host invites friends with "Invite friends..." ("Inviter des amis..." in French) in the waiting room; an invited friend's game joins by itself. While it is on, the LAN list shows only games through Steam: for GameRanger, set it back to Off. Needs the Steam game and Steam running; tested with two PCs.
- Settings inside the game (Options, H5 Together, or F3), a friendlier installer, the version line in the waiting room, crash reports, the bug report script, an optional new version notice, the add-on's words in language files, and a main menu picture and tune (off by default).
- The game's cursor stays above the add-on's panels, and their buttons light up under the cursor.
- open-homm5 is now H5 Together: installing over 0.2.7 renames your files and keeps your settings.


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
If this package has h5-together\steam_api.dll, that file is Valve's, from the Steamworks SDK (not ours, not under our licence: see THIRD-PARTY-NOTICES.txt). The installer puts it in the game's bin\h5-together folder, and the add-on only uses it for online play over Steam, which is off until you turn it on in Options.

Project page, guides and bug reports: https://github.com/jozrcr/h5-together

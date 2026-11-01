// Host rules: what the panel (host_rules_panel.cpp) needs from the mod
// (host_rules.cpp).
#pragma once

#include <string>
#include <vector>

#include "host_rules_logic.h"

namespace mods {
namespace rules {

// What the panel shows, taken under the mod's lock.
struct View {
	enum Role { kNoGame, kSetup, kHost, kClient, kInGame } role = kNoGame;  // kSetup: the host's setup screen
	bool file_known = false;  // this PC's H55-Settings.lua was read
	Values file;              // its values (the host's defaults)
	Values chosen;            // host: its choices (empty = the file's)
	// client: what the host sent in the lobby; in game: the rules of this game
	bool shown_known = false;
	Values shown;
	bool will_apply = false;  // host and client: every player has open-homm5's rules (else the files' rules)
	int map_script = -1;      // host and client: the lobby's map has a map script (1), none (0: MMH5.5's scripts will not run), unknown (-1)
	std::vector<std::string> missing;  // host: the players without it (names)
	std::vector<std::string> differ;   // host: players whose MMH5.5 files differ ("Name: settings"...)
	bool this_differs = false;         // client: the host said this PC's files differ from its own
	bool in_game_rules = false;        // in game: the host's rules are used (else the files')
};
// The view for this role (the panel decides it from the screen on top and LobbyRole()).
View GetView(View::Role role);

// Host: new choices (empty = the file's). Written to open-homm5.ini [rules] and
// sent to the players in the lobby.
void SetChoices(const Values &chosen);

// The panel's settings, read by the mod: its key (F2 by default) and the
// lobby button.
int PanelKey();
bool PanelButton();
// From ApplyHostRulesFromSettings: the overlay client (host_rules_panel.cpp).
bool InstallPanel();

// Is the waiting room on top, and this PC's role there (the logic seen in the
// last second)? kNoGame when not in a waiting room.
View::Role LobbyRole();

}  // namespace rules
}  // namespace mods

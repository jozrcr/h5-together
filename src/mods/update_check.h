// Mod: the opt-in "new version available" check (docs/plans/community-release.md, part 4).
// Off by default ([update] check=0). When on: at most once a day, when the main menu first
// shows, one HTTPS request to GitHub's public "latest release" address for our repo, on a
// thread of its own (short time limits; the game never waits for it). It sends nothing
// about the player (GitHub sees the address it comes from, as with any website). If a newer
// version is out, a small notice in the main menu (and a log line) says so, with the
// Releases page's address. Nothing is ever downloaded or installed.
#pragma once

namespace mods {

// From hooks::InstallEarly: the overlay's Direct3D import, when the check is on.
void InstallUpdateCheckEarly();

// From hooks::InstallAll: the notice's overlay client and the check, when it is on.
void ApplyUpdateCheckFromSettings();

}  // namespace mods

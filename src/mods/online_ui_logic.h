// The online UI's pure work (no game, no Steam), unit-tested by
// src/hook/test/online_ui_test.cpp. The UI itself is online_ui.cpp; the Steam
// transport (virtual_lan_steam.cpp) fills the Status below; the design is
// docs/plans/online-steam.md, "Step 4: the online UI".
//
// In plain words:
// - Status: what Steam play looks like now (is Steam there, and if not why; how
//   many friends play with H5 Together; friends' games this PC cannot join;
//   invites). The LAN screen shows it as a few lines (StatusLines).
// - Friends: the invite picker's list (the host's Steam friends who are online),
//   in the order it shows them (SortFriends).
// - ConnectLobby: the lobby in "+connect_lobby <id>", which Steam adds to the
//   game's command line when it starts the game for a friend's invite.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "virtual_lan_logic.h"

namespace online {

// --- what Steam play looks like now ----------------------------------------------------
enum class State {
	kOff,          // [online] mode is not steam: the UI shows nothing (the game as it was)
	kStarting,     // mode=steam, Steam not started yet (before the game's first DirectInput call)
	kUnavailable,  // mode=steam, Steam could not start (Why)
	kOn,           // Steam runs in the game
};
enum class Why {
	kNone,
	kNoDll,        // no steam_api.dll in bin\h5-together (a GOG copy?)
	kOldDll,       // a steam_api.dll without the functions we call
	kNoSteam,      // the Steam client is not running
	kSteamTooOld,  // the Steam client must be updated
	kRefused,      // Steam refused otherwise (not logged in? the game not owned on Steam?)
	kNoAnswer,     // Steam started but did not answer (an interface or our SteamID missing)
	kNoHooks,      // the game's network imports were not found: no virtual LAN at all
};

const int kNameSize = 64;  // a Steam name, UTF-8 (Steam's own limit is 32 characters)

// A friend's game that this PC cannot join.
struct Problem {
	char name[kNameSize];  // the friend's Steam name: on screen only, never in the log
	vlan::HostCheck check; // kOtherGame or kOtherProtocol
	char version[24];      // their H5 Together
};

// An invite (or "Join game" in the friends list), or Steam's "+connect_lobby".
struct Invite {
	char name[kNameSize];  // who invited ("" = not known: "+connect_lobby", or not through a friend)
	bool visible;          // the host's game is known now: it shows in the game's LAN list
	bool cannot;           // the host's game cannot be joined (another game or protocol)
};

struct Status {
	State state = State::kOff;
	Why why = Why::kNone;
	bool relay_ok = false;    // Steam's relay network is up (friends' games show only then)
	int friends_playing = 0;  // Steam friends playing the game (app 15370) now
	int friends_with_us = 0;  // ... of them with H5 Together's online play on (their rich presence)
	int hosts = 0;            // friends' games this PC can join (they show in the LAN list)
	static const int kMaxProblems = 6;
	int problem_count = 0;
	Problem problems[kMaxProblems] = {};
	static const int kMaxInvites = 4;
	int invite_count = 0;
	Invite invites[kMaxInvites] = {};  // the newest first
	uint32_t invite_serial = 0;        // +1 with each new invite (the UI shows each one once)
	bool own_lobby = false;   // this PC's game hosts and its Steam lobby exists: friends can be invited
	bool overlay = false;     // Steam's overlay is enabled in this game (its invite dialog works)
	bool test = false;        // [online] ui_test=1: made-up friends (robot screenshots), no invite really sent
};

// A line of the LAN screen's status block, and how it looks.
enum class Tone {
	kTitle,  // the block's heading (gold)
	kBody,   // parchment
	kDim,    // a remark
	kWarn,   // something that does not work
	kGood,   // an invite
};
struct Line {
	std::string text;
	Tone tone;
};
// A text key's words (texts::Get).
typedef const char *(*GetText)(const char *key);
// The status block of the game's LAN screen (empty when the state is kOff). `ours`:
// this PC's H5 Together version.
std::vector<Line> StatusLines(const Status &s, GetText get, const char *ours);
// The words of an invite for the banner and the Windows pop-up ("Pierre invites you
// to play: open Multiplayer > LAN.").
std::string InviteText(const Invite &invite, GetText get);
// A Steam name for the screen: control characters out, at most kNameSize - 1 bytes,
// never cut inside a UTF-8 letter.
void CopyName(const char *from, char *out, size_t size);

// --- the invite picker ----------------------------------------------------------------
struct Friend {
	uint64_t id;
	char name[kNameSize];
	int persona;    // EPersonaState: 1 online, 2 busy, 3 away, 4 snooze, 5/6 online (trade, play)
	bool playing;   // playing the game (app 15370) now
	bool with_us;   // ... with H5 Together's online play
	int invited;    // invited from the picker: 0 not yet, 1 sent, -1 Steam refused
};
// Online friends first by what they do (with H5 Together, in the game, online,
// busy, away), then by name (letter case ignored). Offline friends are not in
// Steam's answer's rows we keep (KeepFriend).
void SortFriends(std::vector<Friend> *friends);
bool KeepFriend(int persona);  // not offline (0), not invisible (7)
// The key of the words under a friend's name ("online.picker.playing_us"...).
const char *FriendStateKey(const Friend &f);

// --- Steam's command line --------------------------------------------------------------
// "... +connect_lobby 109775240000000123 ..." -> that lobby; 0 if there is none.
uint64_t ConnectLobby(const wchar_t *command_line);

// --- [online] ui_test=1 (robot screenshots with one PC) ---------------------------------
// Made-up friends instead of Steam's: two playing with H5 Together, one game of another
// MMH5.5, one of another H5 Together, and the picker's list. No Steam call uses them.
void AddTestFriends(Status *s);
std::vector<Friend> TestFriends();
// A made-up invite, as from Steam's friends list (the robot's "panel|online test_invite").
Invite TestInvite();

}  // namespace online

// Test of the online UI's pure work (src/mods/online_ui_logic.cpp): the lines of
// the game's LAN screen for each state of Steam play, the invite banner's words,
// Steam names cut for the screen, the invite list's order, Steam's
// "+connect_lobby" on the command line, the made-up friends of [online]
// ui_test=1, and every word in en.txt and fr.txt. No Steam, no game.
// Usage: online_ui_test.exe <src\mods\texts folder>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../mods/online_ui_logic.h"
#include "../texts_logic.h"

using namespace online;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what, const std::string &detail = "")
{
	printf("%s: %s%s%s\n", ok ? "ok  " : "FAIL", what, ok || detail.empty() ? "" : ": ", ok ? "" : detail.c_str());
	if (!ok)
		++g_failures;
}

std::string ReadAll(const std::string &path)
{
	std::ifstream f(path, std::ios::binary);
	std::ostringstream s;
	s << f.rdbuf();
	return s.str();
}

// The words: one language's book, the key itself when it lacks one (so a missing key shows).
texts::Book *g_book = nullptr;
std::map<std::string, std::string> g_keys_asked;

const char *Words(const char *key)
{
	g_keys_asked[key] = key;
	const auto it = g_book->texts.find(key);
	return it != g_book->texts.end() ? it->second.c_str() : key;
}

std::string Join(const std::vector<Line> &lines)
{
	std::string s;
	for (const Line &l : lines)
		s += l.text + "\n";
	return s;
}

bool Has(const std::vector<Line> &lines, const char *part, Tone tone)
{
	for (const Line &l : lines)
		if (l.text.find(part) != std::string::npos && l.tone == tone)
			return true;
	return false;
}

void TestLines(texts::Book &en)
{
	g_book = &en;
	Status s;
	Check(StatusLines(s, &Words, "0.3.0").empty(), "lines: mode off: nothing (the game's LAN screen as it was)");

	s.state = State::kStarting;
	std::vector<Line> l = StatusLines(s, &Words, "0.3.0");
	Check(l.size() == 2 && l[0].tone == Tone::kTitle && l[0].text == "Online play over Steam" && l[1].text == "Steam is starting...",
	      "lines: Steam not started yet", Join(l));

	s.state = State::kUnavailable;
	const struct {
		Why why;
		const char *part;
	} kWhy[] = {{Why::kNoDll, "steam_api.dll is missing from bin\\h5-together"}, {Why::kOldDll, "too old"},
	            {Why::kNoSteam, "Steam is not running"}, {Why::kSteamTooOld, "must be updated"},
	            {Why::kRefused, "Steam refused"}, {Why::kNoAnswer, "did not answer"}, {Why::kNoHooks, "this exe"}};
	bool all = true;
	std::string bad;
	for (const auto &w : kWhy) {
		s.why = w.why;
		l = StatusLines(s, &Words, "0.3.0");
		if (l.size() != 3 || !Has(l, w.part, Tone::kWarn) || !Has(l, "local network", Tone::kDim)) {
			all = false;
			bad += Join(l);
		}
	}
	Check(all, "lines: Steam not available: why (a warning), then that the list shows the local network", bad);

	s = Status();
	s.state = State::kOn;
	s.relay_ok = true;
	l = StatusLines(s, &Words, "0.3.0");
	Check(l.size() == 2 && Has(l, "none of your friends", Tone::kBody), "lines: Steam on, no friend playing", Join(l));
	s.relay_ok = false;
	l = StatusLines(s, &Words, "0.3.0");
	Check(Has(l, "network is starting", Tone::kDim), "lines: the relay network not up yet: said", Join(l));
	s.relay_ok = true;
	s.friends_playing = 1;
	s.friends_with_us = 1;
	s.hosts = 1;
	l = StatusLines(s, &Words, "0.3.0");
	Check(Has(l, "Steam: 1 friend is playing", Tone::kBody) && Has(l, "list above", Tone::kDim), "lines: one friend, a game to join",
	      Join(l));
	s.friends_playing = 5;
	s.friends_with_us = 3;
	s.hosts = 0;
	l = StatusLines(s, &Words, "0.3.0");
	Check(Has(l, "Steam: 3 friends are playing", Tone::kBody) && Has(l, "2 more are playing Heroes V without", Tone::kDim) &&
	          !Has(l, "list above", Tone::kDim),
	      "lines: three friends with H5 Together, two without, no game hosted", Join(l));
	s.friends_playing = 4;
	l = StatusLines(s, &Words, "0.3.0");
	Check(Has(l, "1 more is playing", Tone::kDim), "lines: one more without H5 Together", Join(l));

	// games this PC cannot join, and invites
	s = Status();
	s.state = State::kOn;
	s.relay_ok = true;
	s.problem_count = 2;
	CopyName("Pierre", s.problems[0].name, kNameSize);
	s.problems[0].check = vlan::HostCheck::kOtherGame;
	CopyName("", s.problems[1].name, kNameSize);
	s.problems[1].check = vlan::HostCheck::kOtherProtocol;
	strcpy_s(s.problems[1].version, sizeof(s.problems[1].version), "0.4.0");
	s.invite_count = 3;
	CopyName("Lucas", s.invites[0].name, kNameSize);
	s.invites[0].visible = true;
	CopyName("Marie", s.invites[1].name, kNameSize);
	CopyName("Eve", s.invites[2].name, kNameSize);
	s.invites[2].cannot = true;
	l = StatusLines(s, &Words, "0.3.0");
	Check(Has(l, "Pierre hosts a game you cannot join: another version of MMH5.5", Tone::kWarn), "lines: another game build", Join(l));
	Check(Has(l, "A friend hosts with H5 Together 0.4.0, which cannot play with yours (0.3.0)", Tone::kWarn),
	      "lines: another H5 Together; a friend without a name", Join(l));
	Check(Has(l, "Lucas invites you: their game is in the list", Tone::kGood) &&
	          Has(l, "Marie invites you: their game shows in the list above in a moment", Tone::kGood) &&
	          Has(l, "Eve invites you, but you cannot join", Tone::kWarn),
	      "lines: invites: visible, on the way, cannot be joined", Join(l));
	size_t invite_at = 0, problem_at = 0;
	for (size_t i = 0; i < l.size(); ++i) {
		if (l[i].text.find("Lucas") != std::string::npos)
			invite_at = i;
		if (l[i].text.find("Pierre") != std::string::npos)
			problem_at = i;
	}
	Check(invite_at && problem_at && invite_at < problem_at, "lines: the invites before the games that cannot be joined");

	Invite v = {};
	CopyName("Lucas", v.name, kNameSize);
	Check(InviteText(v, &Words) == "Lucas invites you to play: open Multiplayer > LAN.", "banner: the invite's words",
	      InviteText(v, &Words));
	v.name[0] = 0;
	Check(InviteText(v, &Words) == "A friend invites you to play: open Multiplayer > LAN.", "banner: +connect_lobby (nobody known)",
	      InviteText(v, &Words));

	// the auto-join's line on the LAN screen
	Check(JoinLine(JoinState::kNone, "Lucas", &Words).text.empty(), "auto-join: no line when there is none");
	const Line wait = JoinLine(JoinState::kWaiting, "Lucas", &Words), join = JoinLine(JoinState::kJoining, "", &Words);
	const Line none = JoinLine(JoinState::kNoRow, "Lucas", &Words), failed = JoinLine(JoinState::kFailed, "Lucas", &Words);
	Check(wait.tone == Tone::kGood && wait.text.find("Lucas invites you: waiting for their game") == 0 &&
	          join.tone == Tone::kGood && join.text == "A friend invites you: joining their game...",
	      "auto-join: waiting and joining lines (a friend without a name)", wait.text + " / " + join.text);
	Check(none.tone == Tone::kWarn && none.text.find("Lucas invites you, but their game did not show") == 0 && failed.tone == Tone::kWarn &&
	          failed.text.find("press Join again") != std::string::npos,
	      "auto-join: the two failures are warnings that say what to do", none.text + " / " + failed.text);
	Invite made = TestInvite();
	Check(made.made_up && !made.host && !made.lobby, "auto-join: ui_test's made-up invite is marked (no auto-join for it)");
}

void TestNames()
{
	char out[kNameSize];
	CopyName("Pierre", out, sizeof(out));
	Check(strcmp(out, "Pierre") == 0, "names: a plain name");
	CopyName("Ann\nB\tob\x7f", out, sizeof(out));
	Check(strcmp(out, "AnnBob") == 0, "names: control characters out");
	CopyName(nullptr, out, sizeof(out));
	Check(out[0] == 0, "names: none");
	char small[6];
	CopyName("ab\xc3\xa9\xc3\xa9", small, sizeof(small));  // "abéé": 6 bytes, 5 fit
	Check(strcmp(small, "ab\xc3\xa9") == 0, "names: cut between letters, never inside one");
	CopyName("a\xc3", small, sizeof(small));
	Check(strcmp(small, "a") == 0, "names: a broken letter at the end is dropped");
	CopyName("\xe4\xb8\xad\xe6\x96\x87", out, sizeof(out));
	Check(strcmp(out, "\xe4\xb8\xad\xe6\x96\x87") == 0, "names: three-byte letters kept");
	std::string long_name(200, 'x');
	CopyName(long_name.c_str(), out, sizeof(out));
	Check(strlen(out) == kNameSize - 1, "names: at most kNameSize - 1 bytes");
}

Friend F(const char *name, int persona, bool playing, bool with_us)
{
	Friend f = {};
	CopyName(name, f.name, kNameSize);
	f.persona = persona;
	f.playing = playing;
	f.with_us = with_us;
	return f;
}

void TestPicker(texts::Book &en)
{
	g_book = &en;
	std::vector<Friend> list = {F("zed", 3, false, false), F("Bob", 1, false, false), F("amy", 1, false, false),
	                            F("Max", 2, false, false), F("Kim", 1, true, false), F("Ann", 6, true, true),
	                            F("Lea", 4, false, false)};
	SortFriends(&list);
	std::string order;
	for (const Friend &f : list)
		order += std::string(f.name) + " ";
	Check(order == "Ann Kim amy Bob Max Lea zed ", "picker: with H5 Together, in the game, online, busy, away; then by name",
	      order);
	Check(strcmp(FriendStateKey(list[0]), "online.picker.playing_us") == 0 &&
	          strcmp(FriendStateKey(list[1]), "online.picker.playing") == 0 &&
	          strcmp(FriendStateKey(list[2]), "online.picker.online") == 0 &&
	          strcmp(FriendStateKey(list[4]), "online.picker.busy") == 0 &&
	          strcmp(FriendStateKey(list[5]), "online.picker.away") == 0,
	      "picker: the words under each name");
	Check(!KeepFriend(0) && KeepFriend(1) && KeepFriend(3) && KeepFriend(6) && !KeepFriend(7) && !KeepFriend(-1),
	      "picker: offline (and invisible) friends are left out");
	for (const Friend &f : list)
		Words(FriendStateKey(f));
	const std::vector<Friend> made = online::TestFriends();
	bool ids = true;
	for (size_t i = 0; i < made.size(); ++i)
		for (size_t j = i + 1; j < made.size(); ++j)
			ids = ids && made[i].id != made[j].id;
	Check(made.size() == 6 && ids && strcmp(made[0].name, "Lucas") == 0 && made[0].with_us && made[0].invited == 0,
	      "ui_test: six made-up friends, sorted, none invited");
	Status s;
	s.state = State::kOn;
	AddTestFriends(&s);
	Check(s.test && s.friends_with_us == 2 && s.friends_playing == 3 && s.problem_count == 2 &&
	          s.problems[0].check == vlan::HostCheck::kOtherGame && s.problems[1].check == vlan::HostCheck::kOtherProtocol,
	      "ui_test: made-up friends in the status: two with H5 Together, two games that cannot be joined");
	Check(strcmp(TestInvite().name, "Lucas") == 0, "ui_test: a made-up invite");
}

void TestConnectLobby()
{
	Check(ConnectLobby(L"MMH55_Utility_64.exe +connect_lobby 109775240000000123") == 109775240000000123ull,
	      "+connect_lobby: at the end");
	Check(ConnectLobby(L"\"C:\\Games\\H5\\bin\\H5_Game.exe\" +connect_lobby 109775240000000123 -windowed") == 109775240000000123ull,
	      "+connect_lobby: in the middle, after a quoted path");
	Check(ConnectLobby(L"x.exe +connect_lobby=12345") == 12345, "+connect_lobby: with =");
	Check(ConnectLobby(L"x.exe") == 0 && ConnectLobby(nullptr) == 0 && ConnectLobby(L"") == 0, "+connect_lobby: none");
	Check(ConnectLobby(L"x.exe +connect_lobby") == 0 && ConnectLobby(L"x.exe +connect_lobby abc") == 0 &&
	          ConnectLobby(L"x.exe +connect_lobby 0") == 0,
	      "+connect_lobby: no number, not a number, 0");
	Check(ConnectLobby(L"x.exe +connect_lobby 12a") == 0 && ConnectLobby(L"x.exe +connect_lobby 99999999999999999999") == 0,
	      "+connect_lobby: letters after the digits, too big for 64 bits");
	Check(ConnectLobby(L"x.exe x+connect_lobby 5 +connect_lobbyx 6 +connect_lobby 7") == 7,
	      "+connect_lobby: only the whole word counts");
}

void TestRooms()
{
	Check(WaitingRoom(".?AVCMPWaitScreen@NUI@@") == Room::kNewGame, "rooms: a new game's waiting room");
	Check(WaitingRoom(".?AVCMPUbiComLoadScreen@NUI@@") == Room::kLoadedGame, "rooms: a loaded game's waiting room");
	Check(WaitingRoom(".?AVCMPCreateScreen@NUI@@") == Room::kNone && WaitingRoom(".?AVCLobbyScreen@NUI@@") == Room::kNone &&
	          WaitingRoom(".?AV?$CUbiComLoadScreen@USLanSaveHeaderInfo@@@NUI@@") == Room::kNone &&
	          WaitingRoom(".?AV?$CLoadScreen@USLanSaveHeaderInfo@@@NUI@@") == Room::kNone,
	      "rooms: the setup screen, the LAN screen and the save picker are not waiting rooms");
	Check(WaitingRoom("") == Room::kNone && WaitingRoom(nullptr) == Room::kNone, "rooms: no screen");
}

void TestWords(const std::string &dir)
{
	texts::Book en, fr;
	Check(texts::Parse(ReadAll(dir + "\\en.txt"), &en) && texts::Parse(ReadAll(dir + "\\fr.txt"), &fr), "words: en.txt and fr.txt read");
	TestLines(en);
	TestPicker(en);
	// every key the logic asked for, and the UI's own, in both languages with the same {names}
	const char *const kUi[] = {"online.invite_button", "online.toast_title", "online.picker.title", "online.picker.sub",
	                           "online.picker.none", "online.picker.invite", "online.picker.invited", "online.picker.refused",
	                           "online.picker.close", "online.off.no_dll", "online.off.old_dll", "online.off.no_steam",
	                           "online.off.steam_old", "online.off.refused", "online.off.no_answer", "online.off.no_hooks",
	                           "online.others_one", "online.others", "online.friends_one", "online.friends"};
	for (const char *k : kUi)
		g_keys_asked[k] = k;
	std::string missing, differ;
	for (const auto &k : g_keys_asked) {
		const auto e = en.texts.find(k.first), f = fr.texts.find(k.first);
		if (e == en.texts.end() || f == fr.texts.end())
			missing += " " + k.first;
		else if (texts::Marks(e->second) != texts::Marks(f->second))
			differ += " " + k.first;
	}
	Check(missing.empty(), "words: every text of the online UI is in en.txt and fr.txt", missing);
	Check(differ.empty(), "words: the same {names} in both", differ);
	// French: the game's own names of Multiplayer > LAN
	g_book = &fr;
	Invite v = {};
	CopyName("Lucas", v.name, kNameSize);
	Check(InviteText(v, &Words) == "Lucas vous invite \xc3\xa0 jouer : ouvrez Multijoueur > R\xc3\xa9seau local.",
	      "words: French banner, with the game's own menu names", InviteText(v, &Words));
	Check(JoinLine(JoinState::kJoining, "", &Words).text == "Un ami vous invite : connexion \xc3\xa0 sa partie..." &&
	          JoinLine(JoinState::kNoRow, "Lucas", &Words).text.find("Rejoindre") != std::string::npos,
	      "words: French auto-join lines, with the game's own button name (Rejoindre)");
}

}  // namespace

int main(int argc, char **argv)
{
	TestNames();
	TestConnectLobby();
	TestRooms();
	if (argc > 1)
		TestWords(argv[1]);
	else
		Check(false, "usage: online_ui_test.exe <src\\mods\\texts folder>");
	printf("%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
	return g_failures ? 1 : 0;
}

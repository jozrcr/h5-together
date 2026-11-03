// The online UI's pure work (see online_ui_logic.h).
#include "online_ui_logic.h"

#include <string.h>
#include <wchar.h>

#include <algorithm>

#include "../hook/texts_logic.h"

namespace online {
namespace {

std::string Fill(GetText get, const char *key, std::initializer_list<texts::Arg> args)
{
	const char *text = get(key);
	return texts::Fill(text ? text : "", args);
}

// A friend's name for a line: theirs, or "A friend" when Steam did not say.
std::string Who(const char *name, GetText get)
{
	return name && name[0] ? std::string(name) : std::string(get("online.someone"));
}

const char *WhyKey(Why why)
{
	switch (why) {
	case Why::kNoDll: return "online.off.no_dll";
	case Why::kOldDll: return "online.off.old_dll";
	case Why::kNoSteam: return "online.off.no_steam";
	case Why::kSteamTooOld: return "online.off.steam_old";
	case Why::kNoAnswer: return "online.off.no_answer";
	case Why::kNoHooks: return "online.off.no_hooks";
	default: return "online.off.refused";
	}
}

int Rank(const Friend &f)
{
	if (f.playing && f.with_us)
		return 0;
	if (f.playing)
		return 1;
	if (f.persona == 1 || f.persona == 5 || f.persona == 6)
		return 2;
	if (f.persona == 2)
		return 3;
	return 4;  // away, snooze
}

int CompareNames(const char *a, const char *b)
{
	for (;; ++a, ++b) {
		unsigned char x = static_cast<unsigned char>(*a), y = static_cast<unsigned char>(*b);
		if (x >= 'A' && x <= 'Z')
			x = static_cast<unsigned char>(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z')
			y = static_cast<unsigned char>(y - 'A' + 'a');
		if (x != y || !x)
			return static_cast<int>(x) - static_cast<int>(y);
	}
}

void SetName(char *out, const char *name)
{
	CopyName(name, out, kNameSize);
}

}  // namespace

void CopyName(const char *from, char *out, size_t size)
{
	if (!out || size == 0)
		return;
	size_t n = 0;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(from ? from : ""); *p && n + 1 < size;) {
		if (*p < 0x20 || *p == 0x7f) {  // control characters (a new line in a name) out
			++p;
			continue;
		}
		// a whole UTF-8 letter, or nothing of it
		size_t len = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : (*p & 0xf8) == 0xf0 ? 4 : 1;
		size_t got = 1;
		while (got < len && (p[got] & 0xc0) == 0x80)
			++got;
		if (got < len) {  // a broken letter: skip its first byte
			++p;
			continue;
		}
		if (n + len >= size)
			break;
		memcpy(out + n, p, len);
		n += len;
		p += len;
	}
	out[n] = 0;
}

std::vector<Line> StatusLines(const Status &s, GetText get, const char *ours)
{
	std::vector<Line> out;
	if (s.state == State::kOff)
		return out;
	out.push_back({get("online.title"), Tone::kTitle});
	if (s.state == State::kStarting) {
		out.push_back({get("online.starting"), Tone::kDim});
		return out;
	}
	if (s.state == State::kUnavailable) {
		out.push_back({get(WhyKey(s.why)), Tone::kWarn});
		out.push_back({get("online.off.lan"), Tone::kDim});
		return out;
	}
	if (!s.relay_ok)
		out.push_back({get("online.relay"), Tone::kDim});
	const int n = s.friends_with_us;
	if (n <= 0)
		out.push_back({get("online.friends_none"), Tone::kBody});
	else if (n == 1)
		out.push_back({get("online.friends_one"), Tone::kBody});
	else
		out.push_back({Fill(get, "online.friends", {{"n", std::to_string(n)}}), Tone::kBody});
	const int others = s.friends_playing - s.friends_with_us;
	if (others == 1)
		out.push_back({get("online.others_one"), Tone::kDim});
	else if (others > 1)
		out.push_back({Fill(get, "online.others", {{"n", std::to_string(others)}}), Tone::kDim});
	if (s.hosts > 0)
		out.push_back({get("online.list"), Tone::kDim});
	for (int i = 0; i < s.invite_count && i < Status::kMaxInvites; ++i) {
		const Invite &v = s.invites[i];
		const char *key = v.cannot ? "online.invite_cannot" : v.visible ? "online.invite" : "online.invite_wait";
		out.push_back({Fill(get, key, {{"player", Who(v.name, get)}}), v.cannot ? Tone::kWarn : Tone::kGood});
	}
	for (int i = 0; i < s.problem_count && i < Status::kMaxProblems; ++i) {
		const Problem &p = s.problems[i];
		if (p.check == vlan::HostCheck::kOtherProtocol)
			out.push_back({Fill(get, "online.other_protocol",
			                    {{"player", Who(p.name, get)}, {"version", p.version[0] ? p.version : "?"}, {"ours", ours ? ours : "?"}}),
			               Tone::kWarn});
		else
			out.push_back({Fill(get, "online.other_game", {{"player", Who(p.name, get)}}), Tone::kWarn});
	}
	return out;
}

std::string InviteText(const Invite &invite, GetText get)
{
	return Fill(get, "online.banner", {{"player", Who(invite.name, get)}});
}

bool KeepFriend(int persona)
{
	return persona >= 1 && persona <= 6;
}

void SortFriends(std::vector<Friend> *friends)
{
	std::stable_sort(friends->begin(), friends->end(), [](const Friend &a, const Friend &b) {
		const int ra = Rank(a), rb = Rank(b);
		if (ra != rb)
			return ra < rb;
		return CompareNames(a.name, b.name) < 0;
	});
}

const char *FriendStateKey(const Friend &f)
{
	switch (Rank(f)) {
	case 0: return "online.picker.playing_us";
	case 1: return "online.picker.playing";
	case 2: return "online.picker.online";
	case 3: return "online.picker.busy";
	default: return "online.picker.away";
	}
}

uint64_t ConnectLobby(const wchar_t *command_line)
{
	if (!command_line)
		return 0;
	const wchar_t kWord[] = L"+connect_lobby";
	const size_t n = wcslen(kWord);
	for (const wchar_t *p = wcsstr(command_line, kWord); p; p = wcsstr(p + 1, kWord)) {
		const bool starts = p == command_line || p[-1] == L' ' || p[-1] == L'\t' || p[-1] == L'"';
		const wchar_t *q = p + n;
		if (!starts || (*q != L' ' && *q != L'\t' && *q != L'='))
			continue;
		while (*q == L' ' || *q == L'\t' || *q == L'=' || *q == L'"')
			++q;
		uint64_t id = 0;
		int digits = 0;
		for (; *q >= L'0' && *q <= L'9' && digits < 20; ++q, ++digits) {
			const uint64_t next = id * 10 + static_cast<uint64_t>(*q - L'0');
			if (next / 10 != id)
				return 0;  // too big for 64 bits
			id = next;
		}
		const bool ends = !*q || *q == L' ' || *q == L'\t' || *q == L'"';
		if (digits && ends && id)
			return id;
	}
	return 0;
}

void AddTestFriends(Status *s)
{
	s->test = true;
	s->friends_playing += 3;
	s->friends_with_us += 2;
	if (s->problem_count < Status::kMaxProblems) {
		Problem &p = s->problems[s->problem_count++];
		p = Problem();
		SetName(p.name, "Pierre");
		p.check = vlan::HostCheck::kOtherGame;
		strcpy_s(p.version, sizeof(p.version), "0.3.0");
	}
	if (s->problem_count < Status::kMaxProblems) {
		Problem &p = s->problems[s->problem_count++];
		p = Problem();
		SetName(p.name, "Marie");
		p.check = vlan::HostCheck::kOtherProtocol;
		strcpy_s(p.version, sizeof(p.version), "0.4.0");
	}
}

std::vector<Friend> TestFriends()
{
	const struct {
		const char *name;
		int persona;
		bool playing, with_us;
	} kMade[] = {
		{"Lucas", 1, true, true}, {"Marie", 1, true, false}, {"Pierre", 3, false, false},
		{"Camille", 1, false, false}, {"Antoine", 2, false, false}, {"Sophie", 1, true, true},
	};
	std::vector<Friend> out;
	uint64_t id = 0x0110000100000001ull;
	for (const auto &m : kMade) {
		Friend f = {};
		f.id = id++;
		SetName(f.name, m.name);
		f.persona = m.persona;
		f.playing = m.playing;
		f.with_us = m.with_us;
		out.push_back(f);
	}
	SortFriends(&out);
	return out;
}

Invite TestInvite()
{
	Invite v = {};
	SetName(v.name, "Lucas");
	return v;
}

}  // namespace online

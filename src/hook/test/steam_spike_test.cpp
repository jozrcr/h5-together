// Test of the Steam spike's pure work (src/mods/steam_spike_logic.cpp) and of
// our SteamID <-> network identity helpers (src/mods/steam_api_flat.h): the
// [online] spike setting, the names in the log, the test messages, the
// round-trip statistics. No Steam needed.
#include <stdio.h>
#include <string.h>

#include "../../mods/steam_api_flat.h"
#include "../../mods/steam_spike_logic.h"

using namespace steamspike;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

void TestMode()
{
	Check(ParseMode(L"0") == Mode::kOff, "spike=0 is off");
	Check(ParseMode(L"") == Mode::kOff, "an empty spike= is off");
	Check(ParseMode(nullptr) == Mode::kOff, "no text is off");
	Check(ParseMode(L"1") == Mode::kSolo, "spike=1 is the solo test");
	Check(ParseMode(L"Solo") == Mode::kSolo, "spike=solo too, any case");
	Check(ParseMode(L"HOST") == Mode::kHost, "spike=host");
	Check(ParseMode(L"join") == Mode::kJoin, "spike=join");
	Check(ParseMode(L"yes") == Mode::kOff, "anything else is off");
	Check(strcmp(ModeName(Mode::kJoin), "join") == 0, "mode names");
}

void TestNames()
{
	Check(strcmp(InitResultName(0), "OK") == 0, "init OK");
	Check(strstr(InitResultName(2), "Steam running") != nullptr, "init: no Steam client says to check Steam");
	Check(strcmp(AvailabilityName(100), "OK (current)") == 0, "relay available");
	Check(strcmp(AvailabilityName(-101), "failed") == 0, "relay failed");
	Check(strcmp(AvailabilityName(12345), "other") == 0, "unknown availability");
	Check(strcmp(ConnectionStateName(3), "connected") == 0, "connected");
	Check(strcmp(ConnectionStateName(5), "problem detected locally") == 0, "problem detected locally");
	Check(strcmp(ResultName(1), "OK") == 0, "EResult OK");
	Check(strcmp(ResultName(15), "AccessDenied") == 0, "EResult AccessDenied");
	Check(strcmp(ResultName(999), "other") == 0, "unknown EResult");
	Check(AppIdOfGameId(15370) == 15370, "a plain app's game id is its app id");
	Check(AppIdOfGameId((0x80000000ull << 32) | 0x02000000ull | 15370) == 15370, "the app id is the low 24 bits");
}

void TestPing()
{
	uint8_t buf[512];
	const Ping ping = {Kind::kPing, 0x01020304u, 987654u};
	const size_t n = EncodePing(ping, buf, sizeof(buf), 300);
	Check(n == 300, "a 300-byte message");
	Ping back = {};
	Check(DecodePing(buf, n, &back), "it reads back");
	Check(back.kind == Kind::kPing && back.sequence == 0x01020304u && back.sent_ms == 987654u, "with the same fields");
	Check(EncodePing(ping, buf, sizeof(buf), kHeaderSize) == kHeaderSize && DecodePing(buf, kHeaderSize, &back),
	      "the smallest message has no padding");
	Check(EncodePing(ping, buf, sizeof(buf), kHeaderSize - 1) == 0, "smaller than the header: refused");
	Check(EncodePing(ping, buf, 100, 300) == 0, "bigger than the buffer: refused");
	EncodePing(ping, buf, sizeof(buf), 300);
	buf[200] ^= 1;
	Check(!DecodePing(buf, 300, &back), "a damaged padding byte is caught");
	EncodePing(ping, buf, sizeof(buf), 300);
	buf[0] = 'X';
	Check(!DecodePing(buf, 300, &back), "another program's message is not ours");
	EncodePing(ping, buf, sizeof(buf), 300);
	buf[4] = 9;
	Check(!DecodePing(buf, 300, &back), "an unknown kind is not ours");
	Check(!DecodePing(buf, 5, &back), "too short");
	const Ping echo = {Kind::kEcho, 7, 8};
	EncodePing(echo, buf, sizeof(buf), 64);
	Check(DecodePing(buf, 64, &back) && back.kind == Kind::kEcho, "an echo");
}

void TestRtt()
{
	RttStats stats;
	Check(stats.Count() == 0 && stats.Min() == 0 && stats.Median() == 0 && stats.Max() == 0, "empty statistics");
	const uint32_t values[] = {40, 10, 30, 20, 50};
	for (uint32_t v : values)
		stats.Add(v);
	Check(stats.Count() == 5 && stats.Min() == 10 && stats.Max() == 50 && stats.Median() == 30, "min, median, max");
	for (int i = 0; i < 2000; ++i)
		stats.Add(1);
	Check(stats.Count() == 1024, "at most 1024 values kept");
}

void TestIdentity()
{
	const steamflat::SteamId id = 76561197960287930ull;  // Valve's example SteamID
	const steamflat::Identity identity = steamflat::IdentityOf(id);
	Check(identity.type == steamflat::kIdentitySteamId && identity.size == 8, "a SteamID identity");
	Check(steamflat::SteamIdOf(identity) == id, "and back");
	steamflat::Identity other = identity;
	other.type = 2;
	Check(steamflat::SteamIdOf(other) == 0, "another kind of identity has no SteamID");
	steamflat::FriendGameInfo info = {};
	for (int i = 0; i < 8; ++i)
		info.lobby[i] = static_cast<uint8_t>(id >> (8 * i));
	Check(steamflat::LobbyOf(info) == id, "a friend's lobby id");
	Check(sizeof(steamflat::Identity) == 136 && sizeof(steamflat::Message) == 208 &&
	          sizeof(steamflat::ConnectionInfo) == 696 && sizeof(steamflat::FriendGameInfo) == 24,
	      "the structures' sizes on 32-bit Windows (steam_abi_check.cpp compares them with the SDK)");
}

}  // namespace

int main()
{
	TestMode();
	TestNames();
	TestPing();
	TestRtt();
	TestIdentity();
	printf("%s\n", g_failures ? "SOME TESTS FAILED" : "all Steam spike tests passed");
	return g_failures ? 1 : 0;
}

// Test of the virtual LAN's pure work (src/mods/virtual_lan_logic.cpp): the
// [online] mode setting, fake addresses from ids, who is behind each address,
// the 2-byte header, where each packet the game sends goes, hosting detection,
// the known hosts, the query copies, the loopback transport's datagrams. No
// network, no game.
#include <stdio.h>
#include <string.h>

#include "../../mods/virtual_lan_logic.h"

using namespace vlan;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

uint32_t Ip(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return a << 24 | b << 16 | c << 8 | d;
}

bool ValidFake(uint32_t ip)
{
	const uint32_t last = ip & 0xffu;
	return IsFake(ip) && last >= 1 && last <= 254 && ip != kProbeAddress && !IsBroadcast(ip);
}

void TestSettings()
{
	Check(ParseMode(L"off") == Mode::kOff, "mode=off");
	Check(ParseMode(L"") == Mode::kOff, "an empty mode= is off");
	Check(ParseMode(nullptr) == Mode::kOff, "no text is off");
	Check(ParseMode(L"loopback") == Mode::kLoopback, "mode=loopback");
	Check(ParseMode(L" LoopBack ") == Mode::kLoopback, "any case, spaces around");
	Check(ParseMode(L"steam") == Mode::kSteam, "mode=steam");
	Check(ParseMode(L"1") == Mode::kOff && ParseMode(L"lan") == Mode::kOff, "anything else is off");
	Check(strcmp(ModeName(Mode::kLoopback), "loopback") == 0, "mode names");

	uint16_t ports[8];
	Check(ParsePorts(L"47101, 47102", ports, 8) == 2 && ports[0] == 47101 && ports[1] == 47102, "two ports");
	Check(ParsePorts(L"", ports, 8) == 0 && ParsePorts(nullptr, ports, 8) == 0, "no ports");
	Check(ParsePorts(L"0,70000,5;x9", ports, 8) == 2 && ports[0] == 5 && ports[1] == 9, "0 and too big are skipped");
	Check(ParsePorts(L"1 2 3", ports, 2) == 2, "at most max");
}

void TestAddresses()
{
	Check(IsFake(Ip(198, 18, 0, 0)) && IsFake(Ip(198, 19, 255, 255)), "198.18.0.0/15 is fake");
	Check(!IsFake(Ip(198, 17, 255, 255)) && !IsFake(Ip(198, 20, 0, 0)) && !IsFake(Ip(192, 168, 1, 22)),
	      "the addresses around it are not");
	Check(IsBroadcast(0xffffffffu) && IsBroadcast(Ip(198, 18, 7, 255)) && IsBroadcast(Ip(10, 255, 255, 255)),
	      "broadcasts: 255.255.255.255 and .255");
	Check(!IsBroadcast(Ip(198, 18, 7, 21)), "a fake address is no broadcast");

	bool all_valid = true;
	int same_as_probe = 0;
	for (uint64_t id = 0; id < 200000; ++id) {
		const uint32_t ip = FakeAddressFor(0x0110000100000000ull + id * 7919);
		all_valid = all_valid && ValidFake(ip);
		same_as_probe += ip == kProbeAddress;
	}
	Check(all_valid && same_as_probe == 0, "200,000 ids: every address in 198.18.0.0/15, last byte 1 to 254, never 198.18.0.1");
	Check(FakeAddressFor(76561197960287930ull) == FakeAddressFor(76561197960287930ull), "the same id, the same address");
	Check(FakeAddressFor(76561197960287930ull) != FakeAddressFor(76561197960287931ull), "neighbouring ids differ");
	int in_19 = 0;
	for (uint64_t id = 1; id <= 1000; ++id)
		in_19 += (FakeAddressFor(id) >> 16) == 0xc613u;
	Check(in_19 > 400 && in_19 < 600, "addresses use 198.18.x and 198.19.x about equally");

	Check(NextFakeAddress(Ip(198, 18, 3, 7)) == Ip(198, 18, 3, 8), "next address");
	Check(NextFakeAddress(Ip(198, 18, 3, 254)) == Ip(198, 18, 4, 1), "next address skips .255 and .0");
	Check(NextFakeAddress(Ip(198, 19, 255, 254)) == Ip(198, 18, 0, 2), "next address wraps around and skips 198.18.0.1");

	char text[20];
	FormatIp(Ip(198, 18, 7, 21), text, sizeof(text));
	Check(strcmp(text, "198.18.7.21") == 0, "address as text");
}

// An id whose address is the same as `id`'s (found by trying).
uint64_t Clash(uint64_t id)
{
	const uint32_t wanted = FakeAddressFor(id);
	for (uint64_t other = id + 1; other < id + 50000000; ++other)
		if (FakeAddressFor(other) == wanted)
			return other;
	return 0;
}

void TestPeers()
{
	PeerTable a;
	a.Reset(LoopbackId(47101));
	Check(a.Count() == 1 && a.SelfId() == LoopbackId(47101) && a.SelfAddress() == FakeAddressFor(LoopbackId(47101)),
	      "ourselves first, at our hash address");
	bool added = false;
	const uint32_t b_ip = a.AddressOf(LoopbackId(47102), &added);
	Check(added && b_ip == FakeAddressFor(LoopbackId(47102)), "a new peer gets its hash address");
	Check(a.AddressOf(LoopbackId(47102), &added) == b_ip && !added, "the second time: the same address, not added again");
	uint64_t id = 0;
	Check(a.IdOf(b_ip, &id) && id == LoopbackId(47102), "who is behind an address");
	Check(!a.IdOf(Ip(198, 18, 0, 1), &id), "nobody is behind the probe address");

	// The robot's two copies: each sees the other at the address the other has for itself.
	PeerTable b;
	b.Reset(LoopbackId(47102));
	Check(b.SelfAddress() == b_ip && b.AddressOf(LoopbackId(47101)) == a.SelfAddress(),
	      "two PCs agree on both addresses (robot ports 47101, 47102)");
	// Three PCs that hear of each other in different orders still agree (no clash).
	PeerTable h, c1, c2;
	h.Reset(LoopbackId(47101));
	c1.Reset(LoopbackId(47102));
	c2.Reset(LoopbackId(47103));
	h.AddressOf(LoopbackId(47103));
	h.AddressOf(LoopbackId(47102));
	c1.AddressOf(LoopbackId(47101));
	c1.AddressOf(LoopbackId(47103));
	c2.AddressOf(LoopbackId(47102));
	c2.AddressOf(LoopbackId(47101));
	Check(h.AddressOf(LoopbackId(47102)) == c1.SelfAddress() && c2.AddressOf(LoopbackId(47102)) == c1.SelfAddress() &&
	          c1.AddressOf(LoopbackId(47103)) == c2.SelfAddress() && h.AddressOf(LoopbackId(47103)) == c2.SelfAddress() &&
	          c1.AddressOf(LoopbackId(47101)) == h.SelfAddress() && c2.AddressOf(LoopbackId(47101)) == h.SelfAddress(),
	      "three PCs, met in different orders, agree on every address");
	bool distinct = true;
	for (uint16_t p = 47100; p < 47120; ++p)
		for (uint16_t q = p + 1; q < 47120; ++q)
			distinct = distinct && FakeAddressFor(LoopbackId(p)) != FakeAddressFor(LoopbackId(q));
	Check(distinct, "loopback ports 47100 to 47119 all get different addresses");

	const uint64_t self = 76561197960287930ull;
	const uint64_t twin = Clash(self);
	Check(twin != 0, "found an id with the same hash address (test setup)");
	PeerTable c;
	c.Reset(self);
	const uint32_t moved = c.AddressOf(twin);
	Check(moved == NextFakeAddress(c.SelfAddress()), "a clash: the later one moves to the next free address");
	Check(c.IdOf(c.SelfAddress(), &id) && id == self && c.IdOf(moved, &id) && id == twin, "both are found again");

	PeerTable full;
	full.Reset(1);
	for (uint64_t i = 2; i <= PeerTable::kMax; ++i)
		full.AddressOf(i * 1000003);
	Check(full.Count() == PeerTable::kMax && full.AddressOf(99999999999ull) == 0, "a full table gives no address");
	PeerTable empty;
	Check(empty.AddressOf(5) == 0, "no address before Reset");
}

// Declared addresses: what a peer says its own game uses beats what we computed.
void TestDeclared()
{
	const uint64_t self = 76561197960287930ull;
	const uint64_t twin = Clash(self);  // the same hash address as ours
	const uint64_t other = 76561197960287999ull;
	bool added = false;
	PeerTable::Learned what = PeerTable::Learned::kSame;

	PeerTable t;
	t.Reset(self);
	const uint32_t mine = t.SelfAddress();
	const uint32_t wanted = Ip(198, 19, 3, 4);
	Check(t.Learn(other, wanted, &what) == wanted && what == PeerTable::Learned::kAdded, "a new peer takes the address it says");
	Check(t.Learn(other, wanted, &what) == wanted && what == PeerTable::Learned::kSame, "the same again: nothing new");
	Check(t.Learn(other, Ip(198, 19, 3, 5), &what) == Ip(198, 19, 3, 5) && what == PeerTable::Learned::kChanged,
	      "a peer that moved (before its game told anybody): its new address");
	uint64_t id = 0;
	Check(!t.IdOf(wanted, &id) && t.IdOf(Ip(198, 19, 3, 5), &id) && id == other, "its old address is free again");
	Check(t.Learn(other, 0, &what) == Ip(198, 19, 3, 5) && what == PeerTable::Learned::kSame,
	      "a message without an address: what we know");
	Check(t.Learn(other, Ip(198, 18, 7, 255), &what) == Ip(198, 19, 3, 5) && what == PeerTable::Learned::kSame,
	      "a broadcast address is never a player's: ignored");
	Check(t.Learn(self, Ip(198, 18, 9, 9), &what) == mine && what == PeerTable::Learned::kSame,
	      "our own messages never move us");
	const uint64_t fresh = 76561197960288001ull;
	Check(t.Learn(fresh, 0, &what) == t.AddressOf(fresh) && what == PeerTable::Learned::kComputed,
	      "no address said: a computed one");

	// a computed peer gives way to one that says it uses that address
	PeerTable c;
	c.Reset(self);
	const uint32_t computed = c.AddressOf(other, &added);
	const uint64_t newcomer = 76561197960288123ull;
	Check(c.Learn(newcomer, computed, &what) == computed && what == PeerTable::Learned::kOtherMoved,
	      "a declared address beats a computed one");
	Check(c.AddressOf(other) == NextFakeAddress(computed) && c.IdOf(computed, &id) && id == newcomer,
	      "the computed peer moved to the next address");

	// two declared: the second one is a clash, it gets a computed address
	Check(c.Learn(other, computed, &what) == NextFakeAddress(computed) && what == PeerTable::Learned::kClash,
	      "an address another peer said first: a clash, the known peer keeps its address");

	// a peer says our address while our game has told nobody: we move
	PeerTable s;
	s.Reset(self);
	Check(!s.SelfLocked(), "not locked at first");
	Check(s.Learn(twin, mine, &what) == mine && what == PeerTable::Learned::kSelfMoved, "a peer with our address: it gets it");
	Check(s.SelfAddress() == NextFakeAddress(mine) && s.IdOf(mine, &id) && id == twin, "and we moved to the next address");
	// ... after our game told it: the peer gets another one
	PeerTable l;
	l.Reset(self);
	l.LockSelf();
	Check(l.Learn(twin, mine, &what) == NextFakeAddress(mine) && what == PeerTable::Learned::kClash && l.SelfAddress() == mine,
	      "locked: we keep ours, the peer is a clash at a computed address");

	// moving off the addresses of a game we may join
	PeerTable a;
	a.Reset(self);
	const uint32_t taken[3] = {Ip(198, 18, 1, 1), mine, NextFakeAddress(mine)};
	Check(a.AvoidForSelf(taken, 3) && a.SelfAddress() == NextFakeAddress(NextFakeAddress(mine)),
	      "our address moves past every taken one");
	const uint32_t before = a.SelfAddress();
	Check(!a.AvoidForSelf(taken, 3) && a.SelfAddress() == before, "then it stays");
	a.LockSelf();
	const uint32_t now_taken[1] = {before};
	Check(!a.AvoidForSelf(now_taken, 1) && a.SelfAddress() == before, "once locked it never moves");
	Check(!a.AvoidForSelf(nullptr, 0), "nothing taken: no move");

	PeerTable none;
	Check(none.Learn(other, wanted, &what) == 0 && what == PeerTable::Learned::kFull, "no address before Reset");
	PeerTable full;
	full.Reset(1);
	for (uint64_t i = 2; i <= PeerTable::kMax; ++i)
		full.AddressOf(i * 1000003);
	Check(full.Learn(5555555, Ip(198, 19, 200, 1), &what) == 0 && what == PeerTable::Learned::kFull,
	      "a full table takes no declared peer");
	Check(strstr(LearnedName(PeerTable::Learned::kClash), "CLASH") != nullptr, "the log names a clash");

	// the robot's case (no clash): a computed peer, then its first message, which says the same address
	PeerTable r;
	r.Reset(LoopbackId(47101));
	const uint32_t peer = r.AddressOf(LoopbackId(47102));
	Check(r.Learn(LoopbackId(47102), FakeAddressFor(LoopbackId(47102)), &what) == peer && what == PeerTable::Learned::kSame,
	      "the loopback's peers say their computed address: nothing changes");
}

void TestMessages()
{
	uint8_t packet[kMaxPacket + 1];
	for (size_t i = 0; i < sizeof(packet); ++i)
		packet[i] = static_cast<uint8_t>(i * 13);
	uint8_t message[kMaxMessage + 8];
	const uint32_t sender = Ip(198, 18, 7, 21);
	size_t n = EncodeMessage(8889, sender, packet, 826, message, sizeof(message));
	Check(n == 832 && message[0] == 0x22 && message[1] == 0xb9, "the source port first, big endian (8889 = 22 b9)");
	Check(message[2] == 198 && message[3] == 18 && message[4] == 7 && message[5] == 21, "then the sender's address");
	uint16_t port = 0;
	uint32_t from = 0;
	const uint8_t *body = nullptr;
	size_t size = 0;
	Check(DecodeMessage(message, n, &port, &from, &body, &size) && port == 8889 && from == sender && size == 826 &&
	          memcmp(body, packet, 826) == 0,
	      "it reads back");
	Check(EncodeMessage(1, sender, packet, kMaxPacket, message, sizeof(message)) == kMaxMessage, "a 2048-byte packet fits");
	Check(EncodeMessage(1, sender, packet, kMaxPacket + 1, message, sizeof(message)) == 0, "a bigger one does not");
	Check(EncodeMessage(1, sender, packet, 100, message, 50) == 0, "nor one bigger than the buffer");
	Check(EncodeMessage(7, sender, nullptr, 0, message, sizeof(message)) == kHeaderSize, "an empty packet is a bare header");
	Check(!DecodeMessage(message, kHeaderSize - 1, &port, &from, &body, &size), "5 bytes are no message");
	Check(kHeaderSize + 984 < 1200, "the game's biggest packet (984) fits in one Steam packet (about 1,200 bytes)");
}

void TestRoutes()
{
	const uint8_t query[5] = {0x02, 0x3a, 0x0a, 0x32, 0x3c};
	const uint8_t info[826] = {0x03};
	const uint8_t data[40] = {0x00};
	const uint32_t self = Ip(198, 18, 7, 21);
	Check(RouteOf(kProbeAddress, self, info, sizeof(info)) == Route::kProbeAnswer, "to the probe address: our probe's answer");
	Check(RouteOf(0xffffffffu, self, query, 5) == Route::kQueryFanOut, "a query to 255.255.255.255: to the known hosts");
	Check(RouteOf(Ip(198, 18, 7, 255), self, query, 5) == Route::kQueryFanOut,
	      "a query to the game's classful broadcast 198.18.7.255: to the known hosts");
	Check(RouteOf(Ip(192, 168, 1, 255), self, query, 5) == Route::kDropBroadcast,
	      "a query to a real adapter's broadcast (lan_discovery's copy): dropped");
	Check(RouteOf(Ip(198, 18, 7, 255), self, data, sizeof(data)) == Route::kDropBroadcast, "another broadcast: dropped");
	Check(RouteOf(self, self, data, sizeof(data)) == Route::kToSelf, "to our own fake address: back to us");
	Check(RouteOf(Ip(198, 19, 40, 3), self, data, sizeof(data)) == Route::kToPeer, "to another fake address: a peer");
	Check(RouteOf(Ip(192, 168, 1, 22), self, data, sizeof(data)) == Route::kDropReal, "to a real address: dropped");
	Check(RouteOf(Ip(127, 0, 0, 1), self, data, sizeof(data)) == Route::kDropReal, "to 127.0.0.1: dropped");
	Check(IsQuery(query, 5) && !IsQuery(query, 4) && !IsQuery(data, 5), "what a query is");
	Check(IsGameInfo(info, sizeof(info)) && !IsGameInfo(query, 5) && !IsGameInfo(info, 1), "what a game description is");
	Check(strcmp(RouteName(Route::kDropReal), "real address, dropped") == 0, "route names");
}

void TestHosting()
{
	HostingDetector h;
	Check(!h.ProbeDue(1000), "no probe before the game sent a query (we need its bytes)");
	const uint8_t query[5] = {0x02, 1, 2, 3, 4};
	h.OnQuerySent(query, 5);
	Check(h.HaveQuery() && h.QuerySize() == 5 && memcmp(h.Query(), query, 5) == 0, "the game's query is kept");
	Check(h.ProbeDue(1000), "then a probe is due");
	h.OnProbeSent(1000);
	Check(!h.ProbeDue(1500) && h.ProbeDue(2000), "then once a second");
	Check(!h.Hosting(1500), "no answer: not hosting");
	h.OnGameInfoSent(2010);
	Check(h.Hosting(2010) && h.Hosting(5000), "an answer: hosting for 3.5 s");
	Check(!h.Hosting(5510), "no answer for 3.5 s: not hosting any more");
	HostingDetector w;
	w.OnQuerySent(query, 5);
	w.OnProbeSent(0xfffffe00u);
	w.OnGameInfoSent(0xffffff00u);
	Check(w.Hosting(0x00000100u) && !w.ProbeDue(0x00000100u) && w.ProbeDue(0x00000300u), "GetTickCount wrapping around");
	const uint8_t data[5] = {0x00, 1, 2, 3, 4};
	HostingDetector x;
	x.OnQuerySent(data, 5);
	Check(!x.HaveQuery(), "only a real query is kept");
}

void TestKnownHosts()
{
	KnownHosts k;
	Check(k.Update(LoopbackId(47101), 8889, 100), "a new host");
	Check(!k.Update(LoopbackId(47101), 8893, 900) && k.Count() == 1 && k.At(0).discovery_port == 8893, "refreshed, new port");
	Check(k.Update(LoopbackId(47103), 8889, 1000) && k.Count() == 2, "a second host");
	Check(!k.Update(LoopbackId(47103), 0, 1100) && k.Count() == 1, "port 0: it no longer hosts");
	Check(!k.Update(LoopbackId(47105), 0, 1100) && k.Count() == 1, "port 0 from a stranger adds nothing");
	k.Expire(5800);
	Check(k.Count() == 1, "still known 4.9 s after its last word");
	k.Expire(5900);
	Check(k.Count() == 0, "forgotten 5 s after");
	KnownHosts full;
	for (int i = 0; i < KnownHosts::kMax; ++i)
		full.Update(static_cast<uint64_t>(i) + 1, 8889, 0);
	Check(!full.Update(999, 8889, 0) && full.Count() == KnownHosts::kMax, "a full list takes no more");
	KnownHosts a;
	a.Update(7, 8889, 0, Ip(198, 18, 4, 4));
	int at = -1;
	Check(a.Has(7, &at) && at == 0 && a.At(0).address == Ip(198, 18, 4, 4) && !a.Has(8), "a host's address, and who is one");
	a.Update(7, 8889, 100);
	Check(a.At(0).address == Ip(198, 18, 4, 4), "a refresh without an address keeps it");
}

void TestTexts()
{
	Check(ParseIp("198.18.7.21") == Ip(198, 18, 7, 21) && ParseIp("0.0.0.0") == 0 && ParseIp("255.255.255.255") == 0xffffffffu,
	      "addresses from text");
	Check(!ParseIp("198.18.7") && !ParseIp("198.18.7.256") && !ParseIp("198.18.7.21x") && !ParseIp("a.b.c.d") &&
	          !ParseIp("") && !ParseIp(nullptr) && !ParseIp("1988.1.1.1"),
	      "not addresses");
	uint32_t list[4];
	Check(ParseAddressList("198.18.7.21 198.19.0.4,x 10.0.0.1", list, 4) == 3 && list[1] == Ip(198, 19, 0, 4) &&
	          list[2] == Ip(10, 0, 0, 1),
	      "a list of addresses");
	Check(ParseAddressList("1.1.1.1 2.2.2.2 3.3.3.3", list, 2) == 2 && ParseAddressList(nullptr, list, 4) == 0,
	      "at most max; none");
	char text[64];
	const uint32_t two[2] = {Ip(198, 18, 7, 21), Ip(198, 19, 0, 4)};
	FormatAddressList(two, 2, text, sizeof(text));
	Check(strcmp(text, "198.18.7.21 198.19.0.4") == 0, "a list as text");
	FormatAddressList(two, 2, text, 14);
	Check(strcmp(text, "198.18.7.21") == 0, "a list cut where it does not fit, never half an address");
	Check(ParseId("76561197960287930") == 76561197960287930ull && ParseId("18446744073709551615") == 0xffffffffffffffffull,
	      "SteamIDs from text");
	Check(!ParseId("18446744073709551616") && !ParseId("12a") && !ParseId("") && !ParseId(nullptr) && !ParseId("-1"),
	      "not SteamIDs");
	char a[12], b[12];
	FormatShortId(76561197960287930ull, a, sizeof(a));
	FormatShortId(76561197960287931ull, b, sizeof(b));
	Check(strlen(a) == 5 && a[0] == '#' && strcmp(a, b) != 0, "short ids for the log: #abcd, neighbours differ");
	Check(strstr(a, "7656") == nullptr, "a short id is not a piece of the SteamID");
	const uint8_t bytes[3] = {'a', 'b', 'c'};
	Check(HashBytes(bytes, 3) == 0x1a47e90bu && HashBytes(bytes, 0) == 0x811c9dc5u, "FNV-1a of \"abc\" and of nothing");
	FormatGame("mmh55", true, 0x0a1b2c3du, text, sizeof(text));
	Check(strcmp(text, "mmh55/0a1b2c3d") == 0, "the game's name");
	FormatGame("vanilla", false, 0, text, sizeof(text));
	Check(strcmp(text, "vanilla/no-55u") == 0, "the game's name without MMH5.5");
}

void TestLobbyInfo()
{
	const uint64_t host = 76561197960287930ull;
	LobbyInfo i = ParseLobbyInfo("1", "0.3.0", "mmh55/0a1b2c3d", "76561197960287930", "8889", "198.18.7.21",
	                             "198.18.7.21 198.19.0.4");
	Check(i.protocol == 1 && strcmp(i.version, "0.3.0") == 0 && strcmp(i.game, "mmh55/0a1b2c3d") == 0 && i.host == host &&
	          i.port == 8889 && i.address == Ip(198, 18, 7, 21) && i.taken_count == 2 && i.taken[1] == Ip(198, 19, 0, 4),
	      "a lobby's data");
	Check(CheckHost(i, host, "mmh55/0a1b2c3d") == HostCheck::kOk, "the friend hosts the same game: a host");
	Check(CheckHost(i, host + 1, "mmh55/0a1b2c3d") == HostCheck::kNotTheFriend, "the friend is in somebody else's lobby");
	Check(CheckHost(i, host, "mmh55/ffffffff") == HostCheck::kOtherGame, "another MMH5.5: left out");
	LobbyInfo old = ParseLobbyInfo("2", "0.4.0", "mmh55/0a1b2c3d", "76561197960287930", "8889", "198.18.7.21", "");
	Check(CheckHost(old, host, "mmh55/0a1b2c3d") == HostCheck::kOtherProtocol, "another protocol: left out");
	LobbyInfo stopped = ParseLobbyInfo("1", "0.3.0", "mmh55/0a1b2c3d", "76561197960287930", "0", "198.18.7.21", "");
	Check(CheckHost(stopped, host, "mmh55/0a1b2c3d") == HostCheck::kNotHosting, "port 0: no longer hosts");
	LobbyInfo empty = ParseLobbyInfo(nullptr, "", nullptr, nullptr, nullptr, nullptr, nullptr);
	Check(empty.protocol == 0 && CheckHost(empty, host, "mmh55/0a1b2c3d") == HostCheck::kNoData, "no data (yet)");
	LobbyInfo odd = ParseLobbyInfo("1", "0.3.0", "g", "76561197960287930", "70000", "198.18.7.255", "");
	Check(odd.port == 0 && odd.address == 0, "a port above 65535 and a broadcast address are not taken");
	char long_version[64];
	memset(long_version, 'v', sizeof(long_version) - 1);
	long_version[sizeof(long_version) - 1] = 0;
	LobbyInfo cut = ParseLobbyInfo("1", long_version, "g", "1", "1", "", "");
	Check(strlen(cut.version) == sizeof(cut.version) - 1, "a long text is cut, never overflows");
	Check(strcmp(HostCheckName(HostCheck::kOtherGame), "another game build or MMH5.5 version") == 0, "check names");
}

void TestLobbyPlan()
{
	uint64_t lobby = 0;
	LobbyPlan p;
	Check(p.Next(0, &lobby) == LobbyPlan::Action::kNone && p.Where() == LobbyPlan::In::kNone, "nothing to do at first");
	// hosting: create, wait for Steam, keep, leave a minute after
	p.SetHosting(8889, 1000);
	Check(p.Next(1000, &lobby) == LobbyPlan::Action::kCreate, "our game hosts: make a lobby");
	Check(p.Next(1020, &lobby) == LobbyPlan::Action::kNone && p.Waiting(), "then wait for Steam's answer");
	p.Created(0x1234);
	Check(p.Where() == LobbyPlan::In::kOwn && p.Lobby() == 0x1234 && p.Next(1100, &lobby) == LobbyPlan::Action::kNone,
	      "in our lobby");
	p.SetHosting(0, 5000);
	Check(p.Next(5000 + LobbyPlan::kKeepMs - 1, &lobby) == LobbyPlan::Action::kNone, "kept a minute after the game stopped hosting");
	p.SetHosting(8889, 30000);
	p.SetHosting(0, 31000);
	Check(p.Next(31000 + LobbyPlan::kKeepMs - 1, &lobby) == LobbyPlan::Action::kNone, "hosting again restarts the minute");
	Check(p.Next(31000 + LobbyPlan::kKeepMs, &lobby) == LobbyPlan::Action::kLeave, "then left");
	p.Left();
	Check(p.Where() == LobbyPlan::In::kNone && p.Next(100000, &lobby) == LobbyPlan::Action::kNone, "and nothing more");

	// Steam refuses: try again after kRetryMs
	LobbyPlan f;
	f.SetHosting(8889, 0);
	Check(f.Next(0, &lobby) == LobbyPlan::Action::kCreate, "make a lobby");
	f.Failed(10);
	Check(f.Next(10 + LobbyPlan::kRetryMs - 1, &lobby) == LobbyPlan::Action::kNone, "refused: wait");
	Check(f.Next(10 + LobbyPlan::kRetryMs, &lobby) == LobbyPlan::Action::kCreate, "then try again");

	// playing with a host: join its lobby, keep it while packets go either way
	LobbyPlan j;
	j.PlaysWith(77, 0x5555, 1000);
	Check(j.Next(1000, &lobby) == LobbyPlan::Action::kJoin && lobby == 0x5555, "our game plays with a host: join its lobby");
	j.Joined(0x5555);
	Check(j.Where() == LobbyPlan::In::kJoined && j.Host() == 77, "in the host's lobby");
	j.HeardFrom(77, 50000);
	Check(j.Next(50000 + LobbyPlan::kKeepMs - 1, &lobby) == LobbyPlan::Action::kNone, "kept while the host's packets come");
	j.HeardFrom(78, 100000);
	Check(j.Next(50000 + LobbyPlan::kKeepMs, &lobby) == LobbyPlan::Action::kLeave,
	      "a minute without a packet to or from the host (others do not count): left");
	j.Left();
	// another host: leave the first one's lobby, join the second's
	LobbyPlan k;
	k.PlaysWith(77, 0x5555, 0);
	k.Next(0, &lobby);
	k.Joined(0x5555);
	k.PlaysWith(88, 0x6666, 100);
	Check(k.Next(100, &lobby) == LobbyPlan::Action::kLeave, "another host: leave the old lobby");
	k.Left();
	Check(k.Next(120, &lobby) == LobbyPlan::Action::kJoin && lobby == 0x6666, "then join the new host's");
	k.Joined(0x6666);
	// our game hosts while we are in a host's lobby: ours wins
	k.SetHosting(8889, 200);
	Check(k.Next(200, &lobby) == LobbyPlan::Action::kLeave, "our game hosts now: leave the host's lobby");
	k.Left();
	Check(k.Next(220, &lobby) == LobbyPlan::Action::kCreate, "and make ours");
	// the host left its lobby: no going back
	LobbyPlan g;
	g.PlaysWith(77, 0x5555, 0);
	g.Next(0, &lobby);
	g.Joined(0x5555);
	g.Left();
	g.Forget();
	Check(g.Next(100, &lobby) == LobbyPlan::Action::kNone, "the host left: we do not join its lobby again");
	g.PlaysWith(77, 0x5555, 200);
	Check(g.Next(200, &lobby) == LobbyPlan::Action::kNone,
	      "not even when our game still sends to that host (Steam answered 2, no such lobby, every 5 s: two-PC test 3)");
	g.PlaysWith(77, 0x7777, 300);
	Check(g.Next(300, &lobby) == LobbyPlan::Action::kJoin && lobby == 0x7777, "a new lobby of that host: join it");
	// no lobby known for the host yet
	LobbyPlan u;
	u.PlaysWith(77, 0, 0);
	Check(u.Next(0, &lobby) == LobbyPlan::Action::kNone, "a host without a known lobby: nothing to join");
	u.PlaysWith(77, 0x5555, 10);
	Check(u.Next(10, &lobby) == LobbyPlan::Action::kJoin && lobby == 0x5555, "its lobby known later: joined then");
	// GetTickCount wrapping around
	LobbyPlan w;
	w.SetHosting(8889, 0xfffff000u);
	w.Next(0xfffff000u, &lobby);
	w.Created(1);
	w.SetHosting(0, 0xfffff000u);
	Check(w.Next(0x00001000u, &lobby) == LobbyPlan::Action::kNone && w.Next(0xfffff000u + LobbyPlan::kKeepMs, &lobby) ==
	                                                                     LobbyPlan::Action::kLeave,
	      "GetTickCount wrapping around");
}

void TestQueryCopies()
{
	QueryDedup d;
	const uint8_t q1[5] = {0x02, 1, 2, 3, 4};
	const uint8_t q2[5] = {0x02, 1, 2, 3, 5};
	Check(d.First(q1, 5, 1000), "the first copy goes");
	Check(!d.First(q1, 5, 1001), "the game's second broadcast of the same query does not");
	Check(d.First(q2, 5, 1002), "the next query goes");
	Check(d.First(q2, 5, 1300), "the same bytes later go again");
}

void TestLoopback()
{
	const uint8_t message[4] = {0x22, 0xb9, 0x03, 0x01};
	uint8_t buf[64];
	const size_t n = EncodeLoopback(LoopbackKind::kData, LoopbackId(47101), 8891, message, 4, buf, sizeof(buf));
	Check(n == kLoopbackHeaderSize + 4 && memcmp(buf, "H5VL", 4) == 0, "a loopback datagram");
	LoopbackDatagram d = {};
	Check(DecodeLoopback(buf, n, &d) && d.kind == LoopbackKind::kData && d.from == LoopbackId(47101) && d.channel == 8891 &&
	          d.payload_size == 4 && memcmp(d.payload, message, 4) == 0,
	      "it reads back");
	buf[4] = 9;
	Check(!DecodeLoopback(buf, n, &d), "another version is refused");
	buf[4] = kLoopbackVersion;
	buf[5] = 7;
	Check(!DecodeLoopback(buf, n, &d), "an unknown kind is refused");
	buf[0] = 'X';
	Check(!DecodeLoopback(buf, n, &d), "a datagram that is not ours is refused");
	Check(!DecodeLoopback(buf, 5, &d), "too short");
	Check(EncodeLoopback(LoopbackKind::kAnnounce, 1, 0, message, 4, buf, 10) == 0, "too small a buffer");
	Check(LoopbackPort(LoopbackId(47101)) == 47101, "a loopback id gives its port");
	Check(LoopbackPort(76561197960287930ull) == 0, "a SteamID is no loopback id");
	Check((LoopbackId(1) >> 52) == 0x011, "loopback ids look like SteamIDs of people");
}

}  // namespace

int main()
{
	TestSettings();
	TestAddresses();
	TestPeers();
	TestDeclared();
	TestMessages();
	TestRoutes();
	TestHosting();
	TestKnownHosts();
	TestQueryCopies();
	TestLoopback();
	TestTexts();
	TestLobbyInfo();
	TestLobbyPlan();
	printf("%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
	return g_failures ? 1 : 0;
}

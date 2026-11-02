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

void TestMessages()
{
	uint8_t packet[kMaxPacket + 1];
	for (size_t i = 0; i < sizeof(packet); ++i)
		packet[i] = static_cast<uint8_t>(i * 13);
	uint8_t message[kMaxMessage + 8];
	size_t n = EncodeMessage(8889, packet, 826, message, sizeof(message));
	Check(n == 828 && message[0] == 0x22 && message[1] == 0xb9, "the source port first, big endian (8889 = 22 b9)");
	uint16_t port = 0;
	const uint8_t *body = nullptr;
	size_t size = 0;
	Check(DecodeMessage(message, n, &port, &body, &size) && port == 8889 && size == 826 && memcmp(body, packet, 826) == 0,
	      "it reads back");
	Check(EncodeMessage(1, packet, kMaxPacket, message, sizeof(message)) == kMaxMessage, "a 2048-byte packet fits");
	Check(EncodeMessage(1, packet, kMaxPacket + 1, message, sizeof(message)) == 0, "a bigger one does not");
	Check(EncodeMessage(1, packet, 100, message, 50) == 0, "nor one bigger than the buffer");
	Check(EncodeMessage(7, nullptr, 0, message, sizeof(message)) == 2, "an empty packet is a bare header");
	Check(!DecodeMessage(message, 1, &port, &body, &size), "one byte is no message");
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
	TestMessages();
	TestRoutes();
	TestHosting();
	TestKnownHosts();
	TestQueryCopies();
	TestLoopback();
	printf("%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
	return g_failures ? 1 : 0;
}

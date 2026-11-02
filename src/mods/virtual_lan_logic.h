// The virtual LAN's pure work (no Winsock, no Steam), unit-tested by
// src/hook/test/virtual_lan_test.cpp. The hooks that use it are in
// virtual_lan.cpp; the design is docs/plans/online-steam.md, "The design".
//
// In plain words: every player gets a made-up ("fake") IPv4 address in
// 198.18.0.0/15, a range set aside for network tests, computed from the
// player's id (a SteamID; the loopback test makes ids up). The game only ever
// sees those addresses. When it sends a packet to one, we look up whose it is
// and hand the packet to a transport (Steam later; for the robot a local UDP
// socket), with the game's destination port as the "channel" and a 2-byte
// header holding the source port. The other PC gives the packet to its game as
// if it came from the sender's fake address and that port.
//
// Addresses here are in host byte order: 198.18.0.1 is 0xc6120001.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vlan {

// --- the [online] mode setting -------------------------------------------------
enum class Mode { kOff, kLoopback, kSteam };
// [online] mode=off|loopback|steam (any case). Anything else (and empty) is off.
Mode ParseMode(const wchar_t *text);
const char *ModeName(Mode mode);

// "47101, 47102" -> ports (1 to 65535), at most `max`; the count. Other words are skipped.
int ParsePorts(const wchar_t *text, uint16_t *out, int max);

// --- fake addresses --------------------------------------------------------------
const uint32_t kFakeNet = 0xc6120000u;   // 198.18.0.0
const uint32_t kFakeMask = 0xfffe0000u;  // /15: 198.18.0.0 to 198.19.255.255
// The sender of our hosting probes (below). Never anybody's address, never sent.
const uint32_t kProbeAddress = 0xc6120001u;  // 198.18.0.1

bool IsFake(uint32_t ip);  // in 198.18.0.0/15
// 255.255.255.255, or any address ending in .255 (the game's own "classful"
// broadcast of 198.18.x.y is 198.18.x.255). Fake addresses never end in .255.
bool IsBroadcast(uint32_t ip);
// The address of an id: a hash of it, 17 bits in 198.18.0.0/15, last byte 1 to
// 254 (never .0 or .255), never the probe address. Every PC computes the same.
uint32_t FakeAddressFor(uint64_t id);
// The next valid fake address after `ip` (wraps around), for a clash.
uint32_t NextFakeAddress(uint32_t ip);
// "198.18.7.21"
void FormatIp(uint32_t ip, char *out, size_t size);

// --- who is behind each fake address ----------------------------------------------
// Ourselves first (Reset), then each peer when it is first seen. A peer whose
// hash address is taken moves to the next free one. Every PC must see the
// clashing peers in the same order for that to agree: with Steam, the lobby's
// member list gives that order (step 3); the loopback test has no clash.
class PeerTable {
public:
	static const int kMax = 32;
	void Reset(uint64_t self_id);
	uint64_t SelfId() const { return ids_[0]; }
	uint32_t SelfAddress() const { return addresses_[0]; }
	// The address of `id`, added now if new (*added says so, if given). 0 when the table is full.
	uint32_t AddressOf(uint64_t id, bool *added = nullptr);
	// Whose address is it? False for an address nobody has.
	bool IdOf(uint32_t address, uint64_t *id) const;
	int Count() const { return count_; }  // ourselves included

private:
	uint64_t ids_[kMax] = {};
	uint32_t addresses_[kMax] = {};
	int count_ = 0;
};

// --- the 2-byte header ---------------------------------------------------------
// A message between two PCs = the game's source port (big endian) + the packet.
const size_t kHeaderSize = 2;
const size_t kMaxPacket = 2048;  // the game's receive buffer (0x00484ea0)
const size_t kMaxMessage = kHeaderSize + kMaxPacket;
// Returns the message size, or 0 if it does not fit.
size_t EncodeMessage(uint16_t source_port, const uint8_t *packet, size_t size, uint8_t *out, size_t out_size);
// False if too short to be a message.
bool DecodeMessage(const uint8_t *message, size_t size, uint16_t *source_port, const uint8_t **packet, size_t *packet_size);

// --- what to do with a packet the game sends ---------------------------------------
enum class Route {
	kToPeer,        // a fake address: to that peer, on channel = destination port
	kToSelf,        // our own fake address: back to our own game, as a LAN would
	kQueryFanOut,   // an "any games here?" query (02) to a broadcast: to every known host
	kProbeAnswer,   // to the probe address: the game's answer to our hosting probe; swallowed
	kDropBroadcast, // any other broadcast (e.g. lan_discovery's copies to real adapters)
	kDropReal,      // a real address: the real network is not used while the virtual LAN is on
};
// The game's "any games here?" query: 5 bytes, the first 02.
bool IsQuery(const uint8_t *packet, size_t size);
// The game's game description, a host's answer to a query: first byte 03.
bool IsGameInfo(const uint8_t *packet, size_t size);
Route RouteOf(uint32_t destination, uint32_t self_address, const uint8_t *packet, size_t size);
const char *RouteName(Route route);

// --- hosting detection -------------------------------------------------------------
// Once a second we hand the game's own discovery socket a copy of its last
// "any games here?" query, as if it came from kProbeAddress. A PC that hosts a
// game answers with its game description (03): then we know it hosts, with no
// knowledge of the game's code or of that packet's layout.
class HostingDetector {
public:
	static const uint32_t kProbeEveryMs = 1000;
	static const uint32_t kHostingForMs = 3500;  // hosting while an answer came this recently
	// Is a probe due now? (Only once a query of the game was seen: Probe() needs its bytes.)
	bool ProbeDue(uint32_t now_ms) const;
	void OnProbeSent(uint32_t now_ms);
	// The game sent a game description (to the probe address or to anyone): it hosts.
	void OnGameInfoSent(uint32_t now_ms);
	bool Hosting(uint32_t now_ms) const;
	// The game's own query, kept for the probes.
	void OnQuerySent(const uint8_t *packet, size_t size);
	bool HaveQuery() const { return query_size_ != 0; }
	const uint8_t *Query() const { return query_; }
	size_t QuerySize() const { return query_size_; }

private:
	bool probed_ = false;
	uint32_t last_probe_ = 0;
	bool answered_ = false;
	uint32_t last_answer_ = 0;
	uint8_t query_[8] = {};
	size_t query_size_ = 0;
};

// --- hosts that get our queries --------------------------------------------------
// A host is known while its lobby (Steam) or its announcements (loopback) say
// it hosts; it is forgotten kForgetAfterMs after the last word from it.
struct Host {
	uint64_t id;
	uint16_t discovery_port;  // where its game listens for queries
	uint32_t seen_ms;
};
class KnownHosts {
public:
	static const int kMax = 16;
	static const uint32_t kForgetAfterMs = 5000;
	// Adds or refreshes a host; port 0 = it no longer hosts (removed). True if it is new.
	bool Update(uint64_t id, uint16_t discovery_port, uint32_t now_ms);
	void Expire(uint32_t now_ms);
	int Count() const { return count_; }
	const Host &At(int i) const { return hosts_[i]; }

private:
	Host hosts_[kMax] = {};
	int count_ = 0;
};

// The game sends each query to two broadcast addresses at once (its classful one
// and 255.255.255.255): the hosts need only one copy.
class QueryDedup {
public:
	static const uint32_t kWindowMs = 200;
	// True the first time these bytes are seen within the window.
	bool First(const uint8_t *packet, size_t size, uint32_t now_ms);

private:
	uint8_t last_[8] = {};
	size_t last_size_ = 0;
	uint32_t last_ms_ = 0;
};

// --- the loopback transport's datagrams (robot tests: two copies on one PC) --------
// "H5VL", version, kind, the sender's id, a channel, then the payload.
//   kData:     payload = a message (EncodeMessage)
//   kAnnounce: payload = the discovery port of the game it hosts (2 bytes, 0 = none):
//              the stand-in for a Steam lobby, sent to every other copy once a second.
enum class LoopbackKind : uint8_t { kData = 1, kAnnounce = 2 };
const size_t kLoopbackHeaderSize = 4 + 1 + 1 + 8 + 2;
const uint8_t kLoopbackVersion = 1;
struct LoopbackDatagram {
	LoopbackKind kind;
	uint64_t from;
	uint16_t channel;
	const uint8_t *payload;
	size_t payload_size;
};
size_t EncodeLoopback(LoopbackKind kind, uint64_t from, uint16_t channel, const uint8_t *payload, size_t size,
                      uint8_t *out, size_t out_size);
bool DecodeLoopback(const uint8_t *data, size_t size, LoopbackDatagram *out);
// The made-up id of the copy whose transport listens on 127.0.0.1:port (shaped like
// a SteamID of a person, so the same code paths handle it).
uint64_t LoopbackId(uint16_t port);
// The port of a loopback id; 0 if it is not one.
uint16_t LoopbackPort(uint64_t id);

}  // namespace vlan

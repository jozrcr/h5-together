// The virtual LAN's pure work (no Winsock, no Steam), unit-tested by
// src/hook/test/virtual_lan_test.cpp. The hooks that use it are in
// virtual_lan.cpp; the design is docs/plans/online-steam.md, "The design".
//
// In plain words: every player gets a made-up ("fake") IPv4 address in
// 198.18.0.0/15, a range set aside for network tests, computed from the
// player's id (a SteamID; the loopback test makes ids up). The game only ever
// sees those addresses. When it sends a packet to one, we look up whose it is
// and hand the packet to a transport (Steam; for the robot a local UDP socket),
// with the game's destination port as the "channel" and a 6-byte header holding
// the source port and the sender's own fake address. The other PC gives the
// packet to its game as if it came from that address and port.
//
// Two players whose ids give the same address: each PC says in its messages
// (and in its Steam lobby data) which address its own game uses, and the
// others take that one ("declared"). A PC moves off an address it knows to be
// taken, but only until its game has told its address to anybody.
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
// An address a player may have: fake, last byte 1 to 254, not the probe address.
bool IsPlayerAddress(uint32_t ip);
// The address of an id: a hash of it, 17 bits in 198.18.0.0/15, last byte 1 to
// 254 (never .0 or .255), never the probe address. Every PC computes the same.
uint32_t FakeAddressFor(uint64_t id);
// The next valid fake address after `ip` (wraps around), for a clash.
uint32_t NextFakeAddress(uint32_t ip);
// "198.18.7.21"
void FormatIp(uint32_t ip, char *out, size_t size);
// "198.18.7.21" -> the address; 0 if it is not one.
uint32_t ParseIp(const char *text);
// For the log, instead of a whole SteamID: "#3f2a", 4 hex digits of a hash of it.
// Two ids may share one (1 in 65,536); the log never needs more.
void FormatShortId(uint64_t id, char *out, size_t size);

// --- who is behind each fake address ----------------------------------------------
// Ourselves first (Reset), then each peer when it is first seen.
//   - A peer's address is the one it declares (Learn: its messages and its lobby
//     data carry the address its own game uses), else computed (AddressOf: its
//     hash address, or the next free one).
//   - A declared address beats a computed one: the computed peer moves on.
//   - Our own address moves off one that another PC declares or uses (AvoidForSelf),
//     until our game has told it to anybody (LockSelf): after that it never moves.
class PeerTable {
public:
	static const int kMax = 32;
	void Reset(uint64_t self_id);
	uint64_t SelfId() const { return ids_[0]; }
	uint32_t SelfAddress() const { return addresses_[0]; }
	void LockSelf() { self_locked_ = true; }
	bool SelfLocked() const { return self_locked_; }
	// Our address moves off `taken` (the addresses in use in a game this PC may
	// join), unless locked. True if it moved.
	bool AvoidForSelf(const uint32_t *taken, int count);
	// The address of `id`, added now at a computed address if new (*added says so,
	// if given). 0 when the table is full.
	uint32_t AddressOf(uint64_t id, bool *added = nullptr);
	enum class Learned {
		kSame,       // nothing new
		kAdded,      // a new peer, at the address it declares
		kChanged,    // a known peer now has the address it declares (it moved, or had a computed one)
		kOtherMoved, // ... which a computed peer had: that one moved to the next free address
		kSelfMoved,  // ... which was ours: we moved (our game had not told it to anybody)
		kClash,      // ... which another PC declared, or ours after our game told it: the peer gets
		             //     (or keeps) a computed address, and the two PCs disagree (logged)
		kComputed,   // no address declared: as AddressOf
		kFull,       // the table is full: 0
	};
	// The address of `id`, which says its own game uses `declared` (0: unknown).
	uint32_t Learn(uint64_t id, uint32_t declared, Learned *what = nullptr);
	// Whose address is it? False for an address nobody has.
	bool IdOf(uint32_t address, uint64_t *id) const;
	int Count() const { return count_; }  // ourselves included

private:
	int IndexOfId(uint64_t id) const;
	int IndexOfAddress(uint32_t address) const;
	// `ip` if nobody has it, else the next address nobody has (and not in `avoid`).
	uint32_t Free(uint32_t ip, const uint32_t *avoid = nullptr, int avoid_count = 0) const;
	uint64_t ids_[kMax] = {};
	uint32_t addresses_[kMax] = {};
	bool declared_[kMax] = {};
	int count_ = 0;
	bool self_locked_ = false;
};
const char *LearnedName(PeerTable::Learned what);

// --- the 6-byte header ---------------------------------------------------------
// A message between two PCs = the game's source port (2 bytes, big endian), the
// sender's own fake address (4 bytes, big endian), then the packet.
const size_t kHeaderSize = 6;
const size_t kMaxPacket = 2048;  // the game's receive buffer (0x00484ea0)
const size_t kMaxMessage = kHeaderSize + kMaxPacket;
// Returns the message size, or 0 if it does not fit.
size_t EncodeMessage(uint16_t source_port, uint32_t sender_address, const uint8_t *packet, size_t size, uint8_t *out,
                     size_t out_size);
// False if too short to be a message.
bool DecodeMessage(const uint8_t *message, size_t size, uint16_t *source_port, uint32_t *sender_address,
                   const uint8_t **packet, size_t *packet_size);

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
	uint32_t address;         // the fake address it declares (Steam: its lobby data); 0 = unknown
};
class KnownHosts {
public:
	static const int kMax = 16;
	static const uint32_t kForgetAfterMs = 5000;
	// Adds or refreshes a host; port 0 = it no longer hosts (removed). True if it is new.
	bool Update(uint64_t id, uint16_t discovery_port, uint32_t now_ms, uint32_t address = 0);
	// Is it one? (*at: its place)
	bool Has(uint64_t id, int *at = nullptr) const;
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

// --- the Steam transport's lobbies (virtual_lan_steam.cpp) -------------------------
// A PC whose game hosts makes a friends-only Steam lobby, with these keys in its
// data. Its friends find it through the friends list (Steam tells which lobby a
// friend is in) and read that data without joining.
#define VLAN_KEY_PROTOCOL "h5t_vlan"   // kLobbyProtocol: how our PCs talk (changes break old ones)
#define VLAN_KEY_VERSION "h5t_version" // H5 Together's version
#define VLAN_KEY_GAME "h5t_game"       // the game: FormatGame below; only the same game can play together
#define VLAN_KEY_HOST "h5t_host"       // the host's SteamID (the friend whose game it is)
#define VLAN_KEY_PORT "h5t_port"       // its game's "any games here?" port (0 = it no longer hosts)
#define VLAN_KEY_IP "h5t_ip"           // its fake address; also every member's own, in the member's data
#define VLAN_KEY_IPS "h5t_ips"         // the fake addresses in use in its game: the host's and its members'
const int kLobbyProtocol = 1;
const int kMaxTaken = 8;
struct LobbyInfo {
	int protocol;  // 0: no data (not one of ours, or not read yet)
	char version[24];
	char game[48];
	uint64_t host;
	uint16_t port;
	uint32_t address;
	uint32_t taken[kMaxTaken];
	int taken_count;
};
// From the lobby's data, one text per key (null or empty: not there).
LobbyInfo ParseLobbyInfo(const char *protocol, const char *version, const char *game, const char *host,
                         const char *port, const char *ip, const char *ips);
// "198.18.7.21 198.19.0.4" <-> addresses (at most max; other words are skipped).
int ParseAddressList(const char *text, uint32_t *out, int max);
void FormatAddressList(const uint32_t *addresses, int count, char *out, size_t size);
// "76561197960287930" -> the number; 0 if it is not one.
uint64_t ParseId(const char *text);

// Can this PC play in the game of the lobby that a friend is in?
enum class HostCheck {
	kOk,
	kNoData,         // no H5 Together data (yet: the first look at a lobby asks Steam for it)
	kOtherProtocol,  // another H5 Together that talks differently
	kNotTheFriend,   // the friend joined somebody else's game (that host is found through its own friend entry)
	kNotHosting,     // the lobby is kept, but its game no longer hosts
	kOtherGame,      // another game build or MMH5.5 version: the two games could not play together
};
HostCheck CheckHost(const LobbyInfo &lobby, uint64_t friend_id, const char *our_game);
const char *HostCheckName(HostCheck check);

// "mmh55/a1b2c3d4": the game build (game_build.h) and a hash of MMH5.5's 55u.dll,
// which changes with each MMH5.5 release ("mmh55/no-55u" without it).
uint32_t HashBytes(const uint8_t *data, size_t size);  // FNV-1a, 32 bits
void FormatGame(const char *build, bool have_55u, uint32_t hash_55u, char *out, size_t size);

// Which Steam lobby this PC should be in. One at a time:
//   - our own, while our game hosts, and for kKeepMs after it stops (a game that
//     stops answering for a moment, a map that loads);
//   - else the lobby of the host that our game plays with (it sends it more than
//     queries), while it does, and for kKeepMs after the last packet either way;
//   - else none.
// The transport asks Next() on every tick of its thread and tells what Steam did.
class LobbyPlan {
public:
	static const uint32_t kKeepMs = 60000;
	static const uint32_t kRetryMs = 5000;  // after Steam refused to create or join
	enum class Action { kNone, kCreate, kJoin, kLeave };
	enum class In { kNone, kOwn, kJoined };
	void SetHosting(uint16_t port, uint32_t now_ms);  // 0: no longer hosts
	// Our game sent this host more than a query; `lobby`: its lobby (0 = unknown).
	void PlaysWith(uint64_t host, uint64_t lobby, uint32_t now_ms);
	// A packet came from the host we play with.
	void HeardFrom(uint64_t host, uint32_t now_ms);
	// What to do now. kJoin: *lobby says which. kCreate and kJoin wait for Steam's answer.
	Action Next(uint32_t now_ms, uint64_t *lobby);
	void Created(uint64_t lobby);  // our lobby exists
	void Joined(uint64_t lobby);   // we are in the lobby of the host we play with
	void Failed(uint32_t now_ms);  // Steam refused (create or join): try again after kRetryMs
	void Left();                   // LeaveLobby done (or the lobby went away)
	void Forget();                 // the host we played with left its lobby: never that lobby again
	In Where() const { return in_; }
	uint64_t Lobby() const { return lobby_; }
	uint64_t Host() const { return host_; }  // the host we play with (or played with last)
	bool Waiting() const { return waiting_; }

private:
	bool WantOwn(uint32_t now_ms) const;
	uint16_t hosting_port_ = 0;
	bool hosted_ = false;
	uint32_t stopped_ms_ = 0;  // when it stopped hosting
	uint64_t host_ = 0, host_lobby_ = 0;
	uint64_t gone_lobby_ = 0;  // (a host that hosts again makes a new lobby)
	bool played_ = false;
	uint32_t played_ms_ = 0;  // last packet to or from that host
	In in_ = In::kNone;
	uint64_t lobby_ = 0;
	bool waiting_ = false;
	bool retry_ = false;
	uint32_t retry_ms_ = 0;
};

}  // namespace vlan

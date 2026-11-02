// How the virtual LAN's messages travel between PCs (docs/plans/online-steam.md,
// "Testing"). The core (virtual_lan.cpp) only talks to this interface, so the
// address mapping, the broadcast fan-out and the hosting detection are the same
// code whatever carries the messages:
//   - loopback (virtual_lan_loopback.cpp): a UDP socket of our own on 127.0.0.1,
//     for the robot's two copies of the game on one PC;
//   - Steam (virtual_lan_steam.cpp): ISteamNetworkingMessages and a lobby.
// It looks like ISteamNetworkingMessages on purpose: messages to a peer's id on a
// numbered channel (the game's destination port), unreliable, like UDP.
#pragma once

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "virtual_lan_logic.h"

namespace vlan {

struct Received {
	uint64_t from;     // the sender's id
	uint16_t channel;  // the game port it was sent to
	uint32_t at_ms;    // GetTickCount() when it arrived
	size_t size;
	uint8_t data[kMaxMessage];  // a message: EncodeMessage's header + the game's packet
};

// A member of the game's "lobby" and the fake address it says its game uses (0: not known).
struct Member {
	uint64_t id;
	uint32_t address;
};

// Every call is made with the core's lock held, on whatever game thread called
// the hooked socket function. The Steam transport also has a thread of its own
// for Steam's callbacks, which takes the same lock (MakeSteamTransport).
class Transport {
public:
	virtual ~Transport() {}
	virtual const char *Name() const = 0;
	// Our own id (a SteamID; the loopback's made-up one). 0 until it is known (Steam:
	// after Boot); the core gives the game no address until then.
	virtual uint64_t SelfId() const = 0;
	// At the game's first DirectInput call, on the game's main thread, without the
	// lock: a transport that must start before the game's LAN screen does it here
	// (Steam). False: it cannot work (it logs why), and the core leaves the game to its
	// own LAN play.
	virtual bool Boot() { return true; }
	// At the first hooked call (the game has started Winsock by then). False if it
	// cannot work; it logs why.
	virtual bool Start() = 0;
	// With every hooked call (about every 10 ms while the game has sockets): take in
	// what arrived, keep the lobby (or its stand-in) going.
	virtual void Poll(uint32_t now_ms) = 0;
	// One message to a peer (ourselves included), on a channel. False if it could not go.
	virtual bool Send(uint64_t to, uint16_t channel, const uint8_t *message, size_t size) = 0;
	// The oldest message waiting on that channel, if any.
	virtual bool Receive(uint16_t channel, Received *out) = 0;
	// This PC's game hosts a game, its "any games here?" socket on this port (0 = no game).
	virtual void SetHosting(uint16_t discovery_port) = 0;
	// This PC's game sent this peer more than a query (a join request, game data): it
	// plays with it. (Steam: if the peer hosts, this PC joins its lobby.)
	virtual void PlaysWith(uint64_t peer) { (void)peer; }
	// This PC's own fake address (the one its game uses), whenever it is set or moves.
	virtual void SetOwnAddress(uint32_t address) { (void)address; }
	// The hosts that get this PC's queries.
	virtual const KnownHosts &Hosts() const = 0;
	// The fake addresses in use in the known hosts' games (Steam: their lobby data),
	// which this PC's own address must avoid; their count.
	virtual int Taken(uint32_t *out, int max) const
	{
		(void)out;
		(void)max;
		return 0;
	}
	// Everybody in the game's "lobby" (Steam: our lobby's members, in the lobby's
	// order; the loopback: every copy heard from), not us; their count.
	virtual int Members(Member *out, int max) const = 0;
	// For the log: "127.0.0.1:47101, other copies at 47102".
	virtual void Describe(char *out, size_t size) const = 0;
};

// [online] mode=loopback: listens on 127.0.0.1:own_port; the other copies of the
// game listen on peer_ports. Our id is LoopbackId(own_port).
Transport *MakeLoopbackTransport(uint16_t own_port, const uint16_t *peer_ports, int peer_count);

// [online] mode=steam: Steam's messages and lobbies as `app_id` (virtual_lan_steam.cpp).
// Its own thread (Steam's callbacks, the lobby, the friends' games) takes `lock`, the
// core's lock.
Transport *MakeSteamTransport(CRITICAL_SECTION *lock, uint32_t app_id);

}  // namespace vlan

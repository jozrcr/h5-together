// How the virtual LAN's messages travel between PCs (docs/plans/online-steam.md,
// "Testing"). The core (virtual_lan.cpp) only talks to this interface, so the
// address mapping, the broadcast fan-out and the hosting detection are the same
// code whatever carries the messages:
//   - loopback (virtual_lan_loopback.cpp): a UDP socket of our own on 127.0.0.1,
//     for the robot's two copies of the game on one PC;
//   - Steam (step 3, not built yet): ISteamNetworkingMessages and a lobby.
// It looks like ISteamNetworkingMessages on purpose: messages to a peer's id on a
// numbered channel (the game's destination port), unreliable, like UDP.
#pragma once

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

// Every call is made with the core's lock held, on whatever game thread called
// the hooked socket function (no thread of the transport's own is needed for
// the loopback; Steam's callbacks will get one, see the plan).
class Transport {
public:
	virtual ~Transport() {}
	virtual const char *Name() const = 0;
	// Our own id (a SteamID; the loopback's made-up one). Known before Start().
	virtual uint64_t SelfId() const = 0;
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
	// The hosts that get this PC's queries.
	virtual const KnownHosts &Hosts() const = 0;
	// For the log: "127.0.0.1:47101, other copies at 47102".
	virtual void Describe(char *out, size_t size) const = 0;
};

// [online] mode=loopback: listens on 127.0.0.1:own_port; the other copies of the
// game listen on peer_ports. Our id is LoopbackId(own_port).
Transport *MakeLoopbackTransport(uint16_t own_port, const uint16_t *peer_ports, int peer_count);

}  // namespace vlan

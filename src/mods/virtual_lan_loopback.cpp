// The virtual LAN's loopback transport: [online] mode=loopback, for the robot's
// LAN test (two copies of the game on one PC, tools/robot/run_lan_test.py --vlan).
//
// Each copy has a UDP socket of its own on 127.0.0.1 ([online] loopback_port)
// and knows the other copies' ports ([online] loopback_peers). It stands in for
// Steam: its id is made up from its port (LoopbackId), messages go as datagrams
// "H5VL" + kind + sender id + channel + message (virtual_lan_logic.h), and once a
// second every copy tells the others whether its game hosts a game and on which
// discovery port (the stand-in for a Steam lobby). Nothing leaves this PC.
//
// The game's own sockets are not used for this: these calls go straight to
// ws2_32.dll, not through the exe's import table, so the hooks of virtual_lan.cpp,
// lan_discovery.cpp and the robot's net log never see them.
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "virtual_lan_transport.h"

#include "../hook/log.h"

namespace vlan {
namespace {

const uint32_t kAnnounceEveryMs = 1000;
const int kQueue = 256;  // messages waiting for the game, all channels together

class LoopbackTransport : public Transport {
public:
	LoopbackTransport(uint16_t own_port, const uint16_t *peer_ports, int peer_count) : own_port_(own_port)
	{
		for (int i = 0; i < peer_count && peer_count_ < kMaxPeers; ++i)
			if (peer_ports[i] != own_port)
				peer_ports_[peer_count_++] = peer_ports[i];
	}

	const char *Name() const override { return "loopback"; }
	uint64_t SelfId() const override { return LoopbackId(own_port_); }

	bool Start() override
	{
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			hooklog::Write("vlan: loopback: WSAStartup failed");
			return false;
		}
		socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (socket_ == INVALID_SOCKET) {
			hooklog::Write("vlan: loopback: socket() failed (error %d)", WSAGetLastError());
			return false;
		}
		sockaddr_in here = {};
		here.sin_family = AF_INET;
		here.sin_port = htons(own_port_);
		here.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(socket_, reinterpret_cast<const sockaddr *>(&here), sizeof(here)) != 0) {
			hooklog::Write("vlan: loopback: cannot listen on 127.0.0.1:%u (error %d): is another copy using that "
			               "loopback_port?",
			               own_port_, WSAGetLastError());
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
			return false;
		}
		u_long on = 1;
		ioctlsocket(socket_, FIONBIO, &on);
		int buffer = 1 << 20;  // a burst of game packets must not overflow the default 64 KB
		setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&buffer), sizeof(buffer));
		hooklog::Write("vlan: loopback: listening on 127.0.0.1:%u", own_port_);
		return true;
	}

	void Poll(uint32_t now_ms) override
	{
		if (socket_ == INVALID_SOCKET)
			return;
		for (int i = 0; i < 256; ++i) {  // everything that arrived (bounded: never stall the game)
			uint8_t buf[kLoopbackHeaderSize + kMaxMessage];
			const int n = recv(socket_, reinterpret_cast<char *>(buf), sizeof(buf), 0);
			if (n == SOCKET_ERROR)
				break;  // WSAEWOULDBLOCK: nothing more (WSAECONNRESET: a copy had closed; harmless)
			LoopbackDatagram d;
			if (!DecodeLoopback(buf, static_cast<size_t>(n), &d)) {
				++bad_;
				continue;
			}
			Heard(d.from);
			if (d.kind == LoopbackKind::kAnnounce) {
				const uint16_t port = d.payload_size >= 2 ? static_cast<uint16_t>(d.payload[0] << 8 | d.payload[1]) : 0;
				if (hosts_.Update(d.from, port, now_ms)) {
					char ip[20];
					FormatIp(FakeAddressFor(d.from), ip, sizeof(ip));
					hooklog::Write("vlan: loopback: the copy at port %u (%s) hosts a game (its discovery port %u)",
					               LoopbackPort(d.from), ip, port);
				}
				continue;
			}
			Push(d, now_ms);
		}
		hosts_.Expire(now_ms);
		if (!announced_ || now_ms - announced_at_ >= kAnnounceEveryMs) {
			announced_ = true;
			announced_at_ = now_ms;
			const uint8_t payload[2] = {static_cast<uint8_t>(hosting_port_ >> 8), static_cast<uint8_t>(hosting_port_)};
			for (int i = 0; i < peer_count_; ++i)
				SendTo(peer_ports_[i], LoopbackKind::kAnnounce, 0, payload, sizeof(payload));
		}
	}

	bool Send(uint64_t to, uint16_t channel, const uint8_t *message, size_t size) override
	{
		const uint16_t port = LoopbackPort(to);
		return port && SendTo(port, LoopbackKind::kData, channel, message, size);
	}

	bool Receive(uint16_t channel, Received *out) override
	{
		int oldest = -1;
		for (int i = 0; i < kQueue; ++i)
			if (used_[i] && queue_[i].channel == channel && (oldest < 0 || order_[i] < order_[oldest]))
				oldest = i;
		if (oldest < 0)
			return false;
		*out = queue_[oldest];
		used_[oldest] = false;
		return true;
	}

	void SetHosting(uint16_t discovery_port) override
	{
		hosting_port_ = discovery_port;
		announced_ = false;  // tell the others at once
	}

	const KnownHosts &Hosts() const override { return hosts_; }

	int Members(uint64_t *out, int max) const override
	{
		int n = 0;
		for (; n < member_count_ && n < max; ++n)
			out[n] = members_[n];
		return n;
	}

	void Describe(char *out, size_t size) const override
	{
		char peers[128] = "";
		for (int i = 0; i < peer_count_; ++i) {
			char one[12];
			_snprintf_s(one, sizeof(one), _TRUNCATE, "%s%u", i ? ", " : "", peer_ports_[i]);
			strncat_s(peers, sizeof(peers), one, _TRUNCATE);
		}
		_snprintf_s(out, size, _TRUNCATE, "127.0.0.1:%u, other copies at %s", own_port_, peer_count_ ? peers : "none");
	}

private:
	static const int kMaxPeers = 8;

	bool SendTo(uint16_t port, LoopbackKind kind, uint16_t channel, const uint8_t *payload, size_t size)
	{
		if (socket_ == INVALID_SOCKET)
			return false;
		uint8_t buf[kLoopbackHeaderSize + kMaxMessage];
		const size_t n = EncodeLoopback(kind, SelfId(), channel, payload, size, buf, sizeof(buf));
		if (!n)
			return false;
		sockaddr_in to = {};
		to.sin_family = AF_INET;
		to.sin_port = htons(port);
		to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		return sendto(socket_, reinterpret_cast<const char *>(buf), static_cast<int>(n), 0,
		              reinterpret_cast<const sockaddr *>(&to), sizeof(to)) == static_cast<int>(n);
	}

	// The copies heard from, in the order first heard (our "lobby").
	void Heard(uint64_t id)
	{
		if (id == SelfId())
			return;
		for (int i = 0; i < member_count_; ++i)
			if (members_[i] == id)
				return;
		if (member_count_ < kMaxPeers)
			members_[member_count_++] = id;
	}

	// Into the queue; when it is full the oldest message goes (as a full UDP buffer drops).
	void Push(const LoopbackDatagram &d, uint32_t now_ms)
	{
		if (d.payload_size > kMaxMessage)
			return;
		int slot = -1;
		for (int i = 0; i < kQueue && slot < 0; ++i)
			if (!used_[i])
				slot = i;
		if (slot < 0) {
			slot = 0;
			for (int i = 1; i < kQueue; ++i)
				if (order_[i] < order_[slot])
					slot = i;
			++overflow_;
		}
		Received &r = queue_[slot];
		r.from = d.from;
		r.channel = d.channel;
		r.at_ms = now_ms;
		r.size = d.payload_size;
		memcpy(r.data, d.payload, d.payload_size);
		used_[slot] = true;
		order_[slot] = ++next_order_;
	}

	uint16_t own_port_;
	uint16_t peer_ports_[kMaxPeers] = {};
	int peer_count_ = 0;
	SOCKET socket_ = INVALID_SOCKET;
	KnownHosts hosts_;
	uint64_t members_[kMaxPeers] = {};
	int member_count_ = 0;
	uint16_t hosting_port_ = 0;
	bool announced_ = false;
	uint32_t announced_at_ = 0;
	Received queue_[kQueue];
	bool used_[kQueue] = {};
	uint64_t order_[kQueue] = {};
	uint64_t next_order_ = 0;
	long bad_ = 0;
	long overflow_ = 0;
};

}  // namespace

Transport *MakeLoopbackTransport(uint16_t own_port, const uint16_t *peer_ports, int peer_count)
{
	return new LoopbackTransport(own_port, peer_ports, peer_count);
}

}  // namespace vlan

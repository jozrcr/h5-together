// Mod: the virtual LAN ([online] mode; off by default), the core of online play
// over Steam (docs/plans/online-steam.md, "The design"; step 2 of its table).
//
// The game keeps its own LAN code. We put hooks on three of its Winsock imports
// (WSOCK32.dll, by number; import table entries, data, not code), stacked with
// lan_discovery.cpp's and the robot's:
//   gethostbyname  The game asks "what are my addresses?" with
//                  gethostbyname(gethostname()) (0x00484ac0, 0x00484db0). For its
//                  own host name we answer with this PC's fake address only
//                  (198.18.x.y, virtual_lan_logic.h), so every address the game
//                  writes into its packets (its own) is fake: nothing to rewrite.
//                  Every other name goes to the real function.
//   sendto         A packet to a fake address goes to that peer through the
//                  transport (virtual_lan_transport.h), on channel = destination
//                  port, with the source port in a 2-byte header. An "any games
//                  here?" query (02) to a broadcast goes to every known host's
//                  discovery port. Anything else is dropped: the real network is
//                  not used while the virtual LAN is on (so no real address of
//                  this PC goes out, and real LAN games do not show up and then
//                  fail to join).
//   recvfrom       The next message for this socket's port, as if it came from
//                  the sender's fake address and source port. Whatever the real
//                  network brings is read and dropped.
// Hosting: once a second the game's discovery socket gets a copy of its own last
// query, as if from 198.18.0.1; a game that hosts answers with its description
// (03), which we swallow. Then the transport announces this PC as a host (the
// loopback's stand-in for a lobby; with Steam, a friends-only lobby).
//
// Addresses: every message says which address its sender's game uses (the
// header, virtual_lan_logic.h), and the receiver takes that one. Our own
// address moves off one that another PC uses (two ids with the same hash),
// until our game has told it to anybody: its first packet to a peer, or when it
// starts to host.
//
// The game polls each socket every 10 ms on its network thread, so the hooks
// also drive the transport. The Steam transport has one thread of its own too
// (Steam's callbacks), under the same lock.
//
// [online] mode=steam: the hooks go in at the DLL's load as for the loopback,
// but Steam starts at the game's first DirectInput call (StartVirtualLan), long
// before the LAN screen. If it cannot start (no steam_api.dll, Steam not
// running), the hooks stand aside and the game plays its own LAN, as mode=off.
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "virtual_lan.h"
#include "virtual_lan_logic.h"
#include "virtual_lan_transport.h"

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace {

typedef int(WINAPI *SendToFn)(SOCKET, const char *, int, int, const sockaddr *, int);
typedef int(WINAPI *RecvFromFn)(SOCKET, char *, int, int, sockaddr *, int *);
typedef hostent *(WINAPI *GetHostByNameFn)(const char *);
SendToFn g_sendto = nullptr;
RecvFromFn g_recvfrom = nullptr;
GetHostByNameFn g_gethostbyname = nullptr;

bool g_install_tried = false;
volatile bool g_on = false;  // (set before the game's network starts; off again if Steam does not start)
vlan::Mode g_mode = vlan::Mode::kOff;
CRITICAL_SECTION g_lock;  // everything below, and every transport call
vlan::Transport *g_transport = nullptr;
bool g_started = false;
bool g_failed = false;
vlan::PeerTable g_peers;
vlan::HostingDetector g_hosting;
vlan::QueryDedup g_dedup;
bool g_hosting_now = false;
SOCKET g_discovery = INVALID_SOCKET;  // the game's "any games here?" socket
uint16_t g_discovery_port = 0;
vlan::Received g_received;
char g_host_name[256] = "";
bool g_said_self = false;
uint32_t g_self_checked_at = 0;

const uint32_t kStaleMs = 3000;  // a message older than this when the game asks is dropped
const uint32_t kStatsEveryMs = 30000;

struct Stats {
	long to_peers, to_peers_bytes, to_self, fanned_out, from_peers, from_peers_bytes, probes, probe_answers;
	long dropped_real, dropped_broadcasts, unknown_peer, send_failed, stale, host_answers;
	volatile LONG dropped_received;  // counted outside the lock
};
Stats g_stats = {};
uint32_t g_stats_at = 0;
long g_stats_logged = -1;
bool g_said_real = false, g_said_broadcast = false, g_said_unknown = false, g_said_received = false;

// The game's own address list (thread-local, as Winsock's own answer is).
struct OwnHost {
	hostent entry;
	char name[256];
	char *aliases[1];
	char *addresses[2];
	in_addr address;
};
__declspec(thread) OwnHost t_own;
volatile LONG g_own_answers = 0;

uint16_t LocalPort(SOCKET s)
{
	sockaddr_in a = {};
	int n = sizeof(a);
	if (getsockname(s, reinterpret_cast<sockaddr *>(&a), &n) != 0 || a.sin_family != AF_INET)
		return 0;
	return ntohs(a.sin_port);
}

void Ip(uint32_t ip, char *out, size_t size)
{
	vlan::FormatIp(ip, out, size);
}

void ShortId(uint64_t id, char *out, size_t size)
{
	vlan::FormatShortId(id, out, size);
}

// Our own fake address, once the transport knows our id (the loopback's at once;
// Steam's after its start, which comes before the game's LAN screen). False until then.
bool HaveSelf()
{
	if (g_peers.Count() == 0 && g_transport->SelfId() != 0) {
		g_peers.Reset(g_transport->SelfId());
		g_transport->SetOwnAddress(g_peers.SelfAddress());
	}
	if (g_peers.Count() != 0 && !g_said_self) {
		g_said_self = true;
		char ip[20], id[12];
		Ip(g_peers.SelfAddress(), ip, sizeof(ip));
		ShortId(g_peers.SelfId(), id, sizeof(id));
		hooklog::Write("vlan: this PC is %s (%s) for the game", ip, id);
	}
	return g_peers.Count() != 0;
}

void SelfMoved(const char *why)
{
	char ip[20];
	Ip(g_peers.SelfAddress(), ip, sizeof(ip));
	hooklog::Write("vlan: our address moved to %s (%s; our game had not told the old one to anybody)", ip, why);
	g_transport->SetOwnAddress(g_peers.SelfAddress());
}

// Until our game tells its address to anybody, it moves off the addresses in use
// in the games this PC may join (two players with the same hash address).
void CheckSelf(uint32_t now, bool force)
{
	if (g_peers.SelfLocked() || (!force && now - g_self_checked_at < 250))
		return;
	g_self_checked_at = now;
	uint32_t taken[vlan::KnownHosts::kMax * (vlan::kMaxTaken + 1)];
	const int n = g_transport->Taken(taken, static_cast<int>(sizeof(taken) / sizeof(taken[0])));
	if (n && g_peers.AvoidForSelf(taken, n))
		SelfMoved("a known host's game has a player there");
}

// Our address is in what our game sends now (a packet to a peer, its game description).
void LockSelf(const char *why)
{
	if (g_peers.SelfLocked())
		return;
	g_peers.LockSelf();
	char ip[20];
	Ip(g_peers.SelfAddress(), ip, sizeof(ip));
	hooklog::Write("vlan: our address %s is final now (%s)", ip, why);
}

// The address of a peer that says its game uses `declared` (0: it does not say).
uint32_t LearnPeer(uint64_t id, uint32_t declared, const char *from_where)
{
	vlan::PeerTable::Learned what = vlan::PeerTable::Learned::kSame;
	const uint32_t address = g_peers.Learn(id, declared, &what);
	if (what == vlan::PeerTable::Learned::kSame)
		return address;
	char ip[20], said[20], who[12];
	Ip(address, ip, sizeof(ip));
	Ip(declared, said, sizeof(said));
	ShortId(id, who, sizeof(who));
	hooklog::Write("vlan: peer %s %s (%s; it says %s): %s", who, ip, from_where, declared ? said : "nothing",
	               vlan::LearnedName(what));
	if (what == vlan::PeerTable::Learned::kSelfMoved)
		SelfMoved("a peer uses our old one");
	return address;
}

// The transport starts at the first hooked call (the game has started Winsock by then).
bool Ready()
{
	if (!HaveSelf())
		return false;
	if (!g_started && !g_failed) {
		g_started = g_transport->Start();
		g_failed = !g_started;
		if (g_failed)
			hooklog::Write("vlan: the %s transport did not start: the game gets no network at all", g_transport->Name());
	}
	return g_started;
}

void SendMessageTo(uint64_t id, uint16_t channel, uint16_t source_port, const uint8_t *packet, size_t size)
{
	uint8_t message[vlan::kMaxMessage];
	const size_t n = vlan::EncodeMessage(source_port, g_peers.SelfAddress(), packet, size, message, sizeof(message));
	if (!n || !g_transport->Send(id, channel, message, n))
		++g_stats.send_failed;
}

void FillFrom(sockaddr *from, int *from_len, uint32_t ip, uint16_t port)
{
	if (!from || !from_len || *from_len < static_cast<int>(sizeof(sockaddr_in)))
		return;
	sockaddr_in *in = reinterpret_cast<sockaddr_in *>(from);
	memset(in, 0, sizeof(*in));
	in->sin_family = AF_INET;
	in->sin_port = htons(port);
	in->sin_addr.s_addr = htonl(ip);
	*from_len = sizeof(sockaddr_in);
}

void LogStats(uint32_t now, bool force)
{
	if (!force && now - g_stats_at < kStatsEveryMs)
		return;
	g_stats_at = now;
	const long activity = g_stats.to_peers + g_stats.from_peers + g_stats.fanned_out + g_stats.probes;
	if (!force && activity == g_stats_logged)
		return;
	g_stats_logged = activity;
	hooklog::Write("vlan: so far: %ld packets to peers (%ld KB), %ld from peers (%ld KB), %ld to ourselves; %ld queries "
	               "sent to hosts; %ld hosting probes, %ld answered; %ld game descriptions sent; dropped: %ld to real "
	               "addresses, %ld other broadcasts, %ld to unknown fake addresses, %ld received from the real network, "
	               "%ld stale; %ld could not be sent; %d known peers",
	               g_stats.to_peers, g_stats.to_peers_bytes / 1024, g_stats.from_peers, g_stats.from_peers_bytes / 1024,
	               g_stats.to_self, g_stats.fanned_out, g_stats.probes, g_stats.probe_answers, g_stats.host_answers,
	               g_stats.dropped_real, g_stats.dropped_broadcasts, g_stats.unknown_peer,
	               static_cast<long>(g_stats.dropped_received), g_stats.stale, g_stats.send_failed, g_peers.Count() - 1);
}

// Everybody the transport knows (lobby members) gets an address now, before a first
// packet: in a game of 3 or more, the host passes the clients' addresses to each
// other, and a client must know who is behind one it never heard from.
void AddMembers()
{
	vlan::Member members[vlan::PeerTable::kMax];
	const int n = g_transport->Members(members, vlan::PeerTable::kMax);
	for (int i = 0; i < n; ++i)
		LearnPeer(members[i].id, members[i].address, "from the lobby's member list");
}

void UpdateHosting(uint32_t now)
{
	const bool hosting = g_hosting.Hosting(now);
	if (hosting == g_hosting_now)
		return;
	g_hosting_now = hosting;
	if (hosting)
		LockSelf("our game hosts: its game description holds it");
	g_transport->SetHosting(hosting ? g_discovery_port : 0);
	if (hosting)
		hooklog::Write("vlan: this PC's game hosts a game (it answers queries); announced, discovery port %u",
		               g_discovery_port);
	else
		hooklog::Write("vlan: this PC's game no longer answers queries: not announced as a host any more");
}

int WINAPI VlanSendTo(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int to_len)
{
	const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(to);
	if (!g_on || !buf || len < 0 || !in || to_len < static_cast<int>(sizeof(sockaddr_in)) || in->sin_family != AF_INET)
		return g_sendto(s, buf, len, flags, to, to_len);
	const uint8_t *packet = reinterpret_cast<const uint8_t *>(buf);
	const size_t size = static_cast<size_t>(len);
	const uint32_t destination = ntohl(in->sin_addr.s_addr);
	const uint16_t port = ntohs(in->sin_port);
	char ip[20];
	EnterCriticalSection(&g_lock);
	const uint32_t now = GetTickCount();  // (inside the lock: no message is newer than "now")
	if (Ready()) {
		g_transport->Poll(now);
		AddMembers();
		CheckSelf(now, false);
		if (vlan::IsGameInfo(packet, size)) {  // only a game that hosts sends its description
			g_hosting.OnGameInfoSent(now);
			g_discovery = s;
			g_discovery_port = LocalPort(s);
		}
		switch (vlan::RouteOf(destination, g_peers.SelfAddress(), packet, size)) {
		case vlan::Route::kProbeAnswer:
			++g_stats.probe_answers;
			break;
		case vlan::Route::kQueryFanOut:
			g_discovery = s;
			g_discovery_port = LocalPort(s);
			g_hosting.OnQuerySent(packet, size);
			if (g_dedup.First(packet, size, now)) {
				const vlan::KnownHosts &hosts = g_transport->Hosts();
				for (int i = 0; i < hosts.Count(); ++i) {
					if (hosts.At(i).id == g_peers.SelfId())
						continue;
					LearnPeer(hosts.At(i).id, hosts.At(i).address, "a known host");  // (its answer will come from there)
					SendMessageTo(hosts.At(i).id, hosts.At(i).discovery_port, g_discovery_port, packet, size);
					++g_stats.fanned_out;
				}
			}
			break;
		case vlan::Route::kToSelf:
			SendMessageTo(g_peers.SelfId(), port, LocalPort(s), packet, size);
			++g_stats.to_self;
			break;
		case vlan::Route::kToPeer: {
			uint64_t id = 0;
			if (g_peers.IdOf(destination, &id)) {
				if (!vlan::IsQuery(packet, size)) {
					LockSelf("our game's first packet to a peer");  // (before it goes: its header holds the address)
					g_transport->PlaysWith(id);
				}
				SendMessageTo(id, port, LocalPort(s), packet, size);
				if (vlan::IsGameInfo(packet, size))
					++g_stats.host_answers;
				++g_stats.to_peers;
				g_stats.to_peers_bytes += len;
			} else {
				++g_stats.unknown_peer;
				if (!g_said_unknown) {
					g_said_unknown = true;
					Ip(destination, ip, sizeof(ip));
					hooklog::Write("vlan: a packet to %s:%u, an address nobody has: dropped (logged once)", ip, port);
				}
			}
			break;
		}
		case vlan::Route::kDropBroadcast:
			++g_stats.dropped_broadcasts;
			if (!g_said_broadcast) {
				g_said_broadcast = true;
				Ip(destination, ip, sizeof(ip));
				hooklog::Write("vlan: a %d-byte broadcast to %s:%u (first byte %02x) dropped: the real network is not "
				               "used (lan_discovery's copies to the adapters land here; logged once)",
				               len, ip, port, len ? packet[0] : 0);
			}
			break;
		default:
			++g_stats.dropped_real;
			if (!g_said_real) {
				g_said_real = true;
				Ip(destination, ip, sizeof(ip));
				hooklog::Write("vlan: a %d-byte packet to the real address %s:%u dropped (logged once)", len, ip, port);
			}
			break;
		}
		UpdateHosting(now);
		LogStats(now, false);
	}
	LeaveCriticalSection(&g_lock);
	return len;  // as UDP: sent (a loss on the way is the game's link's business)
}

int WINAPI VlanRecvFrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *from_len)
{
	if (!g_on)
		return g_recvfrom(s, buf, len, flags, from, from_len);
	EnterCriticalSection(&g_lock);
	const uint32_t now = GetTickCount();  // (inside the lock: no message is newer than "now")
	if (Ready()) {
		g_transport->Poll(now);
		AddMembers();
		CheckSelf(now, false);
		UpdateHosting(now);
		LogStats(now, false);
		const uint16_t port = LocalPort(s);
		// (the port too: Windows gives a closed socket's number to the next new socket)
		if (s == g_discovery && port == g_discovery_port && g_hosting.ProbeDue(now) && buf &&
		    len >= static_cast<int>(g_hosting.QuerySize())) {
			// the hosting probe: the game's own last query, from an address nobody has
			g_hosting.OnProbeSent(now);
			++g_stats.probes;
			const int n = static_cast<int>(g_hosting.QuerySize());
			memcpy(buf, g_hosting.Query(), static_cast<size_t>(n));
			FillFrom(from, from_len, vlan::kProbeAddress, g_discovery_port);
			LeaveCriticalSection(&g_lock);
			return n;
		}
		while (port && g_transport->Receive(port, &g_received)) {
			if (now - g_received.at_ms > kStaleMs) {
				++g_stats.stale;
				continue;
			}
			uint16_t source_port = 0;
			uint32_t declared = 0;
			const uint8_t *packet = nullptr;
			size_t size = 0;
			if (!vlan::DecodeMessage(g_received.data, g_received.size, &source_port, &declared, &packet, &size))
				continue;
			char where[64];
			_snprintf_s(where, sizeof(where), _TRUNCATE, "a packet from its port %u to our port %u", source_port, port);
			const uint32_t address = LearnPeer(g_received.from, declared, where);
			if (!address)
				continue;  // (more than 31 peers)
			++g_stats.from_peers;
			g_stats.from_peers_bytes += static_cast<long>(size);
			const int n = size < static_cast<size_t>(len > 0 ? len : 0) ? static_cast<int>(size) : (len > 0 ? len : 0);
			if (buf && n > 0)
				memcpy(buf, packet, static_cast<size_t>(n));
			FillFrom(from, from_len, address, source_port);
			LeaveCriticalSection(&g_lock);
			if (static_cast<int>(size) > n) {  // as UDP: the rest of a too-big packet is lost
				WSASetLastError(WSAEMSGSIZE);
				return SOCKET_ERROR;
			}
			return n;
		}
	}
	LeaveCriticalSection(&g_lock);
	// Nothing from the virtual LAN. What the real network brought is read and dropped.
	for (int i = 0; i < 64; ++i) {
		const int r = g_recvfrom(s, buf, len, flags, from, from_len);
		if (r == SOCKET_ERROR)
			return r;  // usually WSAEWOULDBLOCK: the game's sockets do not block
		InterlockedIncrement(&g_stats.dropped_received);
		if (!g_said_received) {
			g_said_received = true;
			hooklog::Write("vlan: a %d-byte packet from the real network dropped (logged once)", r);
		}
	}
	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

// Our own host name, as the game's gethostname() gives it.
bool IsOwnName(const char *name)
{
	if (!name || !name[0])
		return true;  // (Winsock's own meaning of an empty name)
	EnterCriticalSection(&g_lock);
	if (!g_host_name[0] && gethostname(g_host_name, sizeof(g_host_name)) != 0)
		g_host_name[0] = 0;
	const bool own = g_host_name[0] && _stricmp(name, g_host_name) == 0;
	LeaveCriticalSection(&g_lock);
	return own;
}

hostent *WINAPI VlanGetHostByName(const char *name)
{
	if (!g_on || !IsOwnName(name))
		return g_gethostbyname(name);
	OwnHost &own = t_own;
	EnterCriticalSection(&g_lock);
	if (HaveSelf())
		CheckSelf(GetTickCount(), true);  // (the game puts this answer in what it sends next)
	if (!HaveSelf()) {
		// no fake address yet: no address at all, never a real one
		LeaveCriticalSection(&g_lock);
		hooklog::Write("vlan: the game asked for this PC's own addresses before the %s transport knew our id: none given",
		               g_transport->Name());
		WSASetLastError(WSANO_DATA);
		return nullptr;
	}
	lstrcpynA(own.name, g_host_name[0] ? g_host_name : "localhost", sizeof(own.name));
	own.address.s_addr = htonl(g_peers.SelfAddress());
	LeaveCriticalSection(&g_lock);
	own.aliases[0] = nullptr;
	own.addresses[0] = reinterpret_cast<char *>(&own.address);
	own.addresses[1] = nullptr;
	own.entry.h_name = own.name;
	own.entry.h_aliases = own.aliases;
	own.entry.h_addrtype = AF_INET;
	own.entry.h_length = 4;
	own.entry.h_addr_list = own.addresses;
	const LONG n = InterlockedIncrement(&g_own_answers);
	if (n == 1 || n == 10 || n == 100) {
		char ip[20];
		Ip(g_peers.SelfAddress(), ip, sizeof(ip));
		hooklog::Write("vlan: the game asked for this PC's own addresses (%ld time%s so far): answered %s only", n,
		               n == 1 ? "" : "s", ip);
	}
	return &own.entry;
}

}  // namespace

void InstallVirtualLan()
{
	if (g_install_tried)
		return;
	g_install_tried = true;
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	wchar_t text[64];
	GetPrivateProfileStringW(L"online", L"mode", L"off", text, 64, path);
	const vlan::Mode mode = vlan::ParseMode(text);
	if (mode == vlan::Mode::kOff)
		return;
	InitializeCriticalSection(&g_lock);
	if (mode == vlan::Mode::kSteam) {
		UINT app_id = GetPrivateProfileIntW(L"online", L"app_id", 15370, path);
		if (app_id < 1 || app_id > 0x7fffffff)
			app_id = 15370;
		g_transport = vlan::MakeSteamTransport(&g_lock, app_id);
	} else {
		UINT own_port = GetPrivateProfileIntW(L"online", L"loopback_port", 47100, path);
		if (own_port < 1 || own_port > 65535)
			own_port = 47100;
		wchar_t peers_text[256];
		GetPrivateProfileStringW(L"online", L"loopback_peers", L"", peers_text, 256, path);
		uint16_t peers[8];
		const int peer_count = vlan::ParsePorts(peers_text, peers, 8);
		g_transport = vlan::MakeLoopbackTransport(static_cast<uint16_t>(own_port), peers, peer_count);
		HaveSelf();  // (the loopback knows its id at once)
		char self[20];
		Ip(g_peers.SelfAddress(), self, sizeof(self));
		for (int i = 0; i < peer_count; ++i)
			if (peers[i] != own_port && vlan::FakeAddressFor(vlan::LoopbackId(peers[i])) == g_peers.SelfAddress())
				hooklog::Write("vlan: loopback port %u gets our own fake address %s: choose other ports", peers[i], self);
	}
	g_gethostbyname = reinterpret_cast<GetHostByNameFn>(
		hookpatch::PatchImport("WSOCK32.dll", "gethostbyname", reinterpret_cast<void *>(&VlanGetHostByName)));
	if (!g_gethostbyname) {
		hooklog::Write("vlan: the game's gethostbyname import was not found: virtual LAN off");
		return;
	}
	g_sendto = reinterpret_cast<SendToFn>(hookpatch::PatchImport("WSOCK32.dll", "sendto", reinterpret_cast<void *>(&VlanSendTo)));
	g_recvfrom = reinterpret_cast<RecvFromFn>(
		hookpatch::PatchImport("WSOCK32.dll", "recvfrom", reinterpret_cast<void *>(&VlanRecvFrom)));
	if (!g_sendto || !g_recvfrom) {
		hooklog::Write("vlan: the game's sendto/recvfrom imports were not found: virtual LAN off");
		return;
	}
	g_mode = mode;
	g_on = true;
	char where[160];
	g_transport->Describe(where, sizeof(where));
	hooklog::Write("vlan: virtual LAN on ([online] mode=%s; %s). The game's packets go only through it: nothing goes to "
	               "the real network, and what comes from it is dropped%s",
	               vlan::ModeName(mode), where,
	               mode == vlan::Mode::kSteam ? ". Steam starts at the game's first DirectInput call; until then the "
	                                            "game gets no address"
	                                          : "");
}

void StartVirtualLan()
{
	if (!g_on || !g_transport)
		return;
	if (!g_transport->Boot()) {
		g_on = false;  // (before any LAN screen: the game's sockets come later)
		hooklog::Write("vlan: the %s transport cannot carry the game's packets (see above): the game's own LAN play "
		               "instead, as with mode=off",
		               g_transport->Name());
		return;
	}
	EnterCriticalSection(&g_lock);
	HaveSelf();
	LeaveCriticalSection(&g_lock);
}

bool VirtualLanOn()
{
	return g_on;
}

}  // namespace mods

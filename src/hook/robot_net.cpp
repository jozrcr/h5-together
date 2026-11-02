// Robot network log: what the game does with sockets, for the LAN robot.
//
// The game uses WSOCK32.dll (imported by number). In robot mode with
// net_log=1 the robot replaces a few of those imports with pass-throughs that
// log the socket type, addresses, ports and errors:
//   socket, bind, connect, setsockopt, and per socket the first 100 packets sent
//   and received (only the first 3 of the "02" packets every copy broadcasts
//   once a second).
// Every call goes to the real function, with two optional changes for running
// several copies of the game on ONE PC. LAN games are found through broadcasts
// on UDP port 8889 (net_lan_match_maker_port), which every copy binds; the second
// copy's bind fails ("address in use", 10048) and its LAN screen never opens.
//   net_share_udp_ports=1   SO_REUSEADDR: every copy binds the same port and
//                           gets the broadcasts. But then all copies have the same
//                           address and the game cannot tell them apart.
//   net_lan_relay_ports=P,Q each copy uses its own discovery port instead (the
//                           runner sets net_lan_match_maker_port per copy), and
//                           every broadcast is also sent to 127.0.0.1:P, Q (the
//                           other copies' ports), so they see each other as
//                           separate machines and can answer.
// Two more for research (docs/plans/online-steam.md, the packet map):
//   net_log_bytes=N         each logged packet's first N bytes too (2048 = whole
//                           packets), 64 bytes per line after the packet's line.
//   net_scan_addresses=1    look inside EVERY packet the game sends or receives
//                           (not only the logged ones) for this PC's own IPv4
//                           addresses, in network and in reversed byte order; log
//                           where each one sits (the first 40) and a count every
//                           10 s. With the virtual LAN on (src/mods/virtual_lan.cpp)
//                           the count must stay at 0.
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "robot.h"

namespace robot {
namespace {

typedef SOCKET(WINAPI *SocketFn)(int, int, int);
typedef int(WINAPI *BindFn)(SOCKET, const sockaddr *, int);
typedef int(WINAPI *SetSockOptFn)(SOCKET, int, int, const char *, int);
typedef int(WINAPI *SendToFn)(SOCKET, const char *, int, int, const sockaddr *, int);
typedef int(WINAPI *RecvFromFn)(SOCKET, char *, int, int, sockaddr *, int *);
typedef int(WINAPI *LastErrorFn)();
SocketFn g_socket = nullptr;
BindFn g_bind = nullptr;
BindFn g_connect = nullptr;
SetSockOptFn g_setsockopt = nullptr;
SendToFn g_sendto = nullptr;
RecvFromFn g_recvfrom = nullptr;
LastErrorFn g_last_error = nullptr;
bool g_share_udp_ports = false;
const int kMaxRelays = 8;
u_short g_relay_ports[kMaxRelays];  // host byte order
int g_relay_count = 0;

void Address(const sockaddr *a, char *out, size_t size)
{
	if (!a || a->sa_family != AF_INET) {
		_snprintf_s(out, size, _TRUNCATE, "?");
		return;
	}
	const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(a);
	const BYTE *ip = reinterpret_cast<const BYTE *>(&in->sin_addr);
	_snprintf_s(out, size, _TRUNCATE, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3],
	            (static_cast<unsigned>(reinterpret_cast<const BYTE *>(&in->sin_port)[0]) << 8) |
	                reinterpret_cast<const BYTE *>(&in->sin_port)[1]);
}

int Error()
{
	return g_last_error ? g_last_error() : 0;
}

// The first few sends and receives per socket are logged.
const int kTracked = 64;
int g_logged_packets = 100;  // per socket and direction (net_log_packets)
const int kLoggedHellos = 3;
SOCKET g_udp[kTracked];  // UDP sockets
struct Counter {
	SOCKET socket;
	int sent, received, hellos_sent, hellos_received;
};

// Log this packet? The once-a-second "02" packets only a few times.
bool Wanted(Counter *c, const char *buf, int len, bool sent)
{
	if (!c)
		return false;
	if (buf && len == 5 && buf[0] == 0x02)
		return (sent ? c->hellos_sent++ : c->hellos_received++) < kLoggedHellos;
	return (sent ? c->sent++ : c->received++) < g_logged_packets;
}
Counter g_counters[kTracked];

Counter *CounterOf(SOCKET s)
{
	for (Counter &c : g_counters) {
		if (c.socket == s)
			return &c;
		if (c.socket == 0) {
			c.socket = s;
			return &c;
		}
	}
	return nullptr;
}

// A few bytes of a packet, as hex.
void Bytes(const char *buf, int len, char *out, size_t size)
{
	out[0] = 0;
	for (int i = 0; buf && i < len && i < 12; ++i) {
		char b[4];
		_snprintf_s(b, sizeof(b), _TRUNCATE, "%02x", static_cast<BYTE>(buf[i]));
		strncat_s(out, size, b, _TRUNCATE);
	}
}

// net_log_bytes: the packet's first N bytes, 64 per line ("+offset hex").
int g_log_bytes = 0;

void Dump(const char *buf, int len, int limit)
{
	const int n = len < limit ? len : limit;
	for (int offset = 0; buf && offset < n; offset += 64) {
		char hex[160];
		int used = 0;
		for (int i = offset; i < n && i < offset + 64; ++i)
			used += _snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE, "%02x", static_cast<BYTE>(buf[i]));
		hex[used] = 0;
		hooklog::Write("robot: net:   +%04x %s", offset, hex);
	}
}

// --- net_scan_addresses: this PC's own addresses inside the game's packets ---------
bool g_scan = false;
CRITICAL_SECTION g_scan_lock;
struct OwnAddress {
	DWORD ip;  // network byte order
	char name[40];
};
const int kMaxOwn = 24;
OwnAddress g_own[kMaxOwn];
int g_own_count = 0;
DWORD g_own_at = 0;
struct ScanCount {
	long packets, with_address, only_reversed;
	long by_type[256];  // packets with an address (in network order), by their first byte
};
ScanCount g_scan_sent = {}, g_scan_received = {};
int g_dumped_by_type[256];  // packets with an address logged whole, by their first byte
DWORD g_scan_logged_at = 0;
long g_scan_logged_packets = 0;

// Every IPv4 address of this PC's adapters (up or not, loopback included), every 10 s.
void RefreshOwnAddresses()
{
	ULONG size = 16 * 1024;
	IP_ADAPTER_ADDRESSES *list = static_cast<IP_ADAPTER_ADDRESSES *>(malloc(size));
	if (!list)
		return;
	const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG r = GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size);
	if (r == ERROR_BUFFER_OVERFLOW) {
		free(list);
		list = static_cast<IP_ADAPTER_ADDRESSES *>(malloc(size));
		r = list ? GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size) : ERROR_NOT_ENOUGH_MEMORY;
	}
	int count = 0;
	for (IP_ADAPTER_ADDRESSES *a = r == NO_ERROR ? list : nullptr; a && count < kMaxOwn; a = a->Next) {
		for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u && count < kMaxOwn; u = u->Next) {
			const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(u->Address.lpSockaddr);
			if (!in || in->sin_family != AF_INET || in->sin_addr.s_addr == 0)
				continue;
			bool known = false;
			for (int i = 0; i < count; ++i)
				known = known || g_own[i].ip == in->sin_addr.s_addr;
			if (known)
				continue;
			g_own[count].ip = in->sin_addr.s_addr;
			WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, g_own[count].name, sizeof(g_own[count].name), nullptr,
			                    nullptr);
			++count;
		}
	}
	free(list);
	const bool first = g_own_at == 0;
	g_own_count = count;
	g_own_at = GetTickCount();
	if (first) {
		char text[900] = "";
		for (int i = 0; i < count; ++i) {
			char one[80];
			const BYTE *b = reinterpret_cast<const BYTE *>(&g_own[i].ip);
			_snprintf_s(one, sizeof(one), _TRUNCATE, "%s%u.%u.%u.%u (%s)", i ? ", " : "", b[0], b[1], b[2], b[3],
			            g_own[i].name);
			strncat_s(text, sizeof(text), one, _TRUNCATE);
		}
		hooklog::Write("robot: net: address scan: this PC's addresses: %s", count ? text : "none found");
	}
}

// Every 10 s while packets come and go (the last line holds the run's totals).
void LogScanCounts(bool force)
{
	const long packets = g_scan_sent.packets + g_scan_received.packets;
	const DWORD now = GetTickCount();
	if (!force && (now - g_scan_logged_at < 10000 || packets == g_scan_logged_packets))
		return;
	g_scan_logged_at = now;
	g_scan_logged_packets = packets;
	char types[400] = "";
	for (int t = 0; t < 256; ++t) {
		const long n = g_scan_sent.by_type[t] + g_scan_received.by_type[t];
		if (!n)
			continue;
		char one[24];
		_snprintf_s(one, sizeof(one), _TRUNCATE, "%s%02x x%ld", types[0] ? ", " : "", t, n);
		strncat_s(types, sizeof(types), one, _TRUNCATE);
	}
	hooklog::Write("robot: net: address scan: %ld packets sent, %ld received; %ld hold an address of this PC%s%s%s; %ld "
	               "more only in reversed byte order",
	               g_scan_sent.packets, g_scan_received.packets, g_scan_sent.with_address + g_scan_received.with_address,
	               types[0] ? " (by first byte: " : "", types, types[0] ? ")" : "",
	               g_scan_sent.only_reversed + g_scan_received.only_reversed);
}

// Looks for each address of this PC in the packet, in network byte order (as the
// game writes them) and reversed. The first 3 packets of each kind (first byte)
// that hold one are logged whole, with where each address sits.
void Scan(const char *buf, int len, bool sent, SOCKET s, const sockaddr *peer)
{
	if (!g_scan || !buf || len <= 0)
		return;
	EnterCriticalSection(&g_scan_lock);
	if (g_own_at == 0 || GetTickCount() - g_own_at > 10000)
		RefreshOwnAddresses();
	ScanCount &count = sent ? g_scan_sent : g_scan_received;
	++count.packets;
	bool straight_found = false, reversed_found = false;
	char hits[700] = "";
	for (int i = 0; i < g_own_count; ++i) {
		const BYTE *ip = reinterpret_cast<const BYTE *>(&g_own[i].ip);
		const BYTE reversed[4] = {ip[3], ip[2], ip[1], ip[0]};
		for (int at = 0; at + 4 <= len; ++at) {
			const bool straight = memcmp(buf + at, ip, 4) == 0;
			if (!straight && memcmp(buf + at, reversed, 4) != 0)
				continue;
			(straight ? straight_found : reversed_found) = true;
			char one[96];
			_snprintf_s(one, sizeof(one), _TRUNCATE, "%s%u.%u.%u.%u (%s)%s at byte %d", hits[0] ? "; " : "", ip[0], ip[1],
			            ip[2], ip[3], g_own[i].name, straight ? "" : " reversed", at);
			strncat_s(hits, sizeof(hits), one, _TRUNCATE);
		}
	}
	const BYTE type = static_cast<BYTE>(buf[0]);
	if (straight_found) {
		++count.with_address;
		++count.by_type[type];
	} else if (reversed_found) {
		++count.only_reversed;
	}
	if ((straight_found || reversed_found) && g_dumped_by_type[type] < 3) {
		++g_dumped_by_type[type];
		char where[32];
		Address(peer, where, sizeof(where));
		hooklog::Write("robot: net: address scan: a %d-byte %02x packet %s %s (socket %u) holds %s", len, type,
		               sent ? "sent to" : "received from", where, static_cast<unsigned>(s), hits);
		Dump(buf, len, 2048);
	}
	LogScanCounts(false);
	LeaveCriticalSection(&g_scan_lock);
}

SOCKET WINAPI RobotSocket(int af, int type, int protocol)
{
	const SOCKET s = g_socket(af, type, protocol);
	if (type == SOCK_DGRAM && s != INVALID_SOCKET) {
		for (SOCKET &u : g_udp) {
			if (u == 0) {
				u = s;
				break;
			}
		}
	}
	hooklog::Write("robot: net: socket(%s) -> %u%s", type == SOCK_DGRAM ? "udp" : type == SOCK_STREAM ? "tcp" : "?",
	               static_cast<unsigned>(s), s == INVALID_SOCKET ? " FAILED" : "");
	return s;
}

bool IsUdp(SOCKET s)
{
	for (SOCKET u : g_udp) {
		if (u == s)
			return true;
	}
	return false;
}

int WINAPI RobotBind(SOCKET s, const sockaddr *a, int len)
{
	if (g_share_udp_ports && IsUdp(s)) {
		const BOOL on = TRUE;
		g_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&on), sizeof(on));
	}
	const int r = g_bind(s, a, len);
	const int err = r ? Error() : 0;
	char where[32];
	Address(a, where, sizeof(where));
	hooklog::Write("robot: net: bind(%u, %s) -> %d (error %d)", static_cast<unsigned>(s), where, r, err);
	return r;
}

int WINAPI RobotConnect(SOCKET s, const sockaddr *a, int len)
{
	const int r = g_connect(s, a, len);
	const int err = r ? Error() : 0;
	char where[32];
	Address(a, where, sizeof(where));
	hooklog::Write("robot: net: connect(%u, %s) -> %d (error %d)", static_cast<unsigned>(s), where, r, err);
	return r;
}

int WINAPI RobotSetSockOpt(SOCKET s, int level, int name, const char *value, int len)
{
	const int r = g_setsockopt(s, level, name, value, len);
	hooklog::Write("robot: net: setsockopt(%u, level %d, option %d, %d) -> %d", static_cast<unsigned>(s), level, name,
	               value && len >= 4 ? *reinterpret_cast<const int *>(value) : -1, r);
	return r;
}

int WINAPI RobotSendTo(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int to_len)
{
	const int r = g_sendto(s, buf, len, flags, to, to_len);
	// broadcasts (x.x.x.255) also go to the other copies of the game on this PC
	const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(to);
	if (g_relay_count && in && in->sin_family == AF_INET && reinterpret_cast<const BYTE *>(&in->sin_addr)[3] == 255) {
		for (int i = 0; i < g_relay_count; ++i) {
			sockaddr_in local = *in;
			const BYTE loopback[4] = {127, 0, 0, 1};
			memcpy(&local.sin_addr, loopback, 4);
			reinterpret_cast<BYTE *>(&local.sin_port)[0] = static_cast<BYTE>(g_relay_ports[i] >> 8);
			reinterpret_cast<BYTE *>(&local.sin_port)[1] = static_cast<BYTE>(g_relay_ports[i] & 0xff);
			g_sendto(s, buf, len, flags, reinterpret_cast<const sockaddr *>(&local), sizeof(local));
		}
	}
	if (Wanted(CounterOf(s), buf, len, true)) {
		char where[32], bytes[32];
		Address(to, where, sizeof(where));
		Bytes(buf, len, bytes, sizeof(bytes));
		char stack[512] = "";
		static int stacks = 0;
		if (len > 100 && stacks < 6) {  // where the first data packets come from
			++stacks;
			DWORD esp;
			esp = reinterpret_cast<DWORD>(&esp);
			DescribeStack(esp, stack, sizeof(stack), 10);
		}
		hooklog::Write("robot: net: sendto(%u, %d bytes %s, %s) -> %d (error %d) from %p%s%s", static_cast<unsigned>(s),
		               len, bytes, where, r, r < 0 ? Error() : 0, _ReturnAddress(), stack[0] ? ", stack" : "", stack);
		Dump(buf, len, g_log_bytes);
	}
	Scan(buf, len, true, s, to);
	return r;
}

int WINAPI RobotRecvFrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *from_len)
{
	const int r = g_recvfrom(s, buf, len, flags, from, from_len);
	if (r >= 0 && Wanted(CounterOf(s), buf, r, false)) {
		char where[32], bytes[32];
		Address(from, where, sizeof(where));
		Bytes(buf, r, bytes, sizeof(bytes));
		hooklog::Write("robot: net: recvfrom(%u) -> %d bytes %s from %s, called from %p", static_cast<unsigned>(s), r, bytes,
		               where, _ReturnAddress());
		Dump(buf, r, g_log_bytes);
	}
	if (r > 0)
		Scan(buf, r, false, s, from);
	return r;
}

}  // namespace

void LogNetSummary()
{
	if (!g_scan)
		return;
	EnterCriticalSection(&g_scan_lock);
	LogScanCounts(true);
	LeaveCriticalSection(&g_scan_lock);
}

void InstallNetLog(bool share_udp_ports, const char *relay_ports, int logged_packets, int log_bytes, bool scan_addresses)
{
	g_share_udp_ports = share_udp_ports;
	g_log_bytes = log_bytes > 0 ? log_bytes : 0;
	g_scan = scan_addresses;
	InitializeCriticalSection(&g_scan_lock);
	if (logged_packets >= 0)
		g_logged_packets = logged_packets;
	for (const char *p = relay_ports; p && *p && g_relay_count < kMaxRelays;) {
		const int port = atoi(p);
		if (port > 0 && port < 65536)
			g_relay_ports[g_relay_count++] = static_cast<u_short>(port);
		p = strchr(p, ',');
		p = p ? p + 1 : nullptr;
	}
	HMODULE ws = GetModuleHandleA("WSOCK32.dll");  // a static import of the game: already mapped
	g_last_error = ws ? reinterpret_cast<LastErrorFn>(GetProcAddress(ws, "WSAGetLastError")) : nullptr;
	g_socket = reinterpret_cast<SocketFn>(PatchImport("WSOCK32.dll", "socket", reinterpret_cast<void *>(&RobotSocket)));
	g_bind = reinterpret_cast<BindFn>(PatchImport("WSOCK32.dll", "bind", reinterpret_cast<void *>(&RobotBind)));
	g_connect = reinterpret_cast<BindFn>(PatchImport("WSOCK32.dll", "connect", reinterpret_cast<void *>(&RobotConnect)));
	g_setsockopt = reinterpret_cast<SetSockOptFn>(
		PatchImport("WSOCK32.dll", "setsockopt", reinterpret_cast<void *>(&RobotSetSockOpt)));
	g_sendto = reinterpret_cast<SendToFn>(PatchImport("WSOCK32.dll", "sendto", reinterpret_cast<void *>(&RobotSendTo)));
	g_recvfrom = reinterpret_cast<RecvFromFn>(PatchImport("WSOCK32.dll", "recvfrom", reinterpret_cast<void *>(&RobotRecvFrom)));
	hooklog::Write("robot: net log: socket %s, bind %s, connect %s, setsockopt %s, sendto %s, recvfrom %s; "
	               "shared UDP ports %s, broadcast relays %d (%s); packet bytes logged %d; address scan %s",
	               g_socket ? "yes" : "no", g_bind ? "yes" : "no", g_connect ? "yes" : "no", g_setsockopt ? "yes" : "no",
	               g_sendto ? "yes" : "no", g_recvfrom ? "yes" : "no", g_share_udp_ports ? "yes" : "no", g_relay_count,
	               relay_ports ? relay_ports : "", g_log_bytes ? g_log_bytes : 12, g_scan ? "on" : "off");
}

}  // namespace robot

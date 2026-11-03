#include "virtual_lan_logic.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

namespace vlan {

namespace {

bool SameWord(const wchar_t *text, const wchar_t *word)
{
	while (*text == L' ' || *text == L'\t')
		++text;
	size_t n = wcslen(text);
	while (n > 0 && (text[n - 1] == L' ' || text[n - 1] == L'\t'))
		--n;
	if (n != wcslen(word))
		return false;
	for (size_t i = 0; i < n; ++i)
		if (towlower(text[i]) != word[i])
			return false;
	return true;
}

// splitmix64's last step: every bit of the id moves every bit of the result.
uint64_t Mix(uint64_t x)
{
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ull;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebull;
	x ^= x >> 31;
	return x;
}

void PutBig16(uint8_t *out, uint16_t v)
{
	out[0] = static_cast<uint8_t>(v >> 8);
	out[1] = static_cast<uint8_t>(v);
}

uint16_t GetBig16(const uint8_t *in)
{
	return static_cast<uint16_t>(in[0] << 8 | in[1]);
}

const uint64_t kLoopbackIdBase = 0x0110000170000000ull;  // + the port; low 16 bits free

}  // namespace

Mode ParseMode(const wchar_t *text)
{
	if (!text)
		return Mode::kOff;
	if (SameWord(text, L"loopback"))
		return Mode::kLoopback;
	if (SameWord(text, L"steam"))
		return Mode::kSteam;
	return Mode::kOff;
}

const char *ModeName(Mode mode)
{
	switch (mode) {
	case Mode::kLoopback:
		return "loopback";
	case Mode::kSteam:
		return "steam";
	default:
		return "off";
	}
}

int ParsePorts(const wchar_t *text, uint16_t *out, int max)
{
	int count = 0;
	for (const wchar_t *p = text; p && *p && count < max;) {
		if (*p < L'0' || *p > L'9') {
			++p;
			continue;
		}
		unsigned long value = 0;
		while (*p >= L'0' && *p <= L'9') {
			if (value < 100000)
				value = value * 10 + static_cast<unsigned long>(*p - L'0');
			++p;
		}
		if (value >= 1 && value <= 65535)
			out[count++] = static_cast<uint16_t>(value);
	}
	return count;
}

bool IsFake(uint32_t ip)
{
	return (ip & kFakeMask) == kFakeNet;
}

bool IsBroadcast(uint32_t ip)
{
	return ip == 0xffffffffu || (ip & 0xffu) == 0xffu;
}

bool IsPlayerAddress(uint32_t ip)
{
	const uint32_t last = ip & 0xffu;
	return IsFake(ip) && last >= 1 && last <= 254 && ip != kProbeAddress;
}

uint32_t NextFakeAddress(uint32_t ip)
{
	uint32_t high = (ip >> 8) & 0x1ffu;  // 9 bits: 198.18.x and 198.19.x
	uint32_t low = ip & 0xffu;
	do {
		if (++low > 254) {
			low = 1;
			high = (high + 1) & 0x1ffu;
		}
		ip = kFakeNet | high << 8 | low;
	} while (ip == kProbeAddress);
	return ip;
}

uint32_t FakeAddressFor(uint64_t id)
{
	const uint64_t h = Mix(id);
	const uint32_t high = static_cast<uint32_t>(h >> 8) & 0x1ffu;
	const uint32_t low = 1 + static_cast<uint32_t>(h & 0xffu) % 254;  // 1 to 254
	const uint32_t ip = kFakeNet | high << 8 | low;
	return ip == kProbeAddress ? NextFakeAddress(ip) : ip;
}

void FormatIp(uint32_t ip, char *out, size_t size)
{
	if (size)
		snprintf(out, size, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xffu, (ip >> 8) & 0xffu, ip & 0xffu);
}

uint32_t ParseIp(const char *text)
{
	if (!text)
		return 0;
	uint32_t ip = 0;
	for (int part = 0; part < 4; ++part) {
		if (part && *text++ != '.')
			return 0;
		if (*text < '0' || *text > '9')
			return 0;
		uint32_t value = 0;
		for (int digits = 0; *text >= '0' && *text <= '9'; ++digits) {
			if (digits == 3)
				return 0;
			value = value * 10 + static_cast<uint32_t>(*text++ - '0');
		}
		if (value > 255)
			return 0;
		ip = ip << 8 | value;
	}
	return *text == 0 ? ip : 0;
}

void FormatShortId(uint64_t id, char *out, size_t size)
{
	if (size)
		snprintf(out, size, "#%04x", static_cast<unsigned>(Mix(id ^ 0x6a09e667f3bcc909ull) >> 48));
}

void PeerTable::Reset(uint64_t self_id)
{
	ids_[0] = self_id;
	addresses_[0] = FakeAddressFor(self_id);
	declared_[0] = true;
	count_ = 1;
	self_locked_ = false;
}

int PeerTable::IndexOfId(uint64_t id) const
{
	for (int i = 0; i < count_; ++i)
		if (ids_[i] == id)
			return i;
	return -1;
}

int PeerTable::IndexOfAddress(uint32_t address) const
{
	for (int i = 0; i < count_; ++i)
		if (addresses_[i] == address)
			return i;
	return -1;
}

uint32_t PeerTable::Free(uint32_t ip, const uint32_t *avoid, int avoid_count) const
{
	for (int tries = 0; tries < 0x20000; ++tries) {  // (the whole range: never endless)
		bool taken = IndexOfAddress(ip) >= 0;
		for (int i = 0; i < avoid_count && !taken; ++i)
			taken = avoid[i] == ip;
		if (!taken)
			return ip;
		ip = NextFakeAddress(ip);
	}
	return ip;
}

bool PeerTable::AvoidForSelf(const uint32_t *taken, int count)
{
	if (count_ == 0 || self_locked_ || !taken)
		return false;
	bool hit = false;
	for (int i = 0; i < count && !hit; ++i)
		hit = taken[i] == addresses_[0];
	if (!hit)
		return false;
	addresses_[0] = Free(NextFakeAddress(addresses_[0]), taken, count);
	return true;
}

uint32_t PeerTable::AddressOf(uint64_t id, bool *added)
{
	if (added)
		*added = false;
	const int i = IndexOfId(id);
	if (i >= 0)
		return addresses_[i];
	if (count_ == 0 || count_ >= kMax)
		return 0;
	ids_[count_] = id;
	addresses_[count_] = Free(FakeAddressFor(id));
	declared_[count_] = false;
	++count_;
	if (added)
		*added = true;
	return addresses_[count_ - 1];
}

uint32_t PeerTable::Learn(uint64_t id, uint32_t declared, Learned *what)
{
	Learned w = Learned::kSame;
	uint32_t result = 0;
	const int i = IndexOfId(id);
	if (count_ == 0) {
		w = Learned::kFull;
	} else if (i == 0) {
		result = addresses_[0];  // (ourselves: our own messages carry our own address)
	} else if (!IsPlayerAddress(declared)) {
		bool added = false;
		result = AddressOf(id, &added);
		w = !result ? Learned::kFull : added ? Learned::kComputed : Learned::kSame;
	} else if (i > 0 && addresses_[i] == declared) {
		declared_[i] = true;
		result = declared;
	} else if (i < 0 && count_ >= kMax) {
		w = Learned::kFull;
	} else {
		const int j = IndexOfAddress(declared);  // its holder (not `id` itself, from the cases above)
		bool clash = false;
		if (j == 0) {
			if (self_locked_)
				clash = true;
			else {
				addresses_[0] = Free(NextFakeAddress(addresses_[0]));
				w = Learned::kSelfMoved;
			}
		} else if (j > 0) {
			if (declared_[j])
				clash = true;
			else {
				addresses_[j] = Free(NextFakeAddress(declared));
				w = Learned::kOtherMoved;
			}
		}
		if (clash) {
			w = Learned::kClash;
			result = AddressOf(id);  // (a known peer keeps its address; a new one gets a computed one)
		} else if (i > 0) {
			addresses_[i] = declared;
			declared_[i] = true;
			if (w == Learned::kSame)
				w = Learned::kChanged;
			result = declared;
		} else {
			ids_[count_] = id;
			addresses_[count_] = declared;
			declared_[count_] = true;
			++count_;
			if (w == Learned::kSame)
				w = Learned::kAdded;
			result = declared;
		}
	}
	if (what)
		*what = w;
	return result;
}

const char *LearnedName(PeerTable::Learned what)
{
	switch (what) {
	case PeerTable::Learned::kSame:
		return "known";
	case PeerTable::Learned::kAdded:
		return "new, at the address it uses";
	case PeerTable::Learned::kChanged:
		return "now at the address it uses";
	case PeerTable::Learned::kOtherMoved:
		return "at the address it uses, which another peer had: that one moved";
	case PeerTable::Learned::kSelfMoved:
		return "at the address it uses, which was ours: ours moved (our game had not told it to anybody)";
	case PeerTable::Learned::kClash:
		return "CLASH: it uses an address another PC (or ours) already uses; it gets another one, and the two PCs "
		       "disagree";
	case PeerTable::Learned::kComputed:
		return "new, at its computed address";
	default:
		return "table full";
	}
}

bool PeerTable::IdOf(uint32_t address, uint64_t *id) const
{
	const int i = IndexOfAddress(address);
	if (i < 0)
		return false;
	if (id)
		*id = ids_[i];
	return true;
}

size_t EncodeMessage(uint16_t source_port, uint32_t sender_address, const uint8_t *packet, size_t size, uint8_t *out,
                     size_t out_size)
{
	if (!out || size > kMaxPacket || kHeaderSize + size > out_size || (size && !packet))
		return 0;
	PutBig16(out, source_port);
	PutBig16(out + 2, static_cast<uint16_t>(sender_address >> 16));
	PutBig16(out + 4, static_cast<uint16_t>(sender_address));
	if (size)
		memcpy(out + kHeaderSize, packet, size);
	return kHeaderSize + size;
}

bool DecodeMessage(const uint8_t *message, size_t size, uint16_t *source_port, uint32_t *sender_address,
                   const uint8_t **packet, size_t *packet_size)
{
	if (!message || size < kHeaderSize || size > kMaxMessage)
		return false;
	*source_port = GetBig16(message);
	*sender_address = static_cast<uint32_t>(GetBig16(message + 2)) << 16 | GetBig16(message + 4);
	*packet = message + kHeaderSize;
	*packet_size = size - kHeaderSize;
	return true;
}

bool IsQuery(const uint8_t *packet, size_t size)
{
	return packet && size == 5 && packet[0] == 0x02;
}

bool IsGameInfo(const uint8_t *packet, size_t size)
{
	return packet && size > 1 && packet[0] == 0x03;
}

Route RouteOf(uint32_t destination, uint32_t self_address, const uint8_t *packet, size_t size)
{
	if (destination == kProbeAddress)
		return Route::kProbeAnswer;
	if (IsBroadcast(destination))
		return IsQuery(packet, size) && (destination == 0xffffffffu || IsFake(destination)) ? Route::kQueryFanOut
		                                                                                    : Route::kDropBroadcast;
	if (self_address && destination == self_address)
		return Route::kToSelf;
	if (IsFake(destination))
		return Route::kToPeer;
	return Route::kDropReal;
}

const char *RouteName(Route route)
{
	switch (route) {
	case Route::kToPeer:
		return "to a peer";
	case Route::kToSelf:
		return "to ourselves";
	case Route::kQueryFanOut:
		return "query to the known hosts";
	case Route::kProbeAnswer:
		return "answer to the hosting probe";
	case Route::kDropBroadcast:
		return "other broadcast, dropped";
	default:
		return "real address, dropped";
	}
}

bool HostingDetector::ProbeDue(uint32_t now_ms) const
{
	return HaveQuery() && (!probed_ || now_ms - last_probe_ >= kProbeEveryMs);
}

void HostingDetector::OnProbeSent(uint32_t now_ms)
{
	probed_ = true;
	last_probe_ = now_ms;
}

void HostingDetector::OnGameInfoSent(uint32_t now_ms)
{
	answered_ = true;
	last_answer_ = now_ms;
}

bool HostingDetector::Hosting(uint32_t now_ms) const
{
	return answered_ && now_ms - last_answer_ < kHostingForMs;
}

void HostingDetector::OnQuerySent(const uint8_t *packet, size_t size)
{
	if (!IsQuery(packet, size))
		return;
	memcpy(query_, packet, size);
	query_size_ = size;
}

bool KnownHosts::Update(uint64_t id, uint16_t discovery_port, uint32_t now_ms, uint32_t address)
{
	for (int i = 0; i < count_; ++i) {
		if (hosts_[i].id != id)
			continue;
		if (discovery_port == 0) {
			hosts_[i] = hosts_[--count_];
			return false;
		}
		hosts_[i].discovery_port = discovery_port;
		hosts_[i].seen_ms = now_ms;
		if (address)
			hosts_[i].address = address;
		return false;
	}
	if (discovery_port == 0 || count_ >= kMax)
		return false;
	hosts_[count_++] = {id, discovery_port, now_ms, address};
	return true;
}

bool KnownHosts::Has(uint64_t id, int *at) const
{
	for (int i = 0; i < count_; ++i) {
		if (hosts_[i].id == id) {
			if (at)
				*at = i;
			return true;
		}
	}
	return false;
}

void KnownHosts::Expire(uint32_t now_ms)
{
	for (int i = 0; i < count_;) {
		if (now_ms - hosts_[i].seen_ms >= kForgetAfterMs)
			hosts_[i] = hosts_[--count_];
		else
			++i;
	}
}

bool QueryDedup::First(const uint8_t *packet, size_t size, uint32_t now_ms)
{
	if (!packet || size > sizeof(last_))
		return true;
	if (last_size_ == size && memcmp(last_, packet, size) == 0 && now_ms - last_ms_ < kWindowMs)
		return false;
	memcpy(last_, packet, size);
	last_size_ = size;
	last_ms_ = now_ms;
	return true;
}

size_t EncodeLoopback(LoopbackKind kind, uint64_t from, uint16_t channel, const uint8_t *payload, size_t size,
                      uint8_t *out, size_t out_size)
{
	if (!out || kLoopbackHeaderSize + size > out_size || (size && !payload))
		return 0;
	memcpy(out, "H5VL", 4);
	out[4] = kLoopbackVersion;
	out[5] = static_cast<uint8_t>(kind);
	for (int i = 0; i < 8; ++i)
		out[6 + i] = static_cast<uint8_t>(from >> (56 - 8 * i));
	PutBig16(out + 14, channel);
	if (size)
		memcpy(out + kLoopbackHeaderSize, payload, size);
	return kLoopbackHeaderSize + size;
}

bool DecodeLoopback(const uint8_t *data, size_t size, LoopbackDatagram *out)
{
	if (!data || size < kLoopbackHeaderSize || memcmp(data, "H5VL", 4) != 0 || data[4] != kLoopbackVersion)
		return false;
	const uint8_t kind = data[5];
	if (kind != static_cast<uint8_t>(LoopbackKind::kData) && kind != static_cast<uint8_t>(LoopbackKind::kAnnounce))
		return false;
	out->kind = static_cast<LoopbackKind>(kind);
	out->from = 0;
	for (int i = 0; i < 8; ++i)
		out->from = out->from << 8 | data[6 + i];
	out->channel = GetBig16(data + 14);
	out->payload = data + kLoopbackHeaderSize;
	out->payload_size = size - kLoopbackHeaderSize;
	return true;
}

uint64_t LoopbackId(uint16_t port)
{
	return kLoopbackIdBase + port;
}

uint16_t LoopbackPort(uint64_t id)
{
	return (id & ~0xffffull) == kLoopbackIdBase ? static_cast<uint16_t>(id & 0xffffu) : 0;
}

uint64_t ParseId(const char *text)
{
	if (!text || !*text)
		return 0;
	uint64_t value = 0;
	for (; *text; ++text) {
		if (*text < '0' || *text > '9')
			return 0;
		const uint64_t digit = static_cast<uint64_t>(*text - '0');
		if (value > (0xffffffffffffffffull - digit) / 10)
			return 0;  // too big
		value = value * 10 + digit;
	}
	return value;
}

int ParseAddressList(const char *text, uint32_t *out, int max)
{
	int count = 0;
	char word[20];
	for (const char *p = text; p && *p && count < max;) {
		while (*p == ' ' || *p == ',' || *p == ';')
			++p;
		size_t n = 0;
		while (p[n] && p[n] != ' ' && p[n] != ',' && p[n] != ';')
			++n;
		if (n && n < sizeof(word)) {
			memcpy(word, p, n);
			word[n] = 0;
			const uint32_t ip = ParseIp(word);
			if (ip)
				out[count++] = ip;
		}
		p += n;
	}
	return count;
}

void FormatAddressList(const uint32_t *addresses, int count, char *out, size_t size)
{
	if (!size)
		return;
	out[0] = 0;
	for (int i = 0; i < count; ++i) {
		char ip[20];
		FormatIp(addresses[i], ip, sizeof(ip));
		const size_t used = strlen(out);
		if (used + strlen(ip) + 2 > size)
			break;
		snprintf(out + used, size - used, "%s%s", i ? " " : "", ip);
	}
}

namespace {

void CopyText(const char *from, char *to, size_t size)
{
	snprintf(to, size, "%s", from ? from : "");
}

uint16_t ParsePort(const char *text)
{
	const uint64_t value = ParseId(text);
	return value <= 65535 ? static_cast<uint16_t>(value) : 0;
}

}  // namespace

LobbyInfo ParseLobbyInfo(const char *protocol, const char *version, const char *game, const char *host,
                         const char *port, const char *ip, const char *ips)
{
	LobbyInfo info = {};
	const uint64_t p = ParseId(protocol);
	info.protocol = p <= 1000000 ? static_cast<int>(p) : 0;
	CopyText(version, info.version, sizeof(info.version));
	CopyText(game, info.game, sizeof(info.game));
	info.host = ParseId(host);
	info.port = ParsePort(port);
	info.address = ParseIp(ip);
	if (!IsPlayerAddress(info.address))
		info.address = 0;
	info.taken_count = ParseAddressList(ips, info.taken, kMaxTaken);
	return info;
}

HostCheck CheckHost(const LobbyInfo &lobby, uint64_t friend_id, const char *our_game)
{
	if (lobby.protocol == 0)
		return HostCheck::kNoData;
	if (lobby.protocol != kLobbyProtocol)
		return HostCheck::kOtherProtocol;
	if (lobby.host == 0 || lobby.host != friend_id)
		return HostCheck::kNotTheFriend;
	if (lobby.port == 0)
		return HostCheck::kNotHosting;
	if (!our_game || strcmp(lobby.game, our_game) != 0)
		return HostCheck::kOtherGame;
	return HostCheck::kOk;
}

const char *HostCheckName(HostCheck check)
{
	switch (check) {
	case HostCheck::kOk:
		return "a host we can join";
	case HostCheck::kNoData:
		return "no H5 Together data (yet)";
	case HostCheck::kOtherProtocol:
		return "another H5 Together that talks differently";
	case HostCheck::kNotTheFriend:
		return "the friend plays in somebody else's game";
	case HostCheck::kNotHosting:
		return "its game no longer hosts";
	default:
		return "another game build or MMH5.5 version";
	}
}

uint32_t HashBytes(const uint8_t *data, size_t size)
{
	uint32_t h = 0x811c9dc5u;
	for (size_t i = 0; i < size; ++i) {
		h ^= data[i];
		h *= 0x01000193u;
	}
	return h;
}

void FormatGame(const char *build, bool have_55u, uint32_t hash_55u, char *out, size_t size)
{
	if (!size)
		return;
	if (have_55u)
		snprintf(out, size, "%s/%08x", build ? build : "?", static_cast<unsigned>(hash_55u));
	else
		snprintf(out, size, "%s/no-55u", build ? build : "?");
}

bool LobbyPlan::WantOwn(uint32_t now_ms) const
{
	return hosting_port_ != 0 || (hosted_ && in_ == In::kOwn && now_ms - stopped_ms_ < kKeepMs);
}

void LobbyPlan::SetHosting(uint16_t port, uint32_t now_ms)
{
	if (port == 0 && hosting_port_ != 0)
		stopped_ms_ = now_ms;
	if (port)
		hosted_ = true;
	hosting_port_ = port;
}

void LobbyPlan::PlaysWith(uint64_t host, uint64_t lobby, uint32_t now_ms)
{
	if (!host)
		return;
	if (lobby == gone_lobby_)
		lobby = 0;  // (our game still sends to the host that left it)
	if (host != host_) {
		host_ = host;
		host_lobby_ = lobby;
	} else if (lobby) {
		host_lobby_ = lobby;
	}
	played_ = true;
	played_ms_ = now_ms;
}

void LobbyPlan::HeardFrom(uint64_t host, uint32_t now_ms)
{
	if (played_ && host == host_)
		played_ms_ = now_ms;
}

LobbyPlan::Action LobbyPlan::Next(uint32_t now_ms, uint64_t *lobby)
{
	if (waiting_)
		return Action::kNone;
	if (retry_) {
		if (now_ms - retry_ms_ < kRetryMs)
			return Action::kNone;
		retry_ = false;
	}
	if (WantOwn(now_ms)) {
		if (in_ == In::kOwn)
			return Action::kNone;
		if (in_ == In::kJoined)
			return Action::kLeave;
		waiting_ = true;
		return Action::kCreate;
	}
	if (played_ && host_lobby_ && now_ms - played_ms_ < kKeepMs) {
		if (in_ == In::kJoined && lobby_ == host_lobby_)
			return Action::kNone;
		if (in_ != In::kNone)
			return Action::kLeave;
		waiting_ = true;
		if (lobby)
			*lobby = host_lobby_;
		return Action::kJoin;
	}
	return in_ != In::kNone ? Action::kLeave : Action::kNone;
}

void LobbyPlan::Created(uint64_t lobby)
{
	waiting_ = false;
	in_ = In::kOwn;
	lobby_ = lobby;
}

void LobbyPlan::Joined(uint64_t lobby)
{
	waiting_ = false;
	in_ = In::kJoined;
	lobby_ = lobby;
}

void LobbyPlan::Failed(uint32_t now_ms)
{
	waiting_ = false;
	retry_ = true;
	retry_ms_ = now_ms;
}

void LobbyPlan::Left()
{
	in_ = In::kNone;
	lobby_ = 0;
}

void LobbyPlan::Forget()
{
	played_ = false;
	if (host_lobby_)
		gone_lobby_ = host_lobby_;
	host_lobby_ = 0;
}

}  // namespace vlan

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

void PeerTable::Reset(uint64_t self_id)
{
	ids_[0] = self_id;
	addresses_[0] = FakeAddressFor(self_id);
	count_ = 1;
}

uint32_t PeerTable::AddressOf(uint64_t id, bool *added)
{
	if (added)
		*added = false;
	for (int i = 0; i < count_; ++i)
		if (ids_[i] == id)
			return addresses_[i];
	if (count_ == 0 || count_ >= kMax)
		return 0;
	uint32_t ip = FakeAddressFor(id);
	for (bool taken = true; taken;) {
		taken = false;
		for (int i = 0; i < count_ && !taken; ++i)
			taken = addresses_[i] == ip;
		if (taken)
			ip = NextFakeAddress(ip);
	}
	ids_[count_] = id;
	addresses_[count_] = ip;
	++count_;
	if (added)
		*added = true;
	return ip;
}

bool PeerTable::IdOf(uint32_t address, uint64_t *id) const
{
	for (int i = 0; i < count_; ++i) {
		if (addresses_[i] == address) {
			if (id)
				*id = ids_[i];
			return true;
		}
	}
	return false;
}

size_t EncodeMessage(uint16_t source_port, const uint8_t *packet, size_t size, uint8_t *out, size_t out_size)
{
	if (!out || size > kMaxPacket || kHeaderSize + size > out_size || (size && !packet))
		return 0;
	PutBig16(out, source_port);
	if (size)
		memcpy(out + kHeaderSize, packet, size);
	return kHeaderSize + size;
}

bool DecodeMessage(const uint8_t *message, size_t size, uint16_t *source_port, const uint8_t **packet, size_t *packet_size)
{
	if (!message || size < kHeaderSize || size > kMaxMessage)
		return false;
	*source_port = GetBig16(message);
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

bool KnownHosts::Update(uint64_t id, uint16_t discovery_port, uint32_t now_ms)
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
		return false;
	}
	if (discovery_port == 0 || count_ >= kMax)
		return false;
	hosts_[count_++] = {id, discovery_port, now_ms};
	return true;
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

}  // namespace vlan

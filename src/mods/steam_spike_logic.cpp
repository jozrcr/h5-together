#include "steam_spike_logic.h"

#include <algorithm>
#include <string.h>
#include <wchar.h>

namespace steamspike {

Mode ParseMode(const wchar_t *text)
{
	if (!text)
		return Mode::kOff;
	if (wcscmp(text, L"1") == 0 || _wcsicmp(text, L"solo") == 0)
		return Mode::kSolo;
	if (_wcsicmp(text, L"host") == 0)
		return Mode::kHost;
	if (_wcsicmp(text, L"join") == 0)
		return Mode::kJoin;
	return Mode::kOff;
}

const char *ModeName(Mode mode)
{
	switch (mode) {
	case Mode::kSolo: return "solo";
	case Mode::kHost: return "host";
	case Mode::kJoin: return "join";
	default: return "off";
	}
}

const char *InitResultName(int result)
{
	switch (result) {
	case 0: return "OK";
	case 1: return "failed (generic)";
	case 2: return "no Steam client (is Steam running?)";
	case 3: return "Steam client too old";
	default: return "unknown result";
	}
}

const char *AvailabilityName(int availability)
{
	switch (availability) {
	case -102: return "cannot try";
	case -101: return "failed";
	case -100: return "worked before, failing now";
	case -10: return "retrying";
	case 0: return "unknown";
	case 1: return "never tried";
	case 2: return "waiting";
	case 3: return "attempting";
	case 100: return "OK (current)";
	default: return "other";
	}
}

const char *ConnectionStateName(int state)
{
	switch (state) {
	case 0: return "none";
	case 1: return "connecting";
	case 2: return "finding route";
	case 3: return "connected";
	case 4: return "closed by peer";
	case 5: return "problem detected locally";
	case -1: return "fin wait";
	case -2: return "linger";
	case -3: return "dead";
	default: return "other";
	}
}

const char *ResultName(int result)
{
	switch (result) {
	case 1: return "OK";
	case 2: return "Fail";
	case 3: return "NoConnection";
	case 8: return "InvalidParam";
	case 9: return "FileNotFound";
	case 10: return "Busy";
	case 11: return "InvalidState";
	case 15: return "AccessDenied";
	case 16: return "Timeout";
	case 20: return "ServiceUnavailable";
	case 21: return "NotLoggedOn";
	case 25: return "LimitExceeded";
	case 29: return "DuplicateRequest";
	case 35: return "ConnectFailed";
	case 40: return "Blocked";
	case 42: return "NoMatch";
	default: return "other";
	}
}

uint32_t AppIdOfGameId(uint64_t game_id)
{
	return static_cast<uint32_t>(game_id & 0xffffff);
}

namespace {

void Put32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; ++i)
		p[i] = static_cast<uint8_t>(v >> (8 * i));
}

uint32_t Get32(const uint8_t *p)
{
	return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
	       static_cast<uint32_t>(p[3]) << 24;
}

const uint8_t kMagic[4] = {'O', 'H', '5', 'S'};

}  // namespace

size_t EncodePing(const Ping &ping, uint8_t *out, size_t out_size, size_t size)
{
	if (!out || size < kHeaderSize || size > out_size)
		return 0;
	memcpy(out, kMagic, 4);
	out[4] = static_cast<uint8_t>(ping.kind);
	Put32(out + 5, ping.sequence);
	Put32(out + 9, ping.sent_ms);
	for (size_t i = kHeaderSize; i < size; ++i)
		out[i] = static_cast<uint8_t>(i);  // padding the receiver can check
	return size;
}

bool DecodePing(const uint8_t *data, size_t size, Ping *ping)
{
	if (!data || !ping || size < kHeaderSize || memcmp(data, kMagic, 4) != 0)
		return false;
	if (data[4] != static_cast<uint8_t>(Kind::kPing) && data[4] != static_cast<uint8_t>(Kind::kEcho))
		return false;
	for (size_t i = kHeaderSize; i < size; ++i)
		if (data[i] != static_cast<uint8_t>(i))
			return false;
	ping->kind = static_cast<Kind>(data[4]);
	ping->sequence = Get32(data + 5);
	ping->sent_ms = Get32(data + 9);
	return true;
}

void RttStats::Add(uint32_t ms)
{
	if (count_ < kMax)
		values_[count_++] = ms;
}

uint32_t RttStats::Min() const
{
	return count_ ? *std::min_element(values_, values_ + count_) : 0;
}

uint32_t RttStats::Max() const
{
	return count_ ? *std::max_element(values_, values_ + count_) : 0;
}

uint32_t RttStats::Median() const
{
	if (!count_)
		return 0;
	uint32_t sorted[kMax];
	std::copy(values_, values_ + count_, sorted);
	std::sort(sorted, sorted + count_);
	return sorted[count_ / 2];
}

}  // namespace steamspike

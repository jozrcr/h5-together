// The Steam spike's pure work, unit-tested by src/hook/test/steam_spike_test.cpp:
// names for Steam's numbers in the log, the test messages, round-trip
// statistics, the [online] spike setting.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace steamspike {

enum class Mode { kOff, kSolo, kHost, kJoin };

// [online] spike=0|1|host|join (1 = solo). Anything else is off.
Mode ParseMode(const wchar_t *text);
const char *ModeName(Mode mode);

const char *InitResultName(int result);          // ESteamAPIInitResult
const char *AvailabilityName(int availability);  // ESteamNetworkingAvailability
const char *ConnectionStateName(int state);      // ESteamNetworkingConnectionState
const char *ResultName(int result);              // EResult (the usual ones; "other" for the rest)

// A CGameID's app id (its low 24 bits).
uint32_t AppIdOfGameId(uint64_t game_id);

// Test messages: "OH5S", kind, sequence number, the sender's clock, padding.
enum class Kind : uint8_t { kPing = 1, kEcho = 2 };
const size_t kHeaderSize = 13;
struct Ping {
	Kind kind;
	uint32_t sequence;
	uint32_t sent_ms;
};
// Writes a message of `size` bytes (at least kHeaderSize) into `out`; returns its size, 0 if it doesn't fit.
size_t EncodePing(const Ping &ping, uint8_t *out, size_t out_size, size_t size);
// Reads a message; false if it is not one of ours.
bool DecodePing(const uint8_t *data, size_t size, Ping *ping);

// Round trips in ms: count, smallest, median, largest.
class RttStats {
public:
	void Add(uint32_t ms);
	int Count() const { return count_; }
	uint32_t Min() const;
	uint32_t Max() const;
	uint32_t Median() const;

private:
	static const int kMax = 1024;
	uint32_t values_[kMax] = {};
	int count_ = 0;
};

}  // namespace steamspike

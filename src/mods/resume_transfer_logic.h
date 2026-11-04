// Sending a host's resume save to a player who lacks it: the pure work (no game, no
// network), unit-tested by src/hook/test/resume_transfer_test.cpp. The glue is
// resume_transfer.cpp; the plan is docs/plans/turn-features.md, part 2.
//
// In plain words: a player whose game crashed has no copy of the "Reprise" save the
// others made, and the game will not let a PC join a loaded game without its own copy.
// So the PC that lacks it asks the hosts it knows "do you offer a save?"; a host that
// does says which (its size and checksum); if this PC has no save with that size and
// checksum, it asks for the file, a window of pieces at a time, and asks again for the
// pieces that did not come (the robot's loopback is plain UDP and may lose some; Steam
// does not). When every piece is there and the checksum matches, the file goes into
// this PC's saves folder, and the player can join.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace transfer {

const uint16_t kChannel = 0xff52;      // the virtual LAN channel (a port no game socket uses in practice)
const size_t kPiece = 1800;            // bytes of the file per message (a message carries at most ~2 KB)
const unsigned kWindow = 32;           // pieces asked for at once
const uint32_t kMaxFile = 64u << 20;   // a save is at most this big (the owner's biggest: 12 MB)

// --- messages ------------------------------------------------------------------------------------
enum class Kind : uint8_t { kNone = 0, kAsk = 'A', kOffer = 'O', kNoOffer = 'N', kGet = 'G', kData = 'D' };

struct Message {
	Kind kind = Kind::kNone;
	uint32_t size = 0;       // offer: the file's size
	uint32_t crc = 0;        // offer: its CRC-32; get and data: the offer's CRC (which file)
	uint32_t offset = 0;     // get, data: where in the file
	uint32_t count = 0;      // get: how many pieces from offset
	std::string name;        // offer: the file's name on the host (for the log)
	const uint8_t *data = nullptr;  // data: its bytes (inside the decoded buffer)
	size_t data_size = 0;
};

std::vector<uint8_t> EncodeAsk();
std::vector<uint8_t> EncodeOffer(uint32_t size, uint32_t crc, const std::string &name);
std::vector<uint8_t> EncodeNoOffer();
std::vector<uint8_t> EncodeGet(uint32_t crc, uint32_t offset, uint32_t count);
std::vector<uint8_t> EncodeData(uint32_t crc, uint32_t offset, const uint8_t *data, size_t size);
// False when it is not one of ours, or broken.
bool Decode(const uint8_t *buf, size_t size, Message *out);

uint32_t Crc32(const uint8_t *data, size_t size, uint32_t crc = 0);

// --- the receiving side ---------------------------------------------------------------------------
// One file arriving: which pieces came, what to ask for next.
class Receiver {
public:
	Receiver() {}
	// A new file of `size` bytes with this CRC.
	void Start(uint32_t size, uint32_t crc);
	bool Active() const { return size_ != 0 && !done_; }
	uint32_t Crc() const { return crc_; }
	// A piece came: false if it is not for this file or not at a piece's place.
	bool Take(uint32_t crc, uint32_t offset, const uint8_t *data, size_t size);
	// What to ask for now: the first missing piece and how many from there (at most
	// kWindow), or false when nothing is missing. `now_ms`: asks again for a window only
	// after `retry_ms` without progress.
	bool Next(uint32_t now_ms, uint32_t retry_ms, uint32_t *offset, uint32_t *count);
	// Every piece came, and the CRC matches (then `file` is the whole file).
	bool Complete() const;
	const std::vector<uint8_t> &File() const { return file_; }
	unsigned Percent() const;
	void Finish() { done_ = true; }

private:
	uint32_t size_ = 0, crc_ = 0;
	std::vector<uint8_t> file_;
	std::vector<bool> have_;
	unsigned missing_ = 0;
	uint32_t asked_at_ = 0;    // when the last window was asked for
	uint32_t asked_from_ = 0;  // its first piece
	unsigned got_since_ask_ = 0;
	bool asked_ = false, done_ = false;
};

}  // namespace transfer

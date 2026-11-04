// Sending a host's resume save to a player who lacks it: the pure work (see
// resume_transfer_logic.h).
#include "resume_transfer_logic.h"

#include <string.h>

namespace transfer {
namespace {

const uint8_t kMagic[3] = {'H', '5', 'T'};  // every message starts with these, then the kind

void Put32(std::vector<uint8_t> *out, uint32_t v)
{
	for (int i = 0; i < 4; ++i)
		out->push_back(static_cast<uint8_t>(v >> (8 * i)));
}

uint32_t Get32(const uint8_t *p)
{
	return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
	       static_cast<uint32_t>(p[3]) << 24;
}

std::vector<uint8_t> Head(Kind kind)
{
	std::vector<uint8_t> out(kMagic, kMagic + 3);
	out.push_back(static_cast<uint8_t>(kind));
	return out;
}

uint32_t Pieces(uint32_t size)
{
	return static_cast<uint32_t>((size + kPiece - 1) / kPiece);
}

}  // namespace

std::vector<uint8_t> EncodeAsk()
{
	return Head(Kind::kAsk);
}

std::vector<uint8_t> EncodeOffer(uint32_t size, uint32_t crc, const std::string &name)
{
	std::vector<uint8_t> out = Head(Kind::kOffer);
	Put32(&out, size);
	Put32(&out, crc);
	const size_t n = name.size() < 200 ? name.size() : 200;
	out.push_back(static_cast<uint8_t>(n));
	out.insert(out.end(), name.begin(), name.begin() + n);
	return out;
}

std::vector<uint8_t> EncodeNoOffer()
{
	return Head(Kind::kNoOffer);
}

std::vector<uint8_t> EncodeGet(uint32_t crc, uint32_t offset, uint32_t count)
{
	std::vector<uint8_t> out = Head(Kind::kGet);
	Put32(&out, crc);
	Put32(&out, offset);
	Put32(&out, count);
	return out;
}

std::vector<uint8_t> EncodeData(uint32_t crc, uint32_t offset, const uint8_t *data, size_t size)
{
	std::vector<uint8_t> out = Head(Kind::kData);
	Put32(&out, crc);
	Put32(&out, offset);
	out.insert(out.end(), data, data + size);
	return out;
}

bool Decode(const uint8_t *buf, size_t size, Message *out)
{
	*out = Message();
	if (!buf || size < 4 || memcmp(buf, kMagic, 3) != 0)
		return false;
	const Kind kind = static_cast<Kind>(buf[3]);
	const uint8_t *p = buf + 4;
	const size_t left = size - 4;
	switch (kind) {
	case Kind::kAsk:
	case Kind::kNoOffer:
		if (left != 0)
			return false;
		break;
	case Kind::kOffer: {
		if (left < 9 || left != 9u + p[8])
			return false;
		out->size = Get32(p);
		out->crc = Get32(p + 4);
		out->name.assign(reinterpret_cast<const char *>(p + 9), p[8]);
		if (out->size == 0 || out->size > kMaxFile)
			return false;
		break;
	}
	case Kind::kGet:
		if (left != 12)
			return false;
		out->crc = Get32(p);
		out->offset = Get32(p + 4);
		out->count = Get32(p + 8);
		if (out->count == 0 || out->count > kWindow || out->offset % kPiece != 0)
			return false;
		break;
	case Kind::kData:
		if (left < 8 || left - 8 > kPiece)
			return false;
		out->crc = Get32(p);
		out->offset = Get32(p + 4);
		out->data = p + 8;
		out->data_size = left - 8;
		break;
	default:
		return false;
	}
	out->kind = kind;
	return true;
}

uint32_t Crc32(const uint8_t *data, size_t size, uint32_t crc)
{
	crc = ~crc;
	for (size_t i = 0; i < size; ++i) {
		crc ^= data[i];
		for (int k = 0; k < 8; ++k)
			crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

void Receiver::Start(uint32_t size, uint32_t crc)
{
	size_ = size;
	crc_ = crc;
	file_.assign(size, 0);
	const uint32_t n = Pieces(size);
	have_.assign(n, false);
	missing_ = n;
	asked_ = false;
	done_ = false;
	got_since_ask_ = 0;
}

bool Receiver::Take(uint32_t crc, uint32_t offset, const uint8_t *data, size_t size)
{
	if (!Active() || crc != crc_ || offset % kPiece != 0 || offset >= size_)
		return false;
	const uint32_t index = offset / kPiece;
	const size_t expected = size_ - offset < kPiece ? size_ - offset : kPiece;
	if (size != expected)
		return false;
	if (!have_[index]) {
		memcpy(&file_[offset], data, size);
		have_[index] = true;
		--missing_;
		++got_since_ask_;
	}
	return true;
}

bool Receiver::Next(uint32_t now_ms, uint32_t retry_ms, uint32_t *offset, uint32_t *count)
{
	if (!Active() || missing_ == 0)
		return false;
	uint32_t first = 0;
	while (first < have_.size() && have_[first])
		++first;
	// A window is out: wait until all of it came (then the next one at once), or retry_ms
	// passed (then again from the first missing piece).
	if (asked_) {
		uint32_t end = asked_from_ + kWindow;
		if (end > have_.size())
			end = static_cast<uint32_t>(have_.size());
		bool window_done = true;
		for (uint32_t i = asked_from_; i < end && window_done; ++i)
			window_done = have_[i];
		if (!window_done && now_ms - asked_at_ < retry_ms)
			return false;
	}
	uint32_t n = 0;
	while (first + n < have_.size() && n < kWindow)
		++n;
	asked_ = true;
	asked_at_ = now_ms;
	asked_from_ = first;
	got_since_ask_ = 0;
	*offset = static_cast<uint32_t>(first * kPiece);
	*count = n;
	return true;
}

bool Receiver::Complete() const
{
	return size_ != 0 && missing_ == 0 && Crc32(file_.data(), file_.size()) == crc_;
}

unsigned Receiver::Percent() const
{
	if (have_.empty())
		return 0;
	return static_cast<unsigned>((have_.size() - missing_) * 100 / have_.size());
}

}  // namespace transfer

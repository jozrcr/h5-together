// Test of sending a resume save to a player who lacks it (src/mods/resume_transfer_logic.cpp):
// the messages, the CRC, and a whole file through a lossy line. No game needed.
#include <stdio.h>
#include <string.h>

#include <vector>

#include "../../mods/resume_transfer_logic.h"

using namespace transfer;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

bool Round(const std::vector<uint8_t> &bytes, Message *m)
{
	return Decode(bytes.data(), bytes.size(), m);
}

// Sends a file from a "host" to a Receiver through a line that loses every `lose`-th
// message (0: none). Returns the number of rounds it took.
int Transfer(const std::vector<uint8_t> &file, unsigned lose, Receiver *rx)
{
	const uint32_t crc = Crc32(file.data(), file.size());
	rx->Start(static_cast<uint32_t>(file.size()), crc);
	unsigned sent = 0;
	uint32_t now = 1000;
	int rounds = 0;
	while (!rx->Complete() && rounds < 100000) {
		++rounds;
		now += 50;
		uint32_t offset, count;
		if (!rx->Next(now, 400, &offset, &count))
			continue;
		Message get;
		if (!Round(EncodeGet(crc, offset, count), &get))
			return -1;
		for (uint32_t i = 0; i < get.count; ++i) {
			const uint32_t at = get.offset + i * static_cast<uint32_t>(kPiece);
			if (at >= file.size())
				break;
			const size_t n = file.size() - at < kPiece ? file.size() - at : kPiece;
			const std::vector<uint8_t> data = EncodeData(crc, at, &file[at], n);
			if (lose && ++sent % lose == 0)
				continue;  // lost on the way
			Message m;
			if (Round(data, &m) && m.kind == Kind::kData)
				rx->Take(m.crc, m.offset, m.data, m.data_size);
		}
	}
	return rounds;
}

}  // namespace

int main()
{
	Message m;
	Check(Round(EncodeAsk(), &m) && m.kind == Kind::kAsk, "ask");
	Check(Round(EncodeNoOffer(), &m) && m.kind == Kind::kNoOffer, "no offer");
	Check(Round(EncodeOffer(1144127, 0xdeadbeef, "sauvegarde_3.sav"), &m) && m.kind == Kind::kOffer && m.size == 1144127 &&
	          m.crc == 0xdeadbeef && m.name == "sauvegarde_3.sav",
	      "offer: size, CRC, name");
	Check(!Round(EncodeOffer(0, 1, "x"), &m) && !Round(EncodeOffer(kMaxFile + 1, 1, "x"), &m), "offer: empty or too big: refused");
	Check(Round(EncodeGet(7, 3 * kPiece, 5), &m) && m.kind == Kind::kGet && m.crc == 7 && m.offset == 3 * kPiece && m.count == 5,
	      "get");
	Check(!Round(EncodeGet(7, 5, 5), &m) && !Round(EncodeGet(7, 0, kWindow + 1), &m) && !Round(EncodeGet(7, 0, 0), &m),
	      "get: not at a piece, too many, none: refused");
	const uint8_t bytes[3] = {1, 2, 3};
	Check(Round(EncodeData(7, kPiece, bytes, 3), &m) && m.kind == Kind::kData && m.offset == kPiece && m.data_size == 3 &&
	          m.data[2] == 3,
	      "data");
	const uint8_t junk[8] = {'H', '5', 'X', 'A', 0, 0, 0, 0}, wrong_kind[4] = {'H', '5', 'T', 'Z'};
	Check(!Decode(junk, 8, &m) && !Decode(wrong_kind, 4, &m) && !Decode(nullptr, 0, &m), "others' bytes: not ours");
	std::vector<uint8_t> ask = EncodeAsk();
	ask.push_back(0);
	Check(!Round(ask, &m), "a message too long: refused");

	const char *text = "123456789";
	Check(Crc32(reinterpret_cast<const uint8_t *>(text), 9) == 0xcbf43926u, "CRC-32 of \"123456789\" is cbf43926");
	Check(Crc32(reinterpret_cast<const uint8_t *>(text) + 4, 5, Crc32(reinterpret_cast<const uint8_t *>(text), 4)) == 0xcbf43926u,
	      "CRC-32 in two parts");

	std::vector<uint8_t> file(1144127);
	for (size_t i = 0; i < file.size(); ++i)
		file[i] = static_cast<uint8_t>(i * 131 + (i >> 9));
	Receiver rx;
	int rounds = Transfer(file, 0, &rx);
	Check(rx.Complete() && rx.File() == file && rx.Percent() == 100, "a 1.1 MB save, nothing lost: complete and the same");
	printf("      (%d rounds)\n", rounds);
	Receiver lossy;
	rounds = Transfer(file, 7, &lossy);
	Check(lossy.Complete() && lossy.File() == file, "every 7th message lost: still complete and the same");
	printf("      (%d rounds)\n", rounds);
	Receiver tiny;
	const std::vector<uint8_t> small(10, 0x5a);
	Transfer(small, 0, &tiny);
	Check(tiny.Complete() && tiny.File() == small, "a file smaller than a piece");
	Receiver exact;
	const std::vector<uint8_t> two(2 * kPiece, 0x11);
	Transfer(two, 3, &exact);
	Check(exact.Complete() && exact.File() == two, "a file of exactly two pieces, losses");

	Receiver r;
	r.Start(static_cast<uint32_t>(file.size()), 99);
	Check(!r.Take(98, 0, file.data(), kPiece), "a piece of another file: refused");
	Check(!r.Take(99, 5, file.data(), kPiece), "a piece not at a piece's place: refused");
	Check(!r.Take(99, 0, file.data(), 10), "a piece of the wrong size: refused");
	Check(r.Take(99, 0, file.data(), kPiece) && r.Percent() == 0, "a right piece: taken");
	for (uint32_t at = 0; at < file.size(); at += kPiece)
		r.Take(99, at, &file[at], file.size() - at < kPiece ? file.size() - at : kPiece);
	Check(!r.Complete(), "every piece but the CRC does not match: not complete");

	printf("%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
	return g_failures ? 1 : 0;
}

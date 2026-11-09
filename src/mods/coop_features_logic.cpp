// The co-op features' agreement, pure work (see coop_features_logic.h).
#include "coop_features_logic.h"

namespace features {

// The value: kMark in bits 24-31, the version in bits 16-23, what the sender knows in
// bits 8-15, what it wants in bits 0-3, the computer bonus in bits 4-6.
uint32_t EncodeHello(unsigned knows, unsigned wants, unsigned ai_bonus)
{
	return kMark << 24 | (kVersion & 0xff) << 16 | (knows & 0xff) << 8 | (ai_bonus & 7) << 4 | (wants & 0x0f);
}

Hello DecodeHello(uint32_t value)
{
	Hello h;
	if (value >> 24 != kMark || ((value >> 16) & 0xff) == 0)
		return h;
	h.valid = true;
	h.version = (value >> 16) & 0xff;
	h.knows = (value >> 8) & 0xff;
	h.wants = value & 0x0f;
	h.ai_bonus = (value >> 4) & 7;
	return h;
}

unsigned Decide(const Hello *hellos, int pcs)
{
	if (pcs < 1 || !hellos[0].valid)
		return 0;
	const unsigned wants = hellos[0].wants & kWantable;
	unsigned all_know = 0xff;
	for (int pc = 0; pc < pcs; ++pc)
		all_know &= hellos[pc].valid ? hellos[pc].knows : 0;
	const unsigned parallel = 1u << kParallelAi;
	unsigned on = wants & all_know & kKnown;
	if ((wants & parallel) && (all_know & (1u << kParallelAiStarts)))
		on |= parallel;  // asked with its old bit, known as the fixed version's
	return on & kWantable;
}

const char *Name(int feature)
{
	switch (feature) {
	case kScriptRhythm:
		return "script_rhythm";
	case kParallelAi:
		return "parallel_ai";
	case kAiBonus:
		return "ai_bonus";
	case kEndlessTurns:
		return "endless_turns";
	case kParallelAiStarts:
		return "parallel_ai_starts";
	default:
		return "?";
	}
}

}  // namespace features

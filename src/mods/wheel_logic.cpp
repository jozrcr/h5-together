// The mouse wheel fix's rules (see wheel_logic.h).
#include "wheel_logic.h"

namespace wheel {

Converter::Converter(Mode start)
	: mode_(start), position_(0), game_z_(0), sum_(0), state_(0), baseline_(false), steps_(0), positions_(0), checks_(0),
	  peek_(false), batch_position_(0), batch_sum_(0), batch_last_(0), batch_count_(0)
{
}

void Converter::Begin(bool peek)
{
	peek_ = peek;
	batch_position_ = position_;
	batch_sum_ = 0;
	batch_last_ = 0;
	batch_count_ = 0;
}

LONG Converter::Next(LONG raw)
{
	++batch_count_;
	batch_sum_ += raw;
	if (mode_ != kConverting) {
		batch_last_ = raw;
		return raw;
	}
	batch_position_ += raw;
	batch_last_ = batch_position_;
	return batch_position_;
}

void Converter::End()
{
	if (peek_ || !batch_count_)
		return;
	if (mode_ == kConverting)
		position_ = batch_position_;
	else
		sum_ += batch_sum_;
	game_z_ = batch_last_;
}

LONG Converter::State(LONG lz)
{
	if (mode_ == kConverting)
		lz = position_;
	game_z_ = lz;
	return lz;
}

void Converter::Acquired()
{
	baseline_ = false;
	sum_ = 0;
}

Vote Converter::Check(LONG last_raw, LONG lz)
{
	if (mode_ != kWatching)
		return kNoVote;
	++checks_;
	// Steps add up to the state (since the last check, or since the Acquire that
	// set it back to 0); positions do not, and the state is the last one.
	const bool sum_explains = lz == sum_ || (baseline_ && lz - state_ == sum_);
	Vote vote = kNoVote;
	if (lz != last_raw && sum_explains)
		vote = kStepsVote;
	else if (lz == last_raw && !sum_explains)
		vote = kPositionsVote;
	if (vote == kStepsVote)
		++steps_;
	else if (vote == kPositionsVote)
		++positions_;
	state_ = lz;
	baseline_ = true;
	sum_ = 0;
	if (steps_ >= 2 && steps_ > 2 * positions_) {
		mode_ = kConverting;
		position_ = game_z_;  // go on from what the game has: no jump
	}
	else if (positions_ >= 3 || checks_ >= kMaxChecks) {
		mode_ = kAsIs;
	}
	return vote;
}

}  // namespace wheel

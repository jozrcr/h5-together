// The mouse wheel fix for Wine (mods/wheel.cpp): its rules, apart from the game
// so that test/wheel_test.cpp can check them.
//
// The game sets its DirectInput mouse to absolute axis mode and works out the
// wheel's motion itself, as "this value minus the last one". Windows then
// reports positions (the running sum of the notches); Wine reports steps
// (+-120 each). The Converter gives the game positions: the running sum of the
// steps, in its events and in GetDeviceState's lZ.
//
// Under Wine with wheel_fix=auto it first watches: after a batch of wheel events
// the device's own state is read. With positions, lZ is the last event's value;
// with steps, it is their sum (Wine keeps lZ as the sum of the steps since the
// mouse was acquired).
#pragma once
#include <windows.h>

namespace wheel {

enum Mode {
	kWatching,    // comparing the events with the device's state
	kAsIs,        // the game gets the events as they are
	kConverting,  // the events are steps: the game gets positions
};

// What one check said.
enum Vote { kNoVote, kStepsVote, kPositionsVote };

class Converter {
public:
	explicit Converter(Mode start);

	Mode mode() const { return mode_; }
	bool WantsCheck() const { return mode_ == kWatching; }

	// One GetDeviceData result: Begin, then Next for each wheel (DIMOFS_Z) event
	// in order, which returns the value the game gets, then End.
	// peek (DIGDD_PEEK, the game only looks): nothing is kept.
	void Begin(bool peek);
	LONG Next(LONG raw);
	void End();

	// GetDeviceState: the lZ the game gets.
	LONG State(LONG lz);

	// The mouse was acquired (Wine sets its lZ back to 0 then).
	void Acquired();

	// While watching, after a batch the game read whole (the buffer was not
	// full) with at least one wheel event: `last_raw` = that batch's last wheel
	// value as the device gave it, `lz` = the device's state read right after.
	Vote Check(LONG last_raw, LONG lz);

	int checks() const { return checks_; }
	int steps_votes() const { return steps_; }
	int positions_votes() const { return positions_; }
	LONG position() const { return position_; }

	static const int kMaxChecks = 16;  // then the events stay as they are

private:
	Mode mode_;
	LONG position_;  // converting: the position the game got last
	LONG game_z_;    // the last wheel value the game got (events or state)
	LONG sum_;       // watching: the sum of the wheel values since the last check
	LONG state_;     // watching: lZ at the last check
	bool baseline_;  // state_ is valid (no Acquire since)
	int steps_, positions_, checks_;
	// the batch being read
	bool peek_;
	LONG batch_position_, batch_sum_, batch_last_;
	int batch_count_;
};

}  // namespace wheel

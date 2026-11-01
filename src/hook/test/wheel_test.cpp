// Test of the mouse wheel fix for Wine (src/mods/wheel_logic.cpp): Wine's wheel
// steps given to the game as positions, and the check that decides it. A small
// model of the game's own reading (0x00c42530: "this value minus the last one",
// and a press, bit 7 of the low byte, dropped while its cursor is outside its
// window), of Windows' mouse (positions) and of Wine's (steps; its lZ is their
// sum since the last Acquire). No game needed.
#include <windows.h>
#include <stdio.h>

#include "../../mods/wheel_logic.h"

using wheel::Converter;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

const LONG kNotch = 120;

// The game's view of the wheel axis.
struct Game {
	LONG prev = 0;
	LONG motion = 0;           // the sum of the motions it worked out (zoom: + in, - out)
	bool drop_presses = false;  // the cursor is "outside" its window: values with bit 7 set are dropped
	void Event(LONG v)
	{
		if (drop_presses && (v & 0x80))
			return;
		motion += v - prev;
		prev = v;
	}
	void Resync(LONG lz)  // GetDeviceState after an Acquire (0x00c420b0)
	{
		motion += lz - prev;
		prev = lz;
	}
};

// A mouse: `steps` = Wine (events are the steps), else Windows (positions).
struct Mouse {
	bool steps;
	LONG lz = 0;
	LONG Turn(LONG step)
	{
		lz += step;
		return steps ? step : lz;
	}
	void Acquire()
	{
		if (steps)
			lz = 0;  // Wine sets its state back on Acquire
	}
};

// One frame: the game reads these notches as one batch, then (while watching)
// the device's state is checked.
void Frame(Converter &c, Game &g, Mouse &m, const LONG *notches, int n)
{
	LONG raw[16];
	for (int i = 0; i < n; ++i)
		raw[i] = m.Turn(notches[i]);
	c.Begin(false);
	for (int i = 0; i < n; ++i)
		g.Event(c.Next(raw[i]));
	c.End();
	if (n && c.WantsCheck())
		c.Check(raw[n - 1], m.lz);
}

void One(Converter &c, Game &g, Mouse &m, LONG notch)
{
	Frame(c, g, m, &notch, 1);
}

void TestConverting()
{
	Converter c(wheel::kConverting);
	Mouse m{true};
	Game g;
	One(c, g, m, kNotch);
	One(c, g, m, kNotch);
	One(c, g, m, -kNotch);
	Check(g.prev == kNotch && g.motion == kNotch, "converting: up, up, down = one notch in, values 120 240 120");
	c.Begin(true);  // a peek
	Check(c.Next(-kNotch) == 0, "converting: a peek sees the next position");
	c.End();
	c.Begin(false);
	Check(c.Next(-kNotch) == 0, "converting: a peek keeps nothing");
	c.End();
	Check(c.State(12345) == 0, "converting: GetDeviceState's lZ is the position the game got");

	// Wine's down steps (-120 = ...88, bit 7 set) with the game dropping presses:
	// positions still carry the motion.
	Converter c2(wheel::kConverting);
	Game g2;
	g2.drop_presses = true;
	Mouse m2{true};
	for (int i = 0; i < 10; ++i)
		One(c2, g2, m2, -kNotch);
	Check(g2.motion <= -9 * kNotch, "converting: 10 notches down, presses dropped: the game still zooms out (9-10 notches)");
}

void TestTheBugWithoutFix()
{
	Converter c(wheel::kAsIs);
	Mouse m{true};
	Game g;
	g.drop_presses = true;
	for (int i = 0; i < 5; ++i)
		One(c, g, m, kNotch);
	const LONG in = g.motion;
	for (int i = 0; i < 10; ++i)
		One(c, g, m, -kNotch);
	Check(in == kNotch && g.motion == kNotch,
	      "no fix (the bug): 5 notches up zoom in once, 10 notches down do nothing");
}

// The Linux player's case: the game drops presses (its cursor check fails).
void TestWatchingWineDropped()
{
	Converter c(wheel::kWatching);
	Mouse m{true};
	Game g;
	g.drop_presses = true;
	int notches = 0;
	while (c.mode() == wheel::kWatching && notches < 20) {
		One(c, g, m, -kNotch);
		++notches;
	}
	const LONG before = g.motion;
	for (int i = 0; i < 6; ++i)
		One(c, g, m, -kNotch);
	const LONG out = g.motion - before;
	for (int i = 0; i < 6; ++i)
		One(c, g, m, kNotch);
	const LONG in = g.motion - before - out;
	// Every other value is dropped as a press: the next one carries the motion,
	// so at most a notch lags at each end.
	Check(c.mode() == wheel::kConverting && out <= -5 * kNotch && in >= 4 * kNotch,
	      "watching, Wine, presses dropped: after the check the wheel zooms out and in again");
}

void TestWatchingWine()
{
	Converter c(wheel::kWatching);
	Mouse m{true};
	Game g;
	m.Acquire();
	c.Acquired();
	g.Resync(c.State(m.lz));
	int notches = 0;
	while (c.mode() == wheel::kWatching && notches < 20) {
		One(c, g, m, -kNotch);
		++notches;
	}
	Check(c.mode() == wheel::kConverting && notches <= 4, "watching, Wine: scrolling down is turned into positions within 4 notches");
	const LONG before = g.motion;
	for (int i = 0; i < 6; ++i)
		One(c, g, m, -kNotch);
	Check(g.motion - before == -6 * kNotch, "watching, Wine: afterwards 6 notches down zoom out 6 notches (no jump)");
	const LONG mid = g.motion;
	for (int i = 0; i < 4; ++i)
		One(c, g, m, kNotch);
	Check(g.motion - mid == 4 * kNotch, "watching, Wine: and 4 notches up zoom in 4 notches");
	// A later Acquire (the window lost and got the focus): the game reads the state.
	m.Acquire();
	c.Acquired();
	const LONG at = g.motion;
	g.Resync(c.State(m.lz));
	Check(g.motion == at, "watching, Wine: a new Acquire makes no jump");
}

void TestWatchingWineAlternating()
{
	Converter c(wheel::kWatching);
	Mouse m{true};
	Game g;
	const LONG pattern[] = {kNotch, -kNotch, kNotch, -kNotch, kNotch, -kNotch, kNotch, -kNotch};
	for (LONG n : pattern)
		One(c, g, m, n);
	Check(c.mode() == wheel::kConverting, "watching, Wine: up and down in turn is recognized too");
}

void TestWatchingWineBatches()
{
	Converter c(wheel::kWatching);
	Mouse m{true};
	Game g;
	const LONG two[] = {-kNotch, -kNotch};
	for (int i = 0; i < 4 && c.mode() == wheel::kWatching; ++i)
		Frame(c, g, m, two, 2);
	Check(c.mode() == wheel::kConverting, "watching, Wine: two notches per frame are recognized");
}

void TestWatchingWindows()
{
	Converter c(wheel::kWatching);
	Mouse m{false};
	Game g;
	for (int i = 0; i < 30; ++i)
		One(c, g, m, i % 3 == 0 ? kNotch : -kNotch);
	Check(c.mode() == wheel::kAsIs && c.steps_votes() == 0, "watching, positions (Windows-like): never converted, the check stops");
	Check(g.motion == m.lz, "watching, positions: the game got every notch as is");

	Converter c2(wheel::kWatching);
	Mouse m2{false};
	Game g2;
	const LONG two[] = {kNotch, kNotch};
	for (int i = 0; i < 3; ++i)
		Frame(c2, g2, m2, two, 2);
	Check(c2.mode() == wheel::kAsIs && c2.positions_votes() == 3, "watching, positions: two notches per frame say positions");
	Check(g2.motion == 6 * kNotch, "watching, positions: and the game got all six");
}

void TestAsIs()
{
	Converter c(wheel::kAsIs);
	c.Begin(false);
	Check(c.Next(-kNotch) == -kNotch && c.Next(240) == 240, "as is: values unchanged");
	c.End();
	Check(c.State(77) == 77 && c.Check(1, 2) == wheel::kNoVote, "as is: state unchanged, no check");
}

}  // namespace

int main()
{
	TestConverting();
	TestTheBugWithoutFix();
	TestWatchingWine();
	TestWatchingWineDropped();
	TestWatchingWineAlternating();
	TestWatchingWineBatches();
	TestWatchingWindows();
	TestAsIs();
	if (g_failures) {
		printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	printf("mouse wheel: all checks passed\n");
	return 0;
}

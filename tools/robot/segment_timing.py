#!/usr/bin/env python3
"""Where co-op time goes between two PCs: their h5-together.log files side by side.

Every 200 segments each PC logs when it saw each player's report of that segment, on its
own clock (src/mods/lockstep.cpp, "lockstep: segment N reported, seen here at ..."):

    lockstep: segment 4200 reported, seen here at (ms, this PC's clock): 0(me) 51234.5, 1 51390.2

For a segment both logs have, with A and B the two PCs:

    there and back = (A saw B's report - A's own report) + (B saw A's report - B's own report)

is the time the reports spend on the way, both ways added (inside each game, our hooks,
Steam and the network): the two clocks' offset cancels out. Compare it with twice the
ping. If it is close, the reports are quick, and a PC that waits is waiting for a PC that
is behind. If it is much larger, the reports themselves are slow.

    python tools/robot/segment_timing.py <log of PC A> <log of PC B>

Each log can be a whole h5-together.log: the last game in it is used (the lines after the
last "H5 Together hook DLL" line). Plain Python 3, no packages.
"""
import re
import statistics
import sys

LINE = re.compile(r"lockstep: segment (\d+) reported, seen here at \(ms, this PC's clock\): (.*)$")
ENTRY = re.compile(r"(\d+)(\(me\))? (-?[\d.]+)")


def last_game(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    start = 0
    for i, line in enumerate(lines):
        if "H5 Together hook DLL" in line:
            start = i
    return lines[start:]


def segments(path):
    """{segment: (me, {player: ms})} from the last game of one log."""
    out = {}
    for line in last_game(path):
        m = LINE.search(line)
        if not m:
            continue
        me, seen = None, {}
        for e in ENTRY.finditer(m.group(2)):
            player, mine, ms = int(e.group(1)), bool(e.group(2)), float(e.group(3))
            seen[player] = ms
            if mine:
                me = player
        if me is not None:
            out[int(m.group(1))] = (me, seen)
    return out


def describe(values):
    values = sorted(values)
    p80 = values[min(len(values) - 1, len(values) * 4 // 5)]
    return "median %.0f ms, p80 %.0f ms, max %.0f ms" % (statistics.median(values), p80, values[-1])


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = segments(sys.argv[1]), segments(sys.argv[2])
    common = sorted(set(a) & set(b))
    if not common:
        sys.exit("no segment is in both logs (the timing lines came with H5 Together after 0.3.0)")
    me_a, me_b = a[common[0]][0], b[common[0]][0]
    print("PC A is player %d, PC B is player %d; %d segments in both logs" % (me_a, me_b, len(common)))
    there_and_back, a_waits, b_waits = [], [], []
    for n in common:
        _, sa = a[n]
        _, sb = b[n]
        if me_b not in sa or me_a not in sb or sa[me_b] <= 0 or sb[me_a] <= 0 or sa[me_a] <= 0 or sb[me_b] <= 0:
            continue
        a_wait = sa[me_b] - sa[me_a]  # A's own report to B's report seen at A
        b_wait = sb[me_a] - sb[me_b]
        there_and_back.append(a_wait + b_wait)
        a_waits.append(a_wait)
        b_waits.append(b_wait)
    if not there_and_back:
        sys.exit("the segments in both logs miss a report time")
    print("there and back (both ways added, clocks cancel): " + describe(there_and_back))
    print("A: B's report came after A's own by: " + describe(a_waits))
    print("B: A's report came after B's own by: " + describe(b_waits))
    half = statistics.median(there_and_back) / 2
    lag = (statistics.median(a_waits) - statistics.median(b_waits)) / 2
    print("so, if the way is as long both ways: each way about %.0f ms, and %s reports about %.0f ms after %s"
          % (half, "B" if lag >= 0 else "A", abs(lag), "A" if lag >= 0 else "B"))


if __name__ == "__main__":
    main()

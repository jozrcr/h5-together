"""Where do addresses and ports sit inside the game's LAN packets? (online plan, step 1)

Usage:
    python tools/robot/packet_map.py build/robot/<run>-lan
    python tools/robot/packet_map.py --last 1

Reads both copies' logs of a robot LAN run made with whole packets and the
address scan on:
    python tools/robot/run_lan_test.py --net-log-bytes 2048 --net-scan [--vlan]
For each kind of packet (its first byte, its socket's port, its direction) it
lists the sizes seen and, at each byte offset, the IPv4 addresses found there
(this PC's own, the virtual LAN's fake 198.18.x.y / 198.19.x.y, 127.0.0.1) and
the game ports (8888 to 8891, when most packets of the kind have them), with how
many of the packets of that kind had it. The packets are the first ones of each
socket and direction (net_log_packets) plus every packet the scan logged whole
because it held an address of this PC. Then the scan's own count, over every
packet. See docs/subsystems/multiplayer-sync.md, "Addresses inside the packets".
"""
import argparse
import collections
import pathlib
import re

REPO = pathlib.Path(__file__).resolve().parents[2]
PORTS = (8888, 8889, 8890, 8891)

PACKET = re.compile(r"robot: net: (sendto|recvfrom)\((\d+)(?:, \d+ bytes [0-9a-f]*, ([\d.:?]+))?\) -> -?\d+"
                    r"(?: bytes [0-9a-f]* from ([\d.:?]+))?")
SCANNED = re.compile(r"address scan: a \d+-byte [0-9a-f]{2} packet (sent to|received from) ([\d.:?]+) \(socket (\d+)\)")
DUMP = re.compile(r"robot: net:   \+([0-9a-f]{4}) ([0-9a-f]+)$")
OWN = re.compile(r"address scan: this PC's addresses: (.*)$")
BIND = re.compile(r"robot: net: bind\((\d+), [\d.]+:(\d+)\)")
TOTALS = re.compile(r"address scan: (\d+ packets sent.*)$")


def read(log):
    """One copy's packets: [direction, socket, peer, bytes, from the scan], its addresses, its sockets' ports
    and the scan's last count."""
    packets, own, ports, totals = [], {}, {}, None
    current = None
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        m = DUMP.search(line)
        if m:
            if current is not None:
                current[3].extend(bytes.fromhex(m.group(2)))
            continue
        current = None
        if m := OWN.search(line):
            for item in m.group(1).split(", "):
                ip, _, name = item.partition(" (")
                own[ip] = name[:-1] if name.endswith(")") else name
        elif m := BIND.search(line):
            ports[m.group(1)] = int(m.group(2))
        elif m := TOTALS.search(line):
            totals = m.group(1)
        elif m := SCANNED.search(line):
            current = ["sent" if m.group(1) == "sent to" else "received", m.group(3), m.group(2), bytearray(), True]
            packets.append(current)
        elif m := PACKET.search(line):
            direction = "sent" if m.group(1) == "sendto" else "received"
            current = [direction, m.group(2), m.group(3) if direction == "sent" else m.group(4), bytearray(), False]
            packets.append(current)
    # a packet both logged and scanned comes twice in a row: keep one
    kept = [p for i, p in enumerate(packets)
            if not (p[4] and i and packets[i - 1][:4] == p[:4])]
    return kept, own, ports, totals


def labels(own):
    found = {}
    for ip, name in own.items():
        b = bytes(int(x) for x in ip.split("."))
        found[b] = "127.0.0.1" if ip == "127.0.0.1" else "this PC's address (%s)" % name
    return found


def report(run):
    print("==", run.name)
    for copy in ("host", "client"):
        log = run / copy / "h5-together.log"
        if not log.exists():
            continue
        packets, own, ports, totals = read(log)
        with_bytes = [p for p in packets if p[3]]
        print("-- %s: %d packets with their bytes (%d of them logged by the scan); this PC has %d addresses" %
              (copy, len(with_bytes), sum(1 for p in with_bytes if p[4]), len(own)))
        kinds = collections.defaultdict(list)
        for direction, sock, peer, data, _scanned in with_bytes:
            kinds[(data[0], ports.get(sock, "?"), direction)].append((peer, bytes(data)))
        look = labels(own)
        for (first, port, direction), items in sorted(kinds.items(), key=lambda kv: (kv[0][0], str(kv[0][1]), kv[0][2])):
            sizes = collections.Counter(len(d) for _, d in items)
            peers = collections.Counter(p for p, _ in items)
            print("  %02x on port %s, %s: %d packets, sizes %s; %s %s" % (
                first, port, direction, len(items), ", ".join("%d x%d" % kv for kv in sorted(sizes.items())),
                "to" if direction == "sent" else "from", ", ".join("%s x%d" % kv for kv in peers.most_common(4))))
            at = collections.Counter()
            for _, data in items:
                seen = set()
                for needle, label in look.items():
                    start = data.find(needle)
                    while start >= 0:
                        seen.add((start, label))
                        start = data.find(needle, start + 1)
                for i in range(len(data) - 3):
                    if data[i] == 198 and data[i + 1] in (18, 19):
                        seen.add((i, "fake address %d.%d.%d.%d" % tuple(data[i:i + 4])))
                for p in PORTS:
                    for order, b in (("big endian", p.to_bytes(2, "big")), ("little endian", p.to_bytes(2, "little"))):
                        start = data.find(b)
                        while start >= 0:
                            seen.add((start, "port %d (%s)" % (p, order)))
                            start = data.find(b, start + 1)
                at.update(seen)
            for (offset, label), n in sorted(at.items()):
                if label.startswith("port") and n < max(2, (len(items) + 1) // 2):
                    continue  # two bytes match by chance in big packets
                print("      byte %4d: %s (%d of %d)" % (offset, label, n, len(items)))
        if totals:
            print("  the scan, every packet:", totals)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("runs", nargs="*")
    p.add_argument("--last", type=int, default=0, help="the last N LAN runs in build/robot")
    args = p.parse_args()
    runs = [pathlib.Path(r) for r in args.runs]
    if args.last:
        runs += sorted((REPO / "build" / "robot").glob("*-lan"))[-args.last:]
    for r in runs:
        report(r)


if __name__ == "__main__":
    main()

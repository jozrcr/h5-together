"""Write a hero looks file (open-homm5-skins.ini format) for robot runs: every
hero of a faction gets the look of one hero of that faction, so whatever heroes
a map gives the robot's players, they show a look that stands out.

    python tools\\robot\\make_skins_file.py out.ini [--game C:\\Games\\H5_test] [--look TOWN_HEAVEN=Freyda ...]

Reads the hero files from the test copy's data\\*.pak (the newest copy of each
file wins, as in the game); nothing is written into the game folder. The file
it writes holds hero names only (each hero's InternalName).
"""
import argparse
import os
import re
import sys
import zipfile

LOOKS = {  # a unique, easy to see look per faction (docs/plans/hero-skins.md, section 2)
    "TOWN_HEAVEN": "Freyda",
    "TOWN_ACADEMY": "Zehir",
    "TOWN_DUNGEON": "Raelag",
    "TOWN_FORTRESS": "Wulfstan",
    "TOWN_INFERNO": "Agrael",
    "TOWN_NECROMANCY": "Giovanni",
    "TOWN_PRESERVE": "Heam",
    "TOWN_STRONGHOLD": "Quroq",
}


def heroes(game):
    best = {}
    data = os.path.join(game, "data")
    for name in sorted(os.listdir(data)):
        if not name.lower().endswith(".pak"):
            continue
        z = zipfile.ZipFile(os.path.join(data, name))
        for i in z.infolist():
            key = i.filename.lower().replace("\\", "/")
            if key.endswith(".(advmapheroshared).xdb") and (key not in best or i.date_time > best[key][1].date_time):
                best[key] = (z, i)
    out = []
    for key in sorted(best):
        z, info = best[key]
        raw = z.read(info)
        text = raw.decode("utf-16") if raw[:2] == b"\xff\xfe" else raw.decode("utf-8", "replace")
        name = re.search(r"<InternalName>([^<]*)<", text)
        town = re.search(r"<TownType>([^<]*)<", text)
        if name and town:
            out.append((name.group(1), town.group(1)))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("out")
    p.add_argument("--game", default=r"C:\Games\H5_test")
    p.add_argument("--look", action="append", default=[], metavar="TOWN=HERO", help="another look for a faction")
    args = p.parse_args()
    looks = dict(LOOKS)
    for item in args.look:
        town, _, hero = item.partition("=")
        looks[town.strip()] = hero.strip()
    found = heroes(args.game)
    names = {n for n, _ in found}
    lines = ["; hero looks for a robot run (tools/robot/make_skins_file.py)", "[skins]"]
    count = 0
    for name, town in found:
        look = looks.get(town)
        if look and look in names and look != name:
            lines.append("%s=%s" % (name, look))
            count += 1
    with open(args.out, "w", encoding="ascii") as f:
        f.write("\n".join(lines) + "\n")
    print("%d heroes, %d looks chosen -> %s" % (len(found), count, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())

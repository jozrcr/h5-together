"""Our own texts (the add-on's panels and messages, src/mods/texts/*.txt).

    python tools/lang/ui_texts.py check            # what build.ps1 -Test runs
    python tools/lang/ui_texts.py new de           # a starter file for a translator: build/texts/de.txt
    python tools/lang/ui_texts.py long             # the long test language: build/texts/xx.txt

check (fails on any error):
  - en.txt and every translation read without a problem (the same rules as the DLL,
    src/hook/texts_logic.cpp: "key = text" lines in [sections], UTF-8);
  - every text key the code uses (src/hook, src/mods: a string literal "section.key"
    whose section is one of en.txt's, or the first argument of texts::Get / Format)
    is in en.txt, and every key of en.txt is used ([language] is for the files only);
  - no translation has a key en.txt lacks, and each line has the same {names} and
    <tags> as the English one. Lines a translation lacks are listed (they show in
    English), not errors.

new: en.txt with the header's language line set, for a translator to start from.
long: every English text 40% longer with accents ("Ŧëxt ~~~~"), placeholders and tags
kept: with [ui] language=xx the robot's screenshots show which panels cut long words.
These are our own texts only; the game's texts (tools/lang/*.py, the private packs) are
something else and never go into these files.
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TEXTS = REPO / "src" / "mods" / "texts"
CODE = [REPO / "src" / "hook", REPO / "src" / "mods"]
MARK = re.compile(r"\{[a-z0-9_]+\}|<[^<>]*>")
KEY = re.compile(r"[a-z0-9_.]+")
FILES_ONLY = ("language.",)  # keys for the files themselves, not used by the code

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def parse(path):
    """{key: text}, [problems] (texts_logic.cpp's Parse)."""
    raw = Path(path).read_bytes()
    problems = []
    if raw.startswith(b"\xff\xfe"):
        text = raw[2:].decode("utf-16-le", errors="replace")
    elif raw.startswith(b"\xef\xbb\xbf"):
        text = raw[3:].decode("utf-8", errors="surrogateescape")
    else:
        text = raw.decode("utf-8", errors="surrogateescape")
    texts, section = {}, ""
    for n, line in enumerate(text.split("\n"), 1):
        line = line.rstrip("\r").strip(" \t")
        if not line or line[0] in "#;":
            continue
        try:
            line.encode("utf-8")
        except UnicodeEncodeError:
            problems.append(f"line {n}: not UTF-8")
            continue
        if line.startswith("["):
            name = line[1:-1].strip() if line.endswith("]") else ""
            if not name or not KEY.fullmatch(name) or "." in name:
                problems.append(f"line {n}: {line!r} is not a section name")
                section = ""
            else:
                section = name
            continue
        if "=" not in line:
            problems.append(f"line {n}: no '='")
            continue
        key, value = (x.strip(" \t") for x in line.split("=", 1))
        if not KEY.fullmatch(key) or key.startswith(".") or key.endswith("."):
            problems.append(f"line {n}: {key!r} is not a key")
        elif not section:
            problems.append(f"line {n}: {key} is not in a [section]")
        elif not value:
            problems.append(f"line {n}: {section}.{key} has no text")
        elif f"{section}.{key}" in texts:
            problems.append(f"line {n}: {section}.{key} is there twice")
        else:
            texts[f"{section}.{key}"] = value.replace("\\\\", "\0").replace("\\n", "\n").replace("\0", "\\")
    return texts, problems


def marks(text):
    return sorted(MARK.findall(text))


def literals(source):
    """The string literals of a C++ file, comments left out, with the text before each."""
    out, i, n = [], 0, len(source)
    while i < n:
        c = source[i]
        if source.startswith("//", i):
            i = source.find("\n", i)
            i = n if i < 0 else i
        elif source.startswith("/*", i):
            i = source.find("*/", i)
            i = n if i < 0 else i + 2
        elif c == "'":
            i += 1
            while i < n and source[i] != "'":
                i += 2 if source[i] == "\\" else 1
            i += 1
        elif c == '"':
            j = i + 1
            while j < n and source[j] != '"':
                j += 2 if source[j] == "\\" else 1
            out.append((source[max(0, i - 24):i], source[i + 1:j]))
            i = j + 1
        else:
            i += 1
    return out


def used_keys(sections):
    used = {}
    for folder in CODE:
        for path in sorted(list(folder.glob("*.cpp")) + list(folder.glob("*.h"))):
            for before, lit in literals(path.read_text(encoding="utf-8")):
                if re.search(r"#\s*include\s*$", before):
                    continue
                called = re.search(r"texts::(Get|Format)\(\s*$", before)
                if called or (KEY.fullmatch(lit) and "." in lit and lit.split(".", 1)[0] in sections and not lit.endswith(".")):
                    used.setdefault(lit, path.name)
    return used


def check():
    errors, notes = [], []
    en, problems = parse(TEXTS / "en.txt")
    errors += [f"en.txt: {p}" for p in problems]
    sections = {k.split(".", 1)[0] for k in en}
    used = used_keys(sections)
    for key, where in sorted(used.items()):
        if key not in en:
            errors.append(f"{where}: the text key {key!r} is not in en.txt")
    for key in sorted(en):
        if key not in used and not key.startswith(FILES_ONLY):
            errors.append(f"en.txt: {key} is not used by the code (an old text? remove it)")
    languages = sorted(p for p in TEXTS.glob("*.txt") if p.name != "en.txt")
    for path in languages:
        texts, problems = parse(path)
        errors += [f"{path.name}: {p}" for p in problems]
        for key, text in sorted(texts.items()):
            if key not in en:
                errors.append(f"{path.name}: {key} is not in en.txt (an old or misspelt key?)")
            elif marks(text) != marks(en[key]):
                errors.append(f"{path.name}: {key}: its {{names}} and <tags> {marks(text)} are not the English ones {marks(en[key])}")
        missing = [k for k in en if k not in texts]
        if missing:
            notes.append(f"{path.name}: {len(missing)} of {len(en)} texts not translated (they show in English): "
                         + ", ".join(missing[:10]) + (" ..." if len(missing) > 10 else ""))
        else:
            notes.append(f"{path.name}: all {len(en)} texts translated")
    for n in notes:
        print("  " + n)
    for e in errors:
        print("  ERROR " + e)
    print(f"texts: en.txt {len(en)} texts, {len(used)} keys used by the code, {len(languages)} translation(s): "
          + ("OK" if not errors else f"{len(errors)} error(s)"))
    return 0 if not errors else 1


def new(code, out):
    text = (TEXTS / "en.txt").read_text(encoding="utf-8")
    text = re.sub(r"(?m)^# H5 Together texts: English$", f"# H5 Together texts: ({code})", text, count=1)
    text = re.sub(r"(?m)^# language: en$", f"# language: {code}", text, count=1)
    text = re.sub(r"(?m)^# translated by: .*$", "# translated by: (your name)", text, count=1)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b"\xef\xbb\xbf" + text.replace("\n", "\r\n").encode("utf-8"))
    print(f"wrote {out}: translate the text right of each '=', then put it in the game's bin\\h5-together\\texts\\")
    return 0


ACCENTS = str.maketrans("aeiouAEIOUcnsyTz", "àéîõüÀÉÎÕÜçñšÿŦž")


def longer(text):
    parts = MARK.split(text)
    kept = MARK.findall(text)
    out = ""
    for i, p in enumerate(parts):
        out += p.translate(ACCENTS)
        if i < len(kept):
            out += kept[i]
    plain = len(MARK.sub("", text))
    return out + (" " + "~" * max(1, round(plain * 0.4) - 1) if plain else "")


def long(out):
    en, _ = parse(TEXTS / "en.txt")
    lines = ["# H5 Together texts: the long test language (tools/lang/ui_texts.py long), never shipped",
             "# language: xx", ""]
    section = None
    for key, text in en.items():
        sec, name = key.split(".", 1)
        if sec != section:
            lines += ["", f"[{sec}]"]
            section = sec
        value = text if key == "language.red_player" else longer(text)
        lines.append(f"{name} = " + value.replace("\\", "\\\\").replace("\n", "\\n"))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(("\n".join(lines) + "\n").encode("utf-8"))
    print(f"wrote {out} ({len(en)} texts): copy it to the test copy's bin\\h5-together\\texts\\ and set [ui] language=xx")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="what", required=True)
    sub.add_parser("check")
    n = sub.add_parser("new")
    n.add_argument("code", help="the language's code: de, ru, pl, pt-br...")
    n.add_argument("--out", type=Path)
    lg = sub.add_parser("long")
    lg.add_argument("--out", type=Path, default=REPO / "build" / "texts" / "xx.txt")
    args = ap.parse_args()
    if args.what == "check":
        return check()
    if args.what == "new":
        if not re.fullmatch(r"[a-z]{2,3}(-[a-z0-9]{1,8})?", args.code):
            sys.exit(f"{args.code!r} is not a language code (2 or 3 small letters, then maybe -xx: de, pt-br)")
        return new(args.code, args.out or REPO / "build" / "texts" / f"{args.code}.txt")
    return long(args.out)


if __name__ == "__main__":
    sys.exit(main())

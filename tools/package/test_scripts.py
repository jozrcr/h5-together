#!/usr/bin/env python3
"""Tests the install and uninstall scripts of the package against FAKE folders.

    python tools/package/test_scripts.py [--bash PATH] [--work-dir DIR] [--keep]

It builds the zip with make_package.py (version "0.0.0-test"), unzips it, makes a fake
Steam folder and fake game folders (empty files named like the game's exes), and
runs the scripts from the unzipped package. It never touches a real game folder.

- Windows scripts (install.bat / install.ps1, uninstall.bat / uninstall.ps1):
  run only on Windows.
- Linux scripts (install.sh / uninstall.sh): need a bash. On Windows pass Git
  Bash with --bash "C:\\Program Files\\Git\\bin\\bash.exe" (the bash that comes
  with WSL does not work with Windows paths). The scripts find Steam below
  $HOME, so the tests point HOME at a fake home folder.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GAME = "Heroes of Might and Magic 5 Tribes of the East"
OURS_MARK = b"open-homm5 hook DLL"

failures = []
checks = 0


sys.stdout.reconfigure(errors="replace")


def check(ok, label, detail=""):
    global checks
    checks += 1
    print(("  ok    " if ok else "  FAIL  ") + label)
    if not ok:
        failures.append(label)
        if detail:
            print("        " + detail.replace("\n", "\n        "))


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def snapshot(folder):
    return {p.name: sha(p) for p in Path(folder).iterdir() if p.is_file()}


def tree(folder):
    """Everything below folder, files and folders: {relative path: sha256, or None for a folder}."""
    folder = Path(folder)
    return {p.relative_to(folder).as_posix(): (sha(p) if p.is_file() else None) for p in sorted(folder.rglob("*"))}


def make_game(library, exes=("H5_Game.exe", "MMH55_Utility_64.exe")):
    """A fake game folder with empty-ish exes. Returns its bin folder."""
    b = Path(library) / "steamapps" / "common" / GAME / "bin"
    b.mkdir(parents=True, exist_ok=True)
    for e in exes:
        (b / e).write_bytes(b"fake " + e.encode())
    (b / "data.txt").write_bytes(b"pretend game data")
    return b


def make_steam(steam, libraries, escape_backslashes=True):
    """A fake Steam folder whose libraryfolders.vdf lists the given library folders."""
    steam = Path(steam)
    (steam / "steamapps").mkdir(parents=True, exist_ok=True)
    lines = ['"libraryfolders"', "{"]
    for i, lib in enumerate(libraries):
        text = str(lib)
        if escape_backslashes:
            text = text.replace("\\", "\\\\")
        lines += [f'\t"{i}"', "\t{", f'\t\t"path"\t\t"{text}"', '\t\t"label"\t\t""',
                  '\t\t"apps"', "\t\t{", '\t\t\t"12345"\t\t"678"', "\t\t}", "\t}"]
    lines.append("}")
    # Steam writes UTF-8 without a byte-order mark
    (steam / "steamapps" / "libraryfolders.vdf").write_bytes(("\n".join(lines) + "\n").encode("utf-8"))


def bat(path, *args):
    """A command line that runs a .bat file as a double-click would, whatever is in its path."""
    return 'cmd /s /c "' + subprocess.list2cmdline([str(path), *args]) + '"'


def run(cmd, stdin="", env=None, cwd=None):
    p = subprocess.run(cmd, input=stdin.encode("utf-8"), capture_output=True, env=env, cwd=cwd, timeout=180)
    out = (p.stdout + p.stderr).decode("utf-8", errors="replace")
    return p.returncode, out


# ---------------------------------------------------------------- templates

TEMPLATES_SRC = REPO / "src" / "mods" / "rmg" / "templates"


def template_dir(pkg):
    return Path(pkg) / "open-homm5" / "templates"


def same_bytes(folder, src_folder, names):
    """True if every name exists in both folders with the same bytes."""
    return all((Path(folder) / n).is_file() and (Path(folder) / n).read_bytes() == (Path(src_folder) / n).read_bytes()
               for n in names)


def package_checks(pkg, zip_path):
    """The ready templates are in the package, byte for byte (UTF-8 with BOM and CRLF)."""
    print("\nTemplates in the package")
    wanted = sorted(p.name for p in TEMPLATES_SRC.glob("*.xdb"))
    packed = sorted(p.name for p in template_dir(pkg).iterdir()) if template_dir(pkg).is_dir() else []
    check(len(wanted) >= 16, f"src/mods/rmg/templates holds {len(wanted)} templates (16 or more)")
    check(packed == wanted, "package: open-homm5/templates holds every src/mods/rmg/templates/*.xdb and nothing else", f"{packed}\n{wanted}")
    check(same_bytes(template_dir(pkg), TEMPLATES_SRC, wanted), "package: the templates are byte for byte the source files")
    data = [(template_dir(pkg) / n).read_bytes() for n in packed]
    check(all(d.startswith(b"\xef\xbb\xbf") and b"\r\n" in d and b"\n" not in d.replace(b"\r\n", b"") for d in data),
          "package: the templates keep their UTF-8 BOM and CRLF line endings")
    with zipfile.ZipFile(zip_path) as z:
        entries = [i for i in z.infolist() if "/open-homm5/templates/" in i.filename]
        check(len(entries) == len(wanted) and all(i.filename.count("/") == 3 for i in entries),
              "package: the templates sit in the zip under <top folder>/open-homm5/templates/")
        check(all(((i.external_attr >> 16) & 0o777) == 0o644 for i in entries), "package: the templates are plain data files (mode 644)")


def licence_checks(pkg, zip_path):
    """Our licence and Dear ImGui's (the only third-party code in the DLL) travel with the DLL."""
    print("\nLicence notices in the package")
    imgui_dir = REPO / "src" / "hook" / "third_party" / "imgui"
    lf = lambda raw: raw.replace(b"\r\n", b"\n")
    check(lf((pkg / "LICENSE.txt").read_bytes()) == lf((REPO / "LICENSE").read_bytes()),
          "licence: LICENSE.txt is the LICENSE of the repo, word for word")
    raw = (pkg / "THIRD-PARTY-NOTICES.txt").read_bytes()
    text = lf(raw).decode("ascii", errors="replace")
    imgui_licence = lf((imgui_dir / "LICENSE.txt").read_bytes()).decode("ascii").strip("\n")
    check(imgui_licence in text, "licence: THIRD-PARTY-NOTICES.txt carries Dear ImGui's LICENSE.txt word for word")
    version = re.search(r'#define IMGUI_VERSION\s+"([^"]+)"', (imgui_dir / "imgui.h").read_text(encoding="utf-8")).group(1)
    check(f"Dear ImGui {version}" in text, f"licence: the notices name the Dear ImGui version we ship ({version})")
    stb = sorted(p.name for p in imgui_dir.glob("imstb_*.h"))
    check(stb == ["imstb_rectpack.h", "imstb_textedit.h", "imstb_truetype.h"],
          "licence: Dear ImGui still bundles exactly the 3 stb files the notices list", str(stb))
    check("stb_truetype" in text and "stb_rect_pack" in text and "stb_textedit" in text and "Sean Barrett" in text,
          "licence: the notices cover the stb files, by Sean Barrett")
    check("Tristan Grimmer" in text and "ProggyClean" in text, "licence: the notices cover the ProggyClean font built into Dear ImGui")
    check(b"@" not in raw and all(b < 0x80 for b in raw) and raw.count(b"\r\n") == raw.count(b"\n"),
          "licence: the notices are plain ASCII with CRLF line endings and no unreplaced placeholder")
    readme = (pkg / "README.txt").read_text(encoding="ascii")
    check("LICENSE.txt" in readme and "THIRD-PARTY-NOTICES.txt" in readme, "licence: README.txt tells where the licences are")
    with zipfile.ZipFile(zip_path) as z:
        names = {i.filename.split("/", 1)[1] for i in z.infolist() if "/" in i.filename}
        check({"LICENSE.txt", "THIRD-PARTY-NOTICES.txt"} <= names, "licence: both are in the zip, next to README.txt")


def template_tests(who, pkg, game_bin, install, uninstall):
    """The templates in a fake game folder: install, a player's own files, reinstall, uninstall.

    install() and uninstall() run the scripts for game_bin and return (exit code, output)."""
    print(f"\nRandom map templates ({who})")
    src = template_dir(pkg)
    ours = sorted(p.name for p in src.iterdir())
    n = len(ours)
    tdir = game_bin / "open-homm5" / "templates"
    before = tree(game_bin)

    def names():
        return sorted(p.name for p in tdir.iterdir()) if tdir.is_dir() else None

    code, out = install()
    check(code == 0, "templates: fresh install: exit 0", out)
    check(names() == ours and same_bytes(tdir, src, ours), "templates: fresh install: exactly our templates, byte for byte, in bin/open-homm5/templates")
    check(f"installed {n} random map templates" in out, "templates: fresh install: the summary says how many were installed", out)

    # a player's own files, and ours changed or missing
    mine = {"my-own.xdb": b"my own template", "OH-mine.xdb": b"mine too, but named like ours", "notes.txt": b"notes"}
    for name, data in mine.items():
        (tdir / name).write_bytes(data)
    (tdir / ours[0]).write_bytes(b"changed by hand")
    (tdir / ours[1]).unlink()
    code, out = install()
    check(code == 0, "templates: reinstall: exit 0", out)
    check(same_bytes(tdir, src, ours), "templates: reinstall overwrites ours (the changed one) and brings back the missing one")
    check(all((tdir / k).read_bytes() == v for k, v in mine.items()), "templates: reinstall keeps the player's own files untouched")
    check(names() == sorted(ours + list(mine)), "templates: reinstall adds nothing else", str(names()))
    check(f"installed {n} random map templates" in out and f"{n - 1} of them replaced" in out and "left the 3 other" in out,
          "templates: reinstall: the summary counts installed, replaced and left alone", out)

    # uninstall removes only ours
    code, out = uninstall()
    check(code == 0 and not (game_bin / "dinput8.dll").exists(), "templates: uninstall: exit 0", out)
    check(names() == sorted(mine) and all((tdir / k).read_bytes() == v for k, v in mine.items()),
          "templates: uninstall removes only our templates; the player's own files stay", str(names()))
    check(f"removed {n} random map templates" in out, "templates: uninstall says how many it removed", out)

    # with nothing of the player's left, uninstall also removes the empty folders
    for name in mine:
        (tdir / name).unlink()
    code, out = install()
    code2, out2 = uninstall()
    check(code == 0 and code2 == 0 and not (game_bin / "open-homm5").exists(), "templates: uninstall removes the empty open-homm5 folders", out2)
    check(tree(game_bin) == before, "templates: after install and uninstall the game folder is exactly as it was")

    # a folder open-homm5 that holds something else stays
    other = game_bin / "open-homm5" / "other.txt"
    other.parent.mkdir()
    other.write_bytes(b"not ours")
    install()
    code, out = uninstall()
    check(code == 0 and other.read_bytes() == b"not ours" and not tdir.exists(),
          "templates: a folder open-homm5 with other things in it stays; the empty templates folder goes", out)
    other.unlink()
    other.parent.rmdir()

    # nothing installed: nothing to do, no error
    code, out = uninstall()
    check(code == 0 and "no random map templates" in out and tree(game_bin) == before, "templates: uninstall with nothing installed: no error, nothing changed", out)


# ---------------------------------------------------------------- the settings file

# A player's open-homm5.ini from 0.2.3 (that version's file, with a few values set by
# hand, builder_last as the DLL writes it, a setting this version does not know, and
# two sections added by hand).
OLD_INI = REPO / "tools" / "package" / "test_data" / "open-homm5-0.2.3.ini"
KEPT = "; kept from your previous file:"


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("ascii")


def mentioned(text):
    """{(section, key)} of the lines ";key=..." and "key=..." (names in lower case)."""
    sec, out = "", set()
    for line in text.replace("\r\n", "\n").split("\n"):
        t = line.strip(" \t")
        if t.startswith("[") and "]" in t:
            sec = t[1:t.index("]")].strip(" \t").lower()
            continue
        m = re.match(r";?([A-Za-z0-9_]+)[ \t]*=", t)
        if m:
            out.add((sec, m.group(1).lower()))
    return out


def replaced(text, pairs):
    for a, b in pairs:
        if text.count(a) != 1:
            check(False, f"settings: test setup: the package's ini should have {a.strip()!r} once")
        text = text.replace(a, b)
    return text


def settings_tests(who, pkg, game_bin, install, uninstall):
    """The settings file: refreshed from the package's, the player's values kept, backups.

    install() and uninstall(answer) run the scripts for game_bin and return (exit code,
    output). Returns {case: the refreshed file's bytes}, to compare Windows with Linux."""
    print(f"\nThe settings file ({who})")
    ini = game_bin / "open-homm5.ini"
    bak = [game_bin / f"open-homm5.ini.{e}" for e in ("bak", "bak2", "bak3")]
    tpl_bytes = (pkg / "open-homm5.ini").read_bytes()
    tpl = tpl_bytes.decode("ascii").replace("\r\n", "\n")
    results = {}

    def backups():
        return [p.read_bytes() if p.exists() else None for p in bak]

    def plain(data):
        return all(b < 0x80 for b in data) and b"\n" not in data.replace(b"\r\n", b"")

    # fresh install: the package's file, no backup
    code, out = install()
    check(code == 0 and ini.read_bytes() == tpl_bytes and backups() == [None] * 3,
          "settings: fresh install: the package's open-homm5.ini, no backup", out)
    code, out = install()
    check(code == 0 and ini.read_bytes() == tpl_bytes and backups() == [None] * 3 and "already up to date (every setting at its default)" in out,
          "settings: install again over the untouched file: nothing changes, no backup", out)

    # a 0.2.3 file with the player's values: refreshed into this version's file
    old = crlf(OLD_INI.read_text(encoding="ascii"))
    ini.write_bytes(old)
    code, out = install()
    want = replaced(tpl, [
        ("\n;view_delay_ms=100\n", "\nview_delay_ms=50\n"),
        ("\n;net_sleep_ms=2\n", f"\nnet_sleep_ms=1\n\n{KEPT}\nfast_turns=1\n"),
        ("\n;language=auto\n", "\nlanguage=fr\n"),
        ("\n;builder_last=\n", "\nbuilder_last=OH-X1-321110nnxxxxxxxa\n"),
        ("\n;original_music=0\n", "\noriginal_music=1\n"),
        ("\n;texts=game\n", "\ntexts=fr\n"),
    ]) + f"\n[mine]\n{KEPT}\nnote=hello\n"
    new = len(mentioned(tpl) - mentioned(old.decode("ascii")))
    got = ini.read_bytes()
    results["0.2.3 file"] = got
    check(code == 0 and got == crlf(want), "settings: a 0.2.3 file: every value kept in its line, the rest is this version's file",
          f"{out}\n--- got:\n{got.decode('ascii', 'replace')}")
    check(plain(got), "settings: the refreshed file is plain ASCII with CRLF line endings, like the package's")
    check(f"settings file refreshed: 8 of your settings kept, {new} new settings available (see the comments)" in out and new > 30,
          "settings: the summary counts the settings kept and the new ones", out)
    check("2 settings not listed in this version" in out and "open-homm5.ini.bak" in out,
          "settings: the summary names the unknown settings and the backup", out)
    check(backups() == [old, None, None], "settings: the previous file is saved as open-homm5.ini.bak, byte for byte")

    # again: nothing to do, no second backup
    code, out = install()
    check(code == 0 and ini.read_bytes() == got and backups() == [old, None, None],
          "settings: a second install changes nothing and makes no new backup", out)
    check("settings file already up to date: 8 of your settings kept" in out, "settings: a second install says the file is up to date", out)

    # odd files: a byte-order mark, LF, a setting before any section, other upper/lower
    # case and spaces, a second copy, comments that are not settings, a line without "="
    odd = (b"\xef\xbb\xbftop=1\n[NET]\n  Tuning = 0  \ntuning=1\n#keepalive_ms=5\n; keepalive_ms=7\n"
           b"lonely line\n[ai]\nfast_paths=0\n")
    ini.write_bytes(odd)
    code, out = install()
    want = replaced(tpl, [
        ("same settings.\n\n[net]\n", f"same settings.\n\n{KEPT}\ntop=1\n\n[net]\n"),
        ("\n;tuning=1\n", "\ntuning=0\n"),
        ("\n;net_sleep_ms=2\n", f"\n;net_sleep_ms=2\n\n{KEPT}\ntuning=1\nlonely line\n"),
        ("\n;fast_paths=1\n", "\nfast_paths=0\n"),
    ])
    got = ini.read_bytes()
    results["odd file"] = got
    check(code == 0 and got == crlf(want), "settings: odd file (BOM, LF, a setting before [net], case, spaces, a second copy): as expected",
          f"{out}\n--- got:\n{got.decode('ascii', 'replace')}")
    check(f"4 of your settings kept, {len(mentioned(tpl)) - 2} new settings available" in out and "1 setting not listed" in out,
          "settings: odd file: the counts", out)
    check(backups() == [odd, old, None], "settings: a second refresh keeps the older backup as open-homm5.ini.bak2")

    # more refreshes: only the last three backups stay
    ini.write_bytes(b"")
    code, out = install()
    results["empty file"] = ini.read_bytes()
    check(code == 0 and ini.read_bytes() == tpl_bytes and backups() == [b"", odd, old],
          "settings: an empty file becomes the package's file; three backups", out)
    one = b"[net]\r\ntuning=0\r\n"
    ini.write_bytes(one)
    code, out = install()
    results["one setting"] = ini.read_bytes()
    check(code == 0 and ini.read_bytes() == crlf(replaced(tpl, [("\n;tuning=1\n", "\ntuning=0\n")])),
          "settings: a file with one setting: refreshed", out)
    check(backups() == [one, b"", odd] and not (game_bin / "open-homm5.ini.bak4").exists(),
          "settings: a fourth refresh drops the oldest backup (three are kept)")

    # a "Unicode" (UTF-16) file is left alone
    current, saved = ini.read_bytes(), backups()
    utf16 = "[net]\r\ntuning=0\r\n".encode("utf-16")
    ini.write_bytes(utf16)
    code, out = install()
    check(code == 0 and ini.read_bytes() == utf16 and backups() == saved and "not a plain text file" in out,
          "settings: a UTF-16 file is kept as it is, no backup", out)
    ini.write_bytes(current)

    # uninstall: one question for the file and its backups
    code, out = uninstall("n\n")
    check(code == 0 and ini.exists() and backups() == saved and "its old copies" in out,
          "settings: uninstall (n): the file and its backups stay", out)
    code, out = uninstall("y\n")
    check(code == 0 and not ini.exists() and backups() == [None] * 3, "settings: uninstall (y): the file and its backups are removed", out)
    bak[1].write_bytes(b"old copy")
    code, out = uninstall("y\n")
    check(code == 0 and not bak[1].exists() and "removed open-homm5.ini.bak2" in out, "settings: uninstall (y) with only a backup left: removed", out)
    return results


# ---------------------------------------------------------------- the bug report script

# What the report may hold: these four files and system.txt, nothing else.
REPORT_FILES = ("open-homm5.log", "open-homm5.old.log", "open-homm5.ini", "open-homm5-skins.ini")
RC19K_DOC = REPO / "docs" / "plans" / "mmh55-rc19k.md"


def package_version(pkg):
    """The version the package's dinput8.dll says in its text, like 0.2.7."""
    return re.search(rb"open-homm5 hook DLL (\S+) loaded", (Path(pkg) / "dinput8.dll").read_bytes()).group(1).decode("ascii")


def make_report_game(pkg, library, add_on=True, pak_version="19k"):
    """A fake game folder for the bug report tests. Returns its bin folder.

    With add_on: our dinput8.dll, a log, an older log, the settings and the skins file. Always
    MMH5.5's version pak, a 55u.dll, and things that must NOT end up in a report (a save, a map,
    an old copy of the settings, another file in bin)."""
    b = make_game(library)
    root = b.parent
    if add_on:
        shutil.copy(Path(pkg) / "dinput8.dll", b / "dinput8.dll")
        shutil.copy(Path(pkg) / "open-homm5.ini", b / "open-homm5.ini")
        (b / "open-homm5.log").write_bytes(b"       0 [ 1234] open-homm5 hook DLL test loaded\n    1000 [ 1234] a line of the log\n")
        (b / "open-homm5.old.log").write_bytes(b"       0 [ 4321] an older log\n")
        (b / "open-homm5-skins.ini").write_bytes(b"[skins]\nhero=1\n")
    (b / "55u.dll").write_bytes(b"pretend 55u.dll")
    (b / "other-mod.asi").write_bytes(b"pretend asi")
    (b / "secret.txt").write_bytes(b"not for the report")
    (b / "open-homm5.ini.bak").write_bytes(b"an old copy of the settings")
    (root / "Profiles").mkdir(exist_ok=True)
    (root / "Profiles" / "game1.sav").write_bytes(b"a save")
    (root / "Maps").mkdir(exist_ok=True)
    (root / "Maps" / "x.h5m").write_bytes(b"a map")
    (root / "data").mkdir(exist_ok=True)
    with zipfile.ZipFile(root / "data" / "MMH55-Index.pak", "w") as z:
        z.writestr("scripts/Other.lua", "-- not it\n")
        if pak_version:
            z.writestr("scripts/H55-Version.lua", f'H55_Version = "{pak_version}"\n-- more\n')
    return b


def read_report(path):
    """{name: bytes} of a .zip or .tar.gz report."""
    path = Path(path)
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            return {i.filename: z.read(i) for i in z.infolist()}
    import tarfile
    with tarfile.open(path) as t:
        return {m.name: t.extractfile(m).read() for m in t.getmembers() if m.isfile()}


def report_files(folder, prefix="open-homm5-bug-report-"):
    return sorted(p for p in Path(folder).iterdir() if p.name.startswith(prefix)) if Path(folder).is_dir() else []


def bug_report_package_checks(pkg, zip_path):
    print("\nBug report script in the package")
    with zipfile.ZipFile(zip_path) as z:
        info = {i.filename.split("/", 1)[1]: i for i in z.infolist() if "/" in i.filename}
    check({"bug-report.bat", "bug-report.ps1", "bug-report.sh"} <= set(info), "bug report: the 3 files are in the zip")
    check(((info["bug-report.sh"].external_attr >> 16) & 0o777) == 0o755, "bug report: bug-report.sh is executable in the zip")
    for name in ("bug-report.bat", "bug-report.ps1"):
        raw = (pkg / name).read_bytes()
        check(raw.count(b"\r\n") == raw.count(b"\n") and raw.count(b"\n") > 0 and all(b < 0x80 for b in raw), f"bug report: {name} is plain ASCII with CRLF line endings")
    raw = (pkg / "bug-report.sh").read_bytes()
    check(b"\r" not in raw and all(b < 0x80 for b in raw), "bug report: bug-report.sh is plain ASCII with LF line endings")
    readme = (pkg / "README.txt").read_text(encoding="ascii")
    check("bug-report.bat" in readme and "bug-report.sh" in readme, "bug report: README.txt tells how to make one")
    # the 55u.dll hashes that name an MMH5.5 version come from the notes in docs/plans/mmh55-rc19k.md
    row = next((l for l in RC19K_DOC.read_text(encoding="utf-8").splitlines() if "`bin\\55u.dll`" in l), "")
    hashes = re.findall(r"`([0-9a-f]{64})`", row)
    check(len(hashes) == 2, "bug report: docs/plans/mmh55-rc19k.md still lists the 55u.dll hashes of RC19d and RC19k", row)
    if len(hashes) == 2:
        forms = {"bug-report.ps1": '{h}" = "{v}"', "bug-report.sh": '{h}) echo "{v}"'}
        for script, form in forms.items():
            text = (pkg / script).read_text(encoding="ascii")
            check(form.format(h=hashes[0], v="RC19d") in text and form.format(h=hashes[1], v="RC19k") in text,
                  f"bug report: {script} names 55u.dll {hashes[0][:8]}... RC19d and {hashes[1][:8]}... RC19k")


def system_text(files):
    return files["system.txt"].decode("utf-8-sig").replace("\r\n", "\n")


def bug_report_tests(who, pkg, work, run_report, fmt=".zip", home_short=False):
    """The bug report script against fake game folders.

    run_report(bin_folder, out_folder, *extra) runs the script and returns (exit code, output).
    fmt: ".zip", or ".tar.gz" when the script is told to make that (a tuple: either).
    home_short: the game folder is below the script's $HOME, so system.txt writes it as ~/..."""
    print(f"\nBug report script ({who})")
    version = package_version(pkg)
    shutil.rmtree(work, ignore_errors=True)  # (so that a second run in the same --work-dir counts the reports again)
    out = work / "reports"
    out.mkdir(parents=True, exist_ok=True)
    b = make_report_game(pkg, work / "report lib")
    sources = {n: (b / n).read_bytes() for n in REPORT_FILES}

    code, text = run_report(b, out)
    made = report_files(out)
    check(code == 0 and len(made) == 1 and made[0].name.endswith(fmt), f"bug report: exit 0 and one {fmt} file", text)
    if len(made) != 1:
        return
    files = read_report(made[0])
    check(sorted(files) == sorted(list(REPORT_FILES) + ["system.txt"]),
          "bug report: holds the log, the older log, the settings, the skins file and system.txt, and nothing else (no save, no map, no .bak)", str(sorted(files)))
    check(all(files.get(n) == sources[n] for n in REPORT_FILES), "bug report: the four files are copied byte for byte")
    check(str(made[0]) in text.replace("\r", "") or made[0].name in text, "bug report: says where the file is", text)
    check("Nothing was sent anywhere" in text and "player names" in text, "bug report: says it sends nothing, and that the log has player names", text)
    s = system_text(files)
    check(f"dinput8.dll in the game folder: open-homm5 {version}" in s, "bug report: system.txt names the add-on version in the game folder", s)
    check(f"the package this script came from: {version}" in s, "bug report: system.txt names the version of the package it came from")
    check('H55_Version in data' in s and s.split("H55_Version in data", 1)[1].split("\n", 1)[0].strip().endswith("19k"),
          "bug report: system.txt has MMH5.5's H55_Version (read from the zip MMH55-Index.pak)", s)
    check(f"55u.dll: a version we have not tested (sha256 {sha(b / '55u.dll')})" in s, "bug report: system.txt names 55u.dll with its hash, and says when it is not a tested version", s)
    exes_ok = all(re.search(rf"^\s+{re.escape(n)}\s+{(b / n).stat().st_size} bytes\b.*{sha(b / n)}", s, re.M) for n in ("H5_Game.exe", "MMH55_Utility_64.exe"))
    check(exes_ok, "bug report: system.txt lists the game exes with size and sha256", s)
    check(re.search(r"^\s+55u\.dll\s+15$", s, re.M) and re.search(r"^\s+other-mod\.asi\s+11$", s, re.M) and re.search(r"^\s+dinput8\.dll\s+\d+$", s, re.M),
          "bug report: system.txt lists the other .dll and .asi files in bin", s)
    check("secret.txt" not in s and "game1.sav" not in s, "bug report: system.txt does not mention other files")
    if home_short:
        check("game folder: ~/report lib/steamapps/common/" in s, "bug report: system.txt writes the home folder as ~", s)
    user = os.environ.get("USERNAME") or os.environ.get("USER") or ""
    check(len(user) < 4 or user not in s, "bug report: system.txt does not carry the user name", s)

    # a second report in the same minute replaces the first; a new one never touches the game folder
    code, text = run_report(b, out)
    check(code == 0 and len(report_files(out)) <= 2, "bug report: running it again works", text)
    check(all((b / n).read_bytes() == sources[n] for n in REPORT_FILES) and (b / "dinput8.dll").exists(),
          "bug report: the game folder is left as it was")

    # no add-on installed, no log: a report with only the facts, and it says so
    b2 = make_report_game(pkg, work / "report lib 2", add_on=False)
    out2 = work / "reports2"
    code, text = run_report(b2, out2)
    made = report_files(out2)
    check(code == 0 and len(made) == 1 and sorted(read_report(made[0])) == ["system.txt"], "bug report: without the add-on or a log: only system.txt", text)
    check("There is no open-homm5.log" in text, "bug report: without a log it says so", text)
    if made:
        s = system_text(read_report(made[0]))
        check("none (the add-on is not installed here)" in s and "there is no open-homm5.log" in s, "bug report: system.txt says the add-on is not installed", s)

    # another mod's dinput8.dll
    b3 = make_report_game(pkg, work / "report lib 3", add_on=False)
    (b3 / "dinput8.dll").write_bytes(b"MZ some other mod")
    out3 = work / "reports3"
    code, text = run_report(b3, out3)
    made = report_files(out3)
    check(code == 0 and made and "NOT the open-homm5 one" in system_text(read_report(made[0])) and (b3 / "dinput8.dll").read_bytes() == b"MZ some other mod",
          "bug report: another mod's dinput8.dll is named as such and left alone", text)

    # MMH5.5 not there, or a pak without the version script
    b4 = make_report_game(pkg, work / "report lib 4", pak_version=None)
    out4 = work / "reports4"
    code, text = run_report(b4, out4)
    made = report_files(out4)
    check(code == 0 and made and "could not read the version" in system_text(read_report(made[0])), "bug report: a pak without H55-Version.lua: says it could not read the version", text)
    b5 = make_report_game(pkg, work / "report lib 5")
    (b5.parent / "data" / "MMH55-Index.pak").unlink()
    out5 = work / "reports5"
    code, text = run_report(b5, out5)
    made = report_files(out5)
    check(code == 0 and made and "MMH55-Index.pak: not found" in system_text(read_report(made[0])), "bug report: no MMH55-Index.pak: says MMH5.5 was not found", text)

    # not a game folder: nothing is made
    out6 = work / "reports6"
    code, text = run_report(work / "nope", out6)
    check(code == 4 and not report_files(out6), "bug report: not a game folder: exit 4, no file", text)


def hold_open_like_the_game(path):
    """Opens a file the way the game holds its log (for appending, others may read but not write).
    Returns a function that closes it."""
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateFileW.restype = wintypes.HANDLE
    k.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                              wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = k.CreateFileW(str(path), 0x4, 0x1, None, 3, 0x80, None)  # FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_EXISTING
    if handle in (None, wintypes.HANDLE(-1).value):
        raise OSError(f"cannot open {path} the way the game does (error {ctypes.get_last_error()})")
    return lambda: k.CloseHandle(handle)


def bug_report_windows_extra(pkg, work, run_report):
    """What only Windows can show: a log in use, the Desktop, an output folder that cannot be made."""
    print("\nBug report script (Windows only)")
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    b = make_report_game(pkg, work / "lib")

    # the game is running: its log is open and others may only read it
    close = hold_open_like_the_game(b / "open-homm5.log")
    try:
        out = work / "running"
        code, text = run_report(b, out)
    finally:
        close()
    made = report_files(out)
    check(code == 0 and len(made) == 1 and read_report(made[0]).get("open-homm5.log") == (b / "open-homm5.log").read_bytes(),
          "bug report: a log that the running game holds open is read and packed", text)

    # no -OutDir: on the Desktop (also when OneDrive moved it), else in the home folder
    code, desktop = run(["powershell", "-NoProfile", "-Command",
                         "$p = [Environment]::GetFolderPath('Desktop'); if (-not (Test-Path -LiteralPath $p -PathType Container)) { $p = [Environment]::GetFolderPath('UserProfile') }; $p"])
    desktop = Path(desktop.strip())
    code, text = run(bat(str(pkg / "bug-report.bat"), "-NoPause", "-GameDir", str(b)))
    m = re.search(r"this file:\r?\n\s+(.+?\.zip)\s*\r?\n", text)
    made = Path(m.group(1)) if m else None
    try:
        check(code == 0 and made is not None and made.is_file() and made.parent.resolve() == desktop.resolve()
              and re.fullmatch(r"open-homm5-bug-report-\d{4}-\d\d-\d\d-\d{4}\.zip", made.name) is not None,
              f"bug report: without -OutDir the file lands on the Desktop ({desktop}), named open-homm5-bug-report-<date>.zip", text)
    finally:
        if made is not None and made.parent.resolve() == desktop.resolve() and made.name.startswith("open-homm5-bug-report-"):
            made.unlink(missing_ok=True)

    # an output folder that cannot be made
    blocker = work / "blocker"
    blocker.write_bytes(b"a file, not a folder")
    code, text = run_report(b, blocker / "sub")
    check(code == 5 and "Could not use the folder" in text, "bug report: an output folder that cannot be made: exit 5 and a message", text)


# ---------------------------------------------------------------- Windows

def windows_tests(pkg, work):
    print("\nWindows scripts (install.bat / uninstall.bat)")
    work.mkdir(parents=True, exist_ok=True)
    steam_only = work / "steam_only"          # Steam folder, game in a second library with accents
    lib2 = work / "Bibliotheque \u00c9t\u00e9 Steam"
    steam_only.mkdir()
    make_steam(steam_only, [steam_only, lib2])
    bin2 = make_game(lib2)

    def install(*args, stdin=""):
        return run(bat(str(pkg / "install.bat"), "-NoPause", *args), stdin)

    def uninstall(*args, stdin=""):
        return run(bat(str(pkg / "uninstall.bat"), "-NoPause", *args), stdin)

    our_dll = (pkg / "dinput8.dll").read_bytes()
    our_ini = (pkg / "open-homm5.ini").read_bytes()
    before = snapshot(bin2)

    # fresh install, game found through the fake libraryfolders.vdf
    code, out = install("-SteamDir", str(steam_only))
    check(code == 0, "fresh install: exit 0", out)
    check((bin2 / "dinput8.dll").read_bytes() == our_dll, "fresh install: dinput8.dll copied")
    check((bin2 / "open-homm5.ini").read_bytes() == our_ini, "fresh install: open-homm5.ini copied")
    after = snapshot(bin2)
    check({k: v for k, v in after.items() if k in before} == before, "fresh install: game files unchanged")
    check(set(after) - set(before) == {"dinput8.dll", "open-homm5.ini"}, "fresh install: only our 2 files added")
    check("open-homm5.log" in out and "added dinput8.dll" in out, "fresh install: says what it did and how to check", out)
    check(sorted(p.name for p in (bin2 / "open-homm5" / "templates").iterdir()) == sorted(p.name for p in template_dir(pkg).iterdir()),
          "fresh install: the templates are installed in bin/open-homm5/templates")

    # install again over our own DLL
    code, out = install("-SteamDir", str(steam_only))
    check(code == 0 and "up to date" in out, "re-install over our DLL: exit 0, says up to date", out)
    (bin2 / "dinput8.dll").write_bytes(OURS_MARK + b" older build")
    code, out = install("-SteamDir", str(steam_only))
    check(code == 0 and "replaced the older" in out, "re-install over an older build of ours: replaced", out)
    check((bin2 / "dinput8.dll").read_bytes() == our_dll, "re-install: new DLL in place")

    # an existing ini is refreshed: this version's file, with the player's values
    (bin2 / "open-homm5.ini").write_bytes(b"[net]\r\ntuning=0\r\n")
    code, out = install("-SteamDir", str(steam_only))
    check(code == 0 and (bin2 / "open-homm5.ini").read_bytes() == our_ini.replace(b"\r\n;tuning=1\r\n", b"\r\ntuning=0\r\n")
          and (bin2 / "open-homm5.ini.bak").read_bytes() == b"[net]\r\ntuning=0\r\n",
          "existing open-homm5.ini: refreshed, the player's value kept, the old file in .bak", out)
    check("settings file refreshed: 1 of your settings kept" in out, "existing ini: says it refreshed it", out)

    # a foreign dinput8.dll is refused
    (bin2 / "dinput8.dll").write_bytes(b"MZ some other mod's dinput8")
    code, out = install("-SteamDir", str(steam_only))
    check(code == 3 and "Stopped" in out, "foreign dinput8.dll: refused with exit 3", out)
    check((bin2 / "dinput8.dll").read_bytes() == b"MZ some other mod's dinput8", "foreign dinput8.dll: left untouched")
    code, out = uninstall("-SteamDir", str(steam_only), stdin="y\n")
    check(code == 0 and "NOT the open-homm5 one" in out, "uninstall leaves a foreign dinput8.dll alone", out)
    check((bin2 / "dinput8.dll").read_bytes() == b"MZ some other mod's dinput8", "uninstall: foreign dll still there")
    (bin2 / "dinput8.dll").unlink()
    (bin2 / "open-homm5.ini").unlink(missing_ok=True)

    # uninstall: ours removed, log removed, ini only after asking
    code, out = install("-SteamDir", str(steam_only))
    (bin2 / "open-homm5.log").write_bytes(b"log")
    (bin2 / "open-homm5.old.log").write_bytes(b"older log")
    code, out = uninstall("-SteamDir", str(steam_only), stdin="n\n")
    check(code == 0, "uninstall (answer n): exit 0", out)
    check(not (bin2 / "dinput8.dll").exists() and not (bin2 / "open-homm5.log").exists() and not (bin2 / "open-homm5.old.log").exists(),
          "uninstall: dinput8.dll, open-homm5.log and the old log (open-homm5.old.log) removed")
    check((bin2 / "open-homm5.ini").exists(), "uninstall (answer n): ini kept")
    code, out = uninstall("-SteamDir", str(steam_only), stdin="y\n")
    check(code == 0 and not (bin2 / "open-homm5.ini").exists(), "uninstall (answer y): ini removed", out)
    check(snapshot(bin2) == before, "uninstall: folder back to exactly how it was")
    check(not (bin2 / "open-homm5").exists(), "uninstall: the (empty) open-homm5 folder is gone too")

    # the game folder or its bin folder can be given directly
    game_folder = bin2.parent
    code, out = install("-GameDir", str(game_folder))
    check(code == 0 and (bin2 / "dinput8.dll").exists(), "-GameDir <game folder> works", out)
    uninstall("-GameDir", str(bin2), stdin="y\n")
    code, out = install("-GameDir", str(bin2))
    check(code == 0 and (bin2 / "dinput8.dll").exists(), "-GameDir <bin folder> works", out)
    uninstall("-GameDir", str(bin2), stdin="y\n")
    code, out = install("-GameDir", str(work / "nope"))
    check(code == 4 and "not the game folder" in out, "-GameDir with a wrong folder: exit 4", out)

    # no game found: asks, accepts a pasted path with quotes, or cancels
    empty_steam = work / "empty_steam"
    make_steam(empty_steam, [empty_steam])
    # (an ASCII path with a space: piped input is not read as Unicode, unlike a real console)
    plain_bin = make_game(work / "plain lib")
    code, out = install("-SteamDir", str(empty_steam), stdin=f'"{plain_bin.parent}"\n')
    check(code == 0 and (plain_bin / "dinput8.dll").exists() and "could not find the game" in out,
          "no game found: asks for the folder, accepts a quoted path", out)
    uninstall("-GameDir", str(plain_bin), stdin="y\n")
    code, out = install("-SteamDir", str(empty_steam), stdin="\n")
    check(code == 4 and not (plain_bin / "dinput8.dll").exists(), "no game found, Enter alone: cancelled, exit 4", out)
    code, out = install("-SteamDir", str(empty_steam), stdin="")
    check(code == 4, "no game found, no input at all: cancelled, exit 4", out)

    # two games found: numbered list
    lib3 = work / "lib3"
    bin3 = make_game(lib3)
    two = work / "two_games"
    make_steam(two, [lib2, lib3])
    code, out = install("-SteamDir", str(two), stdin="2\n")
    check(code == 0 and (bin3 / "dinput8.dll").exists() and not (bin2 / "dinput8.dll").exists(),
          "two games: picking number 2 installs into the second only", out)
    check("several places" in out, "two games: shows the list")
    uninstall("-GameDir", str(bin3), stdin="y\n")

    # old-style vdf ("1" "D:\\path") and single-backslash tolerance
    old = work / "old_steam"
    (old / "steamapps").mkdir(parents=True)
    (old / "steamapps" / "libraryfolders.vdf").write_bytes(
        ('"LibraryFolders"\n{\n\t"TimeNextStatsReport"\t\t"1"\n\t"1"\t\t"' + str(lib3).replace("\\", "\\\\") + '"\n}\n').encode())
    code, out = install("-SteamDir", str(old))
    check(code == 0 and (bin3 / "dinput8.dll").exists(), "old-style libraryfolders.vdf is read", out)
    uninstall("-GameDir", str(bin3), stdin="y\n")

    # no libraryfolders.vdf at all, game in Steam's own folder
    bare = work / "bare_steam"
    bin_bare = make_game(bare)
    code, out = install("-SteamDir", str(bare))
    check(code == 0 and (bin_bare / "dinput8.dll").exists(), "no libraryfolders.vdf: game in Steam's own folder is found", out)
    uninstall("-GameDir", str(bin_bare), stdin="y\n")

    # only H5_Game.exe: installs, but warns that MMH5.5 is missing
    lib4 = work / "lib4"
    bin4 = make_game(lib4, exes=("H5_Game.exe",))
    code, out = install("-GameDir", str(bin4))
    check(code == 0 and "no MMH55_Utility_64.exe" in out, "game without MMH5.5: installs with a note", out)

    # install.bat alone (as when double-clicked inside the zip): explains, installs nothing
    lone = work / "lone"
    lone.mkdir()
    shutil.copy(pkg / "install.bat", lone)
    code, out = run(bat(str(lone / "install.bat")), stdin="\n")
    check(code == 7 and "extract the whole zip" in out, "install.bat without install.ps1: asks to extract the zip", out)
    shutil.copy(pkg / "install.ps1", lone)
    code, out = run(bat(str(lone / "install.bat"), "-NoPause", "-GameDir", str(bin2)), stdin="")
    check(code == 7 and not (bin2 / "dinput8.dll").exists(), "install.ps1 without dinput8.dll: exit 7, nothing installed", out)

    # the package extracted into a folder with spaces and parentheses
    odd = work / "Pack (x86) & more"
    shutil.copytree(pkg, odd)
    code, out = run(bat(str(odd / "install.bat"), "-NoPause", "-GameDir", str(bin2)))
    check(code == 0 and (bin2 / "dinput8.dll").exists(), "package in a folder with spaces and parentheses", out)
    code, out = run(bat(str(odd / "uninstall.bat"), "-NoPause", "-GameDir", str(bin2)), "y\n")
    check(code == 0 and not (bin2 / "dinput8.dll").exists(), "uninstall from that folder", out)

    # the ZeroTier hint (only looks at the network profiles; here faked)
    code, out = install("-GameDir", str(bin2), "-TestProfiles", 'ZeroTier One [abc]|Public;Ethernet|Private')
    check(code == 0 and "Heads-up" in out and "Private" in out and "I did not change anything in Windows" in out,
          "ZeroTier on a Public network: explains how to make it Private", out)
    uninstall("-GameDir", str(bin2), stdin="y\n")
    code, out = install("-GameDir", str(bin2), "-TestProfiles", 'ZeroTier One [abc]|Private;Ethernet|Public')
    check(code == 0 and "Heads-up" not in out, "ZeroTier on a Private network: no hint", out)
    uninstall("-GameDir", str(bin2), stdin="y\n")

    # the settings file: refreshed, values kept, backups
    ini_bin = make_game(work / "ini lib")
    results = settings_tests("Windows", pkg, ini_bin,
                             lambda: install("-GameDir", str(ini_bin)),
                             lambda answer: uninstall("-GameDir", str(ini_bin), stdin=answer))

    # random map templates: install, a player's own files, reinstall, uninstall
    tpl_bin = make_game(work / "tpl lib")
    template_tests("Windows", pkg, tpl_bin,
                   lambda: install("-GameDir", str(tpl_bin)),
                   lambda: uninstall("-GameDir", str(tpl_bin), stdin="y\n"))

    # a package without its templates folder is incomplete: exit 7, nothing installed
    no_tpl = work / "pkg without templates"
    shutil.copytree(pkg, no_tpl, ignore=shutil.ignore_patterns("open-homm5"))
    code, out = run(bat(str(no_tpl / "install.bat"), "-NoPause", "-GameDir", str(tpl_bin)))
    check(code == 7 and "extract the whole zip" in out and not (tpl_bin / "dinput8.dll").exists() and not (tpl_bin / "open-homm5").exists(),
          "package without the templates folder: exit 7, nothing installed", out)

    # the bug report script
    def run_report(game_bin, out_dir, *extra):
        return run(bat(str(pkg / "bug-report.bat"), "-NoPause", "-GameDir", str(game_bin), "-OutDir", str(out_dir), *extra))

    bug_report_tests("Windows", pkg, work / "bug report", run_report)
    bug_report_windows_extra(pkg, work / "bug report extra", run_report)

    # PowerShell 5.1 specifically (the script must run there)
    code, out = run(["powershell", "-NoProfile", "-Command", "$PSVersionTable.PSVersion.ToString()"])
    check(out.strip().startswith("5."), f"tests ran on Windows PowerShell 5.x (found {out.strip()})")
    return results


# ---------------------------------------------------------------- Linux

def posix(p):
    return Path(p).as_posix()


def linux_tests(pkg, work, bash):
    print(f"\nLinux scripts (install.sh / uninstall.sh) with {bash}")
    work.mkdir(parents=True, exist_ok=True)
    home = work / "home"
    steam = home / ".local" / "share" / "Steam"
    flat = home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam"
    lib2 = work / "SteamLibrary"
    home.mkdir()
    steam.mkdir(parents=True)
    make_steam(steam, [steam, lib2], escape_backslashes=False)
    # fix the paths in the vdf to forward slashes (they are plain paths on Linux)
    vdf = steam / "steamapps" / "libraryfolders.vdf"
    vdf.write_bytes(vdf.read_bytes().replace(str(steam).encode(), posix(steam).encode()).replace(str(lib2).encode(), posix(lib2).encode()))
    bin2 = make_game(lib2)
    before = snapshot(bin2)
    env = dict(os.environ, HOME=posix(home))

    def sh(script, *args, stdin=""):
        return run([bash, str(pkg / script), *args], stdin, env=env)

    check(run([bash, "-n", str(pkg / "install.sh")])[0] == 0, "bash -n install.sh")
    check(run([bash, "-n", str(pkg / "uninstall.sh")])[0] == 0, "bash -n uninstall.sh")
    check(b"\r" not in (pkg / "install.sh").read_bytes() and b"\r" not in (pkg / "uninstall.sh").read_bytes(),
          "shell scripts have LF line endings")

    our_dll = (pkg / "dinput8.dll").read_bytes()
    code, out = sh("install.sh")
    check(code == 0, "fresh install (Steam in ~/.local/share/Steam): exit 0", out)
    check((bin2 / "dinput8.dll").read_bytes() == our_dll and (bin2 / "open-homm5.ini").exists(), "fresh install: files copied")
    after = snapshot(bin2)
    check({k: v for k, v in after.items() if k in before} == before and set(after) - set(before) == {"dinput8.dll", "open-homm5.ini"},
          "fresh install: game files unchanged, only our 2 files added")
    check('WINEDLLOVERRIDES="dinput8=n,b" %command%' in out and "MMH55_Utility_64.exe" in out and "open-homm5.log" in out,
          "prints the Steam launch option, the exe to start and how to check", out)
    check(sorted(p.name for p in (bin2 / "open-homm5" / "templates").iterdir()) == sorted(p.name for p in template_dir(pkg).iterdir()),
          "fresh install: the templates are installed in bin/open-homm5/templates")

    code, out = sh("install.sh")
    check(code == 0 and "up to date" in out, "re-install over our DLL: up to date", out)
    (bin2 / "dinput8.dll").write_bytes(OURS_MARK + b" older build")
    code, out = sh("install.sh")
    check(code == 0 and "replaced the older" in out and (bin2 / "dinput8.dll").read_bytes() == our_dll, "re-install over an older build: replaced", out)

    (bin2 / "open-homm5.ini").write_bytes(b"[net]\ntuning=0\n")
    code, out = sh("install.sh")
    our_ini = (pkg / "open-homm5.ini").read_bytes()
    check(code == 0 and (bin2 / "open-homm5.ini").read_bytes() == our_ini.replace(b"\r\n;tuning=1\r\n", b"\r\ntuning=0\r\n")
          and (bin2 / "open-homm5.ini.bak").read_bytes() == b"[net]\ntuning=0\n" and "settings file refreshed: 1 of your settings kept" in out,
          "existing ini: refreshed, the player's value kept, the old file in .bak", out)

    (bin2 / "dinput8.dll").write_bytes(b"MZ some other mod")
    code, out = sh("install.sh")
    check(code == 3 and (bin2 / "dinput8.dll").read_bytes() == b"MZ some other mod", "foreign dinput8.dll: refused (exit 3), untouched", out)
    code, out = sh("uninstall.sh", stdin="y\n")
    check(code == 0 and (bin2 / "dinput8.dll").exists() and "NOT the open-homm5 one" in out, "uninstall leaves a foreign dll alone", out)
    (bin2 / "dinput8.dll").unlink()
    (bin2 / "open-homm5.ini").unlink(missing_ok=True)

    sh("install.sh")
    (bin2 / "open-homm5.log").write_bytes(b"log")
    (bin2 / "open-homm5.old.log").write_bytes(b"older log")
    code, out = sh("uninstall.sh", stdin="n\n")
    check(code == 0 and not (bin2 / "dinput8.dll").exists() and not (bin2 / "open-homm5.log").exists()
          and not (bin2 / "open-homm5.old.log").exists() and (bin2 / "open-homm5.ini").exists(),
          "uninstall (n): dll, log and old log removed, ini kept", out)
    code, out = sh("uninstall.sh", stdin="y\n")
    check(code == 0 and snapshot(bin2) == before, "uninstall (y): folder back to how it was", out)
    check(not (bin2 / "open-homm5").exists(), "uninstall: the (empty) open-homm5 folder is gone too")

    code, out = sh("install.sh", "--game-dir", posix(bin2.parent))
    check(code == 0 and (bin2 / "dinput8.dll").exists(), "--game-dir <game folder>", out)
    sh("uninstall.sh", "--game-dir", posix(bin2), stdin="y\n")
    code, out = sh("install.sh", "--game-dir", posix(work / "nope"))
    check(code == 4, "--game-dir with a wrong folder: exit 4", out)

    # Flatpak Steam only, and no Steam at all (asks)
    home2 = work / "home2"
    flat2 = home2 / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam"
    lib5 = work / "lib5"
    make_steam(flat2, [lib5], escape_backslashes=False)
    v2 = flat2 / "steamapps" / "libraryfolders.vdf"
    v2.write_bytes(v2.read_bytes().replace(str(lib5).encode(), posix(lib5).encode()))
    bin5 = make_game(lib5)
    env2 = dict(os.environ, HOME=posix(home2))
    code, out = run([bash, str(pkg / "install.sh")], "", env=env2)
    check(code == 0 and (bin5 / "dinput8.dll").exists(), "Flatpak Steam folder is found", out)
    run([bash, str(pkg / "uninstall.sh")], "y\n", env=env2)
    home3 = work / "home3"
    home3.mkdir()
    env3 = dict(os.environ, HOME=posix(home3))
    code, out = run([bash, str(pkg / "install.sh")], f"'{posix(bin5.parent)}'\n", env=env3)
    check(code == 0 and (bin5 / "dinput8.dll").exists() and "could not find the game" in out, "no Steam: asks for the folder (quoted path)", out)
    run([bash, str(pkg / "uninstall.sh"), "--game-dir", posix(bin5)], "y\n", env=env3)
    code, out = run([bash, str(pkg / "install.sh")], "\n", env=env3)
    check(code == 4 and not (bin5 / "dinput8.dll").exists(), "no Steam, Enter alone: cancelled (exit 4)", out)

    # two games: numbered list
    two = work / "home4"
    steam4 = two / ".steam" / "steam"
    lib6 = work / "lib6"
    make_steam(steam4, [lib5, lib6], escape_backslashes=False)
    v4 = steam4 / "steamapps" / "libraryfolders.vdf"
    v4.write_bytes(v4.read_bytes().replace(str(lib5).encode(), posix(lib5).encode()).replace(str(lib6).encode(), posix(lib6).encode()))
    bin6 = make_game(lib6)
    code, out = run([bash, str(pkg / "install.sh")], "2\n", env=dict(os.environ, HOME=posix(two)))
    check(code == 0 and (bin6 / "dinput8.dll").exists() and not (bin5 / "dinput8.dll").exists() and "several places" in out,
          "two games: number 2 installs into the second only", out)

    # no MMH5.5 exe
    lib7 = work / "lib7"
    bin7 = make_game(lib7, exes=("H5_Game.exe",))
    code, out = sh("install.sh", "--game-dir", posix(bin7))
    check(code == 0 and "no MMH55_Utility_64.exe" in out, "game without MMH5.5: installs with a note", out)

    # extracted without the DLL
    lone = work / "lone_sh"
    lone.mkdir()
    shutil.copy(pkg / "install.sh", lone)
    code, out = run([bash, str(lone / "install.sh"), "--game-dir", posix(bin2)], "", env=env)
    check(code == 7 and not (bin2 / "dinput8.dll").exists(), "install.sh without dinput8.dll: exit 7", out)

    # the settings file: refreshed, values kept, backups
    ini_bin = make_game(work / "ini_lib")
    results = settings_tests("Linux", pkg, ini_bin,
                             lambda: sh("install.sh", "--game-dir", posix(ini_bin)),
                             lambda answer: sh("uninstall.sh", "--game-dir", posix(ini_bin), stdin=answer))

    # random map templates: install, a player's own files, reinstall, uninstall
    tpl_bin = make_game(work / "tpl_lib")
    template_tests("Linux", pkg, tpl_bin,
                   lambda: sh("install.sh", "--game-dir", posix(tpl_bin)),
                   lambda: sh("uninstall.sh", "--game-dir", posix(tpl_bin), stdin="y\n"))

    # a package without its templates folder is incomplete: exit 7, nothing installed
    no_tpl = work / "pkg_without_templates"
    shutil.copytree(pkg, no_tpl, ignore=shutil.ignore_patterns("open-homm5"))
    code, out = run([bash, str(no_tpl / "install.sh"), "--game-dir", posix(tpl_bin)], "", env=env)
    check(code == 7 and not (tpl_bin / "dinput8.dll").exists() and not (tpl_bin / "open-homm5").exists(),
          "package without the templates folder: exit 7, nothing installed", out)

    # folders that exist under another spelling (Linux file names are case sensitive,
    # Wine finds them anyway): the existing folders and files are reused, none is duplicated
    ci_bin = make_game(work / "ci_lib")
    old = ci_bin / "Open-Homm5" / "Templates"
    old.mkdir(parents=True)
    first = sorted(p.name for p in template_dir(pkg).iterdir())[0]
    (old / first.lower()).write_bytes(b"an old copy of ours, in lower case")
    code, out = sh("install.sh", "--game-dir", posix(ci_bin))
    check(code == 0 and [p.name for p in ci_bin.iterdir() if p.is_dir()] == ["Open-Homm5"], "templates: an existing Open-Homm5 folder is reused", out)
    check(sorted(p.name for p in old.iterdir()) == sorted([first.lower()] + [p.name for p in template_dir(pkg).iterdir() if p.name != first])
          and (old / first.lower()).read_bytes() == (template_dir(pkg) / first).read_bytes(),
          "templates: an existing file of ours under another spelling is overwritten, not duplicated")

    # the bug report script. HOME is the folder above the fake game folders, as a player's
    # game folder is below their home (the report writes that part of the path as ~)
    def report_runner(report_work, *always):
        report_env = dict(os.environ, HOME=posix(report_work))

        def run_report(game_bin, out_dir, *extra):
            return run([bash, str(pkg / "bug-report.sh"), "--game-dir", posix(game_bin), "--out-dir", posix(out_dir), *always, *extra],
                       "", env=report_env)
        return run_report

    run_report = report_runner(work / "bug_report")
    bug_report_tests("Linux", pkg, work / "bug_report", run_report, fmt=(".zip", ".tar.gz"), home_short=True)
    bug_report_tests("Linux, --tar", pkg, work / "bug_report_tar", report_runner(work / "bug_report_tar", "--tar"), fmt=".tar.gz", home_short=True)

    print("\nBug report script (Linux only)")
    extra = work / "bug_report_extra"
    shutil.rmtree(extra, ignore_errors=True)
    extra.mkdir(parents=True, exist_ok=True)
    b = make_report_game(pkg, extra / "lib")
    here = extra / "run here"
    here.mkdir()
    code, out = run([bash, str(pkg / "bug-report.sh"), "--game-dir", posix(b)], "", env=env, cwd=str(here))
    check(code == 0 and len(report_files(here)) == 1, "bug report: without --out-dir the file is made in the folder you run it from", out)
    blocker = extra / "blocker"
    blocker.write_bytes(b"a file, not a folder")
    code, out = run_report(b, blocker / "sub")
    check(code == 5 and "Cannot write" in out, "bug report: an output folder that cannot be made: exit 5 and a message", out)
    check(run([bash, "-n", str(pkg / "bug-report.sh")])[0] == 0, "bash -n bug-report.sh")
    code, out = run([bash, str(pkg / "bug-report.sh"), "--help"], "", env=env)
    check(code == 0 and "bug report" in out and "--out-dir" in out, "bug report: --help prints the header", out)
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bash", help="bash to test the .sh scripts with (default: bash from PATH, if it is not WSL's)")
    ap.add_argument("--work-dir", type=Path, help="folder for the fake trees (default: a new temp folder)")
    ap.add_argument("--keep", action="store_true", help="do not delete the work folder at the end")
    args = ap.parse_args()

    work = args.work_dir or Path(tempfile.mkdtemp(prefix="open-homm5-test-"))
    work.mkdir(parents=True, exist_ok=True)
    out_dir = work / "out"
    r = subprocess.run([sys.executable, str(REPO / "tools" / "package" / "make_package.py"), "--version", "0.0.0-test",
                        "--out-dir", str(out_dir)])
    if r.returncode != 0:
        sys.exit("make_package.py failed")
    with zipfile.ZipFile(out_dir / "open-homm5-0.0.0-test.zip") as z:
        z.extractall(work / "package")
    pkg = work / "package" / "open-homm5-0.0.0-test"  # the zip's one top folder
    package_checks(pkg, out_dir / "open-homm5-0.0.0-test.zip")
    licence_checks(pkg, out_dir / "open-homm5-0.0.0-test.zip")
    bug_report_package_checks(pkg, out_dir / "open-homm5-0.0.0-test.zip")

    win = lin = None
    if os.name == "nt":
        win = windows_tests(pkg, work / "win")
    else:
        print("\n(skipping the Windows script tests: not on Windows)")

    bash = args.bash or shutil.which("bash")
    if bash and os.name == "nt" and "system32" in bash.lower():
        bash = None  # that is WSL's launcher
    if bash:
        (work / "lin").mkdir(exist_ok=True)
        lin = linux_tests(pkg, work / "lin", bash)
    else:
        print("\n(skipping the Linux script tests: no bash; pass --bash)")

    if win and lin:
        print("\nThe settings file: Windows and Linux")
        for case in win:
            check(win[case] == lin.get(case), f"settings: {case}: install.ps1 and install.sh write the same bytes")

    print(f"\n{checks - len(failures)} of {checks} checks passed")
    if failures:
        print("FAILED:\n  " + "\n  ".join(failures))
    if not args.keep and not args.work_dir:
        shutil.rmtree(work, ignore_errors=True)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build the easy-install package: build/package/h5-together-<version>.zip

The zip holds the hook DLL (build/hook/dinput8.dll), the settings template, the
install and uninstall scripts and the bug report script for Windows and Linux, a short
README.txt, the licence notices (LICENSE.txt is the repo's LICENSE; THIRD-PARTY-NOTICES.txt
carries Dear ImGui's licence file), the ready random map templates
(src/mods/rmg/templates/*.xdb, packed byte for byte as h5-together/templates/) and our
language files (src/mods/texts/*.txt, as h5-together/texts/: UTF-8 with a BOM and CRLF).
The scripts and text templates live in tools/package/files/ (plain ASCII: PowerShell 5.1
and cmd read them in the Windows code page); only the language files hold other letters.

    python tools/package/make_package.py                    # the version in src/hook/version.h
    python tools/package/make_package.py --version 0.1.0-test2   # a test build (with a '-')
    python tools/package/make_package.py --rebuild          # build the DLL first

If build/hook/dinput8.dll is missing it is built with src/hook/build.ps1 (Windows
with Visual Studio only). Nothing is uploaded or published; the zip stays local.

Valve's steam_api.dll (online play over Steam) is never in the repo, and GitHub's build has
no copy of it, so the package is built without it. On a PC that has the Steamworks SDK
(ref/ is not committed), it goes in as h5-together/steam_api.dll, the install scripts copy
it to the game's bin/h5-together/ folder. Only the 32-bit file, redistributable_bin/steam_api.dll:

    python tools/package/make_package.py --steam-api ref/steamworks_sdk_165/sdk/redistributable_bin/steam_api.dll
    python tools/package/make_package.py --add-steam-api build/package/h5-together-0.3.0.zip ref/steamworks_sdk_165/sdk/redistributable_bin/steam_api.dll

The first builds the zip with it. The second adds it to a zip that was built already (the
one GitHub's tag build puts in the draft release): the zip is changed in place, every other
file in it keeps its name, its place and its bytes, and the new zip's SHA-256 is printed.
Both refuse a file that is not a 32-bit Windows DLL called steam_api.dll.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FILES = REPO / "tools" / "package" / "files"
DEFAULT_DLL = REPO / "build" / "hook" / "dinput8.dll"
OUT_DIR = REPO / "build" / "package"

# The install scripts tell our dinput8.dll from somebody else's by this text,
# which the DLL carries in its log message (src/hook/dllmain.cpp).
MARKER = b"H5 Together hook DLL"
VERSION_H = REPO / "src" / "hook" / "version.h"

# The licence notices: our LICENSE, and Dear ImGui's (the one third-party code in the DLL).
LICENSE_FILE = REPO / "LICENSE"
IMGUI_DIR = REPO / "src" / "hook" / "third_party" / "imgui"


def source_version():
    """The version in src/hook/version.h (OPEN_HOMM5_VERSION), which the DLL logs."""
    m = re.search(r'#define OPEN_HOMM5_VERSION "([^"]+)"', VERSION_H.read_text(encoding="utf-8"))
    if not m:
        fail(f"no OPEN_HOMM5_VERSION in {VERSION_H}")
    return m.group(1)

# Order in the zip. (name, source file in tools/package/files/ or None for the DLL)
# LICENSE.txt has no file of its own there: it is the LICENSE at the repo's root.
CONTENTS = [
    ("README.txt", "README.txt"),
    ("LICENSE.txt", "LICENSE"),
    ("THIRD-PARTY-NOTICES.txt", "THIRD-PARTY-NOTICES.txt"),
    ("dinput8.dll", None),
    ("h5-together.ini", "h5-together.ini"),
    ("install.bat", "install.bat"),
    ("install.ps1", "install.ps1"),
    ("uninstall.bat", "uninstall.bat"),
    ("uninstall.ps1", "uninstall.ps1"),
    ("bug-report.bat", "bug-report.bat"),
    ("bug-report.ps1", "bug-report.ps1"),
    ("install.sh", "install.sh"),
    ("uninstall.sh", "uninstall.sh"),
    ("bug-report.sh", "bug-report.sh"),
]

# The ready random map templates (tools/rmg/make_templates.js writes them) go in
# the zip as h5-together/templates/*.xdb, the same layout as in the game's bin
# folder, and are copied byte for byte (UTF-8 with BOM and CRLF: .gitattributes
# has "*.xdb -text"). The install scripts copy exactly these names, and the
# uninstall scripts remove exactly these names, so they must be plain names that
# no shell or glob treats specially.
TEMPLATES_SRC = REPO / "src" / "mods" / "rmg" / "templates"
TEMPLATES_IN_ZIP = "h5-together/templates"
TEMPLATE_NAME = re.compile(r"OH-[A-Za-z0-9.-]+\.xdb")

# Our language files (src/mods/texts/<code>.txt: the words of the add-on's panels, English
# and the translations) go in the zip as h5-together/texts/<code>.txt. Unlike the scripts
# they are UTF-8 by design (accents, Cyrillic...): in the zip with a byte-order mark and
# CRLF line ends, so that every Notepad opens and saves them as UTF-8. The DLL reads them
# with or without the mark, CRLF or LF; its English is built in from en.txt anyway.
TEXTS_SRC = REPO / "src" / "mods" / "texts"
TEXTS_IN_ZIP = "h5-together/texts"
TEXT_NAME = re.compile(r"[a-z]{2,3}(-[a-z0-9]{1,8})?\.txt")

# Valve's steam_api.dll: the DLL loads it from bin\h5-together\ when the player turns on
# online play over Steam (src/mods/steam_runtime.cpp). Last in the zip, whichever way it
# was added (--steam-api or --add-steam-api). The game is 32-bit, so the DLL must be too.
STEAM_API_NAME = "steam_api.dll"
STEAM_API_IN_ZIP = "h5-together/" + STEAM_API_NAME
PE_MACHINE_I386 = 0x14C
PE_FILE_DLL = 0x2000

# Line endings inside the zip: Windows files get CRLF (Notepad, cmd.exe), shell
# scripts must stay LF. The repo keeps LF (and CRLF for .bat, see .gitattributes).
CRLF_SUFFIXES = {".bat", ".ps1", ".txt", ".ini"}
EXECUTABLE_SUFFIXES = {".sh"}

# The code that finds the game and says what it found (Steam, GOG, the folder window, the
# MMH5.5 version, the "game is running" check) is written three times (install, uninstall
# and the bug report script) so that each script stands alone. The copies sit between these
# lines and must match (one set of three for Windows, one for Linux).
SHARED_PAIRS = [("install.ps1", "uninstall.ps1"), ("install.ps1", "bug-report.ps1"),
                ("install.sh", "uninstall.sh"), ("install.sh", "bug-report.sh")]
SHARED_START = "# ---- shared with uninstall."
SHARED_END = "# ---- end of the shared block ----"


def fail(message):
    print("error: " + message, file=sys.stderr)
    sys.exit(1)


def read_ascii(path):
    """Text of a file with LF line endings; must be plain ASCII (PowerShell 5.1
    reads a script without a byte-order mark as ANSI, so accents would break)."""
    raw = Path(path).read_bytes()
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError as e:
        fail(f"{Path(path).name} must be plain ASCII ({e})")
    return text.replace("\r\n", "\n")


def read_template(name):
    return read_ascii(FILES / name)


def imgui_version():
    """Dear ImGui's version, from its header (IMGUI_VERSION)."""
    m = re.search(r'#define IMGUI_VERSION\s+"([^"]+)"', (IMGUI_DIR / "imgui.h").read_text(encoding="utf-8"))
    if not m:
        fail(f"no IMGUI_VERSION in {IMGUI_DIR / 'imgui.h'}")
    return m.group(1)


def shared_block(name, text):
    lines = text.split("\n")
    start = [i for i, l in enumerate(lines) if l.startswith(SHARED_START)]
    end = [i for i, l in enumerate(lines) if l == SHARED_END]
    if len(start) != 1 or len(end) != 1 or start[0] >= end[0]:
        fail(f"{name}: need exactly one shared block ('{SHARED_START}...' to '{SHARED_END}')")
    return "\n".join(lines[start[0]:end[0] + 1])


def check_shared_blocks():
    for a, b in SHARED_PAIRS:
        if shared_block(a, read_template(a)) != shared_block(b, read_template(b)):
            fail(f"the shared 'find the game' blocks of {a} and {b} differ; keep them identical")


def build_dll():
    if os.name != "nt":
        fail("the DLL must be built on Windows: powershell -File src\\hook\\build.ps1")
    print("building the hook DLL (src/hook/build.ps1) ...")
    r = subprocess.run(["powershell", "-NoProfile", "-File", str(REPO / "src" / "hook" / "build.ps1")])
    if r.returncode != 0:
        fail("the DLL build failed")


def file_bytes(name, source, dll_path, version):
    if source is None:
        data = dll_path.read_bytes()
        if MARKER not in data:
            fail(f"{dll_path} does not contain the text '{MARKER.decode()}', "
                 "which the install scripts use to recognise our DLL")
        logged = MARKER + b" " + source_version().encode() + b" loaded"
        if logged not in data:
            fail(f"{dll_path} is not version {source_version()} (src/hook/version.h): rebuild it (--rebuild)")
        return data
    if name == "LICENSE.txt":
        text = read_ascii(LICENSE_FILE)
    else:
        text = read_template(source)
    if name == "README.txt":
        text = text.replace("@VERSION@", version)
    if name == "THIRD-PARTY-NOTICES.txt":
        # Dear ImGui's licence file, word for word (a newer ImGui brings its own)
        text = text.replace("@IMGUI_VERSION@", imgui_version())
        text = text.replace("@IMGUI_LICENSE@", read_ascii(IMGUI_DIR / "LICENSE.txt").strip("\n"))
    for placeholder in ("@VERSION@", "@IMGUI_VERSION@", "@IMGUI_LICENSE@"):
        if placeholder in text:
            fail(f"{name}: unreplaced {placeholder}")
    if Path(name).suffix in CRLF_SUFFIXES:
        text = text.replace("\n", "\r\n")
    return text.encode("ascii")


def template_files():
    """[(file name, bytes)] of every ready template, sorted by name, bytes untouched."""
    found = sorted(p for p in TEMPLATES_SRC.glob("*.xdb") if p.is_file())
    if not found:
        fail(f"no templates in {TEMPLATES_SRC}: run  node tools/rmg/make_templates.js")
    out = []
    for p in found:
        if not TEMPLATE_NAME.fullmatch(p.name):
            fail(f"{p.name}: a packaged template must be called OH-<letters, digits, . or ->.xdb")
        data = p.read_bytes()
        # a checkout that converted line endings would change what every player gets
        if not data.startswith(b"\xef\xbb\xbf") or b"\r\n" not in data or re.search(rb"(?<!\r)\n", data):
            fail(f"{p.name} is not UTF-8 with BOM and CRLF only (git changed its line endings? see .gitattributes)")
        out.append((p.name, data))
    return out


def text_files():
    """[(file name, bytes)] of every language file, sorted by name: UTF-8 with a BOM and CRLF."""
    found = sorted(p for p in TEXTS_SRC.glob("*.txt") if p.is_file())
    if not any(p.name == "en.txt" for p in found):
        fail(f"no en.txt in {TEXTS_SRC}")
    out = []
    for p in found:
        if not TEXT_NAME.fullmatch(p.name):
            fail(f"{p.name}: a language file must be called <code>.txt (2 or 3 small letters, maybe -xx: de.txt, pt-br.txt)")
        raw = p.read_bytes()
        try:
            text = raw.decode("utf-8-sig")
        except UnicodeDecodeError as e:
            fail(f"{p.name} must be UTF-8 ({e})")
        if "\0" in text or "﻿" in text:
            fail(f"{p.name}: a zero byte or a second byte-order mark inside")
        text = text.replace("\r\n", "\n").replace("\n", "\r\n")
        out.append((p.name, b"\xef\xbb\xbf" + text.encode("utf-8")))
    return out


def pe_header(data):
    """(machine, characteristics) of a Windows program or DLL (a PE file), or None if it is not one."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    pe = int.from_bytes(data[0x3C:0x40], "little")
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        return None
    # after "PE\0\0": Machine (2 bytes), 16 bytes of counts and offsets, Characteristics (2 bytes)
    return int.from_bytes(data[pe + 4:pe + 6], "little"), int.from_bytes(data[pe + 22:pe + 24], "little")


def steam_api_bytes(path):
    """The bytes of Valve's steam_api.dll, after checking that it is a 32-bit Windows DLL
    called steam_api.dll (the SDK's redistributable_bin/steam_api.dll)."""
    path = Path(path)
    if not path.is_file():
        fail(f"{path} does not exist")
    if path.name.lower() != STEAM_API_NAME:
        fail(f"{path.name}: the file must be called {STEAM_API_NAME} (the SDK's redistributable_bin/steam_api.dll; "
             "not steam_api64.dll, which is for 64-bit programs)")
    data = path.read_bytes()
    header = pe_header(data)
    if header is None:
        fail(f"{path} is not a Windows DLL")
    machine, characteristics = header
    if machine != PE_MACHINE_I386:
        fail(f"{path} is not a 32-bit DLL (machine 0x{machine:x}; 32-bit is 0x14c): the game is 32-bit, "
             "use the SDK's redistributable_bin/steam_api.dll")
    if not characteristics & PE_FILE_DLL:
        fail(f"{path} is a program, not a DLL")
    return data


def zip_entry(name, stamp):
    """How every file goes in the zip: compressed, dated stamp, Unix permissions (so that
    unzip on Linux keeps the .sh scripts executable). name is the full name in the zip."""
    info = zipfile.ZipInfo(name, stamp)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3  # Unix, so that unzip on Linux honours the mode below
    mode = 0o755 if Path(name).suffix in EXECUTABLE_SUFFIXES else 0o644
    info.external_attr = (0o100000 | mode) << 16
    return info


def show(path):
    return path.relative_to(REPO) if path.is_relative_to(REPO) else path


def add_steam_api(zip_path, dll_path):
    """Adds Valve's steam_api.dll to a package zip built already (GitHub's tag build makes
    it without). The new file goes at the end. The zip is appended to, not rebuilt: the
    files already in it keep their bytes (compressed as they were), names and order; only
    the zip's table of contents, at its end, is written again. Works on a copy, which then
    replaces the zip, so a failure leaves the zip as it was."""
    zip_path = Path(zip_path)
    data = steam_api_bytes(dll_path)
    if not zip_path.is_file():
        fail(f"{zip_path} does not exist")
    try:
        with zipfile.ZipFile(zip_path) as z:
            infos = z.infolist()
            damaged = z.testzip()
    except zipfile.BadZipFile as e:
        fail(f"{zip_path} is not a zip file ({e})")
    if damaged is not None:
        fail(f"{zip_path} is damaged ({damaged}): download it again")
    tops = {i.filename.split("/", 1)[0] for i in infos}
    top = tops.pop() if len(tops) == 1 else ""
    names = [i.filename for i in infos]
    if not top.startswith("h5-together-") or not all(n.startswith(top + "/") for n in names) \
            or f"{top}/dinput8.dll" not in names:
        fail(f"{zip_path} is not an H5 Together package (one folder h5-together-<version> with dinput8.dll in it)")
    name = f"{top}/{STEAM_API_IN_ZIP}"
    if name in names:
        with zipfile.ZipFile(zip_path) as z:
            if z.read(name) != data:
                fail(f"{zip_path} already holds another {STEAM_API_NAME}: start again from the zip without it")
        print(f"{show(zip_path)} already holds this {STEAM_API_NAME}: nothing to do")
        print(f"  sha256 {hashlib.sha256(zip_path.read_bytes()).hexdigest()}")
        return
    stamp = infos[names.index(f"{top}/dinput8.dll")].date_time  # the date of the package's other files

    old = zip_path.read_bytes()
    tmp_path = zip_path.with_name(zip_path.name + ".tmp")
    shutil.copyfile(zip_path, tmp_path)
    try:
        with zipfile.ZipFile(tmp_path, "a", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            z.writestr(zip_entry(name, stamp), data)
        new = tmp_path.read_bytes()
        with zipfile.ZipFile(tmp_path) as z:
            after = z.infolist()
            ok = z.testzip() is None and z.read(name) == data
        # the files that were there: the same entries in the same order, and every byte up
        # to the new file (their headers and compressed data) unchanged
        fields = lambda i: (i.filename, i.CRC, i.compress_size, i.file_size, i.header_offset, i.date_time, i.external_attr)
        ok = ok and [fields(i) for i in after[:-1]] == [fields(i) for i in infos] and after[-1].filename == name
        ok = ok and new[:after[-1].header_offset] == old[:after[-1].header_offset]
        if not ok:
            fail("adding the file changed the zip in a way it should not (nothing was changed)")
        os.replace(tmp_path, zip_path)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()
    print(f"added {name} to {show(zip_path)}")
    print(f"  {len(data):>9}  {name}  (sha256 {hashlib.sha256(data).hexdigest()})")
    print(f"  the {len(infos)} other files are unchanged")
    print(f"  sha256 {hashlib.sha256(new).hexdigest()}  (the new zip)")


def zip_time():
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", time.time()))
    t = time.localtime(max(epoch, 315532800))  # a zip cannot hold dates before 1980
    return (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec - t.tm_sec % 2)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", help="version in the file name and README.txt (default: src/hook/version.h). "
                                      "A release version (no '-') must be the one in version.h")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL, help="the DLL to pack (default build/hook/dinput8.dll)")
    ap.add_argument("--rebuild", action="store_true", help="build the DLL first, even if there is one")
    ap.add_argument("--out-dir", type=Path, default=OUT_DIR, help="where the zip goes (default build/package)")
    ap.add_argument("--steam-api", type=Path, metavar="DLL",
                    help="also pack Valve's steam_api.dll (32-bit) as h5-together/steam_api.dll")
    ap.add_argument("--add-steam-api", type=Path, nargs=2, metavar=("ZIP", "DLL"),
                    help="add Valve's steam_api.dll to a package zip built already (changes ZIP in place; builds nothing)")
    args = ap.parse_args()

    if args.add_steam_api:
        if args.steam_api or args.rebuild or args.version:
            fail("--add-steam-api changes a zip that was built already: it takes no other option")
        add_steam_api(*args.add_steam_api)
        return
    steam_api = steam_api_bytes(args.steam_api) if args.steam_api else None

    if args.version is None:
        args.version = source_version()
    if "-" not in args.version and args.version != source_version():
        fail(f"release {args.version}, but src/hook/version.h says {source_version()}: change one of them")
    if not re.fullmatch(r"[0-9A-Za-z][0-9A-Za-z._+-]*", args.version):
        fail("the version may only hold letters, digits and . _ + -")
    check_shared_blocks()

    if args.rebuild or (args.dll == DEFAULT_DLL and not args.dll.exists()):
        build_dll()
    if not args.dll.exists():
        fail(f"{args.dll} does not exist")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    zip_path = args.out_dir / f"h5-together-{args.version}.zip"
    tmp_path = zip_path.with_suffix(".zip.tmp")
    stamp = zip_time()
    entries = [(name, file_bytes(name, source, args.dll, args.version)) for name, source in CONTENTS]
    entries += [(f"{TEMPLATES_IN_ZIP}/{n}", data) for n, data in template_files()]
    entries += [(f"{TEXTS_IN_ZIP}/{n}", data) for n, data in text_files()]
    if steam_api is not None:
        entries.append((STEAM_API_IN_ZIP, steam_api))
    with zipfile.ZipFile(tmp_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in entries:
            z.writestr(zip_entry(f"h5-together-{args.version}/{name}", stamp), data)  # one top folder
    os.replace(tmp_path, zip_path)

    print(f"wrote {show(zip_path)}")
    with zipfile.ZipFile(zip_path) as z:
        for i in z.infolist():
            print(f"  {i.file_size:>9}  {i.filename}")
    print(f"  sha256 {hashlib.sha256(zip_path.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
